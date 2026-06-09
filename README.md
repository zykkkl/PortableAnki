# PortableAnki

<p align="center">
  <b>A portable, offline Anki review terminal — powered by ESP32-S3 & E-ink</b><br>
  <b>基于 ESP32-S3 与电子墨水屏的便携式离线 Anki 复习终端</b>
</p>

---

## 🇬🇧 English

### What is PortableAnki?

PortableAnki is a dedicated hardware device for Anki/FSRS spaced-repetition review. It lets you study flashcards **without a computer and without internet**, then sync your offline progress back to desktop Anki when you reconnect.

**Core principle:**
```
Offline: Mini Anki OS advances the local collection autonomously
Online:  Sync local changes and merge with desktop Anki state
```

### Current Progress

| Module | Status | Description |
|--------|--------|-------------|
| **FSRS-6 Scheduler** | ✅ Complete | Full C++ implementation of new / learning / review / relearning states; cross-checked against py-fsrs line-by-line |
| **Review State Machine** | ✅ Complete | Flip → Rate (Again/Hard/Good/Easy) → Next card; learning cards reappear automatically after their short step interval |
| **LittleFS Storage** | ✅ Complete | `cards.jsonl`, `review_state.jsonl`, `review_events.jsonl` read/write; data survives power loss |
| **Wi-Fi Import** | ✅ Complete | HTTP download of import packs from a PC on the same LAN |
| **Time & RTC** | ✅ Complete | NTP sync on cold boot; deep-sleep RTC retention means no reconnection needed after wake |
| **Power Management** | ✅ Complete | Deep-sleep instead of power-off; RTC keeps ticking |
| **PC Export Tool** | ✅ Complete | `anki-tools/export_debug.py` runs inside Anki Debug Console to export decks + FSRS params |
| **FSRS Cross-Check** | ✅ Complete | `fsrs-check/compare.py` compiles and validates C++ output against official py-fsrs |
| **Rust FFI** | ✅ Complete | `rs-fsrs-c` wraps `rs-fsrs` via cbindgen into a C header |
| **Serial UI** | 🔄 In Progress | Serial echo display for pre-hardware debugging; E-ink driver (GxEPD2) integration pending screen arrival |
| **Physical Buttons** | 🔄 In Progress | Emulated via serial characters (`f` / `1` / `2` / `3` / `4`) until hardware buttons are wired |
| **Anki Plugin (Bridge)** | ✅ Complete | `anki-addon/minianki_bridge` runs an HTTP service inside Anki: GET exports cards/state/params, POST writes review events back (memory state + due + revlog) |
| **Two-Way Sync** | 🔄 In Progress | Write-back path verified (plugin + device upload code ready); full round-trip pending hardware |
| **Backup & Restore** | ⏳ Planned | Full device backup JSON export/import |

### Target Hardware

| Component | Recommendation |
|-----------|----------------|
| MCU | ESP32-S3 with PSRAM (e.g. 4D Systems GEN4-ESP32S3) |
| Display | 2.9" E-ink (black/white preferred for speed; tri-color supported later) |
| Storage | On-board Flash + LittleFS for V1-lite; microSD for V1-full |
| Input | 6 tactile buttons (Flip, Again, Hard, Good, Easy, Sync) |
| Battery | 3.7 V single-cell Li-Po, 1000–2000 mAh |

### Quick Start (Firmware)

```bash
cd ESP4Anki

# Build
pio run

# Flash to board
pio run -t upload

# Monitor serial output (115200 baud)
pio device monitor
```

**First-time setup:** create `ESP4Anki/lib/NetTime/secrets.h` with your Wi-Fi credentials and PC IP (see `.gitignore` — never commit this file).

### Serial Commands (Pre-Button Phase)

| Key | Action |
|-----|--------|
| `f` | Flip card |
| `1` | Rate **Again** |
| `2` | Rate **Hard** |
| `3` | Rate **Good** |
| `4` | Rate **Easy** |
| `z` | Deep sleep 5 s (test RTC retention) |
| `x` | Format LittleFS and rewrite demo cards |
| `d` | Download import pack from PC over Wi-Fi |
| `u` | Upload review events back to Anki |

### Data Formats on Device

| File | Format | Content |
|------|--------|---------|
| `/cards.jsonl` | JSON Lines | Card ID, front, back |
| `/review_state.jsonl` | JSON Lines | Card state, stability, difficulty, due, reps, lapses, step |
| `/review_events.jsonl` | JSON Lines (append-only) | Rating events: ease, time, ratedAt, nextDue |
| `/decks.json` | JSON | FSRS parameter snapshot: `w` (21 weights), `desiredRetention`, `learningSteps`, `relearningSteps`, `maximumInterval` |

### Repository Layout

```
PortableAnki/
├── ESP4Anki/          # Device firmware (PlatformIO)
│   ├── src/main.cpp
│   └── lib/           # Modular libraries: Core, Ui, Review, Scheduler, Storage, Sync, NetTime, Clock, Power
├── py-fsrs/           # Official Python FSRS reference implementation (MIT)
├── rs-fsrs-c/         # Rust FFI → C header (cbindgen)
├── anki-addon/        # Anki plugin (minianki_bridge): HTTP export + write-back
├── fsrs-check/        # C++ vs py-fsrs cross-check tool
├── anki-tools/        # Anki Debug Console scripts (export / write-back probe)
└── import-pack/       # Exported data (gitignored — generated at runtime)
```

### Validation

After any change to `ESP4Anki/lib/Scheduler/Fsrs.cpp`, run:

```bash
cd fsrs-check
python compare.py
# Expected: X passed / 0 failed
```

---

## 🇨🇳 中文

### PortableAnki 是什么？

PortableAnki 是一台**便携式离线 Anki 复习终端**。它基于 ESP32-S3 与电子墨水屏，让你**不带电脑、不上网**也能按 Anki/FSRS 算法复习卡片；联网后再把离线进度同步回桌面端 Anki。

**核心理念：**
```
离线时：Mini Anki OS 本地集合自主推进，像 AnkiDroid 一样可持续复习
联网后：同步本地集合变更，并与 Anki 端状态合并
```

### 当前开发进度

| 模块 | 状态 | 说明 |
|------|------|------|
| **FSRS-6 调度器** | ✅ 已完成 | 完整实现新卡/学习中/复习/重新学习四状态；与 py-fsrs 逐条对拍验证 |
| **复习状态机** | ✅ 已完成 | 翻面 → 评分 → 下一张；学习中卡片按短步长自动重现 |
| **LittleFS 存储** | ✅ 已完成 | `cards.jsonl`、`review_state.jsonl`、`review_events.jsonl` 读写；断电不丢数据 |
| **Wi-Fi 导入** | ✅ 已完成 | 通过 HTTP 从局域网 PC 下载导入包 |
| **时间与 RTC** | ✅ 已完成 | 冷启动 NTP 对时；深睡后 RTC 保持，唤醒无需再联网 |
| **电源管理** | ✅ 已完成 | 深睡代替关机，RTC 持续走时 |
| **PC 导出脚本** | ✅ 已完成 | `anki-tools/export_debug.py` 在 Anki Debug Console 内运行，导出牌组与 FSRS 参数 |
| **FSRS 对拍验证** | ✅ 已完成 | `fsrs-check/compare.py` 编译并验证 C++ 输出与官方 py-fsrs 一致 |
| **Rust FFI** | ✅ 已完成 | `rs-fsrs-c` 通过 cbindgen 将 `rs-fsrs` 封装为 C 头文件 |
| **串口 UI** | 🔄 进行中 | 串口回显用于无屏调试；GxEPD2 墨水屏驱动待屏幕到货后接入 |
| **实体按键** | 🔄 进行中 | 目前通过串口字符模拟（`f` / `1` / `2` / `3` / `4`），待硬件焊接 |
| **Anki 插件(桥接)** | ✅ 已完成 | `anki-addon/minianki_bridge` 在 Anki 内起 HTTP 服务:GET 导出卡片/状态/参数,POST 把评分写回(memory state + due + revlog) |
| **双向同步** | 🔄 进行中 | 写回链路已验证(插件 + 设备上传代码就绪);完整闭环待硬件 |
| **备份与恢复** | ⏳ 计划中 | 完整设备备份 JSON 导出/导入 |

### 目标硬件

| 模块 | 建议 |
|------|------|
| 主控 | ESP32-S3 带 PSRAM（如 4D Systems GEN4-ESP32S3） |
| 屏幕 | 2.9 寸电子墨水屏；V1 优先黑白双色（刷新更快），三色屏后续支持 |
| 存储 | V1-lite 用板载 Flash + LittleFS；V1-full 加 microSD |
| 按键 | 6 个轻触按键（翻面、Again、Hard、Good、Easy、同步） |
| 电池 | 3.7 V 单节锂电，1000–2000 mAh |

### 快速开始（固件）

```bash
cd ESP4Anki

# 编译
pio run

# 烧录到开发板
pio run -t upload

# 打开串口监视器（波特率 115200）
pio device monitor
```

**首次使用：**新建 `ESP4Anki/lib/NetTime/secrets.h` 填入 Wi-Fi 密码与 PC IP（该路径已被 `.gitignore` 忽略，切勿提交到版本库）。

### 串口操作（实体按键到货前）

| 按键 | 功能 |
|------|------|
| `f` | 翻面 |
| `1` | 评分 **Again** |
| `2` | 评分 **Hard** |
| `3` | 评分 **Good** |
| `4` | 评分 **Easy** |
| `z` | 深睡 5 秒后唤醒（验证 RTC 保持） |
| `x` | 格式化 LittleFS 并重写示例卡 |
| `d` | 通过 Wi-Fi 从 PC 下载导入包 |
| `u` | 上传复习记录写回 Anki |

### 设备端数据格式

| 文件 | 格式 | 内容 |
|------|------|------|
| `/cards.jsonl` | JSON Lines | 卡片 ID、正面、背面 |
| `/review_state.jsonl` | JSON Lines | 卡片状态、stability、difficulty、到期时间、复习次数、遗忘次数、步进 |
| `/review_events.jsonl` | JSON Lines（追加） | 评分事件：ease、耗时、评分时间、下次到期 |
| `/decks.json` | JSON | 牌组 FSRS 参数快照：`w`（21 个权重）、`desiredRetention`、`learningSteps`、`relearningSteps`、`maximumInterval` |

### 仓库结构

```
PortableAnki/
├── ESP4Anki/          # 设备固件（PlatformIO 项目）
│   ├── src/main.cpp
│   └── lib/           # 按模块划分的库：Core、Ui、Review、Scheduler、Storage、Sync、NetTime、Clock、Power
├── py-fsrs/           # 官方 Python FSRS 参考实现（MIT 协议）
├── rs-fsrs-c/         # Rust FFI → C 头文件（cbindgen）
├── anki-addon/        # Anki 插件（minianki_bridge）：HTTP 导出 + 写回
├── fsrs-check/        # C++ 与 py-fsrs 对拍验证工具
├── anki-tools/        # Anki Debug Console 脚本（导出 / 写回探针）
└── import-pack/       # 导出数据（已 gitignore，运行时生成）
```

### 验证

修改 `ESP4Anki/lib/Scheduler/Fsrs.cpp` 后，必须运行对拍脚本：

```bash
cd fsrs-check
python compare.py
# 期望输出：X passed / 0 failed
```

---

## License

- `py-fsrs/` is licensed under the [MIT License](py-fsrs/LICENSE) (Open Spaced Repetition).
- Other original code in this repository is released under the [MIT License](LICENSE).
