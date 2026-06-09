#pragma once

// 同步模块:连 WiFi,从 PC 的 HTTP 服务下载导入包(cards.jsonl / review_state.jsonl),
// 写入 LittleFS,然后关 WiFi。PC 地址在 secrets.h 的 PC_HOST。
class Sync {
public:
  static bool downloadImportPack();   // 成功返回 true
};
