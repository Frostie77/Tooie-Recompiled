#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import sys
r,t=map(Path,sys.argv[1:])
w=(r/'lib/rt64/src/hle/rt64_workload_queue.cpp').read_text()
p=(r/'lib/rt64/src/hle/rt64_present_queue.cpp').read_text()
c=(r/'lib/rt64/src/hle/rt64_shared_queue_resources.h').read_text()
a=c.index('    struct InterpolatedFrameCounters {');b=c.index('    };',a)+len('    };')
(t/'frame-counters.inc').write_text(c[a:b])
a=w.index('                for (uint32_t frame = 0; (frame < displayFrames)');b=w.index('                threadConfigurationValidate();',a)
(t/'producer-loop.inc').write_text(w[a:b])
a=p.index('            if ((framesToPresent > 1)');b=p.index('            uint32_t swapChainIndex',a)
(t/'present-select.inc').write_text(p[a:b])
a=p.index('            if (frameCountersNextPresented > 0)');b=p.index('// As soon as',a)
(t/'present-retire.inc').write_text(p[a:b])
a=w.index('                    const bool resetTicks = ');b=w.index('\n                    }',a)+len('\n                    }')
(t/'tick-reset.inc').write_text(w[a:b])
a=w.index('                uint32_t displayFrames = 1;',b);b=w.index('                ext.sharedResources->viOriginalRate',a)
(t/'tick-schedule.inc').write_text(w[a:b])
PY
"${CXX:-g++}" -std=c++17 -pthread -fsanitize=address,undefined -fno-omit-frame-pointer -I"$r/src" -I"$t" "$r/tests/frame_protocol.cpp" -o "$t/protocol"
ASAN_OPTIONS=detect_leaks=0 "$t/protocol"
