#include "Power.h"
#include <Arduino.h>
#include <esp_sleep.h>

bool Power::wokeFromDeepSleep() {
  // 不是从任何深睡唤醒源醒来的,就说明是冷启动(上电/复位)
  return esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
}

void Power::deepSleepForSeconds(uint64_t sec) {
  Serial.printf("[电源] 进入深睡 %lu 秒(模拟关机)...\n", (unsigned long)sec);
  Serial.flush();                              // 确保上面这行真发出去再睡
  esp_sleep_enable_timer_wakeup(sec * 1000000ULL);  // 定时唤醒(微秒)
  esp_deep_sleep_start();                      // 此后不返回,唤醒会从 setup() 重新开始
}
