#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
u="$r/lib/RecompFrontend/recompui/lib/RmlUi"
if [ -n "${TOOIE_RML_TEST_BUILD_DIR:-}" ];then b=$TOOIE_RML_TEST_BUILD_DIR;mkdir -p "$b";else b=$(mktemp -d);trap 'rm -rf "$b"' EXIT;fi
core=${TOOIE_RML_TEST_CORE_DIR:-$b/core}
flags=(-std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DRMLUI_STATIC_LIB -I"$u/Include")
cmake -S "$u" -B "$core" -G Ninja -DBUILD_SHARED_LIBS=OFF -DRMLUI_FONT_ENGINE=none -DRMLUI_PRECOMPILED_HEADERS=OFF -DRMLUI_COMPILER_OPTIONS=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER="${CXX:-g++}" -DCMAKE_CXX_FLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build "$core" --target rmlui_core -j"${TOOIE_RML_TEST_JOBS:-3}"
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
archive="$core/Source/Core/librmlui.a"
"${CXX:-g++}" "${flags[@]}" "$r/tests/rml_document_lifetime.cpp" "$archive" -o "$b/current"
"$b/current"
"$b/current" --destroy-query
# Only two implementation objects differ. Compile their exact parent source
# against the same ABI headers (new helper is nonvirtual, no layout changes).
mkdir -p "$b/parent/Source/Core" "$b/guard"
ln -sfn "$u/Include" "$b/parent/Include"
for unit in Element ElementDocument;do
 git -C "$r" show "01495d04a248bb263a0f0d18695c9cfc983cf2d5:lib/RecompFrontend/recompui/lib/RmlUi/Source/Core/$unit.cpp" > "$b/parent/Source/Core/$unit.cpp"
 "${CXX:-g++}" "${flags[@]}" -I"$u/Source/Core" -c "$b/parent/Source/Core/$unit.cpp" -o "$b/parent/$unit.cpp.o"
done
cp "$archive" "$b/parent.a"
ar d "$b/parent.a" Element.cpp.o ElementDocument.cpp.o
ar r "$b/parent.a" "$b/parent/Element.cpp.o" "$b/parent/ElementDocument.cpp.o"
"${CXX:-g++}" "${flags[@]}" "$r/tests/rml_document_lifetime.cpp" "$b/parent.a" -o "$b/parent-test"
python3 - "$b" <<'PY'
from pathlib import Path
import subprocess,sys
b=Path(sys.argv[1])
for mode,needle in [('normal','does not point to an object of type'),('query','stack-overflow')]:
 args=[str(b/'parent-test')]+(['--destroy-query'] if mode=='query' else [])
 p=subprocess.run(args,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 (b/(mode+'-parent.log')).write_text(p.stdout)
 assert p.returncode!=0 and needle in p.stdout,(mode,p.returncode,p.stdout[-2000:])
 if mode=='query':assert 'Rml::Element::IsLayoutDirty()' in p.stdout
print('PASS: exact 01495d0/f017d00 source fails normal document detachment (vptr lifetime) and controlled destruction query (actual recursive stack)')
PY
# Test the Android-only diagnostic body on host with an abort-message adapter.
# Other core units need no recompilation: there are no conditional class fields.
"${CXX:-g++}" "${flags[@]}" -I"$r/tests/android_headers" -D__ANDROID__ -DTOOIE_ANDROID_CORRECTION_TRACE=1 -c "$u/Source/Core/Element.cpp" -o "$b/guard/Element.cpp.o"
cp "$archive" "$b/guard.a"
ar d "$b/guard.a" Element.cpp.o
ar r "$b/guard.a" "$b/guard/Element.cpp.o"
"${CXX:-g++}" "${flags[@]}" "$r/tests/rml_document_lifetime.cpp" "$b/guard.a" -o "$b/guard-test"
"$b/guard-test"
"$b/guard-test" --destroy-query
python3 - "$b" <<'PY'
from pathlib import Path
import signal,subprocess,sys
b=Path(sys.argv[1]);p=subprocess.run([str(b/'guard-test'),'--cycle'],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(b/'guard-cycle.log').write_text(p.stdout)
assert p.returncode==-signal.SIGABRT,(p.returncode,p.stdout[-2000:])
assert 'layout recursion: depth=16' in p.stdout and 'caller=' in p.stdout and 'vtable=' in p.stdout
assert 'stack-overflow' not in p.stdout
print('PASS: Android diagnostic recursion guard aborts with original query addresses before stack exhaustion; normal layout/lifetime remains valid')
PY
