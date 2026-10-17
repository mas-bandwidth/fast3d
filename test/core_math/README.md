One file per CORE-MATH row, added by that row card and touching nothing else; see docs/determinism-math.md.
test/CMakeLists.txt globs core_math/*.c into the test target and defines B3_CORE_MATH_ROW_<STEM>=1 for each file present.
test_core_math.c runs the row subtest under that define and otherwise prints core-math row=<name> pending: <card>.
