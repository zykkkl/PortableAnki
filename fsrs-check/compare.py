"""
对拍:我们的 C++ Fsrs  vs  官方 py-fsrs。
- 同一批用例,两边各跑一遍,逐条比对 state/step/stability/difficulty/间隔。
- 两边都关 fuzz、用 FSRS-6 默认 21 参数。
用法:  python compare.py
"""
import os
import sys
import subprocess
from datetime import datetime, timezone, timedelta

HERE = os.path.dirname(os.path.abspath(__file__))
SCHED_DIR = os.path.join(HERE, "..", "ESP4Anki", "lib", "Scheduler")
PYFSRS_DIR = os.path.join(HERE, "..", "py-fsrs")
RUNNER_CPP = os.path.join(HERE, "runner.cpp")
RUNNER_EXE = os.path.join(HERE, "runner.exe")

sys.path.insert(0, PYFSRS_DIR)
from fsrs import Scheduler, Card, Rating, State  # noqa: E402

NOW_TS = 1705320000  # 必须与 runner.cpp 的 NOW 一致
NOW = datetime.fromtimestamp(NOW_TS, tz=timezone.utc)

# 用例: (state, step, stability, difficulty, elapsedDays, rating)
#   stability/difficulty = -1 -> None(新卡);elapsedDays = -1 -> last_review None
cases = []
labels = []
def add(label, state, step, stab, diff, elapsed):
    for r in (1, 2, 3, 4):
        cases.append((state, step, stab, diff, elapsed, r))
        labels.append(f"{label} rating={r}")

add("新卡Learning",        1, 0, -1,  -1,  -1)
add("Learning step0 同日", 1, 0, 5.0, 5.0, 0)
add("Learning step1 同日", 1, 1, 5.0, 5.0, 0)
add("Review 跨15天",       2, -1, 10.0, 5.0, 15)
add("Review 跨5天大S",     2, -1, 100.0, 7.0, 5)
add("Review 同日",         2, -1, 10.0, 5.0, 0)
add("Relearning step0同日",3, 0, 8.0, 6.0, 0)

# ---- 1) 编译并运行 C++ runner ----
print("编译 runner.cpp ...")
subprocess.run(
    ["g++", "-O2", "-I", SCHED_DIR, RUNNER_CPP,
     os.path.join(SCHED_DIR, "Fsrs.cpp"), "-o", RUNNER_EXE],
    check=True,
)
csv_in = "".join("%d,%d,%g,%g,%d,%d\n" % c for c in cases)
res = subprocess.run([RUNNER_EXE], input=csv_in, capture_output=True, text=True, check=True)
cpp_rows = [l for l in res.stdout.strip().splitlines() if l]

# ---- 2) py-fsrs 跑同样用例 ----
sch = Scheduler(enable_fuzzing=False)

def py_review(case):
    state, step, stab, diff, elapsed, rating = case
    if stab < 0:  # 新卡
        card = Card(card_id=1)  # 默认 Learning/step0/None/last_review None
    else:
        last = None if elapsed < 0 else NOW - timedelta(days=elapsed)
        card = Card(
            card_id=1,
            state=State(state),
            step=(None if step < 0 else step),
            stability=stab,
            difficulty=diff,
            last_review=last,
            due=NOW,
        )
    newc, _ = sch.review_card(card, Rating(rating), review_datetime=NOW)
    ivl = int(round((newc.due - NOW).total_seconds()))
    st = newc.step if newc.step is not None else -1
    return (newc.state.value, st, newc.stability, newc.difficulty, ivl)

# ---- 3) 比对 ----
ok = fail = 0
for i, case in enumerate(cases):
    py = py_review(case)
    c = cpp_rows[i].split(",")
    cpp = (int(c[0]), int(c[1]), float(c[2]), float(c[3]), int(c[4]))
    same = (
        py[0] == cpp[0] and py[1] == cpp[1]
        and abs(py[2] - cpp[2]) < 1e-6
        and abs(py[3] - cpp[3]) < 1e-6
        and py[4] == cpp[4]
    )
    if same:
        ok += 1
    else:
        fail += 1
        print(f"\n[FAIL] {labels[i]}  输入={case}")
        print(f"   py : state={py[0]} step={py[1]} s={py[2]:.6f} d={py[3]:.6f} ivl={py[4]}s")
        print(f"   cpp: state={cpp[0]} step={cpp[1]} s={cpp[2]:.6f} d={cpp[3]:.6f} ivl={cpp[4]}s")

print(f"\n==== {ok} passed / {fail} failed / {len(cases)} total ====")
sys.exit(1 if fail else 0)
