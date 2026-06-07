#pragma once
#include "Types.h"
#include "IDisplay.h"
#include "Scheduler.h"
#include "ReviewQueue.h"

// 复习状态机:串起 队列 + 调度器 + 显示,驱动 正面->背面->评分->下一张 的流程。
// 它"拥有"一个调度器和一个队列,并"借用"一个显示器(由外面传进来的指针)。
class ReviewSession {
public:
  explicit ReviewSession(IDisplay* display);

  void begin(const SessionItem* items, int n, long long now);  // 载入今日队列,显示第一张
  void handleButton(Button b, long long now);                  // 处理一次按键事件
  void update(long long now);                                  // 每个 loop 调一次(处理等待)
  FlowState state() const { return state_; }

private:
  void advance(long long now);   // 取下一张到期卡 -> 显示正面 / 等待 / 完成

  IDisplay*   display_;
  Scheduler   scheduler_;
  ReviewQueue queue_;
  FlowState   state_;
  int         currentIdx_;       // 当前正在复习的卡在队列里的下标
  bool        waiting_;          // 队首还没到点,正在等
  long long   waitUntil_;        // 等到这个时间(秒)
};
