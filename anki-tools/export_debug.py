# Mini Anki OS - 导出脚本(在 Anki 的 Debug Console 里运行)
# 打开方式:Anki 主界面按 Ctrl+Shift+;  ->  粘贴本文件全部内容 -> Ctrl+Enter
# 作用:把指定牌组的卡 + 每卡 FSRS memory state + 牌组 FSRS 参数,导成设备格式,写到 OUT_DIR。
# 只读 Anki 数据,不修改你的牌组。

import os, json, time
from aqt import mw

# ===== 可调参数 =====
DECK          = "NEW-JLPT::NEW-N2"     # 要导出的牌组(用优化过、21参数的 N2)
LIMIT         = 20                     # 先导前 N 张测试
OUT_DIR       = r"./import-pack "
FORCE_DUE_NOW = True                   # True=把所有卡 due 设为现在(导入后能立刻复习,测试用)
# ====================

os.makedirs(OUT_DIR, exist_ok=True)
col   = mw.col
now   = int(time.time())
today = col.sched.today
cids  = col.find_cards('deck:"%s"' % DECK)[:LIMIT]

def fld(note, name):
    keys = note.keys()
    return note[name].strip() if name in keys else ""

cards_lines, state_lines = [], []
for cid in cids:
    card = col.get_card(cid)
    note = card.note()

    front = fld(note, "VocabKanji")
    furi  = fld(note, "VocabFurigana")
    pos   = fld(note, "VocabPoS")
    defcn = fld(note, "VocabDefCN")
    sent  = fld(note, "SentKanji1")
    sentd = fld(note, "SentDef1")

    parts = []
    head = furi + (" [%s]" % pos if pos else "")
    if head:  parts.append(head)
    if defcn: parts.append(defcn)
    if sent:  parts.append("例: " + sent + (" / " + sentd if sentd else ""))
    back = "\n".join(parts)

    cards_lines.append(json.dumps(
        {"cardId": cid, "front": front, "back": back}, ensure_ascii=False))

    ms = card.memory_state
    s  = ms.stability  if ms else 0.0
    d  = ms.difficulty if ms else 0.0

    state = card.type
    if FORCE_DUE_NOW:
        due_unix = now
        last = -1 if card.type == 0 else (now - card.ivl * 86400 if card.ivl > 0 else now)
    elif card.type == 0:
        due_unix, last = now, -1
    elif card.type in (1, 3):
        due_unix = card.due if card.due > 1_000_000_000 else now
        last = due_unix - card.ivl * 86400 if card.ivl > 0 else now
    else:
        due_unix = now + (card.due - today) * 86400
        last = due_unix - card.ivl * 86400 if card.ivl > 0 else now

    step = -1 if card.type == 2 else 0
    state_lines.append(json.dumps({
        "cardId": cid, "state": state, "s": s, "d": d,
        "due": due_unix, "reps": card.reps, "lapses": card.lapses,
        "step": step, "lastReview": last,
    }, ensure_ascii=False))

with open(os.path.join(OUT_DIR, "cards.jsonl"), "w", encoding="utf-8") as f:
    f.write("\n".join(cards_lines) + "\n")
with open(os.path.join(OUT_DIR, "review_state.jsonl"), "w", encoding="utf-8") as f:
    f.write("\n".join(state_lines) + "\n")

# ---- 牌组 FSRS 参数 -> decks.json(设备用它,而不是默认参数)----
did2 = col.decks.id(DECK)
conf = col.decks.config_dict_for_deck_id(did2)
w = conf.get("fsrsParams6") or conf.get("fsrsParams5") or conf.get("fsrsWeights") or []
deck_obj = {
    "w":                list(w),
    "desiredRetention": conf.get("desiredRetention", 0.9),
    "learningSteps":    [int(x * 60) for x in conf["new"]["delays"]],     # 分 -> 秒
    "relearningSteps":  [int(x * 60) for x in conf["lapse"]["delays"]],
    "maximumInterval":  conf["rev"]["maxIvl"],
}
with open(os.path.join(OUT_DIR, "decks.json"), "w", encoding="utf-8") as f:
    json.dump(deck_obj, f, ensure_ascii=False)

print("导出 %d 张 -> %s" % (len(cids), OUT_DIR))
print("decks.json: %d 个权重, retention=%s, steps=%s/%s" % (
    len(w), deck_obj["desiredRetention"], deck_obj["learningSteps"], deck_obj["relearningSteps"]))
