#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
if [ "$#" -gt 0 ];then
 python3 "$r/tests/step118_evidence.py" "$1" "$t"
else
 echo 'SKIP: full original Step 118 archive not supplied; pinned retained clock fixture still tested'
fi
TOOIE_CAPTURED_CLOCK_FIXTURE="$r/tests/fixtures/odin-step118-clocks.txt" "$r/tools/test_guest_timing_trace.sh"
