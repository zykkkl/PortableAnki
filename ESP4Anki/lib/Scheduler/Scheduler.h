#pragma once
#include "Types.h"

// 调度一次评分的结果
struct GradeResult {
  CardState newState;
  long long nextDue;        // 下次到期(与传入的 now 同一时间基)
  double    newStability;
  double    newDifficulty;
};

// 调度器:把"评分 -> 下次什么时候复习"封装起来。
// M0 是占位实现(固定间隔);M2 换成真正的 FSRS 公式时,这个头文件接口不变。
class Scheduler {
public:
  GradeResult grade(const ReviewState& st, Rating rating, long long now);
};
