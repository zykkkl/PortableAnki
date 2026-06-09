#pragma once
#include "Types.h"
#include "Fsrs.h"

// 调度一次评分的结果
struct GradeResult {
  CardState newState;
  long long nextDue;        // 下次到期(与传入的 now 同一时间基)
  double    newStability;
  double    newDifficulty;
  int       newStep;        // 新的 learning/relearning 步;Review 为 -1
};

// 调度器:把"评分 -> 下次什么时候复习"封装起来。
// 内部用真正的 FSRS-6(Fsrs),并负责设备 ReviewState <-> FsrsCard 的转换。
class Scheduler {
public:
  Scheduler();                            // 用 FSRS-6 默认 21 参数
  explicit Scheduler(const FsrsParams& p);// M3 导入时可换成 Anki 牌组的实际参数
  void setParams(const FsrsParams& p);    // 运行时切换参数(同步到牌组实际参数)
  GradeResult grade(const ReviewState& st, Rating rating, long long now);
private:
  Fsrs fsrs_;
};
