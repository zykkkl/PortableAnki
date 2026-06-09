#pragma once
#include "Types.h"
#include "IDisplay.h"
#include "Scheduler.h"
#include "ReviewQueue.h"
#include "IReviewSink.h"

// 复习状态机:串起 队列 + 调度器 + 显示,驱动 正面->背面->评分->下一张 的流程。
// 它"拥有"一个调度器和一个队列,并"借用"显示器和评分接收器(由外面传进来的指针)。
class ReviewSession {
public:
  // sink 可空:不传就只复习不写盘(便于纯逻辑测试)
  ReviewSession(IDisplay* display, IReviewSink* sink = nullptr);

  void begin(const SessionItem* items, int n, long long now);  // 载入今日队列,显示第一张
  void setParams(const FsrsParams& p);                         // 设置调度器参数(同步牌组参数)
  void handleButton(Button b, long long now);                  // 处理一次按键事件
  void update(long long now);                                  // 每个 loop 调一次(处理等待)
  FlowState state() const { return state_; }

private:
  void advance(long long now);   // 取下一张到期卡 -> 显示正面 / 等待 / 完成

  IDisplay*    display_;
  IReviewSink* sink_;
  Scheduler    scheduler_;
  ReviewQueue  queue_;
  FlowState   state_;
  int         currentIdx_;       // 当前正在复习的卡在队列里的下标
  bool        waiting_;          // 队首还没到点,正在等
  long long   waitUntil_;        // 等到这个时间(秒)
};
