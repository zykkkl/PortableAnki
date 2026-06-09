# M4 完整写回探针(在 Anki Debug Console 里运行)
# 模拟"设备上传了一次 Good 评分",把【新 memory state + 新 due + 一条 revlog】完整写回一张 N2 卡,
# 读回确认后【完整恢复】(包括删掉补的 revlog)。单卡 + 恢复 + 你已备份,安全。
# 把全部输出贴回来。

from aqt import mw
import time
col = mw.col

cid  = col.find_cards('deck:"NEW-JLPT::NEW-N2"')[0]
card = col.get_card(cid)
ms   = card.memory_state
MS   = ms.__class__

# 备份原值
o = dict(s=ms.stability, d=ms.difficulty, ivl=card.ivl, due=card.due, reps=card.reps)
rev0 = col.db.scalar("select count() from revlog where cid=?", cid)
print("BEFORE : s=%.3f d=%.3f ivl=%d due=%d reps=%d | revlog=%d" %
      (o["s"], o["d"], o["ivl"], o["due"], o["reps"], rev0))

def make_ms(s, d):
    m = MS(); m.stability = s; m.difficulty = d; return m

# --- 模拟设备上传的新状态:Good,新 s=o.s+5,下次 30 天后 ---
new_s, new_d, days, ease = o["s"] + 5.0, o["d"], 30, 3
rid = None
try:
    today = col.sched.today
    card.memory_state = make_ms(new_s, new_d)
    card.ivl  = days
    card.due  = today + days          # review 卡 due = 天序号
    card.reps = o["reps"] + 1
    col.update_card(card)

    # 补一条 revlog(usn=-1 表示待同步;type=1 复习)
    rid = int(time.time() * 1000)
    col.db.execute(
        "insert into revlog (id, cid, usn, ease, ivl, lastIvl, factor, time, type) "
        "values (?,?,?,?,?,?,?,?,?)",
        rid, cid, -1, ease, days, o["ivl"], 0, 5000, 1)

    c2   = col.get_card(cid)
    rev1 = col.db.scalar("select count() from revlog where cid=?", cid)
    print("AFTER  : s=%.3f d=%.3f ivl=%d due=%d reps=%d | revlog=%d" %
          (c2.memory_state.stability, c2.memory_state.difficulty, c2.ivl, c2.due, c2.reps, rev1))
finally:
    # 完整恢复
    card.memory_state = make_ms(o["s"], o["d"])
    card.ivl  = o["ivl"]; card.due = o["due"]; card.reps = o["reps"]
    col.update_card(card)
    if rid is not None:
        col.db.execute("delete from revlog where id=?", rid)
    c3   = col.get_card(cid)
    rev2 = col.db.scalar("select count() from revlog where cid=?", cid)
    print("RESTORE: s=%.3f d=%.3f ivl=%d due=%d reps=%d | revlog=%d" %
          (c3.memory_state.stability, c3.memory_state.difficulty, c3.ivl, c3.due, c3.reps, rev2))
