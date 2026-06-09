# AGENTS.md — PortableAnki 项目指南

本文件面向 AI 编程助手。阅读前默认你对本项目一无所知。

---

## 1. 项目概述

PortableAnki 是一个**便携式离线 Anki 复习终端**。核心目标：制作一台基于 ESP32-S3 的专用学习硬件，配备电子墨水屏和实体按键，让用户脱离电脑、脱离网络也能按 Anki/FSRS 算法复习卡片，联网后再把离线进度同步回桌面端 Anki。

项目由以下几条线组成：

- **ESP32-S3 固件** (`ESP4Anki/`)：设备端主程序，C++ / Arduino 框架 / PlatformIO。
- **FSRS 算法**
  - `py-fsrs/`：官方 Python 实现（MIT 协议），用于在 PC 端验证和参考。
  - `rs-fsrs-c/`：Rust FFI 封装，把 `rs-fsrs` 库导出为 C 头文件（供其他 C/C++ 调用）。
  - `ESP4Anki/lib/Scheduler/`：设备端独立的 C++ 精简实现（与 py-fsrs 逐条对拍验证）。
- **PC 端工具**
  - `anki-addon/`：Anki 桥接插件，在 Anki 内提供 HTTP 服务供设备下载/上传。
  - `anki-tools/`：Anki Debug Console 脚本，从桌面端 Anki 导出卡片 + FSRS 参数。
- **对拍验证** (`fsrs-check/`)：Python 脚本 + C++ runner，验证设备端 C++ FSRS 与官方 py-fsrs 输出一致。

> **注意**：`docs/archive/CLAUDE.md` 目前处于过期归档状态（它声称 `main.cpp` 仍是 PlatformIO 模板，实际上已完成大量开发），应以本文件和 `docs/PortableAnki_V1_Plan.md` 为准。

---

## 2. 目录结构与模块划分

```text
PortableAnki/
├── ESP4Anki/              # 设备固件（PlatformIO 项目）
│   ├── src/main.cpp       # 入口：初始化、按键/串口命令、主循环
│   ├── lib/               # 按模块划分的库（PlatformIO 自动编译）
│   │   ├── Core/          # 公共类型与接口（Types.h、IReviewSink.h）
│   │   ├── Ui/            # 显示抽象（IDisplay）+ 串口回显实现（SerialDisplay）
│   │   ├── Review/        # 复习状态机（ReviewSession）+ 会话队列（ReviewQueue）
│   │   ├── Scheduler/     # FSRS-6 C++ 实现（Fsrs.h/cpp）+ 调度器包装（Scheduler.h/cpp）
│   │   ├── Storage/       # LittleFS 文件读写（cards/review_state/review_events jsonl）
│   │   ├── Sync/          # Wi-Fi HTTP 对接 Anki 插件下载/上传
│   │   ├── NetTime/       # Wi-Fi 连网 + NTP 对时（secrets.h 放 WiFi 密码）
│   │   ├── Clock/         # 内部 RTC 时间封装
│   │   └── Power/         # 深度睡眠管理（用深睡代替关机，RTC 保持走时）
│   └── platformio.ini     # 板级配置：4d_systems_esp32s3_gen4_r8n16
├── py-fsrs/               # 官方 Python FSRS 实现（可独立使用）
│   ├── fsrs/              # 核心包：scheduler.py、card.py、rating.py、state.py…
│   └── tests/             # pytest 测试集
├── rs-fsrs-c/             # Rust FFI -> C 头文件生成
│   ├── src/               # Rust 源码（lib.rs、fsrs_def.rs）
│   ├── include/fsrs.h     # cbindgen 生成的 C 头（自动产出，勿手动改）
│   ├── build.rs           # cbindgen 调用
│   └── build.sh           # 一键编译 + 运行 C 示例 + 生成覆盖率
├── anki-addon/            # Anki 插件（minianki_bridge）
│   ├── minianki_bridge/   # 插件源码与默认配置
│   └── dist/              # 本地 .ankiaddon 打包产物（gitignored）
├── anki-tools/            # Anki Debug Console 辅助脚本
│   ├── export_debug.py
│   └── probe_writeback.py
├── fsrs-check/            # C++ FSRS 与 py-fsrs 对拍工具
│   ├── compare.py
│   └── runner.cpp
├── docs/                  # 规划、剩余功能和归档文档
│   ├── PortableAnki_V1_Plan.md
│   ├── PortableAnki_Remaining_Work.md
│   └── archive/CLAUDE.md
```

### 2.1 固件架构原则

- **接口隔离**：`IDisplay`（显示）、`IReviewSink`（评分接收）都是纯虚接口，状态机只依赖接口，不依赖具体实现。
- **分层**：UI 层 → 复习流程层（ReviewSession/ReviewQueue）→ 调度层（Scheduler/Fsrs）→ 存储层（Storage/LittleFS）→ 同步层（Sync/NetTime）。
- ** LittleFS 为默认存储**：目前使用板载 Flash + LittleFS；后续可扩展到 microSD。

---

## 3. 技术栈

| 层级 | 技术 |
|---|---|
| 设备固件 | C++17、Arduino 框架、PlatformIO |
| 设备依赖库 | ArduinoJson (^7)、LittleFS（框架自带）、ESP32 WiFi |
| 目标硬件 | ESP32-S3（4D Systems GEN4-ESP32 等带 PSRAM 版本优先） |
| 显示 | 目前用串口回显（SerialDisplay），后续接入 GxEPD2 驱动电子墨水屏 |
| FSRS 参考实现 | Python 3.10+（py-fsrs） |
| FSRS FFI | Rust + cbindgen → C header |
| PC 端脚本 | Python（依赖 Anki 内置环境，如 `aqt`、`mw`） |
| 数据交换格式 | JSON Lines（`.jsonl`）+ JSON（`.json`） |

---

## 4. 构建与运行命令

### 4.1 设备固件（ESP4Anki）

在 `ESP4Anki/` 目录下执行：

```bash
# 编译
pio run

# 编译并烧录到开发板
pio run -t upload

# 打开串口监视器（波特率 115200）
pio device monitor

# 清理构建产物
pio run -t clean
```

环境名：`4d_systems_esp32s3_gen4_r8n16`。如果新增环境，用 `-e <env>` 指定。

> **首次使用前**：复制 `lib/NetTime/secrets.h.example`（如有）或新建 `lib/NetTime/secrets.h`，填入 `WIFI_SSID`、`WIFI_PASSWORD`、`PC_HOST`，并按需配置 `ANKI_DECK_ID` / `ANKI_DECK_NAME` / `ANKI_CARD_LIMIT`。该文件已被 `.gitignore` 忽略，不会提交到版本库。

### 4.2 py-fsrs（Python FSRS 参考实现）

在 `py-fsrs/` 目录下：

```bash
# 安装（开发依赖包含 pytest、ruff 等）
pip install -e ".[dev]"

# 运行测试
pytest

# 代码检查
ruff check fsrs/
```

### 4.3 rs-fsrs-c（Rust FFI）

在 `rs-fsrs-c/` 目录下：

```bash
# 编译 Rust 库 + 生成 C 头
cargo build

# 一键编译、运行 C 示例、生成覆盖率报告
./build.sh
```

生成的头文件位于 `include/fsrs.h`，由 `cbindgen` 自动产出，**不要手动修改**。

### 4.4 FSRS 对拍验证

在 `fsrs-check/` 目录下：

```bash
python compare.py
```

它会：
1. 编译 `runner.cpp` + `ESP4Anki/lib/Scheduler/Fsrs.cpp`；
2. 同一批用例在 py-fsrs 上跑一遍；
3. 逐条比对 state、step、stability、difficulty、间隔。

输出 `X passed / Y failed`，失败时打印差异详情。

---

## 5. 代码风格与约定

### 5.1 C++（固件）

- 文件扩展名：`.cpp` / `.h`。
- 头文件使用 `#pragma once`。
- 命名风格：
  - 类名：`PascalCase`（`ReviewSession`、`FsrsParams`）
  - 函数/变量：`camelCase`（`loadDueCards`、`numLearningSteps`）
  - 宏/常量：`UPPER_SNAKE_CASE`（`MAX_SESSION_CARDS`、`RATING_AGAIN`）
  - 接口类前缀 `I`：`IDisplay`、`IReviewSink`
- 注释使用中文（因为团队主要使用中文沟通）。
- 数据类型偏好：固件中时间戳用 `long long`（Unix 秒）；浮点用 `double`。
- 不抛异常：错误用返回值（`bool`）或串口日志表达。
- 内存：避免动态内存（`new`/`delete`）在核心路径频繁使用；`SessionItem` 用定长数组，`ReviewQueue` 上限 `MAX_SESSION_CARDS`（默认 64）。

### 5.2 Python

- 遵循 `py-fsrs` 原有风格：PEP 8、类型注解、`from __future__ import annotations`。
- 使用 `ruff` 做 lint。
- Anki 导出脚本（`anki-tools/export_debug.py`）直接在 Anki Debug Console 运行，依赖 Anki 运行时环境（`aqt`、`mw`），**不能在外部普通 Python 环境直接运行**。

### 5.3 Rust

- `rs-fsrs-c` 是薄封装层，核心逻辑在依赖 `rs-fsrs`（crate）中。
- 使用 `cbindgen` 生成 C 绑定；`build.rs` 在编译时自动生成头文件。
- `#![deny(warnings)]`，不允许警告。

---

## 6. 测试策略

### 6.1 单元/集成测试

| 项目 | 测试方式 | 入口 |
|---|---|---|
| py-fsrs | pytest | `py-fsrs/tests/test_basic.py`、`test_optimizer.py` |
| C++ FSRS | 对拍脚本 | `fsrs-check/compare.py` |
| rs-fsrs-c | 运行 C 示例 | `rs-fsrs-c/build.sh` |

### 6.2 固件本地验证流程

1. 烧录固件后打开串口监视器。
2. 按以下字符操作（串口模拟按键）：
   - `f` — 翻面
   - `1/2/3/4` — 评分 Again/Hard/Good/Easy
   - `z` — 深睡 5 秒后唤醒（验证 RTC 保持）
   - `x` — 格式化 LittleFS 并重写示例卡（清数据重来）
   - `l` — 从 Anki 插件读取并打印卡组列表
   - `d` — 从 Anki 插件下载卡片/状态/参数（需 PC 在同一 WiFi 并已打开 Anki）
   - `u` — 上传离线复习记录并写回 Anki
3. 观察串口输出的 `[FSRS] state=... s=... d=... -> next=...` 验证调度结果。

### 6.3 数据一致性验证

- 每次修改 `ESP4Anki/lib/Scheduler/Fsrs.cpp` 后，**必须**运行 `fsrs-check/compare.py`，确保与 py-fsrs 输出一致。
- 若引入新参数或改算法，同步更新 `FsrsParams::defaults()` 和 `DEFAULT_PARAMETERS`。

---

## 7. 数据格式与文件约定

设备端 LittleFS（或 microSD）使用以下文件：

| 文件 | 格式 | 说明 |
|---|---|---|
| `/cards.jsonl` | JSON Lines | 卡片内容：`cardId`、`front`、`back` |
| `/review_state.jsonl` | JSON Lines | 卡片状态：`cardId`、`state`、`s`、`d`、`due`、`reps`、`lapses`、`step`、`lastReview` |
| `/review_events.jsonl` | JSON Lines（追加） | 评分事件：`cardId`、`ease`、`timeMs`、`ratedAt`、`nextDue` |
| `/decks.json` | JSON | 牌组 FSRS 参数快照：`w`（21 个权重）、`desiredRetention`、`learningSteps`、`relearningSteps`、`maximumInterval` |

### 7.1 状态枚举（与 py-fsrs 保持一致）

- `0` = New（新卡）
- `1` = Learning（学习中）
- `2` = Review（复习中）
- `3` = Relearning（重新学习）

### 7.2 评分枚举

- `1` = Again
- `2` = Hard
- `3` = Good
- `4` = Easy

---

## 8. 安全与隐私注意事项

- **Wi-Fi 凭据**：保存在 `ESP4Anki/lib/NetTime/secrets.h`，该路径已被 `.gitignore` 排除，**切勿**将其提交到版本库。
- **Anki 数据**：`anki-tools/export_debug.py` 是只读脚本，不会修改 Anki 集合，但在共享导出文件时注意卡片内容隐私。
- **设备端无加密**：LittleFS 上的卡片文本和复习记录以明文 JSON/JSONL 存储，若设备丢失存在数据泄露风险。当前版本（V1）不解决此问题。
- **HTTP 明文传输**：设备通过 HTTP（非 HTTPS）从局域网 PC 下载导入包，仅在受信任的内网使用。

---

## 9. 开发流程与关键决策

### 9.1 如何新增固件模块

1. 在 `ESP4Anki/lib/` 下新建目录（如 `lib/MyModule/`）。
2. 放置 `.h` + `.cpp`，头文件可被其他模块 `#include`。
3. `platformio.ini` 已设置 `lib_ldf_mode = deep+`，库依赖会自动发现。
4. 如使用第三方库，在 `platformio.ini` 的 `lib_deps` 中添加。

### 9.2 修改 FSRS 算法时的 checklist

- [ ] 同步修改 `Fsrs.cpp` 的数学公式
- [ ] 更新 `FsrsParams::defaults()` 的默认值（若默认值变动）
- [ ] 运行 `fsrs-check/compare.py` 确保与 py-fsrs 一致
- [ ] 更新 `docs/PortableAnki_V1_Plan.md` 中相关章节（若设计文档涉及）

### 9.3 设备与 PC 的同步流程（当前实现）

1. PC 打开 Anki 并启用 `anki-addon/minianki_bridge` 插件（默认端口 8766）。
2. 设备串口发送 `l`，通过 `/decks` 查看卡组列表；把选定的 `deckId` 或 `deck` 写入 `secrets.h`。
3. 设备串口发送 `d`，`Sync::downloadImportPack()` 通过 WiFi 下载 `cards.jsonl`、`review_state.jsonl`、`decks.json`。
4. 下载成功后写入 LittleFS，随后重载今日队列。
5. 离线复习后串口发送 `u`，`Sync::uploadEvents()` 上传 `review_events.jsonl` 并由插件写回 Anki。

---

## 10. 参考文档

- `docs/PortableAnki_V1_Plan.md`：硬件方案、接线示例、外壳设计、十阶段开发路线图（中文）。
- `docs/PortableAnki_Remaining_Work.md`：当前闭环和剩余未实现功能清单。
- `ESP4Anki/platformio.ini`：固件编译配置、第三方库依赖。
- `py-fsrs/README.md`：Python FSRS 的 API 说明。

---

*最后更新：2026-06-09*
