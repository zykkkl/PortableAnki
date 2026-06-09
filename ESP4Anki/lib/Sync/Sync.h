#pragma once

// 同步模块:连 WiFi,访问 Anki 插件 HTTP 服务。
// PC 地址和导出参数在 secrets.h 配置;下载后写入 LittleFS,然后关 WiFi。
class Sync {
public:
  static bool downloadImportPack();   // 下载 cards/review_state/decks,成功返回 true
  static bool uploadEvents();         // 上传 review_events 到 Anki 插件写回,成功返回 true
  static bool printDecks();           // 拉取 /decks 并打印到串口,用于选择 deckId/deck
};
