# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

PortableAnki(代号 Mini Anki OS)是一个**便携、离线的 Anki 复习设备**:ESP32-S3 + 墨水屏 + 实体按键。设备通过 WiFi 从电脑端 Anki 拉取卡片,**离线用 FSRS-6 算法复习**(像 AnkiDroid 一样本地推进卡片状态),再把评分上传写回 Anki。

三个部分:
- **ESP32-S3 固件**(`ESP4Anki/`):离线复习 + 本地 FSRS-6 调度 + WiFi 同步。
- **Anki 插件 `minianki_bridge`**(`anki-addon/`):在 Anki 内常驻 HTTP 服务,导出卡片给设备 + 接收设备评分写回 Anki。
- **PC 端工具**(`anki-tools/`、`fsrs-check/`):debug console 探针脚本、FSRS 对拍工具。

`PortableAnki_V1_Plan.md`(中文)是设计文档/路线图,但**部分细节已随实现演进**(例如它写"三色屏 + AnkiConnect",实际用黑白屏 + 专用插件)。以代码为准。

## 数据流(核心闭环)

```
Anki + 插件 minianki_bridge(HTTP :8766)  ⇄ WiFi  ⇄  ESP32 设备(LittleFS)
```

- **导出(设备 GET)**:`cards.jsonl`(内容)+ `review_state.jsonl`(每卡 memory state)+ `decks.json`(牌组 FSRS 参数)。
- **写回(设备 POST /upload)**:`review_events.jsonl` → 插件直接写 card 的 `memory_state` + `due` + 补一条 `revlog`。
- **权威性约定**:**Anki 是权威调度者**(参数基于完整历史 Optimize 得来);设备离线是临时调度,同步后以 Anki 为准。因为两端用**同一套 FSRS-6 参数**(从 `decks.json` 同步,不是默认值),设备算的 = Anki 会算的。

## 固件模块(`ESP4Anki/lib/`,各模块一对 .h/.cpp)

- **Core**:`Types.h`(Card/ReviewState/SessionItem/枚举)、`IDisplay.h`/`IReviewSink.h`(接口,解耦显示与存储)。
- **Ui**:`SerialDisplay`(串口模拟显示;墨水屏到货后写 `EinkDisplay` 实现 `IDisplay` 即可替换,状态机不用改)。
- **Review**:`ReviewSession`(复习状态机:FRONT/BACK/EMPTY/DONE)、`ReviewQueue`(到期队列,AnkiDroid 式当天重现)。
- **Scheduler**:`Fsrs`(**纯 C++ FSRS-6 算法**,只依赖 `<math.h>`,对齐 py-fsrs)、`Scheduler`(ReviewState ↔ FsrsCard 转换,持有参数)。
- **Storage**:LittleFS 读写 `cards/review_state/decks/review_events`。
- **Clock**:POSIX 时间。**NetTime**:WiFi + NTP 对时。**Power**:深度睡眠。**Sync**:下载导入包 / 上传评分。
- 入口 `src/main.cpp`:串口命令驱动 —— `f`=翻面 `1-4`=评分 `d`=下载 `u`=上传 `z`=深睡 `x`=重置数据。

## 命令(在 `ESP4Anki/`)

```bash
pio run -d /f/PortableAnki/ESP4Anki              # 编译(工作目录不持久,务必用 -d 指定项目目录)
pio run -d /f/PortableAnki/ESP4Anki -t upload    # 编译并烧录
pio device monitor -d /f/PortableAnki/ESP4Anki   # 串口监视(烧录与监视不能同时占用 COM 口)
```

构建环境:`4d_systems_esp32s3_gen4_r8n16`(借用作通用 ESP32-S3 N16R8 板定义)。

## 关键约定与坑

- **串口走 UART0,不是原生 USB**:板子带 **CH343** USB-UART 桥(`pio device list` 可见)。`platformio.ini` 必须 `-DARDUINO_USB_CDC_ON_BOOT=0`,否则串口在 COM 口上一片空白。
- **lib 模块化**:`lib_ldf_mode = deep+`(各 lib 互相引用,避免"找不到库")。
- **WiFi/PC 凭据**:`lib/NetTime/secrets.h`(已 gitignore),含 `WIFI_SSID`/`WIFI_PASSWORD`/`PC_HOST`(指向插件 `http://<PC_IP>:8766`)。模板见 `secrets.h.example`。PC 和设备需在同一局域网(开发时让 PC 也连手机热点)。
- **FSRS 统一 FSRS-6(21 参数)**:设备和 Anki 用同一组参数。**测试用 N2 牌组**(已 Optimize,21 参数,retention 0.81);N5 是导入成品、无历史、参数是 FSRS-4.5 默认,别用它。
- **改了 `Fsrs.cpp` 必须回归对拍**:`cd fsrs-check && python compare.py`(用 g++ 编 Fsrs.cpp,逐条比对 py-fsrs;关 fuzz)。当前 28/28 通过。
- **时间**:NTP 对一次时 + 内部 RTC + 用深睡代替关机维持(手机模式)。上电复位(RST)会清 RTC 需重新联网;深睡唤醒不清。

## Anki 插件 `minianki_bridge`

- 源码 `anki-addon/minianki_bridge/__init__.py`;安装到 Anki `addons21/`(改动后需重启 Anki)。
- Anki 内起 HTTP `:8766`。**所有 col 操作必须在主线程**,用 `run_on_main_sync` 调度。
- POST `/upload?dry=1` 是**预览模式**(只算不写),用于安全测试。
- `CONFIG` 里设牌组/limit/force_due_now。

## PC 端参考与工具

- `fsrs-check/`:C++ Fsrs vs py-fsrs 自动对拍(`compare.py` + `runner.cpp`)。
- `py-fsrs/`(翻译蓝本,FSRS-6)、`rs-fsrs-c/`(Rust+FFI,**不能上设备**,仅参照)。
- `anki-tools/`:debug console 脚本(`export_debug.py` 导出、`probe_*.py` 写回探针)——已被插件取代,保留作参考。

## 当前进度

- **已通过串口验证**(不依赖屏幕):状态机、本地存储、断电持久化、NTP/RTC/深睡、真 FSRS-6(对拍+实测)、Anki 导入(卡片+memory state+N2 参数)、Anki 写回(插件 GET/POST,dry-run+探针验证)。设备上传代码已就绪。
- **待做(依赖硬件)**:墨水屏显示(写 `EinkDisplay`)+ 实体按键扫描 + 首页菜单;接入硬件后烧录跑完整闭环(`d` 下载 → 评分 → `u` 上传);后期:准确同步(按 `ratedAt` 应用)、microSD、便携供电、3D 外壳。
