# Agent 10 — utils.h + unit tests
Date: 2026-02-25

## Files written / modified
- src/utils.h: added build_telemetry_topic() and battery_status_str() inline functions
- test/test_all.cpp: added 6 Unity tests (2 × telemetry topic, 4 × battery_status_str)

## Steps performed
1. Added build_telemetry_topic() after format_float_1dp() in src/utils.h
2. Added battery_status_str() after build_telemetry_topic() in src/utils.h
3. Added 6 test functions before main() in test/test_all.cpp
4. Added 6 RUN_TEST() calls inside main() before UNITY_END()

## Build / test results
N/A — build verification run by orchestrator after commit

## Issues encountered
None

## Exit status
PASS
