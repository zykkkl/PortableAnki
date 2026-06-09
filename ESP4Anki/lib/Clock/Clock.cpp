#include "Clock.h"
#include <time.h>
#include <sys/time.h>

// 2020-01-01 00:00:00 UTC 的 Unix 秒。小于它就认为时间还没校准过(开机停在1970)。
static const long long YEAR_2020 = 1577836800LL;

long long Clock::now() {
  return (long long)time(nullptr);
}

void Clock::setUnix(long long sec) {
  struct timeval tv;
  tv.tv_sec  = (time_t)sec;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

bool Clock::isSet() {
  return now() > YEAR_2020;
}

long long Clock::today(int cutoffHour) {
  time_t t = time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);               // 转本地时间(NTP 对时时已设好时区)
  if (lt.tm_hour < cutoffHour) {      // 凌晨 cutoff 之前,算作前一天
    t -= 24 * 3600;
    localtime_r(&t, &lt);
  }
  return (long long)(lt.tm_year + 1900) * 10000
       + (long long)(lt.tm_mon + 1) * 100
       + (long long)lt.tm_mday;       // YYYYMMDD
}
