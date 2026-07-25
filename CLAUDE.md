<!-- HOT:BEGIN -->
**WHAT.** Performance fork of erincatto/box3d. Floats stay floats; wins are FMA, LTO,
SIMD, cache layout. NOT mas-bandwidth/fixed3d, the Q48.16 fixed-point sibling where
determinism IS the point.

**DECISION: cross-platform determinism is traded for speed, deliberately.**
`BOX3D_GO_FAST` (CMakeLists.txt, default **ON**) adds `-ffp-contract=fast` on
Apple/Unix/MinGW plus Release LTO. Not an oversight.

**So an optimized ARM build FAILS `DeterminismTest`. EXPECTED.** Measured, M3 Ultra:
Release + GO_FAST gives sleepStep 344 / hash 0xD12BC462 vs the goldens 308 / 0x1E5EDD79
in test/test_determinism.c. Isolated: fp-contract=fast with LTO off still fails; LTO with
fp-contract=off passes; Debug passes. So the cause is contraction, not LTO. It is NOT a
worker-count effect: workers 1-8 all give the same 344 / 0xD12BC462, and the
contraction-off control gives 308 / 0x1E5EDD79 at all eight. The optimized build is
internally deterministic across schedules; it simply does not match the pinned goldens.
Do NOT rebaseline goldens, flip the default, or file it as a bug. CI cannot see it: the
test matrix is Debug, and its one optimized test job (windows-mingw) is x86-64, no baseline FMA.

**INVARIANT: the determinism line.** DETERMINISM-PRESERVING work (cache/SoA layout,
branchless, prefetch, algorithmic/integer, deterministic SIMD) may be offered
upstream to box3d, which requires bit-identical results on every platform.
DETERMINISM-BREAKING work (FMA, fp-contract, reassociation, fast-math, any float reorder)
is fork-only forever and useless upstream. ERIN.md's "Easy wins" list is
determinism-preserving ONLY.

**TRAPS.** Nothing inside is named fast3d: project, headers, options, symbols are all
`box3d` / `BOX3D_` / `b3`. An upstream port is a plain merge of `upstream/main`. In any
checkout carrying an `upstream` remote, `gh` silently resolves to erincatto/box3d, so
`gh run list` / `gh pr list` return HIS runs and PRs. Always pass
`--repo mas-bandwidth/fast3d`. CI fires on push, pull_request and workflow_dispatch, so
one sha gets duplicate runs; gate on matching headSha.

**NEVER** push or open a PR against erincatto/box3d. Give any `upstream` remote a dead push URL.
<!-- HOT:END -->

## Build and test

Default build (`build.sh`): `cmake -S . -B build && cmake --build build`.

What CI builds and runs (`.github/workflows/build.yml`), from the repo root:

```
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBOX3D_SAMPLES=OFF -DBUILD_SHARED_LIBS=OFF \
      -DBOX3D_VALIDATE=ON -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
cmake --build build --config Debug
cd build && ./bin/test
```

The unit test binary is `bin/test` (`bin/Debug/test` with multi-config generators such as
MSVC). CI's `windows-mingw` job is the one optimized job: `-DCMAKE_BUILD_TYPE=RelWithDebInfo`.

## Layout

`src` library, `include/box3d` public headers, `test` unit tests, `benchmark`, `samples`,
`shared` code shared by samples and benchmarks, `extern` third-party, `docs`.
`ERIN.md` is the optimization lab notebook and the determinism-preserving/breaking ledger.
