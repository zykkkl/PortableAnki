#pragma once
#include "Types.h"

// 复习结果"接收器"接口:每评一张卡,状态机就把更新后的状态丢给它,
// 由具体实现决定怎么处理(写盘 / 上传 / 测试时打印)。
// 这样状态机不直接依赖存储层 —— 和 IDisplay 是同一个套路(依赖接口,不依赖实现)。
class IReviewSink {
public:
  virtual ~IReviewSink() {}
  // st:这张卡评分后的最新状态;rating:本次评分;now:评分时刻
  virtual void onReviewed(const ReviewState& st, Rating rating, long long now) = 0;
};
