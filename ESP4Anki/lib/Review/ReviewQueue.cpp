#include "ReviewQueue.h"

void ReviewQueue::clear() { count_ = 0; }

bool ReviewQueue::add(const SessionItem& item) {
  if (count_ >= MAX_SESSION_CARDS) return false;
  items_[count_++] = item;
  return true;
}

int ReviewQueue::earliestIndex() {
  if (count_ == 0) return -1;
  int best = 0;
  for (int i = 1; i < count_; i++) {
    if (items_[i].state.due < items_[best].state.due) best = i;
  }
  return best;
}

void ReviewQueue::removeAt(int idx) {
  if (idx < 0 || idx >= count_) return;
  // 用最后一个元素填补空位(顺序无所谓,反正每次都重新找最小)
  items_[idx] = items_[count_ - 1];
  count_--;
}
