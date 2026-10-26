#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# RUN the demo that mayhem/build.sh produced (normal flags) over every GoPro sample MP4 and compare
# its extracted telemetry (video framerate, payload time, stream index, scaled sensor data, computed
# sample rates) byte-for-byte with the committed golden output; plus a few explicit known answers.
RUNNER=/mayhem/gpmfdemo-test
[ -x "$RUNNER" ] || { echo "missing $RUNNER (mayhem/build.sh should have built it)" >&2; emit_ctrf "gpmfdemo-golden" 0 1 0; exit 1; }

passed=0 failed=0
check() {  # check <name> <ok:0|1>
  if [ "$2" -eq 0 ]; then passed=$((passed+1)); echo "PASS $1"; else failed=$((failed+1)); echo "FAIL $1"; fi
}

for golden in mayhem/test/golden/*.txt; do
  name="$(basename "$golden" .txt)"
  "$RUNNER" "samples/$name.mp4" -i > "/tmp/gpmf-$name.out" 2>&1
  cmp -s "/tmp/gpmf-$name.out" "$golden"; check "golden $name" $?
done

expect() {  # expect <sample> <fixed string that must appear in the output>
  "$RUNNER" "samples/$1.mp4" -i > /tmp/gpmf-expect.out 2>&1
  grep -aqF -- "$2" /tmp/gpmf-expect.out; check "$1 has '$2'" $?
}
expect hero8 "29.970 with 379 frames"
expect hero8 "STRM of ACCL of type s with 202 samples -- 3 elements per sample"
expect hero8 "ACCL 9.832m/s"
expect hero8 "STRM of GPS5 of type l with 19 samples -- 5 elements per sample"
expect Fusion "29.970 with 454 frames"
expect karma "29.970 with 362 frames"

"$RUNNER" mayhem/test/golden/hero8.txt >/tmp/gpmf-bad.out 2>&1
grep -aqF "invalid MP4/MOV or it has no GPMF data" /tmp/gpmf-bad.out; check "non-MP4 input is rejected" $?

emit_ctrf "gpmfdemo-golden" "$passed" "$failed" 0
