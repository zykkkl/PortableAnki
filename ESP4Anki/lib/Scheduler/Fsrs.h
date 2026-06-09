#pragma once

// 纯 C++ 的 FSRS-6 调度算法 —— 只依赖 <math.h>,不依赖 Arduino。
// 完整复刻 py-fsrs 的 Scheduler.review_card(关闭 fuzz),供:
//   1) 设备固件使用
//   2) 在 PC 上用 g++ 单独编译,和 py-fsrs 逐条对拍验证
//
// 状态约定与 py-fsrs 完全一致:Learning=1, Review=2, Relearning=3。
// 新卡 = Learning 状态 + step=0 + stability<0(表示尚未初始化,对应 py-fsrs 的 None)。

enum FsrsStateE {
  FSRS_LEARNING   = 1,
  FSRS_REVIEW     = 2,
  FSRS_RELEARNING = 3,
};

struct FsrsParams {
  double w[21];                // FSRS-6 的 21 个权重
  double desiredRetention;     // 目标保留率,例 0.9
  long   learningSteps[8];     // 学习步(秒);默认 {60, 600}
  int    numLearningSteps;
  long   relearningSteps[8];   // 重学步(秒);默认 {600}
  int    numRelearningSteps;
  long   maximumInterval;      // 最大间隔(天);默认 36500

  static FsrsParams defaults();   // FSRS-6 官方默认 21 参数 + 默认步长
};

struct FsrsCard {
  int       state;       // FsrsStateE
  int       step;        // learning/relearning 当前步;Review 用 -1
  double    stability;   // <0 表示 None(新卡)
  double    difficulty;  // <0 表示 None
  long long lastReview;  // 上次复习 Unix 秒;<0 表示 None(从未复习)
  long long due;         // 输出:下次到期 Unix 秒
};

class Fsrs {
public:
  explicit Fsrs(const FsrsParams& p);

  // 评分一次:输入评分前的卡 + 评分(1..4)+ 当前时间,返回评分后的新卡。
  FsrsCard review(const FsrsCard& card, int rating, long long now) const;

private:
  FsrsParams p_;
  double decay_;
  double factor_;

  double retrievability(const FsrsCard& c, long long now) const;
  double initialStability(int rating) const;
  double initialDifficulty(int rating, bool clamp) const;
  long   nextIntervalDays(double stability) const;
  double shortTermStability(double stability, int rating) const;
  double nextDifficulty(double difficulty, int rating) const;
  double nextStability(double d, double s, double r, int rating) const;
  double nextForgetStability(double d, double s, double r) const;
  double nextRecallStability(double d, double s, double r, int rating) const;
};
