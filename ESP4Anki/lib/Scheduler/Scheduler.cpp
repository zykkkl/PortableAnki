#include "Scheduler.h"

Scheduler::Scheduler() : fsrs_(FsrsParams::defaults()) {}
Scheduler::Scheduler(const FsrsParams& p) : fsrs_(p) {}

void Scheduler::setParams(const FsrsParams& p) { fsrs_ = Fsrs(p); }

GradeResult Scheduler::grade(const ReviewState& st, Rating rating, long long now) {
  // 设备 ReviewState -> FsrsCard
  FsrsCard c;
  if (st.state == CARD_NEW) {
    // py-fsrs 没有 New:新卡 = Learning + step0 + stability/difficulty/lastReview 为 None(<0)
    c.state      = FSRS_LEARNING;
    c.step       = 0;
    c.stability  = -1;
    c.difficulty = -1;
    c.lastReview = -1;
  } else {
    c.state      = (int)st.state;   // CARD_LEARNING/REVIEW/RELEARNING == 1/2/3,与 FsrsState 一致
    c.step       = st.step;
    c.stability  = st.stability;
    c.difficulty = st.difficulty;
    c.lastReview = st.lastReview;
  }
  c.due = 0;

  FsrsCard o = fsrs_.review(c, (int)rating, now);

  GradeResult r;
  r.newState      = (CardState)o.state;   // 1/2/3 -> LEARNING/REVIEW/RELEARNING
  r.newStability  = o.stability;
  r.newDifficulty = o.difficulty;
  r.nextDue       = o.due;
  r.newStep       = o.step;
  return r;
}
