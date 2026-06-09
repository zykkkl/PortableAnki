#include "NetTime.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "Clock.h"
#include "secrets.h"   // 你的 WiFi 账号密码(此文件已被 .gitignore 忽略,不进代码库)

bool NetTime::syncTime(unsigned long timeoutMs) {
  Serial.printf("[NTP] 连接 WiFi: %s ...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // 1) 等 WiFi 连上
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      Serial.println("[NTP] WiFi 连接超时");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return false;
    }
    delay(200);
  }
  Serial.printf("[NTP] WiFi 已连接, IP=%s\n", WiFi.localIP().toString().c_str());

  // 2) 配置 NTP:东八区(8*3600 秒),无夏令时,两个时间服务器
  configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");

  // 3) 等对时成功(time() 跨过 2020 年就说明同步上了)
  start = millis();
  while (!Clock::isSet()) {
    if (millis() - start > timeoutMs) {
      Serial.println("[NTP] 对时超时");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return false;
    }
    delay(200);
  }

  // 4) 打印一下可读时间确认
  time_t t = (time_t)Clock::now();
  struct tm lt;
  localtime_r(&t, &lt);
  Serial.printf("[NTP] 对时成功: %04d-%02d-%02d %02d:%02d:%02d\n",
                lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                lt.tm_hour, lt.tm_min, lt.tm_sec);

  // 5) 时间已写入内部 RTC,关掉 WiFi 省电(之后断网也准)
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return true;
}
