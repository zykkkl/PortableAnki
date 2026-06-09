# PortableAnki 剩余未实现功能清单

最后更新：2026-06-09

## 1. 当前已经具备的闭环

- Anki 插件在当前打开的 profile 内提供 HTTP 服务。
- 设备可通过 `PC_HOST` 访问插件。
- 设备串口输入 `l` 可读取 `/decks` 并打印卡组列表。
- 设备串口输入 `d` 可按 `secrets.h` 中的 `ANKI_DECK_ID` / `ANKI_DECK_NAME` / `ANKI_CARD_LIMIT` 下载：
  - `/cards.jsonl`
  - `/review_state.jsonl`
  - `/decks.json`
- 设备离线读取 LittleFS 中的卡片、复习状态和 FSRS 参数。
- 设备离线完成新卡学习、学习中等待、复习、重学的核心 FSRS 调度。
- 设备离线评分会追加到 `/review_events.jsonl`。
- 设备串口输入 `u` 可上传复习事件,由插件写回 Anki card 状态和 revlog。

## 2. 还未实现的产品功能

### 2.1 设备端卡组选择 UI

当前只能在串口里输入 `l` 查看卡组列表,再手动把 `deckId` 或 `deck` 写入 `secrets.h` 重新编译。

后续应实现：

- 在设备屏幕上显示卡组列表。
- 用实体按键选择卡组。
- 把选中的 `deckId` 保存在 LittleFS 配置文件中。
- 下载时直接使用设备本地保存的卡组配置。

### 2.2 每日新卡和复习数量限制

当前 `ANKI_CARD_LIMIT` 只是下载上限,不是完整的 Anki 每日限制。

后续应实现：

- `newLimit`：今日新卡上限。
- `reviewLimit`：今日复习上限。
- `learningLimit`：学习中/重学卡优先级规则。
- 不同卡组可独立配置限额。

### 2.3 Anki 选卡顺序和混排规则

当前固件按本地 `due` 最早优先。它可以复习,但没有完整复刻 Anki 的排序策略。

后续应实现或明确取舍：

- 新卡顺序：按加入顺序、随机、卡片模板顺序等。
- 新卡与复习卡混排：新卡在复习前/后/混合。
- 到期复习卡排序：按到期时间、间隔、难度等。
- bury sibling card 规则。

### 2.4 卡片模板渲染

当前插件把 notes 的字段做通用拆分：第一个非空字段作为 front,其余字段拼成 back。

后续应实现：

- 读取 Anki note type/card template。
- 按模板渲染正面/背面。
- 支持按 card ordinal 区分同一 note 的多张卡。
- 保留必要 HTML 格式,同时适配电子墨水屏显示。

### 2.5 媒体文件同步

当前 V1 主要支持文本卡。

后续应实现：

- 从 Anki `collection.media` 导出图片/音频。
- 设备下载媒体 manifest 和实际文件。
- 设备端缓存媒体文件。
- 显示图片,播放或忽略音频。
- 清理不再使用的媒体文件。

### 2.6 同步冲突处理

当前默认 Anki 是主库,设备上传离线复习事件后写回 Anki。

后续应处理：

- 同一张卡在设备离线期间也在桌面 Anki 复习过。
- 卡片在 Anki 中被删除、移动或改模板。
- 插件写回失败后的重试和幂等处理。
- 事件去重,避免重复写 revlog。

### 2.7 Anki learning queue 细节校准

当前设备端已有 Learning/Relearning FSRS 状态转换,插件写回也能更新 card 状态和 revlog。

仍需继续验证：

- Anki intraday learning 与 interday learning 的 `queue`/`due` 语义。
- `left` 字段与设备 `step` 的双向映射。
- 重学卡 Again/Hard/Good/Easy 写回后的桌面端显示是否完全一致。
- FSRS memory state 写回在不同 Anki 版本中的兼容性。

### 2.8 真实使用时长统计

当前固件上传事件里的 `timeMs` 仍为 0。

后续应实现：

- 正面展示开始计时。
- 翻面和评分计时。
- 防止长时间挂机导致异常大值。
- 上传真实 `timeMs` 写入 revlog。

### 2.9 可靠性与安全

后续应补：

- HTTP 请求超时和重试。
- 下载文件校验和临时文件替换,避免半包覆盖旧数据。
- 上传成功后按服务端确认范围清理事件。
- 插件访问鉴权或局域网 token。
- LittleFS 明文数据的隐私风险提示或加密方案。

## 3. 建议优先级

1. 设备端卡组选择 UI。
2. 每日新卡/复习上限。
3. Anki learning queue 写回细节对拍。
4. 卡片模板渲染。
5. 媒体同步。
6. 冲突处理和同步幂等。
