#include "Sync.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include "secrets.h"   // WIFI_SSID / WIFI_PASSWORD / PC_HOST

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

  String host = PC_HOST;   // 例:"http://192.168.1.100:8000"
  bool ok = downloadTo(host + "/cards.jsonl",        "/cards.jsonl")
         && downloadTo(host + "/review_state.jsonl", "/review_state.jsonl")
         && downloadTo(host + "/decks.json",         "/decks.json");

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return ok;
}
