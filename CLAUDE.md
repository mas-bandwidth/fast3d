<!-- HOT:BEGIN -->
**WHAT.** Box3D, but faster: the float/double line of erincatto/box3d, optimized and SIMD'd
as far as it goes while staying deterministic. NOT fixed3d, the Q48.16 sibling.

**DECISION: fast3d is deterministic across platforms and compilers, always.** The judge is
fast3d's own determinism scenes (test/test_determinism.c), bit-identical on x86-64 and
arm64, gcc, clang and MSVC, Debug and optimized. Bit-identity with box3d is not a goal:
fast3d may diverge from box3d for speed, never from itself. The old `BOX3D_GO_FAST` option
(`-ffp-contract=fast` + Release LTO) was removed 2026-10-15 because it is what broke
determinism (optimized arm64 missed the goldens). Do not bring back FMA contraction,
reassociation, fast-math or any other value-changing float transform; upstream's
`-ffp-contract=off` guard in CMakeLists.txt is on for every gcc and clang build. A failing `DeterminismTest` in
any build is a bug.

**INVARIANT: every speedup preserves determinism** (cache/SoA layout, branchless, prefetch,
algorithmic/integer, deterministic SIMD). Those are also the ones worth offering upstream
to box3d.

**TRAPS.** Nothing inside is named fast3d: project, headers, options and symbols are all
`box3d` / `BOX3D_` / `b3`. An upstream port applies each box3d commit in order
(cherry-pick, original author, `(cherry picked from box3d <sha>)` trailer), one branch,
one PR. In any checkout with an `upstream` remote, `gh` silently resolves to
erincatto/box3d and returns HIS runs and PRs, so always pass `--repo mas-bandwidth/fast3d`.
CI fires on push, pull_request and workflow_dispatch, so one sha gets duplicate runs; gate
on headSha.

**NEVER** push or PR against erincatto/box3d. Give any `upstream` remote a dead push URL.
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
MSVC). CI's optimized jobs (`windows-mingw`, `windows-relwithdebinfo`,
`macos-relwithdebinfo`) build `RelWithDebInfo`.

## Layout

`src` library, `include/box3d` public headers, `test` unit tests, `benchmark`, `samples`,
`shared` code shared by samples and benchmarks, `extern` third-party, `docs`.
`ERIN.md` is the optimization lab notebook.
