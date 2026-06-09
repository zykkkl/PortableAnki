#pragma once
#include "Types.h"
#include "Fsrs.h"

// 存储模块:负责把卡片、复习状态、复习事件读写到 LittleFS(板载 Flash 文件系统)。
// 文件(都在 LittleFS 根目录):
//   /cards.jsonl         每行一张卡的内容    {"cardId","front","back"}
//   /review_state.jsonl  每行一张卡的状态    {"cardId","state","s","d","due","reps","lapses"}
//   /review_events.jsonl 每行一次评分(追加) {"cardId","ease","timeMs","ratedAt","nextDue"}
class Storage {
public:
  bool begin();                          // 挂载 LittleFS(失败时自动格式化)
  bool seedIfEmpty();                    // 若没有卡片文件,写入几张示例卡(测试用)
  bool reset();                          // 格式化并重写示例卡(测试时清空重来)

  // 读出"今日到期(due <= now)"的卡,组装成 SessionItem 写入 out[],返回数量。
  int  loadDueCards(SessionItem* out, int maxItems, long long now);

  // 读 decks.json 里的牌组 FSRS 参数到 out。没有该文件返回 false(用默认参数)。
  bool loadParams(FsrsParams& out);

  // 评分后持久化:更新该卡在 review_state 里的行 + 往 review_events 追加一条。
  void saveReview(const ReviewState& st, Rating rating, long long now, long long timeMs);
};
