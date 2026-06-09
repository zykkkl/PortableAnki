#include "ReviewSession.h"
#include <Arduino.h>
#include <stdio.h>

ReviewSession::ReviewSession(IDisplay* display, IReviewSink* sink)
  : display_(display), sink_(sink), state_(FLOW_EMPTY), currentIdx_(-1),
    waiting_(false), waitUntil_(0) {}

void ReviewSession::setParams(const FsrsParams& p) {
  scheduler_.setParams(p);
}

void ReviewSession::begin(const SessionItem* items, int n, long long now) {
  queue_.clear();
  for (int i = 0; i < n; i++) queue_.add(items[i]);
  if (queue_.count() == 0) {
    state_ = FLOW_EMPTY;
    display_->showMessage("今日没有到期卡片");
    return;
  }
  advance(now);
}

// 取"到期时间最早"的卡:已到期就显示正面;还没到点就进入等待;队列空就完成。
void ReviewSession::advance(long long now) {
  int idx = queue_.earliestIndex();
  if (idx < 0) {                       // 队列空 -> 今日完成
    state_      = FLOW_DONE;
    currentIdx_ = -1;
    waiting_    = false;
    display_->showMessage("今日复习完成");
    return;
  }
  SessionItem& it = queue_.at(idx);
  if (it.state.due > now) {            // 队首还没到点(learning 卡在等)
    waiting_   = true;
    waitUntil_ = it.state.due;
    char buf[48];
    snprintf(buf, sizeof(buf), "下一张约 %ld 秒后...", (long)(it.state.due - now));
    display_->showMessage(buf);
    return;
  }
  waiting_    = false;                 // 有到期卡:显示正面
  currentIdx_ = idx;
  state_      = FLOW_FRONT;
  display_->showFront(it.card, queue_.count());
}

// 每个 loop 调用:只有在"等 learning 卡到点"时才需要轮询推进。
void ReviewSession::update(long long now) {
  if (waiting_ && now >= waitUntil_) {
    advance(now);
  }
}

void ReviewSession::handleButton(Button b, long long now) {
  switch (state_) {
    case FLOW_FRONT:
      if (b == BTN_FLIP) {                       // 翻面 -> 背面
        state_ = FLOW_BACK;
        display_->showBack(queue_.at(currentIdx_).card);
      }
      // 在正面按评分键:无效(强制先翻面),直接忽略
      break;

    case FLOW_BACK: {
      Rating rating;
      switch (b) {
        case BTN_AGAIN: rating = RATING_AGAIN; break;
        case BTN_HARD:  rating = RATING_HARD;  break;
        case BTN_GOOD:  rating = RATING_GOOD;  break;
        case BTN_EASY:  rating = RATING_EASY;  break;
        default: return;                         // 背面只认评分键,其余忽略
      }
      SessionItem& it  = queue_.at(currentIdx_);
      GradeResult  res = scheduler_.grade(it.state, rating, now);

      // 更新这张卡的状态(内存)
      it.state.state      = res.newState;
      it.state.stability  = res.newStability;
      it.state.difficulty = res.newDifficulty;
      it.state.due        = res.nextDue;
      it.state.step       = res.newStep;
      it.state.lastReview = now;
      it.state.reps      += 1;
      if (rating == RATING_AGAIN) it.state.lapses += 1;

      // 立即持久化:交给注入的 sink(写 review_state + 追加 review_events)
      if (sink_) sink_->onReviewed(it.state, rating, now);

      // 毕业到 Review 就移出今日队列;仍在 Learning/Relearning 则留下当天重现
      if (res.newState == CARD_REVIEW) {
        queue_.removeAt(currentIdx_);
      }
      currentIdx_ = -1;
      advance(now);
      break;
    }

    default:
      break;   // EMPTY / DONE:忽略按键
  }
}
