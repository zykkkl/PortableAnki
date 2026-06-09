#pragma once

// 联网对时模块:连 WiFi -> 向 NTP 服务器要当前时间 -> 写入内部 RTC(Clock)。
// 对完时即关 WiFi 省电;之后断网也照样准(靠内部 RTC 继续走)。
class NetTime {
public:
  // 连 WiFi + NTP 对时。成功返回 true;超时/失败返回 false。
  static bool syncTime(unsigned long timeoutMs = 15000);
};
