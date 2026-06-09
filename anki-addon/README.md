# Anki 插件

`minianki_bridge/` 是 PortableAnki 设备和桌面端 Anki 之间的桥接插件。

## 目录

- `minianki_bridge/__init__.py`：插件主程序,启动 HTTP 服务。
- `minianki_bridge/config.json`：插件默认配置,当前包含端口和导出默认行为。
- `minianki_bridge/manifest.json`：Anki 插件清单。
- `dist/`：本地 `.ankiaddon` 打包产物,已被 `.gitignore` 忽略。

## HTTP 接口

- `GET /profiles`：当前 profile 和本机检测到的 profiles。
- `GET /decks`：当前 profile 的卡组列表。
- `GET /cards.jsonl?deckId=<id>&limit=<n>`：导出卡片内容。
- `GET /review_state.jsonl?deckId=<id>&limit=<n>`：导出复习状态。
- `GET /decks.json?deckId=<id>`：导出牌组 FSRS 参数。
- `POST /upload`：接收设备离线复习事件并写回 Anki。

设备端对应配置在 `ESP4Anki/lib/NetTime/secrets.h.example`。
