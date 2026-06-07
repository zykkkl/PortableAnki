#include "Scheduler.h"

// 占位实现:用很短的"秒级"间隔,方便在串口里立刻看到 learning 卡重现的效果。
// 真实 FSRS 的间隔是分钟/天级,而且会重新计算 stability/difficulty —— 那是 M2 的事。
GradeResult Scheduler::grade(const ReviewState& st, Rating rating, long long now) {
  GradeResult r;
  r.newStability  = st.stability;     // 占位:暂不改记忆参数
  r.newDifficulty = st.difficulty;
  switch (rating) {
    case RATING_AGAIN:                 // 没记住:几秒后再来(留在今天)
      r.newState = CARD_LEARNING;
      r.nextDue  = now + 5;
      break;
    case RATING_HARD:                  // 吃力:稍久一点再来(留在今天)
      r.newState = CARD_LEARNING;
      r.nextDue  = now + 10;
      break;
    case RATING_GOOD:                  // 记住:毕业,排到一天后(今天不再出现)
      r.newState = CARD_REVIEW;
      r.nextDue  = now + 86400;
      break;
    case RATING_EASY:                  // 很简单:排得更久
      r.newState = CARD_REVIEW;
      r.nextDue  = now + 4 * 86400;
      break;
    default:
      r.newState = st.state;
      r.nextDue  = now + 60;
      break;
  }
  return r;
}
