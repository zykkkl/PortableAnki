// 对拍用:读 stdin 的用例,调用我们的 Fsrs,输出结果到 stdout。
// 每行输入: state,step,stability,difficulty,elapsedDays,rating
//   stability/difficulty = -1 表示 None(新卡);elapsedDays = -1 表示从未复习
// 每行输出: state,step,stability,difficulty,intervalSec
#include "Fsrs.h"
#include <cstdio>

int main() {
  const long long NOW = 1705320000LL;   // 必须与 compare.py 的 NOW_TS 一致
  Fsrs fsrs(FsrsParams::defaults());

  char line[256];
  while (fgets(line, sizeof(line), stdin)) {
    int state, step, rating; double stab, diff; long elapsed;
    if (sscanf(line, "%d,%d,%lf,%lf,%ld,%d",
               &state, &step, &stab, &diff, &elapsed, &rating) != 6) {
      continue;
    }
    FsrsCard c;
    c.state      = state;
    c.step       = step;
    c.stability  = stab;
    c.difficulty = diff;
    c.lastReview = (elapsed < 0) ? -1 : (NOW - (long long)elapsed * 86400);
    c.due        = 0;

    FsrsCard o = fsrs.review(c, rating, NOW);
    printf("%d,%d,%.10f,%.10f,%lld\n",
           o.state, o.step, o.stability, o.difficulty, (long long)(o.due - NOW));
  }
  return 0;
}
