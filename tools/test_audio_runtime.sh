#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
read -ra sdl_cflags <<< "$(pkg-config --cflags sdl2)"
read -ra sdl_libs <<< "${SDL_TEST_LIBS:-$(pkg-config --libs sdl2)}"
flags=(-std=c++17 -pthread -fsanitize=address,undefined -fno-omit-frame-pointer -I"$r/src" -I"$t" "${sdl_cflags[@]}")
"${CXX:-clang++}" "${flags[@]}" "$r/tests/audio_queue.cpp" "${sdl_libs[@]}" -ldl -o "$t/audio"
ASAN_OPTIONS=detect_leaks=0 "$t/audio"
"${CXX:-clang++}" "${flags[@]}" -DTOOIE_AUDIO_REAL_DUMMY "$r/tests/audio_queue.cpp" "${sdl_libs[@]}" -ldl -o "$t/dummy"
SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0 "$t/dummy"
# Exercise the Android-only SDL event bridge on the host. The event watch must
# not call audio APIs while Android's playback-driver locks may still be held.
"${CXX:-clang++}" "${flags[@]}" -D__ANDROID__ -DSDL_MAIN_HANDLED "$r/tests/audio_queue.cpp" "${sdl_libs[@]}" -ldl -o "$t/resume"
ASAN_OPTIONS=detect_leaks=0 timeout 20 "$t/resume" resume
ASAN_OPTIONS=detect_leaks=0 timeout 20 "$t/resume" all
"${CXX:-clang++}" "${flags[@]}" -D__ANDROID__ -DSDL_MAIN_HANDLED -DTOOIE_AUDIO_REAL_DUMMY "$r/tests/audio_queue.cpp" "${sdl_libs[@]}" -ldl -o "$t/resume-dummy"
SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0 timeout 20 "$t/resume-dummy"
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import sys,re
r,t=map(Path,sys.argv[1:]);s=(r/'lib/N64ModernRuntime/ultramodern/src/events.cpp').read_text()
deadline=re.search(r'        auto next = ultramodern::get_start\(\).*;',s).group().strip()
index=re.search(r'        uint64_t new_total_vis = .*;',s).group().strip()
(t/'vi-equations.inc').write_text('using namespace std::chrono_literals;\nnamespace ultramodern { inline auto get_start(){return std::chrono::high_resolution_clock::time_point{};} inline unsigned get_speed_multiplier(){return 1u;} }\nuint64_t sourceNextViNs(uint64_t total_vis){'+deadline+' return std::chrono::duration_cast<std::chrono::nanoseconds>(next.time_since_epoch()).count();}\nuint64_t sourceViIndex(uint64_t ns){std::chrono::nanoseconds time_now(ns);'+index+' return new_total_vis;}\n')
PY
"${CXX:-clang++}" -std=c++17 -I"$r/src" -I"$t" "$r/tests/runtime_diagnostics.cpp" -o "$t/timing"
"$t/timing"
if [ "${1:-}" = --baseline ]; then
 git -C "$r" show a0ac6e2a670de99934c839c26ead58de205d217a:src/main.cpp > "$t/main-baseline.cpp"
 python3 - "$t" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=(p/'main-baseline.cpp').read_text();start=s.index('static SDL_AudioCVT audio_convert;');end=s.index('// --- Input',start)
(p/'audio-baseline.inc').write_text(s[start:end])
PY
 "${CXX:-clang++}" "${flags[@]}" -DTOOIE_AUDIO_BASELINE "$r/tests/audio_queue.cpp" "${sdl_libs[@]}" -ldl -o "$t/baseline"
 for mode in prefix rate alignment; do
  if ASAN_OPTIONS=detect_leaks=0 "$t/baseline" "$mode" >"$t/control.log" 2>&1;then echo "FAIL: baseline passed $mode";exit 1;else test "$?" -eq 1;fi
  grep -q 'FAIL' "$t/control.log"
 done
 echo 'PASS: three tested-base audio controls reproduce prefix, output-rate accounting and fragmented stereo-frame defects'
fi
if [ -n "${TOOIE_JDK:-}" ]; then
 "$TOOIE_JDK/bin/javac" --release 17 -d "$t/classes" "$r/android/app/src/main/java/io/github/banjotooierecompiled/WindowSnapshotLog.java" "$r/tests/java_window/io/github/banjotooierecompiled/WindowSnapshotLogTest.java"
 "$TOOIE_JDK/bin/java" -ea -cp "$t/classes" io.github.banjotooierecompiled.WindowSnapshotLogTest
fi
