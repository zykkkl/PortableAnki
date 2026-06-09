#include "Storage.h"
#include <Arduino.h>
#include <string.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// 文件路径常量
static const char* CARDS_PATH  = "/cards.jsonl";
static const char* STATE_PATH  = "/review_state.jsonl";
static const char* EVENTS_PATH = "/review_events.jsonl";
static const char* DECKS_PATH  = "/decks.json";

// 把一个 ReviewState 序列化成一行 JSON 文本
static String stateToLine(const ReviewState& st) {
  JsonDocument doc;
  doc["cardId"]     = st.cardId;
  doc["state"]      = (int)st.state;
  doc["s"]          = st.stability;
  doc["d"]          = st.difficulty;
  doc["due"]        = st.due;
  doc["reps"]       = st.reps;
  doc["lapses"]     = st.lapses;
  doc["step"]       = st.step;
  doc["lastReview"] = st.lastReview;
  String line;
  serializeJson(doc, line);
  return line;
}

bool Storage::begin() {
  // 参数 true:挂载失败(如首次使用、未格式化)时自动格式化
  return LittleFS.begin(true);
}

bool Storage::seedIfEmpty() {
  if (LittleFS.exists(CARDS_PATH)) return false;   // 已有数据,不覆盖

  struct Seed { long long id; const char* front; const char* back; };
  static const Seed seeds[] = {
    {1001, "水", "みず / water"},
    {1002, "火", "ひ / fire"},
    {1003, "山", "やま / mountain"},
    {1004, "川", "かわ / river"},
    {1005, "空", "そら / sky"},
  };

  File fc = LittleFS.open(CARDS_PATH, "w");
  File fs = LittleFS.open(STATE_PATH, "w");
  if (!fc || !fs) return false;

  for (const Seed& s : seeds) {
    JsonDocument card;
    card["cardId"] = s.id;
    card["front"]  = s.front;
    card["back"]   = s.back;
    serializeJson(card, fc);
    fc.print("\n");

    JsonDocument st;             // 初始状态:新卡,due=0(立即到期)
    st["cardId"]     = s.id;
    st["state"]      = (int)CARD_NEW;
    st["s"]          = 0.0;
    st["d"]          = 0.0;
    st["due"]        = 0;
    st["reps"]       = 0;
    st["lapses"]     = 0;
    st["step"]       = 0;
    st["lastReview"] = -1;       // 从未复习
    serializeJson(st, fs);
    fs.print("\n");
  }
  fc.close();
  fs.close();
  return true;
}

bool Storage::reset() {
  LittleFS.format();             // 清空整个文件系统
  return seedIfEmpty();          // 重写示例卡
}

bool Storage::loadParams(FsrsParams& out) {
  if (!LittleFS.exists(DECKS_PATH)) return false;
  File f = LittleFS.open(DECKS_PATH, "r");
  if (!f) return false;
  String s = f.readString();
  f.close();

  JsonDocument doc;
  if (deserializeJson(doc, s)) return false;

  JsonArray w = doc["w"].as<JsonArray>();
  int wn = w.size();
  if (wn < 17 || wn > 21) return false;          // 只接受 FSRS-4.5/5/6 的参数个数
  for (int i = 0; i < 21; i++) out.w[i] = (i < wn) ? (double)(w[i] | 0.0) : 0.0;

  out.desiredRetention = doc["desiredRetention"] | 0.9;

  JsonArray ls = doc["learningSteps"].as<JsonArray>();
  out.numLearningSteps = 0;
  for (JsonVariant v : ls) {
    if (out.numLearningSteps >= 8) break;
    out.learningSteps[out.numLearningSteps++] = (long)(v | 0);
  }
  if (out.numLearningSteps == 0) { out.learningSteps[0] = 60; out.learningSteps[1] = 600; out.numLearningSteps = 2; }

  JsonArray rs = doc["relearningSteps"].as<JsonArray>();
  out.numRelearningSteps = 0;
  for (JsonVariant v : rs) {
    if (out.numRelearningSteps >= 8) break;
    out.relearningSteps[out.numRelearningSteps++] = (long)(v | 0);
  }
  if (out.numRelearningSteps == 0) { out.relearningSteps[0] = 600; out.numRelearningSteps = 1; }

  out.maximumInterval = doc["maximumInterval"] | 36500;
  return true;
}

int Storage::loadDueCards(SessionItem* out, int maxItems, long long now) {
  int n = 0;

  // 第一遍:读 review_state,挑出 due <= now 的卡,先填状态、占好位
  File fs = LittleFS.open(STATE_PATH, "r");
  if (fs) {
    while (fs.available() && n < maxItems) {
      String line = fs.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;

      JsonDocument doc;
      if (deserializeJson(doc, line)) continue;     // 解析失败:跳过这行

      long long due = doc["due"] | (long long)0;
      if (due > now) continue;                       // 还没到期:不进今日队列

      ReviewState& st = out[n].state;
      st.cardId     = doc["cardId"] | (long long)0;
      st.state      = (CardState)(int)(doc["state"] | 0);
      st.stability  = doc["s"]   | 0.0;
      st.difficulty = doc["d"]   | 0.0;
      st.due        = due;
      st.reps       = doc["reps"]   | 0;
      st.lapses     = doc["lapses"] | 0;
      st.step       = doc["step"]   | 0;
      st.lastReview = doc["lastReview"] | (long long)-1;

      out[n].card.cardId   = st.cardId;              // 内容稍后第二遍填
      out[n].card.front[0] = '\0';
      out[n].card.back[0]  = '\0';
      n++;
    }
    fs.close();
  }

  // 第二遍:读 cards,按 cardId 把正反面内容填进去
  File fc = LittleFS.open(CARDS_PATH, "r");
  if (fc) {
    while (fc.available()) {
      String line = fc.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;

      JsonDocument doc;
      if (deserializeJson(doc, line)) continue;
      long long id = doc["cardId"] | (long long)0;

      for (int i = 0; i < n; i++) {
        if (out[i].card.cardId == id) {
          strlcpy(out[i].card.front, doc["front"] | "", sizeof(out[i].card.front));
          strlcpy(out[i].card.back,  doc["back"]  | "", sizeof(out[i].card.back));
          break;
        }
      }
    }
    fc.close();
  }

  return n;
}

void Storage::saveReview(const ReviewState& st, Rating rating, long long now, long long timeMs) {
  // 1) 更新 review_state:整文件读进来,替换目标卡那一行,再写回。
  //    TODO: 卡量大时这样全量重写不划算,后续改成临时文件 + rename 或分块。
  String rebuilt;
  bool replaced = false;
  File in = LittleFS.open(STATE_PATH, "r");
  if (in) {
    while (in.available()) {
      String line = in.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;

      JsonDocument doc;
      if (!deserializeJson(doc, line)) {
        long long id = doc["cardId"] | (long long)0;
        if (id == st.cardId) {
          rebuilt += stateToLine(st);      // 用新状态替换
          replaced = true;
          rebuilt += "\n";
          continue;
        }
      }
      rebuilt += line;                     // 其余行原样保留
      rebuilt += "\n";
    }
    in.close();
  }
  if (!replaced) { rebuilt += stateToLine(st); rebuilt += "\n"; }   // 没找到就追加

  File out = LittleFS.open(STATE_PATH, "w");
  if (out) { out.print(rebuilt); out.close(); }

  // 2) 追加一条复习事件
  File ev = LittleFS.open(EVENTS_PATH, "a");
  if (ev) {
    JsonDocument doc;
    doc["cardId"]  = st.cardId;
    doc["ease"]    = (int)rating;
    doc["timeMs"]  = timeMs;
    doc["ratedAt"] = now;
    doc["nextDue"] = st.due;
    serializeJson(doc, ev);
    ev.print("\n");
    ev.close();
  }
}
