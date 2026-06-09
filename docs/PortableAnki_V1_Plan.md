# Portable Anki Mini OS 第一版制作方案

## 1. 项目定位

制作一个便携式水墨屏 Anki 学习设备，并为它设计一个专用的 Mini Anki OS。

这里的 Mini Anki OS 不是从零编写通用操作系统，也不是移植 Android 或 Linux，而是运行在 ESP32-S3 上的专用学习固件：

```text
ESP32-S3
   ↓
Mini Anki OS 固件
   ↓
本地卡片库 / 本地调度 / 本地复习记录
   ↓
墨水屏显示 + 实体按键操作
```

第一版目标：

- 设备可以脱离电脑独立复习
- 设备本地保存卡片、牌组和复习进度
- 离线时设备本地按照 Anki 兼容调度算法计算下一次复习时间
- 像 AnkiDroid 一样，即使没有网络，复习后也立即推进本地卡片状态
- 设备记录完整本地集合变更，包含评分、时间、耗时和调度前后状态
- 联网后把本地集合变更同步给 Anki，而不是等联网后才计算排程
- 同步完成后合并 Anki 端变化，并处理可能的冲突

第一版仍然只做 Anki 兼容子集，不移植完整 Anki：

- 不支持完整 Anki 数据库格式
- 不支持完整 `.apkg` 解析
- 不支持完整 AnkiWeb 同步协议
- 第一版不保证和当前 Anki/FSRS 的所有细节完全一致
- 优先支持纯文本卡片、简单牌组和实体按键复习

核心原则：

```text
离线时：Mini Anki OS 本地集合继续推进，像 AnkiDroid 一样可持续复习
联网后：同步本地集合变更，并和 Anki 端集合状态合并
```

## 2. 总体架构

```text
┌──────────────────────────────┐
│         桌面端 Anki           │
│ AnkiConnect / 专用同步插件    │
│ 导入 / 导出 / 变更合并        │
└───────────────┬──────────────┘
                │ USB 或 Wi-Fi
┌───────────────▼──────────────┐
│        Mini Anki OS           │
│   运行在 ESP32-S3 固件中      │
├──────────────────────────────┤
│ UI 层：墨水屏 + 实体按键       │
│ 学习层：翻面 / 评分 / 队列     │
│ 调度层：Anki 兼容本地排程      │
│ 存储层：本地集合 / 状态 / 事件日志│
│ 同步层：变更上传 / 状态合并      │
└───────────────┬──────────────┘
                │ SPI
┌───────────────▼──────────────┐
│         三色墨水屏模块         │
└──────────────────────────────┘
```

## 3. 第一版功能范围

### 3.1 必须支持

- 开机进入学习首页
- 显示今日待复习数量
- 选择默认牌组开始复习
- 显示卡片正面
- 按键翻到背面
- 使用 Again / Hard / Good / Easy 评分
- 本地更新卡片下一次复习时间
- 本地保存复习事件日志
- 断电后数据不丢失
- 离线期间持续按照本地调度结果安排后续复习
- 联网后上传本地复习事件和集合变更
- 联网后合并 Anki 端卡片内容、牌组和调度状态变化
- 通过 USB 或 Wi-Fi 导入卡片、导出备份

### 3.2 第一版暂不支持

- 音频播放
- 图片显示
- 富文本 HTML/CSS
- LaTeX
- 手写输入
- 设备端编辑复杂模板
- AnkiWeb 官方同步
- 完整 `.apkg` 直接导入
- 多用户账户
- 完整 FSRS 参数训练
- Anki 官方同步协议的完整双向同步
- 多设备同时离线复习后的复杂冲突合并

### 3.3 第一版推荐卡片类型

只支持最简单的正反面文本卡：

```text
正面：abandon
背面：v. 放弃；遗弃
```

后续再扩展：

- 例句字段
- 标签
- 词性
- 简单备注
- 多牌组
- 图片和音频

## 4. 硬件清单

### 4.1 推荐配置

| 模块 | 建议 |
|---|---|
| 主控 | ESP32-S3 开发板，优先选择带 PSRAM 的版本 |
| 屏幕 | 2.9 寸三色墨水屏模块，优先选择带驱动板的模块 |
| 存储 | microSD 卡模块，或使用 ESP32-S3 Flash/LittleFS 做小容量版本 |
| 按键 | 6 个轻触按键 |
| 电池 | 3.7V 单节锂电，1000-2000mAh |
| 电源模块 | 1S 锂电充电保护 + 5V 升压模块，输出建议不低于 1A |
| 开关 | 拨动电源开关 |
| 连接材料 | 洞洞板、排针、硅胶线、JST 插座、杜邦线 |
| 外壳 | 3D 打印外壳，M2 螺丝柱固定 |

### 4.2 为什么建议加 microSD

如果只是几十到几百张纯文本卡，ESP32-S3 内置 Flash + LittleFS 可以先跑起来。

如果希望更像独立设备，建议加 microSD：

- 卡片容量更大
- 导入导出更方便
- 备份简单
- 复习事件日志不容易占满内部 Flash
- 后续支持图片和音频更现实

第一版可以分两档：

| 版本 | 存储方式 | 适合阶段 |
|---|---|---|
| V1-lite | LittleFS | 点亮屏幕、验证调度、少量卡片 |
| V1-full | microSD | 便携独立设备、长期使用 |

### 4.3 屏幕选择说明

如果还没买屏幕，建议优先选择黑白双色墨水屏，因为黑白屏通常刷新更快，并且部分型号支持局部刷新。

如果坚持使用三色墨水屏，也可以完成第一版，但要接受这些限制：

- 翻页慢
- 一般不支持局部刷新
- 红色刷新更慢
- 不适合高频快速复习

三色屏适合学习硬件驱动和制作原型，但如果目标是每天大量刷 Anki，黑白屏体验更好。

## 5. Mini Anki OS 软件模块

### 5.1 模块划分

```text
MiniAnkiOS
├── boot          开机、初始化、错误恢复
├── ui            墨水屏界面、菜单、提示
├── input         按键扫描、消抖、长按
├── card_db       卡片、牌组、标签、本地索引
├── scheduler     Anki 兼容本地调度
├── review        复习状态机
├── event_log     离线复习事件日志
├── storage       LittleFS / microSD 文件读写
├── import_export 导入、导出、备份、格式转换
├── sync          离线事件上传、Anki 状态合并
└── power         休眠、电量、低功耗
```

### 5.2 推荐开发方式

第一版建议使用 PlatformIO 或 Arduino IDE。

推荐库：

| 功能 | 库 |
|---|---|
| 墨水屏驱动 | GxEPD2 |
| JSON 解析 | ArduinoJson |
| 本地文件系统 | LittleFS |
| microSD | SD / SdFat |
| Wi-Fi | ESP32 WiFi |
| HTTP 服务 | WebServer 或 ESPAsyncWebServer |

如果后续代码变复杂，可以从 Arduino 框架迁移到 ESP-IDF，但第一版不建议一开始就上 ESP-IDF。

## 6. 本地数据设计

### 6.1 目录结构

如果使用 microSD，建议目录结构：

```text
/minianki
├── decks.json
├── cards.jsonl
├── review_state.jsonl
├── review_events.jsonl
├── sync_state.json
├── settings.json
└── backups
    └── backup_2026-06-07.json
```

如果使用 LittleFS，也可以沿用同样结构，但容量要更保守。

### 6.2 牌组文件

`decks.json`：

```json
{
  "version": 1,
  "decks": [
    {
      "deckId": 1,
      "ankiDeckName": "English",
      "name": "English",
      "presetId": 101,
      "presetName": "Default",
      "scheduler": "fsrs",
      "desiredRetention": 0.9,
      "fsrsParams": [0.212, 1.2931, 2.3065, 8.2956, 6.4133, 0.8334, 3.0194, 0.001, 1.8722, 0.1666, 0.796, 1.4835, 0.0614, 0.2629, 1.6483, 0.6014, 1.8729, 0.5425, 0.0912],
      "presetVersion": "2026-06-07T10:00:00+08:00",
      "lastPresetSyncedAt": 1780790000,
      "createdAt": "2026-06-07T10:00:00+08:00"
    }
  ]
}
```

字段说明：

| 字段 | 含义 |
|---|---|
| presetId/presetName | Anki 牌组选项 preset 的标识 |
| scheduler | `fsrs` 或 `sm2`，第一目标是支持 `fsrs` |
| desiredRetention | Anki preset 中的目标保留率 |
| fsrsParams | 当前 preset 的 FSRS 参数快照 |
| presetVersion | 导出参数时的版本标记 |
| lastPresetSyncedAt | 最近一次同步 preset 参数的时间 |

### 6.3 卡片文件

`cards.jsonl`：

```json
{"localCardId":1001,"ankiCardId":1498938915662,"ankiNoteId":1498938915600,"deckId":1,"front":"abandon","back":"v. 放弃；遗弃","tags":["cet4"],"contentHash":"sha1:..."}
{"localCardId":1002,"ankiCardId":1498938915663,"ankiNoteId":1498938915601,"deckId":1,"front":"brief","back":"adj. 简短的；n. 摘要","tags":["cet4"],"contentHash":"sha1:..."}
```

字段说明：

| 字段 | 含义 |
|---|---|
| localCardId | 设备本地卡片 ID |
| ankiCardId | Anki 桌面端卡片 ID，用于同步 |
| ankiNoteId | Anki 桌面端笔记 ID，用于更新内容 |
| contentHash | 正反面内容哈希，用于识别内容是否变化 |

### 6.4 复习状态文件

`review_state.jsonl`：

```json
{"localCardId":1001,"ankiCardId":1498938915662,"state":"new","due":0,"scheduledDays":0,"elapsedDays":0,"stability":0.0,"difficulty":0.0,"reps":0,"lapses":0,"dirty":false,"lastSyncedAt":1780790000}
{"localCardId":1002,"ankiCardId":1498938915663,"state":"review","due":1780800000,"scheduledDays":3,"elapsedDays":3,"stability":4.82,"difficulty":5.31,"reps":4,"lapses":1,"dirty":true,"lastSyncedAt":1780790000}
```

字段说明：

| 字段 | 含义 |
|---|---|
| localCardId | 设备本地卡片 ID |
| ankiCardId | Anki 桌面端卡片 ID |
| state | new / learning / review / relearning |
| due | 下次到期时间，Unix 时间戳 |
| scheduledDays | 当前安排的间隔天数 |
| elapsedDays | 距离上次复习的实际天数 |
| stability | FSRS 记忆稳定性 |
| difficulty | FSRS 难度 |
| reps | 复习次数 |
| lapses | 遗忘次数 |
| dirty | 本地状态是否有未同步变更 |
| lastSyncedAt | 最近一次和 Anki 合并状态的时间 |

### 6.5 离线复习事件文件

`review_events.jsonl`：

```json
{"eventId":"dev01-00000001","localCardId":1001,"ankiCardId":1498938915662,"ratedAt":1780800000,"ease":3,"timeMs":8200,"oldState":"new","newState":"review","oldDue":0,"nextDue":1780886400,"syncStatus":"pending"}
{"eventId":"dev01-00000002","localCardId":1002,"ankiCardId":1498938915663,"ratedAt":1780800100,"ease":1,"timeMs":15500,"oldState":"review","newState":"relearning","oldDue":1780800000,"nextDue":1780800700,"syncStatus":"pending"}
```

字段说明：

| 字段 | 含义 |
|---|---|
| eventId | 设备生成的唯一事件 ID，避免重复同步 |
| ratedAt | 用户实际离线复习的时间 |
| ease | 评分，1=Again，2=Hard，3=Good，4=Easy |
| oldState/newState | 设备本地调度前后的状态 |
| oldDue/nextDue | 设备本地调度前后的到期时间 |
| syncStatus | pending / uploaded / confirmed / conflict |

### 6.6 同步状态文件

`sync_state.json`：

```json
{
  "deviceId": "minianki-dev01",
  "lastSyncAt": 1780790000,
  "lastAnkiCollectionModified": 1780789000,
  "pendingEventCount": 2,
  "conflictCount": 0
}
```

## 7. 离线排程算法

设备必须在没有网络时继续安排后续复习。因此 Mini Anki OS 需要在本地实现一个 Anki 兼容排程器。

目标不是手写一套和 Anki 无关的间隔规则，而是：

```text
联网同步时：从 Anki 导出牌组 preset 和 FSRS 参数
离线复习时：设备使用这份参数快照继续排程
下次同步时：上传复习进度，并拉取新的 FSRS 参数快照
```

Anki 手册中 FSRS 参数、desired retention 等属于牌组选项 preset。设备端应按牌组保存 preset 快照，而不是给每张卡手动配置一套参数。

### 7.1 卡片状态

```text
new        新卡
learning   学习中
review     复习中
relearning 复习失败后重新学习
```

### 7.2 评分含义

| 按键 | ease | 含义 |
|---|---|---|
| Again | 1 | 忘了，短时间后重来 |
| Hard | 2 | 记得吃力，缩短间隔 |
| Good | 3 | 正常记住，按正常间隔 |
| Easy | 4 | 很容易，拉长间隔 |

### 7.3 FSRS 参数快照

每次联网同步时，电脑端管理工具从 Anki 导出：

| 数据 | 用途 |
|---|---|
| presetId / presetName | 识别牌组选项 |
| scheduler | 判断使用 FSRS 还是旧调度 |
| fsrsParams | 离线时计算 stability、difficulty、scheduledDays |
| desiredRetention | 根据目标保留率计算下次间隔 |
| learningSteps / relearningSteps | 处理新卡和遗忘后的短间隔学习 |
| maximumInterval | 限制最大复习间隔 |
| newCardGather/order | 后续如需更接近 Anki 的新卡顺序 |

设备将这些数据写入 `decks.json`。离线时即使没有网络，也继续使用最后一次同步得到的参数快照。

### 7.4 离线 FSRS 排程流程

复习时流程：

```text
读取卡片当前 review_state
   ↓
根据 deckId 找到 FSRS 参数快照
   ↓
用户按 Again / Hard / Good / Easy
   ↓
使用 FSRS 参数计算新的 stability / difficulty
   ↓
根据 desiredRetention 计算 scheduledDays
   ↓
更新 due / state / reps / lapses
   ↓
追加 review_events.jsonl
```

第一版需要实现：

- 新卡初始 stability/difficulty 计算
- 复习卡 stability/difficulty 更新
- Again / Hard / Good / Easy 四种评分分支
- scheduledDays 和 due 的计算
- 学习中卡片的短间隔 step
- 遗忘后 relearning step

如果某些 Anki 桌面端细节暂时无法实现，设备端必须在 `settings.json` 里记录 `schedulerCompatibility`，避免误以为完全一致。

### 7.5 参数更新原则

同步时如果发现 Anki 端 preset 参数变了：

```text
下载新的 fsrsParams / desiredRetention / steps
   ↓
保存为新的 preset 快照
   ↓
后续新发生的离线复习使用新参数
   ↓
已经排好的 due 不强制全量重排，除非用户主动选择 Reschedule
```

这更接近 Anki 的实际使用逻辑：参数更新影响后续复习决策，不应该在设备上静默重排所有卡片。

### 7.6 AnkiDroid 式本地推进原则

目标行为应接近 AnkiDroid：

- 没网时也能继续复习
- 每次评分都会立即更新本地卡片状态
- 后续复习队列基于本地最新状态继续生成
- 联网后再把本地变更同步到 Anki 生态

```text
设备离线评分
   ↓
设备本地计算 nextDue
   ↓
立即更新本地 review_state
   ↓
后续复习继续基于本地状态排队
   ↓
联网后上传本地 collection changes / review_events
   ↓
同步插件合并 Anki 端变化
   ↓
设备保存合并后的集合状态
```

这意味着 `review_events.jsonl` 不只是临时日志，而是本地集合变更日志。设备本地 `review_state.jsonl` 必须在每次评分后立即更新。

如果设备端调度器和 Anki 端存在细微偏差，同步时应尽量合并并提示风险，而不是静默丢弃设备端离线复习结果。

## 8. 设备端交互流程

### 8.1 开机首页

```text
Portable Anki

Today: 42
New: 10
Review: 32

[Start] [Deck] [Sync]
```

### 8.2 复习流程

```text
开机
 ↓
读取本地卡片库和复习状态
 ↓
计算今日到期队列
 ↓
显示首页统计
 ↓
按 Start
 ↓
显示卡片正面
 ↓
按翻面键
 ↓
显示背面
 ↓
按 Again / Hard / Good / Easy
 ↓
更新 review_state.jsonl
 ↓
追加 review_events.jsonl
 ↓
进入下一张
 ↓
今日完成
```

### 8.3 菜单结构

```text
Home
├── Start Review
├── Decks
│   ├── English
│   └── All Cards
├── Stats
│   ├── Today
│   └── Streak
├── Sync
│   ├── Import
│   ├── Export Backup
│   └── Wi-Fi Setup
└── Settings
    ├── Screen
    ├── Power
    └── About
```

第一版可以只实现 `Home`、`Start Review`、`Sync Import`、`Export Backup`。

## 9. 按键设计

### 9.1 按键数量

第一版建议 6 个按键：

| 按键 | 学习界面功能 | 菜单界面功能 |
|---|---|---|
| 翻面/确认 | 正面翻背面 | 确认 |
| Again/上 | 评分 1 | 上一项 |
| Hard/下 | 评分 2 | 下一项 |
| Good/返回 | 评分 3 | 返回 |
| Easy/菜单 | 评分 4 | 菜单 |
| 同步/电源 | 同步或长按休眠 | 同步或长按休眠 |

### 9.2 推荐布局

```text
┌────────────────────────┐
│        墨水屏          │
├────────────────────────┤
│ 确认 │ Again │ Hard │
│ Good │ Easy  │ 同步 │
└────────────────────────┘
```

### 9.3 按键接线

每个按键一端接 GPIO，另一端接 GND。

ESP32 程序中 GPIO 使用 `INPUT_PULLUP`：

```cpp
pinMode(BUTTON_GOOD, INPUT_PULLUP);
```

按下时读取到 `LOW`，松开时读取到 `HIGH`。

## 10. 接线示例

以下 GPIO 只是示例，实际应按开发板和屏幕模块说明调整：

```text
墨水屏 VCC  → 3V3 或 5V，按模块说明
墨水屏 GND  → GND
墨水屏 DIN  → GPIO11
墨水屏 CLK  → GPIO12
墨水屏 CS   → GPIO10
墨水屏 DC   → GPIO13
墨水屏 RST  → GPIO14
墨水屏 BUSY → GPIO9

microSD CS   → GPIO8
microSD MOSI → GPIO11
microSD CLK  → GPIO12
microSD MISO → GPIO18

确认按键    → GPIO4
Again 按键  → GPIO5
Hard 按键   → GPIO6
Good 按键   → GPIO7
Easy 按键   → GPIO15
同步按键    → GPIO16
```

建议：

- 墨水屏和 microSD 可以共享 SPI 的 MOSI/CLK，但 CS 必须不同。
- 屏幕和 ESP32 必须共地。
- 避开 `GPIO0` 等启动相关脚位。
- 如果接线后 ESP32 无法启动，优先更换按键 GPIO。
- 先用杜邦线验证，再焊接到洞洞板。

## 11. 供电方案

### 11.1 调试阶段

调试时优先使用 USB 供电：

```text
电脑 USB / 充电宝
        ↓
ESP32 USB 口
        ↓
墨水屏模块 + microSD
```

优点：

- 安全
- 接线简单
- 方便串口调试
- 不需要一开始处理电池和充电问题

### 11.2 便携阶段

装入外壳时使用内置锂电：

```text
3.7V 锂电
   ↓
充电保护模块
   ↓
5V 升压模块
   ↓
ESP32 5V/VIN 或 USB 输入
   ↓
墨水屏模块 + microSD
```

注意事项：

- 不要把单节锂电直接接到 ESP32 的 5V/VIN。
- 不要使用纽扣电池，ESP32 开 Wi-Fi 时电流峰值较高。
- 升压模块输出电流建议至少 1A。
- microSD 写入时不要突然断电。
- 锂电软包不能被螺丝、焊点、尖角挤压。
- 外壳内建议给电池单独留电池仓。
- 充电口、电源开关和复位按键需要从外壳露出。

## 12. 导入、同步和备份方案

### 12.1 第一版导入格式

第一版可以先不直接导入 `.apkg`，而是由电脑端管理工具从 Anki 导出数据，再转换成设备 JSON。

电脑端导入流程：

```text
Anki 桌面端
   ↓ AnkiConnect / 专用导出插件
电脑端管理工具
   ↓ 生成 Mini Anki OS 导入包：卡片 + 状态 + FSRS preset 参数
ESP32-S3 设备
   ↓ 写入 decks.json / cards.jsonl / review_state.jsonl
离线复习
```

设备导入包必须包含：

- Anki 的 `ankiCardId`
- Anki 的 `ankiNoteId`
- 牌组对应的 preset 信息
- FSRS 参数快照
- 当前卡片复习状态

否则设备无法用当前 Anki 设置离线排程，也无法把离线复习结果同步回 Anki。

推荐 CSV：

```csv
ankiCardId,ankiNoteId,deck,front,back,tags
1498938915662,1498938915600,English,abandon,v. 放弃；遗弃,cet4
1498938915663,1498938915601,English,brief,adj. 简短的；n. 摘要,cet4
```

推荐 JSON：

```json
{
  "version": 1,
  "presets": [
    {
      "presetId": 101,
      "presetName": "Default",
      "scheduler": "fsrs",
      "desiredRetention": 0.9,
      "fsrsParams": [0.212, 1.2931, 2.3065, 8.2956, 6.4133, 0.8334, 3.0194, 0.001, 1.8722, 0.1666, 0.796, 1.4835, 0.0614, 0.2629, 1.6483, 0.6014, 1.8729, 0.5425, 0.0912]
    }
  ],
  "decks": [
    {
      "name": "English",
      "presetId": 101,
      "cards": [
        {
          "ankiCardId": 1498938915662,
          "ankiNoteId": 1498938915600,
          "front": "abandon",
          "back": "v. 放弃；遗弃",
          "tags": ["cet4"]
        }
      ]
    }
  ]
}
```

### 12.2 电脑端管理工具

电脑端管理工具承担三个职责：

- 从 Anki 导出卡片和初始复习状态
- 从 Anki 导出牌组 preset 和 FSRS 参数快照
- 接收设备上传的本地集合变更和复习事件
- 合并设备端与 Anki 端的集合变化
- 把合并后的状态导回设备

具体功能：

- 将 Anki 卡片转换为设备 JSON
- 将 Anki 牌组选项和 FSRS 参数转换为设备 `decks.json`
- 将 Anki 导出的文本文件转换为 Mini Anki OS 格式
- 从 AnkiConnect 读取卡片 ID、牌组、字段和标签
- 从专用 Anki 插件读取 preset、FSRS 参数、学习步长和最大间隔
- 从设备读取 `review_events.jsonl`
- 将离线评分提交给 Anki
- 从 Anki 重新导出最新卡片状态、调度状态和 FSRS 参数
- 将合并后的状态写回设备
- 将设备复习事件日志转换成人类可读报表

### 12.3 同步模式

第一版建议把同步分成两档。

#### 12.3.1 简单同步

简单同步适合先跑通产品闭环。

```text
设备上传 pending review_events 和 dirty review_state
   ↓
电脑端用 AnkiConnect 逐条提交评分
   ↓
电脑端导出 Anki 最新状态和 FSRS preset 参数
   ↓
设备标记事件 confirmed，并合并/更新 review_state
   ↓
设备更新 decks.json 中的 preset 参数快照
```

风险：

- 普通 AnkiConnect 提交评分时，Anki 可能按“提交时刻”记录复习，而不是按设备的 `ratedAt` 离线复习时间记录。
- 如果离线时间很长，Anki 端最终状态可能和设备本地已推进状态不同。
- AnkiConnect 未必直接暴露所有 FSRS preset 细节，可能需要专用 Anki 插件导出参数。

这个模式能先验证整体链路，但不是最终高精度同步方案。

#### 12.3.2 准确同步

准确同步需要电脑端写一个专用 Anki 插件或扩展 AnkiConnect 行为。

目标：

- 按设备事件里的 `ratedAt` 应用评分
- 保留真实离线复习时间
- 尽量按 Anki/AnkiDroid 的本地集合推进语义合并状态
- 同步 Anki 端最新 FSRS preset 参数
- 避免重复提交同一个 `eventId`
- 应用后返回 Anki 最新卡片状态
- 发现冲突时返回 conflict，而不是静默覆盖

准确同步更符合“离线像 Anki 一样复习，联网后同步”的目标，但开发难度高于简单同步。

### 12.4 冲突处理

第一版只支持简单冲突策略：

| 场景 | 处理 |
|---|---|
| 设备离线复习，Anki 端没有变化 | 上传事件，Anki 应用，设备标记 confirmed |
| 设备离线复习，同一卡片也在电脑 Anki 复习过 | 标记 conflict，默认以 Anki 为准 |
| Anki 端修改了卡片内容，设备也有旧内容 | 更新设备内容，保留设备复习事件 |
| 设备重复上传同一事件 | 根据 eventId 忽略重复事件 |

第一版建议避免多端同时复习同一卡片。最稳的使用习惯是：

```text
外出时只用设备复习
回到电脑后先同步
同步完成后再在电脑 Anki 复习
```

### 12.5 备份策略

设备每次导出生成一个完整备份：

```text
minianki_backup_2026-06-07.json
```

备份内容包括：

- 牌组
- 卡片
- 复习状态
- 离线复习事件
- 同步状态
- 设置

第一版必须优先保证“能导出完整数据”。只要备份完整，即使同步代码出错，也可以人工恢复复习记录。

## 13. 外壳方案

### 13.1 结构

建议外壳分为三层：

```text
上盖：屏幕开窗 + 按键孔
中框：固定 ESP32、屏幕模块、microSD 模块、洞洞板
下盖：电池仓 + 螺丝柱
```

### 13.2 开孔

需要预留：

- 屏幕窗口
- 6 个按键孔
- USB-C 充电/调试口
- 电源拨动开关
- microSD 卡槽，可选
- 复位孔，可选
- 挂绳孔，可选

### 13.3 尺寸建议

第一版可以按较宽松尺寸设计：

```text
长：110mm
宽：60mm
厚：20mm
```

实际尺寸应根据屏幕模块、ESP32 开发板、电池、microSD 模块尺寸调整。

### 13.4 固定方式

- 屏幕使用泡棉胶或薄双面胶固定。
- ESP32、microSD 模块和洞洞板用 M2 螺丝柱固定。
- 电池用泡棉胶固定在独立电池仓。
- 不要让外壳直接压迫墨水屏玻璃。
- 不要让螺丝柱、焊点、排针顶到锂电池。

## 14. 开发步骤

### 阶段 1：屏幕点亮

目标：

- ESP32-S3 通过 USB 供电
- 墨水屏显示 `Mini Anki OS`

完成标准：

- 屏幕能稳定刷新
- 断电后画面保留
- 串口无明显错误

### 阶段 2：按键验证

目标：

- 接入 6 个按键
- 串口打印每个按键事件

完成标准：

- 每个按键都能被识别
- 没有明显误触发
- 简单消抖可用

### 阶段 3：本地假卡复习

目标：

- 固件内写死 3-5 张假卡
- 实现正面、背面、评分、下一张

完成标准：

- 能完成一轮复习
- 评分后能在串口看到新状态和下一次复习时间

### 阶段 4：实现 FSRS 参数快照离线调度器

目标：

- 实现 new / learning / review / relearning 状态
- 从 `decks.json` 读取 `fsrsParams` 和 `desiredRetention`
- 根据 Again / Hard / Good / Easy 计算 stability、difficulty 和 nextDue
- 支持断网情况下继续安排后续复习

完成标准：

- 同一张卡评分后能按 FSRS 参数快照得到合理的下次复习时间
- 重启后不丢失调度状态
- 设备不连接电脑也能完成多轮复习

### 阶段 5：本地文件系统和事件日志

目标：

- 使用 LittleFS 或 microSD 保存卡片和复习状态
- 支持 `cards.jsonl`、`review_state.jsonl`、`review_events.jsonl`
- 每次评分生成唯一 `eventId`

完成标准：

- 断电后数据不丢失
- 能继续未完成的复习
- 离线复习事件能追加保存
- 重复启动不会重复生成同一评分事件

### 阶段 6：首页和菜单

目标：

- 显示今日待复习数量
- 支持 Start Review
- 支持简单 Sync/Import 菜单

完成标准：

- 不连接电脑也能进入学习流程
- 操作路径清晰

### 阶段 7：从 Anki 导入卡片

目标：

- 电脑端工具从 Anki 导出卡片
- 电脑端工具从 Anki 导出牌组 preset 和 FSRS 参数
- 生成包含 `ankiCardId`、`presetId`、`fsrsParams` 的设备导入包
- 设备通过 USB、Wi-Fi 或 microSD 导入 JSON

完成标准：

- 能导入新牌组
- 能避免重复导入同一张卡
- 导入后首页统计正确
- 每张设备卡片都能映射回 Anki 的 `ankiCardId`
- 每个牌组都能找到对应的 FSRS 参数快照

### 阶段 8：简单同步到 Anki

目标：

- 设备上传 pending `review_events`
- 电脑端工具通过 AnkiConnect 提交评分
- 电脑端工具从 Anki 导出最新状态
- 设备合并 Anki 最新状态和本地 `review_state`

完成标准：

- pending 事件能变成 confirmed
- Anki 中对应卡片状态发生变化
- 设备同步后待复习数量和 Anki 基本一致

### 阶段 9：完整备份和恢复

目标：

- 导出完整备份 JSON
- 支持从备份恢复设备数据

完成标准：

- 备份文件包含卡片、状态、事件日志、同步状态和设置
- 电脑端可以读取备份内容
- 清空设备后可以从备份恢复

### 阶段 10：准确同步方案

目标：

- 评估是否需要专用 Anki 插件
- 支持按 `ratedAt` 应用离线评分
- 支持 eventId 去重和 conflict 返回

完成标准：

- 离线复习时间能被正确保留
- 重复同步不会重复评分
- 同一张卡多端冲突时能被识别

这一阶段可以放到 V1 后半段或 V2，不建议在屏幕、按键、存储还没稳定前投入。

### 阶段 11：便携供电

目标：

- 加入锂电、充电保护、升压模块、电源开关

完成标准：

- 设备可以脱离 USB 工作
- Wi-Fi 同步时不重启
- microSD 写入稳定
- 充电和开关正常

### 阶段 12：焊接与外壳

目标：

- 从面包板迁移到洞洞板或模块固定结构
- 装入 3D 打印外壳

完成标准：

- 摇晃设备不会断线
- 按键手感稳定
- USB 口、开关、microSD 卡槽可正常使用

## 15. 主要风险

| 风险 | 说明 | 规避方式 |
|---|---|---|
| FSRS 实现与 Anki 有偏差 | Mini Anki OS 使用同一组 FSRS 参数，但公式、取整、学习步长等细节可能和 Anki 不完全一致 | 优先复用成熟 FSRS 实现，做桌面端对照测试 |
| FSRS 参数导出不完整 | AnkiConnect 未必直接暴露全部 preset 参数 | 写专用 Anki 插件导出 preset、fsrsParams、desiredRetention 和 steps |
| AnkiConnect 时间戳限制 | 简单同步可能按提交时刻记录复习，而不是离线复习时刻 | V1 先接受偏差，V2 写专用 Anki 插件 |
| 同步冲突 | 同一卡片可能在设备和电脑端都被复习 | 第一版避免多端同时复习，冲突时保留设备事件并提示用户 |
| 重复提交评分 | 网络中断后可能重复上传同一事件 | 每条复习事件使用 eventId 去重 |
| 三色屏刷新慢 | 高频翻卡体验差 | 接受原型限制，后续换黑白屏 |
| 文件写入损坏 | 断电时写 microSD 可能损坏文件 | 写入时显示提示，做备份和临时文件 |
| ESP32 内存有限 | 不能一次加载大量卡片 | 使用 JSONL 流式读取，建立简单索引 |
| 导入格式复杂 | `.apkg` 解析和 Anki 数据库兼容难度较高 | 第一版从 Anki 导出 CSV/JSON 导入包 |
| 电池供电不稳 | Wi-Fi 峰值电流导致重启 | 选择足够电流的升压模块 |
| 外壳挤压屏幕 | 墨水屏易损坏 | 屏幕周围留缓冲，不直接压玻璃 |
| 卡片格式复杂 | HTML、图片、音频难适配 | 第一版只支持纯文本 |

## 16. 最终第一版配置

```text
ESP32-S3 开发板
+ 2.9 寸三色墨水屏模块
+ microSD 卡模块
+ 6 个实体按键
+ 2000mAh 单节锂电
+ 1S 充电保护升压模块
+ 洞洞板焊接
+ 3D 打印外壳
+ Mini Anki OS 固件
+ 电脑端导入/备份工具
```

如果实际复习体验优先，建议将三色墨水屏替换为黑白双色墨水屏。

## 17. 后续升级方向

第一版完成后，可以逐步升级：

- 换黑白局刷墨水屏，提高翻页速度
- 增加休眠和低功耗管理
- 增加电量显示
- 增加多牌组选择
- 增加搜索功能
- 增加图片压缩显示
- 增加音频播放模块
- 支持 `.apkg` 转换工具
- 支持和 Anki 桌面端导入导出
- 改为定制 PCB
- 优化外壳尺寸和按键手感
- 做一个更完整的桌面端管理工具
