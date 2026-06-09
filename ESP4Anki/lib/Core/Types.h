#pragma once
#include <stdint.h>

// 这个文件只放"公共数据类型",谁都可能用到,所以单独成一个头文件。
// 注意:不放任何函数实现,纯粹是"长什么样"的定义。

// ===== 评分(对应 Anki 的 ease)=====
enum Rating : uint8_t {
  RATING_AGAIN = 1,
  RATING_HARD  = 2,
  RATING_GOOD  = 3,
  RATING_EASY  = 4,
};

// ===== 物理按键 =====
enum Button : uint8_t {
  BTN_NONE = 0,
  BTN_FLIP,    // 翻面 / 确认
  BTN_AGAIN,
  BTN_HARD,
  BTN_GOOD,
  BTN_EASY,
  BTN_SYNC,    // 同步 / 返回
};

// ===== 复习流程状态(设备界面在哪一步)=====
enum FlowState : uint8_t {
  FLOW_EMPTY,  // 没有到期卡
  FLOW_FRONT,  // 显示正面
  FLOW_BACK,   // 显示背面
  FLOW_DONE,   // 今日完成
};

// ===== FSRS 卡片调度状态 =====
enum CardState : uint8_t {
  CARD_NEW,
  CARD_LEARNING,
  CARD_REVIEW,
  CARD_RELEARNING,
};

// ===== 卡片内容(对应 cards.jsonl 的子集)=====
// front/back 用定长数组(不是指针),这样从文件读出的字符串有地方存。
struct Card {
  long long cardId;
  char      front[64];
  char      back[256];
};

// ===== 卡片复习状态(对应 review_state.jsonl)=====
struct ReviewState {
  long long cardId;
  CardState state;
  double    stability;
  double    difficulty;
  long long due;        // 下次到期时间(与 now 同一时间基)
  int       reps;
  int       lapses;
  int       step;       // learning/relearning 当前步;Review 用 -1。FSRS 需要
  long long lastReview; // 上次复习 Unix 秒;-1 表示从未复习。FSRS 算 elapsed 用
};

// ===== 会话项:把"内容"和"状态"凑成一张待复习的卡 =====
struct SessionItem {
  Card        card;
  ReviewState state;
};
