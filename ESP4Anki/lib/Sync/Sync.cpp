#include "Sync.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include "secrets.h"   // WIFI_SSID / WIFI_PASSWORD / PC_HOST

#ifndef ANKI_DECK_ID
#define ANKI_DECK_ID 0
#endif

#ifndef ANKI_DECK_NAME
#define ANKI_DECK_NAME ""
#endif

#ifndef ANKI_CARD_LIMIT
#define ANKI_CARD_LIMIT 0
#endif

#ifndef ANKI_INCLUDE_SUBDECKS
#define ANKI_INCLUDE_SUBDECKS 1
#endif

#ifndef ANKI_FORCE_DUE_NOW
#define ANKI_FORCE_DUE_NOW 1
#endif

static bool wifiConnect(unsigned long timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > timeoutMs) return false;
    delay(200);
  }
  return true;
}

static String boolParam(bool v) {
  return v ? "1" : "0";
}

static bool hasDeckName() {
  return String(ANKI_DECK_NAME).length() > 0;
}

static String int64ToString(long long v) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", v);
  return String(buf);
}

static String urlEncode(const String& s) {
  const char* hex = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0x0F];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

static void appendParam(String& query, const String& key, const String& value) {
  if (query.length() > 0) query += "&";
  query += key;
  query += "=";
  query += urlEncode(value);
}

static String deckQuery() {
  String query;
  if (ANKI_DECK_ID > 0) {
    appendParam(query, "deckId", int64ToString((long long)ANKI_DECK_ID));
  } else if (hasDeckName()) {
    appendParam(query, "deck", String(ANKI_DECK_NAME));
  }
  appendParam(query, "includeSubdecks", boolParam(ANKI_INCLUDE_SUBDECKS != 0));
  appendParam(query, "forceDueNow", boolParam(ANKI_FORCE_DUE_NOW != 0));
  return query;
}

static String cardQuery() {
  String query = deckQuery();
  if (ANKI_CARD_LIMIT > 0) {
    appendParam(query, "limit", int64ToString((long long)ANKI_CARD_LIMIT));
  }
  return query;
}

static String withQuery(const String& host, const String& path, const String& query) {
  if (query.length() == 0) return host + path;
  return host + path + "?" + query;
}

// 下载一个 URL 的内容,整体写入 LittleFS 的 path(导入包是小文件,直接读全文)
static bool downloadTo(const String& url, const char* path) {
  HTTPClient http;
  if (!http.begin(url)) {
    Serial.printf("[同步] begin 失败: %s\n", url.c_str());
    return false;
  }
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[同步] GET 失败 code=%d : %s\n", code, url.c_str());
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  File f = LittleFS.open(path, "w");
  if (!f) {
    Serial.printf("[同步] 打开文件失败: %s\n", path);
    return false;
  }
  f.print(body);
  f.close();
  Serial.printf("[同步] %s -> %s (%u 字节)\n", url.c_str(), path, (unsigned)body.length());
  return true;
}

static bool printGet(const String& url) {
  HTTPClient http;
  if (!http.begin(url)) {
    Serial.printf("[同步] begin 失败: %s\n", url.c_str());
    return false;
  }
  int code = http.GET();
  String body = http.getString();
  http.end();
  if (code != 200) {
    Serial.printf("[同步] GET 失败 code=%d : %s\n", code, url.c_str());
    Serial.println(body);
    return false;
  }
  Serial.println(body);
  return true;
}

bool Sync::uploadEvents() {
  Serial.printf("[同步] 连接 WiFi: %s ...\n", WIFI_SSID);
  if (!wifiConnect(15000)) {
    Serial.println("[同步] WiFi 连接超时");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  // 读出待上传的复习记录
  File f = LittleFS.open("/review_events.jsonl", "r");
  if (!f || f.size() == 0) {
    if (f) f.close();
    Serial.println("[同步] 没有待上传的复习记录");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return true;
  }
  String body = f.readString();
  f.close();

  HTTPClient http;
  String url = String(PC_HOST) + "/upload";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  String resp = http.getString();
  http.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  if (code == 200) {
    Serial.printf("[同步] 上传成功: %s\n", resp.c_str());
    LittleFS.remove("/review_events.jsonl");   // 已写回,清空避免重复上传
    return true;
  }
  Serial.printf("[同步] 上传失败 code=%d: %s\n", code, resp.c_str());
  return false;
}

bool Sync::downloadImportPack() {
  Serial.printf("[同步] 连接 WiFi: %s ...\n", WIFI_SSID);
  if (!wifiConnect(15000)) {
    Serial.println("[同步] WiFi 连接超时");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }
  Serial.printf("[同步] 已连接, 设备 IP=%s\n", WiFi.localIP().toString().c_str());

  String host = PC_HOST;   // 例:"http://192.168.1.100:8766"
  String cq = cardQuery();
  String dq = deckQuery();
  Serial.printf("[同步] 牌组参数: deckId=%lld deck=\"%s\" limit=%lld includeSubdecks=%d forceDueNow=%d\n",
                (long long)ANKI_DECK_ID, ANKI_DECK_NAME, (long long)ANKI_CARD_LIMIT,
                (int)(ANKI_INCLUDE_SUBDECKS != 0), (int)(ANKI_FORCE_DUE_NOW != 0));

  bool ok = downloadTo(withQuery(host, "/cards.jsonl", cq),        "/cards.jsonl")
         && downloadTo(withQuery(host, "/review_state.jsonl", cq), "/review_state.jsonl")
         && downloadTo(withQuery(host, "/decks.json", dq),         "/decks.json");

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return ok;
}

bool Sync::printDecks() {
  Serial.printf("[同步] 连接 WiFi: %s ...\n", WIFI_SSID);
  if (!wifiConnect(15000)) {
    Serial.println("[同步] WiFi 连接超时");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }
  Serial.printf("[同步] 已连接, 设备 IP=%s\n", WiFi.localIP().toString().c_str());
  bool ok = printGet(String(PC_HOST) + "/decks");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return ok;
}
