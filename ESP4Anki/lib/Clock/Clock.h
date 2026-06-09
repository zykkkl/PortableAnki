#pragma once

// 时间模块:统一管理"现在几点/今天几号/校准时间"。
// 底层用 ESP32 内部 RTC(POSIX time):只要芯片持续供电(含深度睡眠),时间就一直走。
// 校准来源(NTP / 未来的 DS3231 / 同步时由PC给)都通过 setUnix() 注入。
class Clock {
public:
  static long long now();                    // 当前 Unix 秒(UTC)
  static void      setUnix(long long sec);   // 手动校准(供 DS3231 / PC 同步用)
  static bool      isSet();                  // 时间是否已校准(还停在1970则为 false)
  static long long today(int cutoffHour = 4);// "复习日"标识 YYYYMMDD(凌晨cutoff前算前一天)
};
