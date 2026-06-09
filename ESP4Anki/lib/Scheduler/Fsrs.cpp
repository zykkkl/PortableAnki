#include "Fsrs.h"
#include <math.h>

static const double STABILITY_MIN = 0.001;
static const double DIFF_MIN = 1.0;
static const double DIFF_MAX = 10.0;

FsrsParams FsrsParams::defaults() {
  // 与 py-fsrs DEFAULT_PARAMETERS 完全一致(FSRS-6,21 个,末位是 decay)
  static const double W[21] = {
    0.212, 1.2931, 2.3065, 8.2956, 6.4133, 0.8334, 3.0194, 0.001,
    1.8722, 0.1666, 0.796, 1.4835, 0.0614, 0.2629, 1.6483, 0.6014,
    1.8729, 0.5425, 0.0912, 0.0658, 0.1542
  };
  FsrsParams p;
  for (int i = 0; i < 21; i++) p.w[i] = W[i];
  p.desiredRetention   = 0.9;
  p.learningSteps[0]   = 60;   p.learningSteps[1] = 600;  p.numLearningSteps = 2;
  p.relearningSteps[0] = 600;  p.numRelearningSteps = 1;
  p.maximumInterval    = 36500;
  return p;
}

Fsrs::Fsrs(const FsrsParams& p) : p_(p) {
  decay_  = -p_.w[20];
  factor_ = pow(0.9, 1.0 / decay_) - 1.0;
}

double Fsrs::retrievability(const FsrsCard& c, long long now) const {
  if (c.lastReview < 0 || c.stability < 0) return 0.0;
  long long elapsed = (now - c.lastReview) / 86400;   // 整天数
  if (elapsed < 0) elapsed = 0;
  return pow(1.0 + factor_ * (double)elapsed / c.stability, decay_);
}

double Fsrs::initialStability(int rating) const {
  double s = p_.w[rating - 1];
  return s < STABILITY_MIN ? STABILITY_MIN : s;
}

double Fsrs::initialDifficulty(int rating, bool clamp) const {
  double d = p_.w[4] - exp(p_.w[5] * (rating - 1)) + 1.0;
  if (clamp) {
    if (d < DIFF_MIN) d = DIFF_MIN;
    if (d > DIFF_MAX) d = DIFF_MAX;
  }
  return d;
}

long Fsrs::nextIntervalDays(double stability) const {
  double ni = (stability / factor_) * (pow(p_.desiredRetention, 1.0 / decay_) - 1.0);
  long days = (long)llround(ni);
  if (days < 1) days = 1;
  if (days > p_.maximumInterval) days = p_.maximumInterval;
  return days;
}

double Fsrs::shortTermStability(double stability, int rating) const {
  double inc = exp(p_.w[17] * (rating - 3 + p_.w[18])) * pow(stability, -p_.w[19]);
  if (rating == 3 || rating == 4) {       // Good / Easy:不允许缩短
    if (inc < 1.0) inc = 1.0;
  }
  double s = stability * inc;
  return s < STABILITY_MIN ? STABILITY_MIN : s;
}

double Fsrs::nextDifficulty(double difficulty, int rating) const {
  double arg1 = initialDifficulty(4, false);                 // Easy,不夹紧
  double deltaD = -(p_.w[6] * (rating - 3));
  double linearDamping = (10.0 - difficulty) * deltaD / 9.0;
  double arg2 = difficulty + linearDamping;
  double nd = p_.w[7] * arg1 + (1.0 - p_.w[7]) * arg2;        // mean reversion
  if (nd < DIFF_MIN) nd = DIFF_MIN;
  if (nd > DIFF_MAX) nd = DIFF_MAX;
  return nd;
}

double Fsrs::nextStability(double d, double s, double r, int rating) const {
  double ns = (rating == 1) ? nextForgetStability(d, s, r)
                            : nextRecallStability(d, s, r, rating);
  return ns < STABILITY_MIN ? STABILITY_MIN : ns;
}

double Fsrs::nextForgetStability(double d, double s, double r) const {
  double longTerm = p_.w[11] * pow(d, -p_.w[12])
                  * (pow(s + 1.0, p_.w[13]) - 1.0)
                  * exp((1.0 - r) * p_.w[14]);
  double shortTerm = s / exp(p_.w[17] * p_.w[18]);
  return longTerm < shortTerm ? longTerm : shortTerm;
}

double Fsrs::nextRecallStability(double d, double s, double r, int rating) const {
  double hard = (rating == 2) ? p_.w[15] : 1.0;
  double easy = (rating == 4) ? p_.w[16] : 1.0;
  return s * (1.0
            + exp(p_.w[8])
            * (11.0 - d)
            * pow(s, -p_.w[9])
            * (exp((1.0 - r) * p_.w[10]) - 1.0)
            * hard * easy);
}

FsrsCard Fsrs::review(const FsrsCard& card, int rating, long long now) const {
  FsrsCard out = card;

  bool      hasLast    = (card.lastReview >= 0);
  long long daysSince  = hasLast ? (now - card.lastReview) / 86400 : -1;
  bool      sameDayish = hasLast && daysSince < 1;   // 距上次不足一天

  long intervalSec = 0;
  const bool hge = (rating == 2 || rating == 3 || rating == 4);  // Hard/Good/Easy

  if (card.state == FSRS_LEARNING) {
    // --- 更新记忆参数 ---
    if (card.stability < 0) {                         // 新卡:初始化
      out.stability  = initialStability(rating);
      out.difficulty = initialDifficulty(rating, true);
    } else if (sameDayish) {                          // 同日:短期
      out.stability  = shortTermStability(card.stability, rating);
      out.difficulty = nextDifficulty(card.difficulty, rating);
    } else {                                          // 跨日:正常推进
      double r = retrievability(card, now);
      out.stability  = nextStability(card.difficulty, card.stability, r, rating);
      out.difficulty = nextDifficulty(card.difficulty, rating);
    }
    // --- 间隔 + 状态/步转换 ---
    if (p_.numLearningSteps == 0 || (card.step >= p_.numLearningSteps && hge)) {
      out.state = FSRS_REVIEW; out.step = -1;
      intervalSec = nextIntervalDays(out.stability) * 86400L;
    } else if (rating == 1) {                          // Again
      out.step = 0;
      intervalSec = p_.learningSteps[0];
    } else if (rating == 2) {                          // Hard(step 不变)
      if (card.step == 0 && p_.numLearningSteps == 1)
        intervalSec = (long)(p_.learningSteps[0] * 1.5);
      else if (card.step == 0 && p_.numLearningSteps >= 2)
        intervalSec = (p_.learningSteps[0] + p_.learningSteps[1]) / 2;
      else
        intervalSec = p_.learningSteps[card.step];
    } else if (rating == 3) {                          // Good
      if (card.step + 1 == p_.numLearningSteps) {
        out.state = FSRS_REVIEW; out.step = -1;
        intervalSec = nextIntervalDays(out.stability) * 86400L;
      } else {
        out.step = card.step + 1;
        intervalSec = p_.learningSteps[out.step];
      }
    } else {                                           // Easy:直接毕业
      out.state = FSRS_REVIEW; out.step = -1;
      intervalSec = nextIntervalDays(out.stability) * 86400L;
    }

  } else if (card.state == FSRS_REVIEW) {
    if (sameDayish) {
      out.stability = shortTermStability(card.stability, rating);
    } else {
      double r = retrievability(card, now);
      out.stability = nextStability(card.difficulty, card.stability, r, rating);
    }
    out.difficulty = nextDifficulty(card.difficulty, rating);

    if (rating == 1) {                                 // Again -> 进重学
      if (p_.numRelearningSteps == 0) {
        intervalSec = nextIntervalDays(out.stability) * 86400L;   // 无重学步:留 Review
      } else {
        out.state = FSRS_RELEARNING; out.step = 0;
        intervalSec = p_.relearningSteps[0];
      }
    } else {                                           // Hard/Good/Easy
      intervalSec = nextIntervalDays(out.stability) * 86400L;
    }

  } else {  // FSRS_RELEARNING
    if (sameDayish) {
      out.stability  = shortTermStability(card.stability, rating);
      out.difficulty = nextDifficulty(card.difficulty, rating);
    } else {
      double r = retrievability(card, now);
      out.stability  = nextStability(card.difficulty, card.stability, r, rating);
      out.difficulty = nextDifficulty(card.difficulty, rating);
    }
    if (p_.numRelearningSteps == 0 || (card.step >= p_.numRelearningSteps && hge)) {
      out.state = FSRS_REVIEW; out.step = -1;
      intervalSec = nextIntervalDays(out.stability) * 86400L;
    } else if (rating == 1) {                          // Again
      out.step = 0;
      intervalSec = p_.relearningSteps[0];
    } else if (rating == 2) {                          // Hard(step 不变)
      if (card.step == 0 && p_.numRelearningSteps == 1)
        intervalSec = (long)(p_.relearningSteps[0] * 1.5);
      else if (card.step == 0 && p_.numRelearningSteps >= 2)
        intervalSec = (p_.relearningSteps[0] + p_.relearningSteps[1]) / 2;
      else
        intervalSec = p_.relearningSteps[card.step];
    } else if (rating == 3) {                          // Good
      if (card.step + 1 == p_.numRelearningSteps) {
        out.state = FSRS_REVIEW; out.step = -1;
        intervalSec = nextIntervalDays(out.stability) * 86400L;
      } else {
        out.step = card.step + 1;
        intervalSec = p_.relearningSteps[out.step];
      }
    } else {                                           // Easy:毕业
      out.state = FSRS_REVIEW; out.step = -1;
      intervalSec = nextIntervalDays(out.stability) * 86400L;
    }
  }

  out.due        = now + intervalSec;
  out.lastReview = now;
  return out;
}
