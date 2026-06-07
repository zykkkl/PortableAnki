#pragma once
#include "Types.h"

#define MAX_SESSION_CARDS 64

// 会话队列:本轮复习要过的卡。每次取"到期时间最早"的一张。
// M0 用简单数组 + 线性扫描(卡少时足够);卡量大时可换最小堆优化。
class ReviewQueue {
public:
  void clear();
  bool add(const SessionItem& item);          // 加入一张卡
  int  earliestIndex();                        // 返回 due 最小的卡下标;空返回 -1
  int  count() const { return count_; }
  SessionItem& at(int idx) { return items_[idx]; }
  void removeAt(int idx);                       // 毕业:从本轮移除
private:
  SessionItem items_[MAX_SESSION_CARDS];
  int count_ = 0;
};
