#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
if [ "$#" -gt 0 ];then
 python3 "$r/tests/step115_evidence.py" "$1" "$t"
else
 echo 'SKIP: full original Step 115 archive not supplied; pinned retained clock fixture still tested'
fi
TOOIE_CAPTURED_CLOCK_FIXTURE="$r/tests/fixtures/odin-step115-clocks.txt" "$r/tools/test_guest_timing_trace.sh"
