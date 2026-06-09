"""
Mini Anki Bridge - Anki 插件

在 Anki 内常驻一个 HTTP 服务,给 PortableAnki 设备:
  GET  /profiles           当前 profile 和本机可检测 profile 列表
  GET  /decks              当前 profile 的卡组列表
  GET  /cards.jsonl        导出卡片内容,参数: deck/deckId, limit
  GET  /review_state.jsonl 导出每卡复习状态,参数同上
  GET  /decks.json         导出所选卡组 FSRS 参数
  POST /upload             接收设备上传的评分,写回 card + revlog

读取侧通过当前 Anki profile 的 collection.anki2 建立 SQLite 只读连接。
写回仍在 Anki 主线程执行,并且只写当前打开的 profile。
"""

import html
import json
import os
import re
import sqlite3
import threading
import time
from contextlib import closing
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlparse
from urllib.request import pathname2url

from aqt import gui_hooks, mw


DEFAULT_CONFIG = {
    "port": 8766,
    "force_due_now": True,
    "include_subdecks": True,
}

TRUE_VALUES = {"1", "true", "yes", "y", "on"}
FALSE_VALUES = {"0", "false", "no", "n", "off"}


def addon_config():
    cfg = DEFAULT_CONFIG.copy()
    try:
        stored = mw.addonManager.getConfig(__name__) or {}
        cfg.update({k: v for k, v in stored.items() if v is not None})
    except Exception:  # noqa
        pass
    return cfg


# col 操作必须在主线程,这里把闭包调度到主线程并等结果。
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


def _maybe_call(value):
    return value() if callable(value) else value


def _profile_name():
    pm = getattr(mw, "pm", None)
    name = _maybe_call(getattr(pm, "name", None)) if pm else None
    if name:
        return str(name)

    folder = _profile_folder()
    return os.path.basename(folder) if folder else ""


def _profile_folder():
    pm = getattr(mw, "pm", None)
    if not pm:
        return ""

    folder = _maybe_call(getattr(pm, "profileFolder", None))
    return os.path.abspath(folder) if folder else ""


def _collection_path():
    col = mw.col
    for attr in ("path", "_path"):
        path = _maybe_call(getattr(col, attr, None))
        if path and os.path.exists(path):
            return os.path.abspath(path)

    folder = _profile_folder()
    if folder:
        path = os.path.join(folder, "collection.anki2")
        if os.path.exists(path):
            return os.path.abspath(path)

    raise RuntimeError("无法定位当前 profile 的 collection.anki2")


def _norm_path(path):
    return os.path.normcase(os.path.abspath(path))


def _available_profiles(active_collection, current_name):
    base = ""
    pm = getattr(mw, "pm", None)
    if pm:
        base = _maybe_call(getattr(pm, "base", None)) or ""

    if not base:
        base = os.path.dirname(os.path.dirname(active_collection))

    profiles = []
    if os.path.isdir(base):
        for name in sorted(os.listdir(base)):
            collection = os.path.join(base, name, "collection.anki2")
            if os.path.exists(collection):
                profiles.append(
                    {
                        "name": name,
                        "collectionPath": os.path.abspath(collection),
                        "active": _norm_path(collection)
                        == _norm_path(active_collection),
                    }
                )

    if not profiles:
        profiles.append(
            {
                "name": current_name,
                "collectionPath": active_collection,
                "active": True,
            }
        )

    return profiles


def _current_deck_id_and_name():
    try:
        deck = mw.col.decks.current()
        if isinstance(deck, dict):
            did = int(deck.get("id"))
            name = str(deck.get("name", ""))
        else:
            did = int(_maybe_call(getattr(deck, "id")))
            name = str(_maybe_call(getattr(deck, "name", "")))
        return did, _display_deck_name(name)
    except Exception:  # noqa
        return None, ""


def _active_context():
    cfg = addon_config()
    collection = _collection_path()
    profile = _profile_name()
    current_did, current_name = _current_deck_id_and_name()
    return {
        "profile": profile,
        "collectionPath": collection,
        "profiles": _available_profiles(collection, profile),
        "today": int(_maybe_call(getattr(mw.col.sched, "today", 0))),
        "currentDeckId": current_did,
        "currentDeckName": current_name,
        "forceDueNow": _bool_value(cfg.get("force_due_now"), True),
        "includeSubdecks": _bool_value(cfg.get("include_subdecks"), True),
    }


def _open_collection(path):
    uri = "file:" + pathname2url(os.path.abspath(path)) + "?mode=ro"
    conn = sqlite3.connect(uri, uri=True)
    conn.row_factory = sqlite3.Row
    return conn


def _table_exists(conn, name):
    row = conn.execute(
        "select 1 from sqlite_master where type='table' and name=?",
        (name,),
    ).fetchone()
    return row is not None


def _table_columns(conn, table):
    return {row["name"] for row in conn.execute(f"pragma table_info({table})")}


def _display_deck_name(name):
    return str(name).replace("\x1f", "::")


def _native_deck_name(name):
    return str(name).replace("::", "\x1f")


def _load_decks(conn):
    if _table_exists(conn, "decks"):
        rows = conn.execute("select id, name from decks order by name").fetchall()
        decks = [
            {
                "id": int(row["id"]),
                "name": _display_deck_name(row["name"]),
                "nativeName": str(row["name"]),
            }
            for row in rows
        ]
    else:
        row = conn.execute("select decks from col").fetchone()
        raw = json.loads(row["decks"] or "{}") if row else {}
        decks = []
        for key, deck in raw.items():
            name = str(deck.get("name", ""))
            decks.append(
                {
                    "id": int(deck.get("id", key)),
                    "name": _display_deck_name(name),
                    "nativeName": _native_deck_name(name),
                }
            )
        decks.sort(key=lambda item: item["name"].lower())

    counts = _deck_counts(conn)
    for deck in decks:
        direct = counts.get(deck["id"], {})
        deck["directCardCount"] = int(direct.get("total", 0))
        deck["newCount"] = int(direct.get("new", 0))
        deck["learningCount"] = int(direct.get("learning", 0))
        deck["reviewCount"] = int(direct.get("review", 0))
        deck["suspendedCount"] = int(direct.get("suspended", 0))
        deck["totalCardCount"] = _total_cards_with_children(deck, decks, counts)

    return decks


def _deck_counts(conn):
    rows = conn.execute(
        """
        select
            did,
            count(*) as total,
            sum(case when queue = 0 then 1 else 0 end) as new,
            sum(case when queue in (1, 3) then 1 else 0 end) as learning,
            sum(case when queue = 2 then 1 else 0 end) as review,
            sum(case when queue < 0 then 1 else 0 end) as suspended
        from cards
        group by did
        """
    ).fetchall()
    return {int(row["did"]): dict(row) for row in rows}


def _total_cards_with_children(deck, decks, counts):
    prefix = deck["name"] + "::"
    total = 0
    for item in decks:
        if item["name"] == deck["name"] or item["name"].startswith(prefix):
            total += int(counts.get(item["id"], {}).get("total", 0))
    return total


def _public_decks(decks, ctx):
    current_id = ctx.get("currentDeckId")
    return [
        {
            "id": deck["id"],
            "name": deck["name"],
            "current": deck["id"] == current_id,
            "directCardCount": deck["directCardCount"],
            "totalCardCount": deck["totalCardCount"],
            "newCount": deck["newCount"],
            "learningCount": deck["learningCount"],
            "reviewCount": deck["reviewCount"],
            "suspendedCount": deck["suspendedCount"],
        }
        for deck in decks
    ]


def _first_query(params, *names):
    for name in names:
        values = params.get(name)
        if values:
            return values[0]
    return None


def _int_query(params, *names, default=None):
    raw = _first_query(params, *names)
    if raw is None or str(raw).strip() == "":
        return default
    return int(raw)


def _limit_query(params):
    raw = _first_query(params, "limit")
    if raw is None or str(raw).strip() == "":
        return None
    raw = str(raw).strip()
    if raw.lower() in {"0", "all", "none", "unlimited"}:
        return None
    limit = int(raw)
    if limit < 1:
        raise ValueError("limit 必须大于 0,或使用 limit=all")
    return limit


def _bool_value(value, default=False):
    if value is None:
        return default
    if isinstance(value, bool):
        return value
    text = str(value).strip().lower()
    if text in TRUE_VALUES:
        return True
    if text in FALSE_VALUES:
        return False
    return default


def _bool_query(params, name, default=False):
    return _bool_value(_first_query(params, name), default)


def _indices_query(params, name):
    raw = _first_query(params, name)
    if raw is None or str(raw).strip() == "":
        return None
    return [int(part.strip()) for part in str(raw).split(",") if part.strip()]


def _resolve_deck(decks, params, ctx):
    did = _int_query(params, "deckId", "did")
    deck_name = _first_query(params, "deck", "deckName")

    if did is None and not deck_name:
        did = ctx.get("currentDeckId")

    if did is not None:
        selected = next((deck for deck in decks if deck["id"] == did), None)
        if selected is None:
            raise ValueError(f"找不到 deckId={did} 的卡组")
    elif deck_name:
        selected = _find_deck_by_name(decks, deck_name)
    else:
        selected = None

    if selected is None:
        return None, [deck["id"] for deck in decks]

    include_subdecks = _bool_query(
        params,
        "includeSubdecks",
        ctx.get("includeSubdecks", True),
    )
    if not include_subdecks:
        return selected, [selected["id"]]

    prefix = selected["name"] + "::"
    dids = [
        deck["id"]
        for deck in decks
        if deck["name"] == selected["name"] or deck["name"].startswith(prefix)
    ]
    return selected, dids


def _find_deck_by_name(decks, deck_name):
    target = _display_deck_name(deck_name)
    native = _native_deck_name(deck_name)
    for deck in decks:
        if deck["name"] == target or deck["nativeName"] == native:
            return deck

    lowered = target.lower()
    for deck in decks:
        if deck["name"].lower() == lowered:
            return deck

    raise ValueError(f"找不到名为 {deck_name} 的卡组")


def _select_card_rows(conn, dids, limit):
    if not dids:
        return []

    card_columns = _table_columns(conn, "cards")
    left_expr = "c.left as remaining_steps" if "left" in card_columns else "0 as remaining_steps"
    data_expr = "c.data as data" if "data" in card_columns else "'' as data"
    placeholders = ",".join("?" for _ in dids)
    sql = f"""
        select
            c.id as cid,
            c.nid as nid,
            c.did as did,
            c.type as type,
            c.queue as queue,
            c.due as due,
            c.ivl as ivl,
            c.reps as reps,
            c.lapses as lapses,
            {left_expr},
            {data_expr},
            n.flds as flds
        from cards c
        join notes n on n.id = c.nid
        where c.did in ({placeholders})
        order by
            case when c.queue in (1, 3, 4) then c.due else 0 end,
            case when c.queue = 2 then c.due else 0 end,
            c.id
    """
    args = list(dids)
    if limit is not None:
        sql += " limit ?"
        args.append(limit)
    return conn.execute(sql, args).fetchall()


def _clean_field(value):
    text = "" if value is None else str(value)
    text = re.sub(r"(?is)<(script|style).*?</\1>", "", text)
    text = re.sub(r"(?i)<br\s*/?>", "\n", text)
    text = re.sub(r"(?s)<[^>]+>", "", text)
    text = html.unescape(text).replace("\xa0", " ")
    text = re.sub(r"[ \t\r\f\v]+", " ", text)
    lines = [line.strip() for line in text.splitlines()]
    return "\n".join(line for line in lines if line)


def _split_fields(flds):
    return [_clean_field(part) for part in str(flds or "").split("\x1f")]


def _front_back(fields, front_index=None, back_indices=None):
    if not fields:
        return "", ""

    if front_index is None:
        front_index = next((i for i, value in enumerate(fields) if value), 0)
    front = fields[front_index] if 0 <= front_index < len(fields) else ""

    if back_indices is None:
        back_values = [
            value for i, value in enumerate(fields) if i != front_index and value
        ]
    else:
        back_values = [
            fields[i] for i in back_indices if 0 <= i < len(fields) and fields[i]
        ]
    return front, "\n".join(back_values)


def _card_payload(row, front_index=None, back_indices=None):
    fields = _split_fields(row["flds"])
    front, back = _front_back(fields, front_index, back_indices)
    return {"cardId": int(row["cid"]), "front": front, "back": back}


def _json_dict(value):
    if not value:
        return {}
    try:
        parsed = json.loads(value)
        return parsed if isinstance(parsed, dict) else {}
    except Exception:  # noqa
        return {}


def _float_or_zero(value):
    try:
        return float(value)
    except Exception:  # noqa
        return 0.0


def _review_state_payload(row, now, today, force_due_now):
    state = int(row["type"] or 0)
    due_raw = int(row["due"] or 0)
    ivl = int(row["ivl"] or 0)

    if force_due_now:
        due = now
        last = -1 if state == 0 else (now - ivl * 86400 if ivl > 0 else now)
    elif state == 0:
        due, last = now, -1
    elif state in (1, 3):
        due = due_raw if due_raw > 1_000_000_000 else now
        last = due - ivl * 86400 if ivl > 0 else now
    else:
        due = now + (due_raw - today) * 86400
        last = due - ivl * 86400 if ivl > 0 else now

    data = _json_dict(row["data"])
    remaining_steps = int(row["remaining_steps"] or 0) % 1000
    step = -1 if state == 2 else max(0, remaining_steps - 1)

    return {
        "cardId": int(row["cid"]),
        "state": state,
        "s": _float_or_zero(data.get("s")),
        "d": _float_or_zero(data.get("d")),
        "due": due,
        "reps": int(row["reps"] or 0),
        "lapses": int(row["lapses"] or 0),
        "step": step,
        "lastReview": last,
    }


def build_cards_and_states(params):
    ctx = run_on_main_sync(_active_context)
    limit = _limit_query(params)
    force_due_now = _bool_query(params, "forceDueNow", ctx["forceDueNow"])
    front_index = _int_query(params, "frontIndex")
    back_indices = _indices_query(params, "backIndices")
    now = int(time.time())

    with closing(_open_collection(ctx["collectionPath"])) as conn:
        decks = _load_decks(conn)
        selected, dids = _resolve_deck(decks, params, ctx)
        rows = _select_card_rows(conn, dids, limit)

    cards = [_card_payload(row, front_index, back_indices) for row in rows]
    states = [_review_state_payload(row, now, ctx["today"], force_due_now) for row in rows]
    return {
        "cards": cards,
        "states": states,
        "profile": ctx["profile"],
        "deck": _public_selected_deck(selected, dids),
        "limit": limit,
        "forceDueNow": force_due_now,
    }


def _public_selected_deck(selected, dids):
    if selected is None:
        return {"id": None, "name": None, "selectedDeckIds": dids}
    return {
        "id": selected["id"],
        "name": selected["name"],
        "selectedDeckIds": dids,
    }


def build_deck_listing():
    ctx = run_on_main_sync(_active_context)
    with closing(_open_collection(ctx["collectionPath"])) as conn:
        decks = _load_decks(conn)

    return {
        "profile": ctx["profile"],
        "collectionPath": ctx["collectionPath"],
        "currentDeckId": ctx["currentDeckId"],
        "currentDeckName": ctx["currentDeckName"],
        "decks": _public_decks(decks, ctx),
    }


def build_deck_config(params):
    ctx = run_on_main_sync(_active_context)
    with closing(_open_collection(ctx["collectionPath"])) as conn:
        decks = _load_decks(conn)
        selected, _dids = _resolve_deck(decks, params, ctx)

    did = selected["id"] if selected else ctx.get("currentDeckId")
    return run_on_main_sync(lambda: _deck_config_payload(did))


def _deck_config_payload(did):
    if did is None:
        raise RuntimeError("未选择卡组,无法导出 decks.json")

    conf = mw.col.decks.config_dict_for_deck_id(int(did))
    w = (
        _float_list(_first_present(conf, "fsrsParams6", "fsrs_params_6"))
        or _float_list(_first_present(conf, "fsrsParams5", "fsrs_params_5"))
        or _float_list(_first_present(conf, "fsrsWeights", "fsrs_params_4"))
    )

    new_conf = conf.get("new", {}) if isinstance(conf.get("new"), dict) else {}
    lapse_conf = conf.get("lapse", {}) if isinstance(conf.get("lapse"), dict) else {}
    rev_conf = conf.get("rev", {}) if isinstance(conf.get("rev"), dict) else {}

    learning = _first_present(conf, "learningSteps", "learnSteps", "learn_steps")
    if learning is None:
        learning = new_conf.get("delays", [])

    relearning = _first_present(
        conf,
        "relearningSteps",
        "relearnSteps",
        "relearn_steps",
    )
    if relearning is None:
        relearning = lapse_conf.get("delays", [])

    maximum = _first_present(conf, "maximumInterval", "maximum_review_interval")
    if maximum is None:
        maximum = rev_conf.get("maxIvl", 36500)

    desired = _first_present(conf, "desiredRetention", "desired_retention")
    if desired is None:
        desired = 0.9

    return {
        "w": w,
        "desiredRetention": float(desired),
        "learningSteps": _steps_to_seconds(learning),
        "relearningSteps": _steps_to_seconds(relearning),
        "maximumInterval": int(maximum),
    }


def _first_present(mapping, *keys):
    for key in keys:
        if key in mapping and mapping[key] not in (None, ""):
            return mapping[key]
    return None


def _float_list(value):
    if value is None:
        return []
    if isinstance(value, str):
        value = re.split(r"[\s,]+", value.strip())
    try:
        return [float(item) for item in value]
    except Exception:  # noqa
        return []


def _steps_to_seconds(values):
    out = []
    for value in values or []:
        try:
            out.append(int(float(value) * 60))
        except Exception:  # noqa
            pass
    return out


# 写回(主线程执行)。dry=True 时只计算、不改任何数据。
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

        if state == 2:  # review: due 用天序号
            ivl = max(1, round((due_unix - rated) / 86400))
            new_due = today + max(0, round((due_unix - now) / 86400))
            queue = 2
        else:  # learning/relearning: due 用时间戳
            ivl = card.ivl
            new_due = due_unix
            queue = state

        if dry:
            preview.append(
                {
                    "cardId": cid,
                    "new_s": new_s,
                    "new_d": new_d,
                    "ivl": ivl,
                    "new_due": new_due,
                    "ease": ev.get("ease"),
                }
            )
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
            rid,
            cid,
            -1,
            int(ev.get("ease", 3)),
            card.ivl,
            0,
            0,
            int(ev.get("timeMs", 0)),
            1,
        )
        applied += 1

    if dry:
        return {"dryRun": True, "preview": preview}
    return {"applied": applied}


class Handler(BaseHTTPRequestHandler):
    def _send(self, code, body, content_type="application/json; charset=utf-8"):
        if isinstance(body, (dict, list)):
            body = json.dumps(body, ensure_ascii=False)
        data = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        parsed = urlparse(self.path)
        params = parse_qs(parsed.query)
        path = parsed.path
        try:
            if path == "/":
                ctx = run_on_main_sync(_active_context)
                self._send(
                    200,
                    {
                        "service": "Mini Anki Bridge",
                        "profile": ctx["profile"],
                        "currentDeckId": ctx["currentDeckId"],
                        "currentDeckName": ctx["currentDeckName"],
                        "endpoints": [
                            "/profiles",
                            "/decks",
                            "/cards.jsonl?deck=<name>&limit=<n>",
                            "/review_state.jsonl?deck=<name>&limit=<n>",
                            "/decks.json?deck=<name>",
                            "/upload",
                        ],
                    },
                )
            elif path == "/profiles":
                ctx = run_on_main_sync(_active_context)
                self._send(
                    200,
                    {
                        "currentProfile": ctx["profile"],
                        "activeCollection": ctx["collectionPath"],
                        "profiles": ctx["profiles"],
                    },
                )
            elif path == "/decks":
                self._send(200, build_deck_listing())
            elif path == "/cards.jsonl":
                export = build_cards_and_states(params)
                body = "\n".join(
                    json.dumps(card, ensure_ascii=False) for card in export["cards"]
                )
                self._send(200, body + ("\n" if body else ""), "application/x-ndjson; charset=utf-8")
            elif path == "/review_state.jsonl":
                export = build_cards_and_states(params)
                body = "\n".join(
                    json.dumps(state, ensure_ascii=False) for state in export["states"]
                )
                self._send(200, body + ("\n" if body else ""), "application/x-ndjson; charset=utf-8")
            elif path == "/decks.json":
                self._send(200, build_deck_config(params))
            else:
                self._send(404, {"error": "not found"})
        except Exception as e:  # noqa
            self._send(500, {"error": str(e)})

    def do_POST(self):
        parsed = urlparse(self.path)
        params = parse_qs(parsed.query)
        try:
            if parsed.path != "/upload":
                self._send(404, {"error": "not found"})
                return

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

            dry = _bool_query(params, "dry", False)
            res = run_on_main_sync(lambda: apply_events(events, dry))
            self._send(200, res)
        except Exception as e:  # noqa
            self._send(500, {"error": str(e)})

    def log_message(self, *args):
        pass


_server = None


def start_server():
    global _server
    if _server is not None:
        return

    cfg = addon_config()
    port = int(cfg.get("port", DEFAULT_CONFIG["port"]))
    _server = HTTPServer(("0.0.0.0", port), Handler)
    threading.Thread(target=_server.serve_forever, daemon=True).start()
    print("[Mini Anki Bridge] listening on port", port)


gui_hooks.profile_did_open.append(start_server)
