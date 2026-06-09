# CLAUDE.md

本文件是 PortableAnki 当前版本的协作说明。更完整的项目规范见 `AGENTS.md`，以 `AGENTS.md` 为主；本文件只保留给 Claude/AI 助手快速进入项目用的高频上下文。

## 项目当前状态

PortableAnki 是基于 ESP32-S3 的便携式离线 Anki 复习终端。

当前核心闭环：

1. Anki 桥接插件 `anki-addon/minianki_bridge/` 在桌面端 Anki 内启动 HTTP 服务。
2. ESP32 固件 `ESP4Anki/` 通过 Wi-Fi 下载卡片、复习状态和 FSRS 参数。
3. 设备离线使用本地 LittleFS 数据和 C++ FSRS 调度器复习。
4. 设备把离线评分追加到 `/review_events.jsonl`。
5. 联网后上传事件，插件写回 Anki card 状态和 revlog。

## 关键目录

- `ESP4Anki/`：PlatformIO 固件项目。
- `anki-addon/minianki_bridge/`：Anki HTTP 桥接插件。
- `anki-tools/`：Anki Debug Console 辅助脚本，主要用于调试和探针。
- `fsrs-check/`：C++ FSRS 与 `py-fsrs` 对拍验证。
- `py-fsrs/`：官方 Python FSRS 参考实现，Git submodule。
- `rs-fsrs-c/`：Rust FFI 参考/绑定项目，Git submodule。
- `docs/`：规划文档、剩余功能清单和归档文档。

## 固件命令

在 `ESP4Anki/` 下执行：

```bash
pio run
pio run -t upload
pio device monitor
```

首次使用时复制 `ESP4Anki/lib/NetTime/secrets.h.example` 为 `secrets.h`，配置：

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `PC_HOST`
- `ANKI_DECK_ID` 或 `ANKI_DECK_NAME`
- `ANKI_CARD_LIMIT`

`secrets.h` 不应提交。

## 串口调试命令

- `f`：翻面。
- `1/2/3/4`：Again / Hard / Good / Easy。
- `l`：从 Anki 插件读取并打印卡组列表。
- `d`：下载选中卡组的卡片、复习状态和参数。
- `u`：上传离线复习事件并写回 Anki。
- `x`：格式化 LittleFS 并写入示例卡。
- `z`：深睡 5 秒后唤醒。

## 插件接口

插件默认端口为 `8766`。

- `GET /profiles`
- `GET /decks`
- `GET /cards.jsonl?deckId=<id>&limit=<n>`
- `GET /review_state.jsonl?deckId=<id>&limit=<n>`
- `GET /decks.json?deckId=<id>`
- `POST /upload`

读取侧通过当前 Anki profile 的 `collection.anki2` 建立 SQLite 只读连接；写回仍在 Anki 主线程使用 Anki API 执行。

## 验证要求

- 修改 `ESP4Anki/lib/Scheduler/Fsrs.cpp` 后，运行：

```bash
cd fsrs-check
python compare.py
```

- 修改插件后，至少做 Python 语法和 JSON 配置检查：

```bash
python -c "import ast,json,pathlib; ast.parse(pathlib.Path('anki-addon/minianki_bridge/__init__.py').read_text(encoding='utf-8')); json.loads(pathlib.Path('anki-addon/minianki_bridge/config.json').read_text(encoding='utf-8'))"
```

## 注意事项

- 不要手动修改 `rs-fsrs-c/include/fsrs.h`，它由 cbindgen 生成。
- `py-fsrs/` 和 `rs-fsrs-c/` 是 submodule，不要随意移动目录。
- Anki 插件写回会修改桌面端集合，测试写回前优先使用 dry-run 或单卡探针。
- 当前还未完成的功能见 `docs/PortableAnki_Remaining_Work.md`。
