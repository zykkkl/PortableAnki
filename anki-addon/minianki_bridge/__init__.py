"""
Mini Anki Bridge —— Anki 插件
在 Anki 内常驻一个 HTTP 服务,给 Mini Anki OS 设备:
  GET  /cards.jsonl         导出卡片内容
  GET  /review_state.jsonl  导出每卡 memory state
  GET  /decks.json          导出牌组 FSRS 参数
  POST /upload              接收设备上传的评分,写回 card + revlog
端口默认 8766。只动配置里指定的牌组。
"""
import json
import time
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler

from aqt import mw, gui_hooks

# ===== 配置 =====
CONFIG = {
    "port": 8766,
    "deck": "NEW-JLPT::NEW-N2",
    "limit": 20,
    "force_due_now": True,   # True=导出时把 due 设为现在(测试用)
}

# ---- col 操作必须在主线程,这里把闭包调度到主线程并等结果 ----
def run_on_main_sync(func):
    box = {}
    done = threading.Event()
    def wrapper():
        try:
            box["v"] = func()
        except Exception as e:  # noqa
            box["e"] = e
        finally:
            done.set()
    mw.taskman.run_on_main(wrapper)
    done.wait()
    if "e" in box:
        raise box["e"]
    return box.get("v")

# ---- 导出(主线程执行)----
def build_export():
    col = mw.col
    now = int(time.time())
    today = col.sched.today
    deck = CONFIG["deck"]
    cids = col.find_cards('deck:"%s"' % deck)[: CONFIG["limit"]]

    def fld(note, name):
        return note[name].strip() if name in note.keys() else ""

    cards, states = [], []
    for cid in cids:
        card = col.get_card(cid)
        note = card.note()
        front = fld(note, "VocabKanji")
        furi = fld(note, "VocabFurigana")
        pos = fld(note, "VocabPoS")
        defcn = fld(note, "VocabDefCN")
        sent = fld(note, "SentKanji1")
        sentd = fld(note, "SentDef1")
        parts = []
        head = furi + (" [%s]" % pos if pos else "")
        if head:
            parts.append(head)
        if defcn:
            parts.append(defcn)
        if sent:
            parts.append("例: " + sent + (" / " + sentd if sentd else ""))
        cards.append({"cardId": cid, "front": front, "back": "\n".join(parts)})

        ms = card.memory_state
        s = ms.stability if ms else 0.0
        d = ms.difficulty if ms else 0.0
        if CONFIG["force_due_now"]:
            due = now
            last = -1 if card.type == 0 else (now - card.ivl * 86400 if card.ivl > 0 else now)
        elif card.type == 0:
            due, last = now, -1
        elif card.type in (1, 3):
            due = card.due if card.due > 1_000_000_000 else now
            last = due - card.ivl * 86400 if card.ivl > 0 else now
        else:
            due = now + (card.due - today) * 86400
            last = due - card.ivl * 86400 if card.ivl > 0 else now
        step = -1 if card.type == 2 else 0
        states.append({"cardId": cid, "state": card.type, "s": s, "d": d,
                       "due": due, "reps": card.reps, "lapses": card.lapses,
                       "step": step, "lastReview": last})

    did = col.decks.id(deck)
    conf = col.decks.config_dict_for_deck_id(did)
    w = conf.get("fsrsParams6") or conf.get("fsrsParams5") or conf.get("fsrsWeights") or []
    decks = {
        "w": list(w),
        "desiredRetention": conf.get("desiredRetention", 0.9),
        "learningSteps": [int(x * 60) for x in conf["new"]["delays"]],
        "relearningSteps": [int(x * 60) for x in conf["lapse"]["delays"]],
        "maximumInterval": conf["rev"]["maxIvl"],
    }
    return cards, states, decks

# ---- 写回(主线程执行)。dry=True 时只计算、不改任何数据 ----
def apply_events(events, dry=False):
    col = mw.col
    today = col.sched.today
    now = int(time.time())
    preview = []
    applied = 0
    for ev in events:
        cid = int(ev["cardId"])
        try:
            card = col.get_card(cid)
        except Exception:
            preview.append({"cardId": cid, "error": "card not found"})
            continue

        state = int(ev.get("state", 2))
        due_unix = int(ev.get("due", now))
        rated = int(ev.get("ratedAt", now))
        new_s = float(ev["s"]) if "s" in ev else None
        new_d = float(ev["d"]) if "d" in ev else None

        if state == 2:  # review:due 用天序号
            ivl = max(1, round((due_unix - rated) / 86400))
            new_due = today + max(0, round((due_unix - now) / 86400))
            queue = 2
        else:           # learning/relearning:due 用时间戳
            ivl = card.ivl
            new_due = due_unix
            queue = state

        if dry:
            preview.append({"cardId": cid, "new_s": new_s, "new_d": new_d,
                            "ivl": ivl, "new_due": new_due, "ease": ev.get("ease")})
            continue

        ms = card.memory_state
        if ms is not None and new_s is not None:
            m = ms.__class__()
            m.stability = new_s
            m.difficulty = new_d if new_d is not None else ms.difficulty
            card.memory_state = m
        card.ivl = ivl
        card.due = new_due
        card.queue = queue
        card.type = state
        if "reps" in ev:
            card.reps = int(ev["reps"])
        if "lapses" in ev:
            card.lapses = int(ev["lapses"])
        col.update_card(card)

        rid = int(time.time() * 1000) + applied
        col.db.execute(
            "insert into revlog (id,cid,usn,ease,ivl,lastIvl,factor,time,type) "
            "values (?,?,?,?,?,?,?,?,?)",
            rid, cid, -1, int(ev.get("ease", 3)), card.ivl, 0, 0,
            int(ev.get("timeMs", 0)), 1)
        applied += 1

    if dry:
        return {"dryRun": True, "preview": preview}
    return {"applied": applied}

# ---- HTTP ----
class Handler(BaseHTTPRequestHandler):
    def _send(self, code, body):
        data = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        try:
            cards, states, decks = run_on_main_sync(build_export)
            if self.path.startswith("/cards.jsonl"):
                self._send(200, "\n".join(json.dumps(c, ensure_ascii=False) for c in cards) + "\n")
            elif self.path.startswith("/review_state.jsonl"):
                self._send(200, "\n".join(json.dumps(s, ensure_ascii=False) for s in states) + "\n")
            elif self.path.startswith("/decks.json"):
                self._send(200, json.dumps(decks, ensure_ascii=False))
            else:
                self._send(404, '{"error":"not found"}')
        except Exception as e:  # noqa
            self._send(500, json.dumps({"error": str(e)}))

    def do_POST(self):
        try:
            n = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(n).decode("utf-8").strip()
            events = []
            if body.startswith("["):
                events = json.loads(body)
            else:
                for line in body.splitlines():
                    line = line.strip()
                    if line:
                        events.append(json.loads(line))
            dry = "dry=1" in self.path
            res = run_on_main_sync(lambda: apply_events(events, dry))
            self._send(200, json.dumps(res, ensure_ascii=False))
        except Exception as e:  # noqa
            self._send(500, json.dumps({"error": str(e)}))

    def log_message(self, *args):
        pass  # 静音访问日志

# ---- 启动 ----
_server = None

def start_server():
    global _server
    if _server is not None:
        return
    _server = HTTPServer(("0.0.0.0", CONFIG["port"]), Handler)
    threading.Thread(target=_server.serve_forever, daemon=True).start()
    print("[Mini Anki Bridge] listening on port", CONFIG["port"])

gui_hooks.profile_did_open.append(start_server)
