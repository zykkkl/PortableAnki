#include <Arduino.h>
#include "Types.h"
#include "SerialDisplay.h"
#include "ReviewSession.h"
#include "Storage.h"
#include "IReviewSink.h"
#include "Clock.h"
#include "NetTime.h"
#include "Power.h"
#include "Sync.h"

// ---- 各模块对象 ----
static Storage      g_storage;
static SerialDisplay g_display;

// 把"评分"转发给存储层持久化,并打印真实 FSRS 结果(IReviewSink 的实现)
class StorageSink : public IReviewSink {
public:
  void onReviewed(const ReviewState& st, Rating rating, long long now) override {
    g_storage.saveReview(st, rating, now, 0 /* timeMs:后续再测 */);
    long long ivl = st.due - now;
    if (ivl < 86400)
      Serial.printf("    [FSRS] state=%d s=%.2f d=%.2f -> next=%ld 秒\n",
                    (int)st.state, st.stability, st.difficulty, (long)ivl);
    else
      Serial.printf("    [FSRS] state=%d s=%.2f d=%.2f -> next=%ld 天\n",
                    (int)st.state, st.stability, st.difficulty, (long)(ivl / 86400));
  }
};
static StorageSink g_sink;

static ReviewSession g_session(&g_display, &g_sink);

// 本轮复习的卡缓冲(从文件载入到这里)
static SessionItem g_buf[MAX_SESSION_CARDS];

static void applyParams() {
  FsrsParams p;
  if (g_storage.loadParams(p)) {
    g_session.setParams(p);
    Serial.printf("[参数] 已载入牌组参数: retention=%.2f, learningSteps=%d, 第1权重=%.4f\n",
                  p.desiredRetention, p.numLearningSteps, p.w[0]);
  } else {
    Serial.println("[参数] 无 decks.json,使用 FSRS-6 默认参数");
  }
}

static void startSession() {
  applyParams();   // 先用导入的牌组参数(若有)
  int n = g_storage.loadDueCards(g_buf, MAX_SESSION_CARDS, Clock::now());
  Serial.printf("[载入] 今日到期卡 %d 张\n", n);
  g_session.begin(g_buf, n, Clock::now());
}

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

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Mini Anki OS - 真 FSRS + 时间 + 存储");
  Serial.println("操作: f=翻面  1-4=评分  z=深睡  x=重置  d=从PC同步");

  // 从深睡唤醒且时间还在 -> 跳过联网;冷启动 -> NTP 对时
  if (Power::wokeFromDeepSleep() && Clock::isSet()) {
    Serial.println("[唤醒] 从深睡唤醒,内部 RTC 已保持,无需联网对时");
  } else {
    if (!NetTime::syncTime()) {
      Serial.println("[警告] 对时失败,使用未校准时间(检查 secrets.h 的 WiFi)");
    }
  }
  Serial.printf("[时间] 当前 Unix 秒=%ld, 已校准=%s\n",
                (long)Clock::now(), Clock::isSet() ? "是" : "否");

  if (!g_storage.begin()) {
    Serial.println("[错误] LittleFS 挂载失败");
    return;
  }
  if (g_storage.seedIfEmpty()) {
    Serial.println("[初始化] 已写入示例卡到 LittleFS");
  }

  startSession();
}

void loop() {
  // 1) 读串口模拟按键 / 命令
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == 'z' || c == 'Z') {           // 深睡(模拟关机),5 秒后唤醒
      Power::deepSleepForSeconds(5);       // 不返回
    }
    if (c == 'x' || c == 'X') {            // 重置:格式化 + 重写示例卡 + 重开会话
      Serial.println("[重置] 格式化 LittleFS 并重写示例卡...");
      g_storage.reset();
      startSession();
      continue;
    }
    if (c == 'd' || c == 'D') {            // 从 PC 下载导入包(同步)并重开会话
      Serial.println("[同步] 开始从 PC 下载导入包...");
      if (Sync::downloadImportPack()) {
        Serial.println("[同步] 下载完成");
        startSession();
      } else {
        Serial.println("[同步] 下载失败(检查 PC 服务、PC_HOST 的 IP、是否同一 WiFi)");
      }
      continue;
    }
    Button b = charToButton(c);
    if (b != BTN_NONE) g_session.handleButton(b, Clock::now());
  }
  // 2) 轮询(处理 learning 卡等待到点后自动出现)
  g_session.update(Clock::now());
  delay(20);
}
