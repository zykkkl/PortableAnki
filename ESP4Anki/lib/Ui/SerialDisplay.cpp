#include "SerialDisplay.h"
#include <Arduino.h>

void SerialDisplay::showFront(const Card& c, int remaining) {
  Serial.println("\n==============================");
  Serial.printf("[正面]  本轮剩余 %d 张\n", remaining);
  Serial.printf("    %s\n", c.front);
  Serial.println("  -> 输入 f 翻面");
}

void SerialDisplay::showBack(const Card& c) {
  Serial.println("------------------------------");
  Serial.println("[背面]");
  Serial.printf("    正面: %s\n", c.front);
  Serial.printf("    背面: %s\n", c.back);
  Serial.println("  -> 输入 1=Again 2=Hard 3=Good 4=Easy");
}

void SerialDisplay::showMessage(const char* msg) {
  Serial.println("==============================");
  Serial.printf("[提示] %s\n", msg);
}
