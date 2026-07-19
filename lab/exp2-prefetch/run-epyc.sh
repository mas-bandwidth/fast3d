#!/usr/bin/env bash
# Exp 2 EPYC bench: prefetch patch vs Night-0 SSE2 baseline (SAME build flags — the
# ONLY variable is the patch). Fire only after exp1's sweep is done (pinned core).
set -e
cd "$(dirname "$0")"
ssh space 'bash -s' <<'REMOTE'
set -e
cd ~/fast3d-lab
git stash -q 2>/dev/null || true
git checkout -q main 2>/dev/null || true
git checkout -q -B exp/prefetch-gather
git apply --whitespace=nowarn /tmp/exp2.diff
echo "=== diff applied ==="; git diff --stat
cmake -B build-prefetch -DCMAKE_BUILD_TYPE=Release -DBOX3D_BENCHMARKS=ON -DBOX3D_SAMPLES=OFF \
  -DBOX3D_UNIT_TESTS=OFF -DBOX3D_VALIDATE=OFF 2>&1 | tail -1
cmake --build build-prefetch -j2 2>&1 | tail -1
BIN="build-prefetch/bin/benchmark"; [ -x "$BIN" ] || BIN="build-prefetch/benchmark"
echo "=== bench: prefetch build, 1 worker, 3 reps ==="
for b in convex_pile joint_grid junkyard large_pyramid large_world many_pyramids rain trees100 trees50 trees25 washer; do
  echo "## $b"
  "$BIN" -b=$b -w=1 -r=3 2>&1 | grep -E "run|error" || echo "FAILED: $b"
done
echo "=== DONE ==="
REMOTE
