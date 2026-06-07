#include <Arduino.h>
#include "Types.h"
#include "SerialDisplay.h"
#include "ReviewSession.h"

// ---- 写死的假卡(M0 测试用;M1 起改成从 LittleFS 的 cards/review_state 读取)----
// SessionItem = { Card{cardId, 正面, 背面}, ReviewState{cardId, 状态, s, d, due, reps, lapses} }
// due = 0 表示"立即到期",一开机就会出现。
static SessionItem g_items[] = {
  { { 1001, "\xE6\xB0\xB4", "\xE3\x81\xBF\xE3\x81\x9A / water" },    { 1001, CARD_NEW, 0, 0, 0, 0, 0 } },
  { { 1002, "\xE7\x81\xAB", "\xE3\x81\xB2 / fire" },                { 1002, CARD_NEW, 0, 0, 0, 0, 0 } },
  { { 1003, "\xE5\xB1\xB1", "\xE3\x82\x84\xE3\x81\xBE / mountain" }, { 1003, CARD_NEW, 0, 0, 0, 0, 0 } },
};
static const int g_itemCount = sizeof(g_items) / sizeof(g_items[0]);

// 这两个对象的"组装"就发生在这里:把串口显示器交给状态机。
static SerialDisplay g_display;
static ReviewSession g_session(&g_display);

// 屏幕/实体按键到货前,用串口里敲的字符模拟按键。
static Button charToButton(char c) {
  switch (c) {
    case 'f': case 'F': return BTN_FLIP;
    case '1': return BTN_AGAIN;
    case '2': return BTN_HARD;
    case '3': return BTN_GOOD;
    case '4': return BTN_EASY;
    case 's': case 'S': return BTN_SYNC;
    default:  return BTN_NONE;
  }
}

// 当前时间(秒)。M0 先用开机毫秒数凑合;真实需要 RTC/NTP 才能跨断电、按真实日期。
static long long nowSec() { return (long long)(millis() / 1000); }

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Mini Anki OS - 复习状态机串口测试");
  Serial.println("操作: f=翻面  1=Again 2=Hard 3=Good 4=Easy");
  g_session.begin(g_items, g_itemCount, nowSec());
}

void loop() {
  // 1) 读串口模拟按键
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    Button b = charToButton(c);
    if (b != BTN_NONE) g_session.handleButton(b, nowSec());
  }
  // 2) 轮询(处理 learning 卡等待到点后自动出现)
  g_session.update(nowSec());
  delay(20);
}
