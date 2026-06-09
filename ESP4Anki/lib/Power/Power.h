#pragma once
#include <stdint.h>

// 电源管理模块。第一步只做深度睡眠(deep sleep):
// 深睡 = 不是断电,而是芯片进入极低功耗,RTC 域继续供电走时。
// 所以深睡唤醒后,内部 RTC 保持,时间不丢、断网也准 —— 这是"用深睡代替关机"的基础。
class Power {
public:
  static bool wokeFromDeepSleep();           // 本次启动是否来自深睡唤醒(而非上电/复位)
  static void deepSleepForSeconds(uint64_t sec);  // 进入深睡,定时唤醒(不返回)
};
