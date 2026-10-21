#!/usr/bin/env bash
set -euo pipefail

# Reusable workflow: build task-delivery and report-window gap diagnostics with approved actors, mods and artwork.
# Pins both integration parents, selected round BT icon and approved red/blue title.
# Timing observer ON; geometry/correction capture OFF; allocation probe OFF; queue isolation=0;
# transfer/fallback/barrier diagnostic selectors remain UNDEFINED.
# Build/export only. No ADB operations, installation, launch or data changes.
# Preserves existing workspaces, APKs, dependency cache and reports.
# Usage: ~/projects/tooie-tools/tooie build

REPO='https://github.com/Frostie77/Tooie-Recompiled.git'
BRANCH='@@BRANCH@@'
ROOT="$HOME/projects"
TOOL_ROOT="$ROOT/tooie-tools"
LEGACY="$ROOT/Tooie-Recompiled-Test"
SDK="$HOME/Android/Sdk/ndk/28.2.13676358"
SDL="$HOME/Android/prefixes/SDL2-2.32.10-android-arm64"
FT="$HOME/Android/prefixes/freetype-2.13.3-android-arm64"
WINDOWS_DOWNLOADS="${TOOIE_WORKFLOW_EXPORT:?}"
READELF="$SDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf"
MODE='task-delivery-timing'
TIMING_CXX_FLAGS='-DTOOIE_ANDROID_TIMING_TRACE=1 -DTOOIE_ANDROID_CORRECTION_TRACE=0 -DTOOIE_ANDROID_GEOMETRY_TRACE=0 -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=0'

if [[ $# -ne 0 ]]; then
  echo 'Usage: ~/projects/tooie-tools/tooie build' >&2
  exit 2
fi
REQUESTED='@@COMMIT@@'
EXPECTED_PARENT='@@ENGINEERING@@ @@BRANDING@@'

for command in git cmake ninja gradle rsync unzip sha256sum cmp awk grep rg flock file python3 ar sed tail mktemp cp mkdir pkg-config ln; do
  command -v "$command" >/dev/null 2>&1 || { echo "STOP: missing command: $command" >&2; exit 1; }
done
command -v "${CXX:-clang++}" >/dev/null 2>&1 || { echo 'STOP: host C++ compiler missing' >&2; exit 1; }
OUTPUT="$WINDOWS_DOWNLOADS/Tooie-${REQUESTED:0:12}-Demo-Render-Launcher-Timing-Debug.apk"
REPORT="$WINDOWS_DOWNLOADS/Tooie-${REQUESTED:0:12}-Demo-Render-Launcher-Timing-Debug-build-report.txt"
if [[ -e "$OUTPUT" || -e "$REPORT" ]]; then
  echo 'STOP: this gameplay APK or report already exists; existing results are preserved.' >&2
  exit 1
fi
for path in "$SDK" "$SDL" "$FT" "$WINDOWS_DOWNLOADS"; do
  [[ -d "$path" ]] || { echo "STOP: missing directory: $path" >&2; exit 1; }
done
[[ -x "$READELF" ]] || { echo "STOP: missing LLVM readelf: $READELF" >&2; exit 1; }

# New host audio checks require shared Linux SDL2, GCC sanitizer runtimes,
# and a JDK supporting Java 17. The Android SDL library remains the cached one.
CLOCK_CC="${CC:-gcc}"
command -v "$CLOCK_CC" >/dev/null 2>&1 || { echo 'STOP: correction/clock host checks require gcc (or CC).' >&2; exit 1; }
python3 - <<'VERIFY_CORRECTION_PYTHON'
import sys
if sys.version_info < (3, 11):
    raise SystemExit('STOP: correction hook checks require Python 3.11 or later.')
import tomllib
VERIFY_CORRECTION_PYTHON
AUDIO_CXX="${AUDIO_CXX:-g++}"
command -v "$AUDIO_CXX" >/dev/null 2>&1 || { echo 'STOP: audio host checks require g++ (or AUDIO_CXX).' >&2; exit 1; }
if ! pkg-config --exists sdl2; then
  echo 'STOP: host SDL2 development files missing (Ubuntu package libsdl2-dev).' >&2
  echo 'Existing builds are preserved. Install the prerequisite and rerun this builder.' >&2
  exit 1
fi
if [[ -n "${TOOIE_JDK:-}" ]]; then
  AUDIO_JDK="$TOOIE_JDK"
elif [[ -n "${JAVA_HOME:-}" && -x "$JAVA_HOME/bin/javac" ]]; then
  AUDIO_JDK="$JAVA_HOME"
elif command -v javac >/dev/null 2>&1; then
  AUDIO_JDK=$(python3 - "$(command -v javac)" <<'FIND_AUDIO_JDK'
from pathlib import Path
import sys
print(Path(sys.argv[1]).resolve().parent.parent)
FIND_AUDIO_JDK
)
else
  echo 'STOP: host window tests require a JDK 17+; set TOOIE_JDK to its directory.' >&2
  exit 1
fi
[[ -x "$AUDIO_JDK/bin/javac" && -x "$AUDIO_JDK/bin/java" ]] || { echo 'STOP: TOOIE_JDK/JAVA_HOME does not contain executable javac and java.' >&2; exit 1; }
JDK_VERSION=$("$AUDIO_JDK/bin/javac" -version 2>&1)
python3 - "$JDK_VERSION" <<'VERIFY_AUDIO_JDK'
import re, sys
version = re.search(r'javac\s+(\d+)', sys.argv[1])
if not version or int(version[1]) < 17:
    raise SystemExit('STOP: host window tests require JDK 17 or later.')
VERIFY_AUDIO_JDK
# Use the same SDK 36 platform already needed by this Android project.
# Resolve installed tools before creating an immutable build workspace.
ANDROID_JAR=$(python3 - <<'FIND_MOD_ANDROID_JAR'
from pathlib import Path
import os, sys
override = os.environ.get('TOOIE_ANDROID_JAR')
roots = [Path.home() / 'Android/Sdk']
roots += [Path(os.environ[name]) for name in ('ANDROID_SDK_ROOT', 'ANDROID_HOME') if os.environ.get(name)]
candidates = [Path(override)] if override else [root / 'platforms/android-36/android.jar' for root in roots]
for path in candidates:
    if path.is_file():
        print(path.resolve())
        break
else:
    raise SystemExit('STOP: installed Android SDK 36 android.jar missing; set TOOIE_ANDROID_JAR to its path.')
FIND_MOD_ANDROID_JAR
)
AAPT2=$(python3 - <<'FIND_MOD_AAPT2'
from pathlib import Path
import os, re, shutil
override = os.environ.get('TOOIE_AAPT2')
roots = [Path.home() / 'Android/Sdk']
roots += [Path(os.environ[name]) for name in ('ANDROID_SDK_ROOT', 'ANDROID_HOME') if os.environ.get(name)]
def version(path):
    return tuple(int(part) for part in re.findall(r'\d+', path.parent.name))
candidates = [Path(override)] if override else []
if not override:
    for root in roots:
        candidates += sorted(root.glob('build-tools/*/aapt2'), key=version, reverse=True)
    if shutil.which('aapt2'):
        candidates.append(Path(shutil.which('aapt2')))
for path in candidates:
    if path.is_file() and os.access(path, os.X_OK):
        print(path.absolute())
        break
else:
    raise SystemExit('STOP: installed Linux SDK aapt2 missing; set TOOIE_AAPT2 to its executable path.')
FIND_MOD_AAPT2
)
"$AAPT2" version >/dev/null
[[ -f "$AUDIO_JDK/include/jni.h" && -f "$AUDIO_JDK/include/linux/jni_md.h" ]] || {
  echo 'STOP: complete Linux JDK 17+ JNI development headers are required.' >&2; exit 1;
}

read -r -a AUDIO_SDL_CFLAGS <<< "$(pkg-config --cflags sdl2)"
read -r -a AUDIO_SDL_LIBS <<< "${SDL_TEST_LIBS:-$(pkg-config --libs sdl2)}"
AUDIO_PREFLIGHT=$(mktemp -d)
trap 'rm -rf -- "$AUDIO_PREFLIGHT"' EXIT
cat > "$AUDIO_PREFLIGHT/audio.cpp" <<'AUDIO_PREFLIGHT_CPP'
#include <SDL.h>
#include <cstdio>
int main() {
    SDL_version version;
    SDL_GetVersion(&version);
    std::printf("Host audio SDL runtime: %u.%u.%u; revision: %s\n",
        version.major, version.minor, version.patch, SDL_GetRevision());
}
AUDIO_PREFLIGHT_CPP
if ! "$AUDIO_CXX" -std=c++17 -fsanitize=address,undefined -fno-omit-frame-pointer \
  "${AUDIO_SDL_CFLAGS[@]}" "$AUDIO_PREFLIGHT/audio.cpp" "${AUDIO_SDL_LIBS[@]}" \
  -o "$AUDIO_PREFLIGHT/audio" > "$AUDIO_PREFLIGHT/compile.log" 2>&1; then
  cat "$AUDIO_PREFLIGHT/compile.log"
  echo 'STOP: host SDL2/sanitizer preflight failed; existing builds are preserved.' >&2
  exit 1
fi
if ! ASAN_OPTIONS=detect_leaks=0 "$AUDIO_PREFLIGHT/audio" > "$AUDIO_PREFLIGHT/runtime.log" 2>&1; then
  cat "$AUDIO_PREFLIGHT/runtime.log"
  echo 'STOP: host audio sanitizer runtime failed; existing builds are preserved.' >&2
  exit 1
fi
cat "$AUDIO_PREFLIGHT/runtime.log"
printf 'Host audio compiler: %s; window tests: %s\n' "$AUDIO_CXX" "$JDK_VERSION"

# The new geometry checks run both scalar sanitizer and host SIMD builds.
RENDER_SIMD_CXX="${SIMD_CXX:-clang++}"
command -v "$RENDER_SIMD_CXX" >/dev/null 2>&1 || {
  echo 'STOP: geometry SIMD checks need clang++ (or SIMD_CXX); existing builds are preserved.' >&2
  exit 1
}


# Verify actual MIPS words before creating an immutable build folder.
FIRST_PERSON_CLANG=$(python3 - "$SDK" "$AUDIO_PREFLIGHT" <<'FIND_FIRST_PERSON_ASSEMBLER'
from pathlib import Path
import json,os,shutil,struct,subprocess,sys

sdk,folder=map(Path,sys.argv[1:])
override=os.environ.get('TOOIE_FIRST_PERSON_CLANG')
choices=[override] if override else ['clang-19','clang',str(sdk/'toolchains/llvm/prebuilt/linux-x86_64/bin/clang')]
assembly='.set noat\n.set noreorder\n.set mips3\n.text\nlui $at,1\naddu $at,$at,$t7\nlw $t7,-0x5B6C($at)\njr $t7\nnop\n'
source=folder/'first-person-preflight.s';source.write_text(assembly)
errors=[];seen=set()
for choice in choices:
    candidate=shutil.which(choice) if choice else None
    if not candidate:
        errors.append(str(choice)+': not found');continue
    candidate=str(Path(candidate).resolve())
    if candidate in seen:continue
    seen.add(candidate)
    try:
        result=subprocess.run([candidate,'-target','mips-unknown-none','-mno-abicalls','-c',str(source),'-o',str(folder/'first-person-preflight.o')],capture_output=True,text=True,timeout=30)
        if result.returncode:raise ValueError(result.stderr[-600:])
        elf=(folder/'first-person-preflight.o').read_bytes()
        if elf[:6]!=b'\x7fELF\x01\x02' or struct.unpack_from('>H',elf,18)[0]!=8:raise ValueError('not big-endian ELF32 MIPS')
        offset=struct.unpack_from('>I',elf,32)[0]
        size,count,names_index=struct.unpack_from('>HHH',elf,46)
        if size!=40 or not 0<=names_index<count:raise ValueError('invalid section headers')
        headers=[struct.unpack_from('>10I',elf,offset+i*size) for i in range(count)]
        names=elf[headers[names_index][4]:headers[names_index][4]+headers[names_index][5]]
        text=[h for h in headers if names[h[0]:].split(b'\0',1)[0]==b'.text']
        if len(text)!=1 or struct.unpack_from('>5I',elf,text[0][4])!=(0x3C010001,0x002F0821,0x8C2FA494,0x01E00008,0):raise ValueError('MIPS dispatch instruction words differ')
        version=subprocess.run([candidate,'--version'],capture_output=True,text=True,timeout=10,check=True).stdout.splitlines()[0]
        (folder/'first-person-assembler.json').write_text(json.dumps({'compiler':candidate,'version':version,'requested_name':'clang-19','route':'private host-test alias; no source changes','mips_words_verified':True},indent=2)+'\n')
        print(candidate);break
    except (OSError,ValueError,struct.error,IndexError,subprocess.SubprocessError) as error:
        errors.append(candidate+': '+str(error)[-600:])
else:
    raise SystemExit('STOP: no usable MIPS assembler for first-person regression. Set TOOIE_FIRST_PERSON_CLANG to a working Linux Clang executable.\n'+'\n'.join(errors))
FIND_FIRST_PERSON_ASSEMBLER
)
printf 'Host first-person assembler: %s\n' "$FIRST_PERSON_CLANG"

CACHE="$TOOL_ROOT/dependency-cache"
RUN_ROOT="${TOOIE_WORKFLOW_RUN:?}"
SRC="${TOOIE_WORKFLOW_SOURCE:?}"
BUILD="${TOOIE_WORKFLOW_BUILD:?}"
PACKAGE="${TOOIE_WORKFLOW_PACKAGE:?}"
BASELINE="$RUN_ROOT/mod-baseline"
for path in "$CACHE/zstd/CMakeLists.txt" "$CACHE/android/libSDL2.so" "$CACHE/dxc/libdxcompiler.so" "$CACHE/dxc/libdxil.so"; do
  [[ -f "$path" ]] || { echo "STOP: required retained dependency is missing: $path" >&2; exit 1; }
done
[[ -x "$CACHE/host/file_to_c" ]] || { echo 'STOP: retained host generator is missing.' >&2; exit 1; }
if [[ -e "$RUN_ROOT" || -L "$RUN_ROOT" ]]; then
  echo 'STOP: this immutable build folder already exists; files/logs are retained. Do not overwrite it.' >&2
  exit 1
fi
mkdir -p "$TOOL_ROOT/builds"
mkdir "$RUN_ROOT"
mkdir "$RUN_ROOT/logs"
LOGS="$RUN_ROOT/logs"
trap 'status=$?; if (( status != 0 )); then printf "\nBuild stopped. Existing files retained. Logs: %s\n" "$LOGS" >&2; fi; rm -rf -- "$AUDIO_PREFLIGHT"' EXIT
cp "$AUDIO_PREFLIGHT/runtime.log" "$LOGS/audio-host-preflight.log"
printf '%s\n' "$JDK_VERSION" >> "$LOGS/audio-host-preflight.log"
cp "$AUDIO_PREFLIGHT/first-person-assembler.json" "$LOGS/first-person-assembler.json"
mkdir "$RUN_ROOT/first-person-host-bin"
ln -s -- "$FIRST_PERSON_CLANG" "$RUN_ROOT/first-person-host-bin/clang-19"
printf '\n=== 1/7 CLONE AND VERIFY THE PINNED INTEGRATION ===\n'
# The Python runner owns, locks and pins this checkout before starting the build.
[[ "$(git -C "$SRC" rev-parse HEAD)" == "$REQUESTED" ]] || {
  echo 'STOP: owned source checkout differs from the pinned revision.' >&2; exit 1;
}
FULL_SHA="$REQUESTED"
ACTUAL_PARENT=$(git -C "$SRC" show -s --format=%P HEAD)
[[ "$ACTUAL_PARENT" == "$EXPECTED_PARENT" ]] || { echo 'STOP: integration parents differ.' >&2; exit 1; }
printf 'Verified source: %s\nVerified parents: %s\n' "$FULL_SHA" "$ACTUAL_PARENT"

python3 - "$SRC" <<'VERIFY_INTEGRATED_SOURCE'
from pathlib import Path
import hashlib,json,struct,subprocess,sys
source=Path(sys.argv[1])
identity=@@IDENTITY@@
manifest=@@MANIFEST@@
def git(*args):return subprocess.check_output(['git','-C',str(source),*args]).decode().strip()
if git('rev-parse','HEAD')!=identity['commit'] or git('rev-parse','HEAD^{tree}')!=identity['tree']:
    raise SystemExit('STOP: source commit/tree differs from the independently reconstructed integration.')
if git('show','-s','--format=%P','HEAD').split()!=identity['parents']:
    raise SystemExit('STOP: integration parent order differs.')
if git('status','--porcelain','--untracked-files=no'):
    raise SystemExit('STOP: tracked source differs before validation.')
mod={x['path']:x for x in manifest['mod_entries']}
engineering={x['path']:x for x in manifest['engineering_entries']}
if set(mod)&set(engineering):raise SystemExit('STOP: source deltas overlap.')
for parent,names in ((identity['parents'][0],mod),(identity['parents'][1],engineering)):
    if git('diff','--name-only',parent,'HEAD').splitlines()!=sorted(names):
        raise SystemExit('STOP: integration includes changes outside the reviewed disjoint parent deltas.')
for entry in [*mod.values(),*engineering.values()]:
    observed=git('ls-tree','HEAD','--',entry['path']).split()
    if observed!=[entry['mode'],entry['type'],entry['sha'],entry['path']]:
        raise SystemExit('STOP: exact source blob/mode differs: '+entry['path'])
for root in ('assets','android/app/src/main/assets/program/assets'):
    for name,digest,size in (
        ('menu.png','3c125d58ad008e837f3c7dc335133d03bb9aae19086ffbde90d8a71daa52c707',(1672,941)),
        ('menu-recompiled.png','e76b97de0c685566db2505e85d42052a520c8f31237cbfdbfe405a1706295269',(2203,714))):
        data=source.joinpath(root,name).read_bytes()
        if hashlib.sha256(data).hexdigest()!=digest or data[:8]!=b'\x89PNG\r\n\x1a\n' or struct.unpack('>II',data[16:24])!=size:
            raise SystemExit('STOP: approved launcher image differs: '+root+'/'+name)
    if hashlib.sha256(source.joinpath(root,'launcher-layout.json').read_bytes()).hexdigest()!='b242f7244ce35c3eb656e30a48def2ece22c4c7a7ee2b38c8deb25e4b9d79d55':
        raise SystemExit('STOP: approved launcher layout differs.')
if hashlib.sha256(source.joinpath('assets/android/launcher-master.png').read_bytes()).hexdigest()!='4b6db92f4ad3280c10131b5cec745508b19257a93ab1b2615279813ebca06784':
    raise SystemExit('STOP: approved round BT/Jiggy master differs.')
print('PASS: exact integration tree and both parents; 107 engineering files and 69 mod/branding/host files retained byte-for-byte, with no overlap; both approved launcher layers verified.')
VERIFY_INTEGRATED_SOURCE
python3 - "$SRC" <<'VERIFY_FIRST_PERSON_SOURCE'
from pathlib import Path
import hashlib,json,re,struct,sys

root=Path(sys.argv[1])
data=(root/'RecompiledFuncs/funcs_44.c').read_text()
matches=list(re.finditer(r'^RECOMP_FUNC void bsfirstp_entrypoint_26\(.*?(?=^RECOMP_FUNC )',data,re.M|re.S))
if len(matches)!=1:raise SystemExit('STOP: first-person production function missing or duplicated.')
body=matches[0][0]
if hashlib.sha256(body.encode()).hexdigest()!='997edfb876fe4a765e4dce7d7ede0bbbdcf9cbbe55bbb610c4bfec3f6fcb2412':
    raise SystemExit('STOP: complete reviewed first-person consumer differs.')
fixture=json.loads((root/'tests/fixtures/bsfirstp_table_us.json').read_text())
values=[int(v,16) for v in fixture['raw_offsets']]
if len(values)!=47 or hashlib.sha256(struct.pack('>47I',*values)).hexdigest()!='57810e0d7399ae57ef9f0e261ea4c315875400ee67c8ff91174e20751fcd2cd3':
    raise SystemExit('STOP: complete supplied USA table differs.')
actual=[(int(i),int(pc,16)) for i,pc in re.findall(r'case (\d+): goto L_([0-9A-F]+); break;',body)]
if actual!=[(i,0x80800000+v) for i,v in enumerate(values)]:
    raise SystemExit('STOP: ordered USA switch differs.')
print('PASS: exact complete first-person consumer, all 47 ordered USA cases and supplied table hash; captured selector 0x73 reaches internal case 0x5AE0.')
VERIFY_FIRST_PERSON_SOURCE
python3 - "$SRC" "$LOGS/bsfirstp-source-profile.json" <<'VERIFY_BSFIRSTP_SOURCE'
from pathlib import Path
import hashlib,json,re,sys

root=Path(sys.argv[1])
reviewed={'fixture_sha256': '7bc7cd52c090d8da384833f342c9f9bfca6803515ebf72dbe0fd4d6f94612acd', 'audit_function_count': 306, 'table_slots': 23, 'functions': [{'function': 'func_80800F14_bsfirstp', 'file': 'RecompiledFuncs/funcs_44.c', 'normalized_lf_sha256': 'fac5e4b226e2a62374eaead8a1e231fac59c939730c624901fad411fb1ac1b13', 'raw_offsets': ['00000FA4', '00000FE0', '00000FE0', '00000FB8', '00000FCC', '00000FA4'], 'jr_link_address': '80800F9C', 'frame': 40}, {'function': 'func_80801000_bsfirstp', 'file': 'RecompiledFuncs/funcs_44.c', 'normalized_lf_sha256': '5a4e4baae2c1568a9a3e58a227c897aae3fb4c6660618432365772608b3ab245', 'raw_offsets': ['00001554', '00001114', '00001428', '00001428', '000012C0', '00001114'], 'jr_link_address': '8080110C', 'frame': 136}, {'function': 'func_80801E18_bsfirstp', 'file': 'RecompiledFuncs/funcs_44.c', 'normalized_lf_sha256': 'd0e52d83ec95701234a1a54bfe4c77c0265658bff57994945069a20fd22771c3', 'raw_offsets': ['00001E64', '00001EB4', '00001EFC', '00001F38', '00001F68', '00001F98'], 'jr_link_address': '80801E5C', 'frame': 40}, {'function': 'func_8080762C_bsfirstp', 'file': 'RecompiledFuncs/funcs_45.c', 'normalized_lf_sha256': 'a4a06099847fa3fb76c982714c9e3fc1ad9cdb0f61927425a18018677d23173d', 'raw_offsets': ['00007660', '00007690', '00007660', '000076B8', '0000775C'], 'jr_link_address': '80807658', 'frame': 32}]}
raw=(root/'tests/fixtures/bsfirstp_dispatches_us.json').read_bytes()
if hashlib.sha256(raw).hexdigest()!=reviewed['fixture_sha256']:
    raise SystemExit('STOP: four-switch USA fixture differs from the verified Step 108 handoff.')
fixture=json.loads(raw)
sources={relative:(root/relative).read_text() for relative in sorted({f['file'] for f in reviewed['functions']})}
all_source='\n'.join(sources.values())
for spec in reviewed['functions']:
    name=spec['function']
    matches=re.findall(r'^RECOMP_FUNC void '+name+r'\(.*?(?=^RECOMP_FUNC |\Z)',all_source,re.M|re.S)
    if len(matches)!=1 or hashlib.sha256(matches[0].encode()).hexdigest()!=spec['normalized_lf_sha256']:
        raise SystemExit('STOP: complete reviewed first-person consumer differs: '+name)
    actual=[(int(i),int(pc,16)) for i,pc in re.findall(r'case (\d+): goto L_([0-9A-F]+); break;',matches[0])]
    expected=[(i,0x80800000+int(v,16)) for i,v in enumerate(spec['raw_offsets'])]
    if actual!=expected:raise SystemExit('STOP: ordered ROM cases differ: '+name)
    supplied=[t for t in fixture['tables'] if t['function']==name]
    if len(supplied)!=1 or supplied[0]['raw_offsets']!=spec['raw_offsets']:
        raise SystemExit('STOP: source/table provenance differs: '+name)
functions=[];candidates=[]
for m in re.finditer(r'^RECOMP_FUNC void (\w+)\(.*?(?=^RECOMP_FUNC |\Z)',all_source,re.M|re.S):
    name,body=m[1],m[0]
    if not (name.endswith('_bsfirstp') or name.startswith('bsfirstp_entrypoint_')):continue
    functions.append(name)
    for reg,off in re.findall(r'ctx->r(\d+) = MEM_W\(ctx->r1, \(int16_t\)RELOC_LO16\(108, 0X([0-9A-F]+)\)\);',body):
        if int(off,16)&0x8000 and 'sltiu' in body and 'LOOKUP_FUNC(ctx->r'+reg+')' in body:candidates.append(name)
if len(functions)!=reviewed['audit_function_count'] or candidates:
    raise SystemExit('STOP: scoped first-person dispatch audit differs: '+repr(candidates))
Path(sys.argv[2]).write_text(json.dumps(reviewed,indent=2)+'\n')
print('PASS: four complete reviewed consumers, all 23 ordered USA slots and 306-function stale-dispatch audit; retained 47-slot switch checked separately.')
VERIFY_BSFIRSTP_SOURCE
# Queue-isolation A-E exit and early descriptor-allocation probe stay disabled.
# The source uses a numeric #if guard; =0 compiles that path out.
if ! grep -Fq '#if defined(__ANDROID__) && defined(TOOIE_VULKAN_QUEUE_ISOLATION_TEST) && TOOIE_VULKAN_QUEUE_ISOLATION_TEST' \
     "$SRC/lib/rt64/src/hle/rt64_application.cpp"; then
  echo 'STOP: Cannot verify diagnostic-exit compile-time guard in this source revision.' >&2
  exit 1
fi



printf '\n=== 2/7 RUN TASK DELIVERY, PRODUCER TIMING, FIRST-PERSON, MOD AND GAMEPLAY REGRESSIONS ===\n'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" python3 tools/test_launcher_artwork.py) > "$LOGS/launcher-artwork.log" 2>&1; then
  echo 'STOP: approved launcher layers or layout failed verification.' >&2
  tail -80 "$LOGS/launcher-artwork.log"; exit 1
fi
grep -Fq 'PASS: exact approved launcher PNG layers, Android mirrors, registration route and preserved menu actions; no device-rendering claim' "$LOGS/launcher-artwork.log" || {
  echo 'STOP: launcher layer verification result missing.' >&2; exit 1;
}
grep '^PASS:' "$LOGS/launcher-artwork.log"


echo 'Checking MIPS HI16 carry and the complete USA first-person consumer; host compilation may take a few minutes.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" CC="$CLOCK_CC" TOOIE_TEST_JOBS=2 \
  TOOIE_RECOMP_BUILD_DIR="$RUN_ROOT/first-person-generator" \
  bash tools/test_jump_table_analysis.sh) > "$LOGS/jump-table-analysis.log" 2>&1; then
  echo 'STOP: jump-table analyzer/generator regressions failed.' >&2
  tail -100 "$LOGS/jump-table-analysis.log"; exit 1
fi
for marker in \
 'PASS: 20 real analyzer/emitter controls preserve signed HI16/LO16 carry, data-after-code tables, all 47 constructed repeated slots and strict switch errors' \
 'PASS: exact parent loses signed-low relocated tables and emits external lookup; low-positive and nonrelocated controls remain intact'; do
  grep -Fq "$marker" "$LOGS/jump-table-analysis.log" || { echo "STOP: missing generator result: $marker" >&2; exit 1; }
done
if ! (cd "$SRC" && PATH="$RUN_ROOT/first-person-host-bin:$PATH" \
  CXX="$AUDIO_CXX" CC="$CLOCK_CC" TOOIE_TEST_JOBS=2 TOOIE_FIRST_PERSON_APPLY=0 \
  TOOIE_RECOMP_BUILD_DIR="$RUN_ROOT/first-person-generator" \
  bash tools/test_first_person_dispatch.sh) > "$LOGS/first-person-dispatch.log" 2>&1; then
  echo 'STOP: complete first-person consumer/reference regressions failed.' >&2
  tail -100 "$LOGS/first-person-dispatch.log"; exit 1
fi
for marker in \
 'PASS: complete USA switch matches real emitter and committed consumer; only switch/addend/load and required case labels changed' \
 'PASS: original committed consumer reaches real external lookup at an interior target; no fake function entry or skipped case' \
 'PASS: swapped captured case and damaged stack epilogue fail independent loaded-target reference' \
 'PASS: desktop complete consumer uses the same verified USA cases and preserves all non-table behavior'; do
  grep -Fq "$marker" "$LOGS/first-person-dispatch.log" || { echo "STOP: missing first-person result: $marker" >&2; exit 1; }
done
[[ "$(grep -Fc 'PASS: 513 complete consumer/reference scenarios: all 47 USA slots/14 targets' "$LOGS/first-person-dispatch.log")" -eq 2 ]] || {
  echo 'STOP: both 513-scenario Android and desktop first-person runs are required.' >&2; exit 1;
}
if ! (cd "$SRC" && python3 tests/test_first_person_table.py) > "$LOGS/first-person-table.log" 2>&1; then
  echo 'STOP: ROM table extraction/bounds/corruption controls failed.' >&2
  tail -100 "$LOGS/first-person-table.log"; exit 1
fi
grep -Eq '^Ran 1 test in ' "$LOGS/first-person-table.log" && grep -qx 'OK' "$LOGS/first-person-table.log" || {
  echo 'STOP: first-person table extraction test did not complete.' >&2; exit 1;
}

echo 'Checking four complete regenerated first-person consumers against the ROM reference, exact parent and mutations.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" CC="$CLOCK_CC" TOOIE_TEST_JOBS=2 \
  TOOIE_BSFIRSTP_APPLY=0 TOOIE_RECOMP_BUILD_DIR="$RUN_ROOT/first-person-generator" \
  bash tools/test_bsfirstp_dispatches.sh) > "$LOGS/bsfirstp-dispatches.log" 2>&1; then
  echo 'STOP: four-consumer first-person reference/regeneration regressions failed.' >&2
  tail -100 "$LOGS/bsfirstp-dispatches.log"; exit 1
fi
for marker in \
 'PASS: all four real generator outputs match only permitted switch/index/load/label deltas and production consumers' \
 'PASS: scoped 306-function audit detects all four exact-parent candidates and none after correction' \
 'PASS: all 23 ROM targets and bounded inputs verified; previous complete 47-slot consumer is unchanged' \
 'PASS: all four exact-parent misses and eight wrong-case/damaged-frame mutations fail at the real consumers'; do
  grep -Fq "$marker" "$LOGS/bsfirstp-dispatches.log" || { echo "STOP: missing four-consumer result: $marker" >&2; exit 1; }
done
[[ "$(grep -Fc 'PASS: 468 four-consumer/reference scenarios, 23 actual USA slots' "$LOGS/bsfirstp-dispatches.log")" -eq 2 ]] || {
  echo 'STOP: both 468-scenario Android and desktop four-consumer runs are required.' >&2; exit 1;
}

for suite in jump-table-analysis first-person-dispatch first-person-table bsfirstp-dispatches; do
  if grep -Eq '^SKIP:|^FAIL:|Traceback|runtime error:|AddressSanitizer|UndefinedBehaviorSanitizer' "$LOGS/$suite.log"; then
    echo "STOP: $suite includes a skip/failure or sanitizer error." >&2; exit 1
  fi
done
git -C "$SRC" diff --exit-code -- RecompiledFuncs/funcs_44.c RecompiledFuncs/funcs_45.c lib/N64Recomp/src/analysis.cpp patches/N64Recomp.patch tests/fixtures/bsfirstp_table_us.json tests/fixtures/bsfirstp_dispatches_us.json > "$LOGS/first-person-source-clean.log" || {
  echo 'STOP: first-person checks changed tracked production source.' >&2; exit 1;
}
grep '^PASS:' "$LOGS/jump-table-analysis.log"
grep '^PASS:' "$LOGS/first-person-dispatch.log"
grep '^PASS:' "$LOGS/bsfirstp-dispatches.log"
echo 'PASS: first-person table extraction/bounds/corruption controls; no ROM is required or bundled by this builder.'


echo 'Checking independent clock-only tracing, complete guest frame sync, actual replay/timer consumers and unchanged RAM/register digests.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" CC="$CLOCK_CC" \
  bash tools/test_guest_timing_trace.sh) > "$LOGS/guest-timing-trace.log" 2>&1; then
  echo 'STOP: guest clock/frame-sync profile and consumer controls failed.' >&2
  tail -100 "$LOGS/guest-timing-trace.log"; exit 1
fi
for marker in \
 'PASS: complete generated instruction bodies equal exact parent across default/zero/timing and desktop profiles (observer calls excluded only when enabled)' \
 'PASS: identical full RAM/register digest for Android default/zero/timing/legacy and desktop; real clock-only writer, late frame-sync report; exact parent lacks clock-only hooks; altered frame count/replay override rejected'; do
  grep -Fq "$marker" "$LOGS/guest-timing-trace.log" || { echo "STOP: missing guest timing result: $marker" >&2; exit 1; }
done
[[ "$(grep -Ec '^STATE cases=113 ordinary_hash=[0-9a-f]{16} hash=[0-9a-f]{16}$' "$LOGS/guest-timing-trace.log")" -eq 5 ]] || {
  echo 'STOP: all five 113-case generated-consumer profiles with ordinary-state equivalence checked by the source suite are required.' >&2; exit 1;
}
[[ "$(grep -Fc 'PASS: actual generated VI-worker/frame-sync/ordinary/replay/timer consumers;' "$LOGS/guest-timing-trace.log")" -eq 5 ]] || {
  echo 'STOP: all five generated timing profile results are required.' >&2; exit 1;
}
echo 'Checking complete presentation-input decisions and unchanged exact-parent/trace-OFF/desktop publications.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" \
  bash tools/test_presentation_input_trace.sh) > "$LOGS/presentation-input-trace.log" 2>&1; then
  echo 'STOP: presentation input attribution/regressions failed.' >&2
  tail -100 "$LOGS/presentation-input-trace.log"; exit 1
fi
grep -Fq 'PASS: 40 complete updateScreen cases: VI/CPU/FB changes, unchanged/hidden image, early-ignore, early publication, paused repeat; exact-parent/Android/desktop outputs match, actual diagnostic reasons retained; changed publication predicate rejected' "$LOGS/presentation-input-trace.log" || {
  echo 'STOP: complete presentation-input parent/profile/publication controls missing.' >&2; exit 1;
}
for suite in guest-timing-trace presentation-input-trace; do
  if grep -Eq '^SKIP:|^FAIL:|Traceback|runtime error:|AddressSanitizer|UndefinedBehaviorSanitizer' "$LOGS/$suite.log"; then
    echo "STOP: $suite includes a skip/failure or sanitizer error." >&2; exit 1
  fi
  grep '^PASS:' "$LOGS/$suite.log"
done

echo 'Checking full-sync producer boundaries, publication eligibility, late records and the exact-parent/disabled/desktop controls.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" \
  bash tools/test_full_sync_trace.sh) > "$LOGS/full-sync-trace.log" 2>&1; then
  echo 'STOP: full-sync producer observer/publication controls failed.' >&2
  tail -100 "$LOGS/full-sync-trace.log"; exit 1
fi
for marker in \
 'PASS: complete fullSync default/zero/desktop instruction bodies equal exact parent' \
 'PASS: 50 publication-tail cases match exact parent; all eligibility reasons, format/width/address mismatch, coverage-only/duplicate rejection, late report and upstream gap attribution; startup-masked exact-parent window control, missing boundary and unsafe-candidate controls detected'; do
  grep -Fq "$marker" "$LOGS/full-sync-trace.log" || { echo "STOP: missing full-sync result: $marker" >&2; exit 1; }
done
python3 - "$SRC" <<'VERIFY_CAPTURED_TIMING_FIXTURE'
from pathlib import Path
import hashlib,sys
fixture=Path(sys.argv[1])/'tests/fixtures/odin-step115-clocks.txt'
data=fixture.read_bytes()
if len(data)!=236987 or hashlib.sha256(data).hexdigest()!='1019de60047286e9a5c6723e25d945ff0df624b73c9f9e31bfd2f060f28698e5':
    raise SystemExit('STOP: committed Step 115 clock fixture differs from the reviewed exact bytes.')
print('PASS: exact committed Step 115 clock fixture; 633 retained sparse details, without requiring the external evidence ZIP.')
VERIFY_CAPTURED_TIMING_FIXTURE
echo 'Checking all 633 Step 115 captured clock records through the real generated consumers in five profiles.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" CC="$CLOCK_CC" \
  TOOIE_CAPTURED_CLOCK_FIXTURE="$SRC/tests/fixtures/odin-step115-clocks.txt" \
  bash tools/test_guest_timing_trace.sh) > "$LOGS/step115-captured-timing.log" 2>&1; then
  echo 'STOP: captured Step 115 guest timing/profile regressions failed.' >&2
  tail -100 "$LOGS/step115-captured-timing.log"; exit 1
fi
for marker in \
 'PASS: PID 15060; 73 windows/85 buckets/633 sparse details;' \
 'PASS: complete generated instruction bodies equal exact parent across default/zero/timing and desktop profiles (observer calls excluded only when enabled)' \
 'PASS: identical full RAM/register digest for Android default/zero/timing/legacy and desktop; real clock-only writer, late frame-sync report; exact parent lacks clock-only hooks; altered frame count/replay override rejected'; do
  grep -Fq "$marker" "$LOGS/step115-captured-timing.log" || { echo "STOP: missing captured timing result: $marker" >&2; exit 1; }
done
[[ "$(grep -Ec '^STATE cases=113 ordinary_hash=[0-9a-f]{16} hash=[0-9a-f]{16}$' "$LOGS/step115-captured-timing.log")" -eq 5 ]] || {
  echo 'STOP: all five 113-case captured timing profiles with ordinary-state equivalence checked by the source suite are required.' >&2; exit 1;
}
[[ "$(grep -Fc 'PASS: 249 ordinary, 336 successful replay and 48 terminal sampled records through generated input/game-timer/whole attract handler;' "$LOGS/step115-captured-timing.log")" -eq 5 ]] || {
  echo 'STOP: all 633 captured Step 115 records must pass in all five generated-consumer profiles.' >&2; exit 1;
}
[[ "$(grep -Fc 'PASS: actual generated VI-worker/frame-sync/ordinary/replay/timer consumers;' "$LOGS/step115-captured-timing.log")" -eq 5 ]] || {
  echo 'STOP: all five complete generated timing consumer results are required for Step 115.' >&2; exit 1;
}
for suite in full-sync-trace step115-captured-timing; do
  if grep -Eq '^SKIP:|^FAIL:|Traceback|runtime error:|AddressSanitizer|UndefinedBehaviorSanitizer' "$LOGS/$suite.log"; then
    echo "STOP: $suite includes a skip/failure or sanitizer error." >&2; exit 1
  fi
  grep '^PASS:' "$LOGS/$suite.log"
done


echo 'Checking graphics task submission, immutable queue stamps, dispatch, renderer stages and exact-parent/OFF controls.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" \
  bash tools/test_gfx_task_trace.sh) > "$LOGS/gfx-task-trace.log" 2>&1; then
  echo 'STOP: graphics task-delivery/ownership attribution regressions failed.' >&2
  tail -100 "$LOGS/gfx-task-trace.log"; exit 1
fi
for marker in \
 'PASS: full edited runtime/renderer bodies and private action layout equal exact parent in default/zero/desktop profiles' \
 'PASS: 40 actual-source queue/graphics/renderer cases match exact parent; enqueue age, upstream gaps, callbacks, texture/interpret stages, task-copy and skeletal-snapshot lifetime, cross-TU/nested correlation, all action types, failed setup and late reports; exact-parent absence and copy/boundary/order/ownership controls detected'; do
  grep -Fq "$marker" "$LOGS/gfx-task-trace.log" || { echo "STOP: missing graphics task result: $marker" >&2; exit 1; }
done
python3 - "$SRC" <<'VERIFY_STEP118_CLOCK_FIXTURE'
from pathlib import Path
import hashlib,sys
data=(Path(sys.argv[1])/'tests/fixtures/odin-step118-clocks.txt').read_bytes()
if len(data)!=212256 or hashlib.sha256(data).hexdigest()!='523ffe51c04051d383aa0891f8e0d2a2341378862e51b736128740cbc52bde49':
    raise SystemExit('STOP: exact committed Step 118 clock fixture differs.')
print('PASS: exact Step 118 clock fixture, 569 sparse records with captured divisors; no external evidence ZIP required.')
VERIFY_STEP118_CLOCK_FIXTURE
echo 'Checking all 569 Step 118 captured clock details and 113 generated frame-sync controls in five profiles.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" CC="$CLOCK_CC" \
  TOOIE_CAPTURED_CLOCK_FIXTURE="$SRC/tests/fixtures/odin-step118-clocks.txt" \
  bash tools/test_guest_timing_trace.sh) > "$LOGS/step118-captured-timing.log" 2>&1; then
  echo 'STOP: Step 118 captured guest timing/profile regressions failed.' >&2
  tail -100 "$LOGS/step118-captured-timing.log"; exit 1
fi
for marker in \
 'PASS: PID 26855; 65 windows/75 buckets/569 sparse details;' \
 'PASS: complete generated instruction bodies equal exact parent across default/zero/timing and desktop profiles (observer calls excluded only when enabled)' \
 'PASS: identical full RAM/register digest for Android default/zero/timing/legacy and desktop; real clock-only writer, late frame-sync report; exact parent lacks clock-only hooks; altered frame count/replay override rejected'; do
  grep -Fq "$marker" "$LOGS/step118-captured-timing.log" || { echo "STOP: missing Step 118 clock result: $marker" >&2; exit 1; }
done
[[ "$(grep -Ec '^STATE cases=113 ordinary_hash=[0-9a-f]{16} hash=[0-9a-f]{16}$' "$LOGS/step118-captured-timing.log")" -eq 5 ]] || {
  echo 'STOP: all five 113-case Step 118 profiles with ordinary-state equivalence checked by the source suite are required.' >&2; exit 1;
}
[[ "$(grep -Fc 'PASS: 243 ordinary, 278 successful replay and 48 terminal sampled records through generated input/game-timer/whole attract handler;' "$LOGS/step118-captured-timing.log")" -eq 5 ]] || {
  echo 'STOP: all 569 Step 118 records, including captured divisors, must pass in five profiles.' >&2; exit 1;
}
[[ "$(grep -Fc 'PASS: actual generated VI-worker/frame-sync/ordinary/replay/timer consumers;' "$LOGS/step118-captured-timing.log")" -eq 5 ]] || {
  echo 'STOP: all five complete generated timing consumer results are required for Step 118.' >&2; exit 1;
}
for suite in gfx-task-trace step118-captured-timing; do
  if grep -Eq '^SKIP:|^FAIL:|Traceback|runtime error:|AddressSanitizer|UndefinedBehaviorSanitizer' "$LOGS/$suite.log"; then
    echo "STOP: $suite includes a skip/failure or sanitizer error." >&2; exit 1
  fi
  grep '^PASS:' "$LOGS/$suite.log"
done

git -C "$SRC" diff --exit-code > "$LOGS/timing-source-clean.log" || {
  echo 'STOP: timing tests changed tracked source.' >&2; exit 1;
}

# The baseline is a separate detached worktree; no prior working folder changes.
git -C "$SRC" worktree add --detach "$BASELINE" '4a0a30912b4e54a31d00f64ed91f5bc8b2adcc37' > "$LOGS/mod-baseline.log" 2>&1
echo 'Checking the production mod importer/JNI/archive/transaction/texture registration paths.'
if ! (cd "$SRC" && JAVA_HOME="$AUDIO_JDK" PATH="$AUDIO_JDK/bin:$PATH" \
  python3 tools/test_android_mod_support.py --root "$SRC" --build-dir "$RUN_ROOT/mod-host" \
  --android-jar "$ANDROID_JAR" --jni-include "$AUDIO_JDK/include" \
  --tested-base-root "$BASELINE" --cxx "$AUDIO_CXX" --cc "$CLOCK_CC") > "$LOGS/mod-support.log" 2>&1; then
  echo 'STOP: Android mod copy/JNI/archive/transaction/registration checks failed.' >&2
  tail -100 "$LOGS/mod-support.log"; exit 1
fi
for marker in \
  'PASS: 51 actual Java mod-copy checks:' \
  'PASS: real JVM/production JNI bridge,' \
  'PASS: 65 transaction checks:' \
  'PASS: strict C99 POSIX ftello/fseeko prototypes and 64-bit file offsets;' \
  'PASS: 104 actual Android installer checks with production miniz/manifest/version/config code:' \
  'PASS: actual startup registration exposes rt64.json and .rtz,' \
  'PASS: actual Activity, import helpers and packaged SDL Java compile against Android SDK' \
  'PASS: exact-base picker fails the selected-file requirement;' \
  'PASS: Android mod support host suite.'; do
  grep -Fq "$marker" "$LOGS/mod-support.log" || { echo "STOP: missing mod result: $marker" >&2; exit 1; }
done
if grep -Eq '^SKIP:|^FAIL|Traceback|RuntimeError|WARNING in native method|runtime error:|AddressSanitizer' "$LOGS/mod-support.log"; then
  echo 'STOP: mod suite includes a skip/failure or sanitizer/JNI warning.' >&2; exit 1
fi
grep '^PASS:' "$LOGS/mod-support.log"

echo 'Checking generated actor lifetime/array ownership and the exact-parent failing control; host compilation may take a few minutes.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" CC="$CLOCK_CC" \
  bash tools/test_actor_ownership.sh) > "$LOGS/actor-ownership.log" 2>&1; then
  echo 'STOP: generated actor ownership regressions failed.' >&2
  tail -100 "$LOGS/actor-ownership.log"; exit 1
fi
for marker in \
  'PASS: six actor hook/config boundaries;' \
  'PASS: all three generated instruction bodies unchanged with actors OFF on Android and ON-but-compiled-out on desktop' \
  'PASS: actual generated actor constructor, normal/alternate draw wrappers and array compaction;' \
  'PASS: exact ffd520a native root bridge fails actor identity assertion despite valid ownership adapters'; do
  grep -Fq "$marker" "$LOGS/actor-ownership.log" || { echo "STOP: missing actor ownership result: $marker" >&2; exit 1; }
done
if grep -Eq '^SKIP:|^FAIL:|Traceback|runtime error:|AddressSanitizer' "$LOGS/actor-ownership.log"; then
  echo 'STOP: actor ownership suite includes a skip/failure or sanitizer error.' >&2; exit 1
fi
grep '^PASS:' "$LOGS/actor-ownership.log"

echo 'Checking owned player/actor poses, immutable task/RSP delivery, rigid actor identity, hierarchy consumer and SIMD.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" CC="$CLOCK_CC" SIMD_CXX="$RENDER_SIMD_CXX" \
  bash tools/test_skeletal_pose.sh) > "$LOGS/skeletal-pose.log" 2>&1; then
  echo 'STOP: native player hierarchy regressions failed.' >&2
  tail -100 "$LOGS/skeletal-pose.log"; exit 1
fi
grep -Fq 'PASS: exact 60c0c89 parent rejects live hierarchy regression' "$LOGS/skeletal-pose.log" || {
  echo 'STOP: exact-parent live hierarchy failure control absent.' >&2; exit 1;
}
[[ "$(grep -Fc 'PASS:' "$LOGS/skeletal-pose.log")" -ge 3 ]] || {
  echo 'STOP: scalar, exact-parent and independent SIMD hierarchy results are required.' >&2; exit 1;
}

[[ "$(grep -Fc 'PASS: 6 player/constructed-actor triangle cases' "$LOGS/skeletal-pose.log")" -eq 2 ]] || {
  echo 'STOP: both scalar and SIMD player/constructed-actor fixture runs are required.' >&2; exit 1;
}
[[ "$(grep -Fc 'PASS: native actor root groups through actual RSP vertices, full ID matcher and transform consumer;' "$LOGS/skeletal-pose.log")" -eq 2 ]] || {
  echo 'STOP: both scalar and SIMD reordered rigid-actor ownership controls are required.' >&2; exit 1;
}

grep '^PASS:' "$LOGS/skeletal-pose.log"
if ! (cd "$SRC" && CXX="$AUDIO_CXX" CC="$CLOCK_CC" SIMD_CXX="$RENDER_SIMD_CXX" \
  bash tools/test_correction_hierarchy.sh) > "$LOGS/hierarchy-proposal.log" 2>&1; then
  echo 'STOP: retained authored-joint/offline proposal controls failed.' >&2
  tail -100 "$LOGS/hierarchy-proposal.log"; exit 1
fi
grep '^PASS:' "$LOGS/hierarchy-proposal.log"
cat > "$LOGS/robin-header-first.cpp" <<'ROBIN_HEADER_CPP'
#include <RmlUi/Core/Containers/robin_hood.h>
int main() { robin_hood::unordered_flat_map<uint64_t, uint32_t> values; values[7] = 9; return values.at(7) == 9 ? 0 : 1; }
ROBIN_HEADER_CPP
for standard in c++17 c++20; do
  if ! "$AUDIO_CXX" "-std=$standard" -Wall -Wextra -pedantic-errors -fsyntax-only \
     -I"$SRC/lib/RecompFrontend/recompui/lib/RmlUi/Include" "$LOGS/robin-header-first.cpp" \
     > "$LOGS/header-$standard.log" 2>&1; then
    echo 'STOP: standalone fixed-width header compilation failed.' >&2
    tail -60 "$LOGS/header-$standard.log"; exit 1
  fi
done
echo 'PASS: fixed robin_hood header first include with strict C++17/C++20; no forced include or permissive flag.'
echo 'Checking actual RmlUi document destruction and preserved primary guest failures.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" TOOIE_RML_TEST_JOBS=2 \
  TOOIE_RML_TEST_BUILD_DIR="$RUN_ROOT/rml-lifetime-host" \
  bash tools/test_rml_document_lifetime.sh) > "$LOGS/rml-lifetime.log" 2>&1; then
  echo 'STOP: RmlUi lifetime/base/diagnostic-guard regressions failed.' >&2
  tail -100 "$LOGS/rml-lifetime.log"; exit 1
fi
grep -Fq 'PASS: exact 01495d0/f017d00 source fails normal document detachment (vptr lifetime) and controlled destruction query (actual recursive stack)' "$LOGS/rml-lifetime.log" || {
  echo 'STOP: exact-parent UI lifetime controls missing.' >&2; exit 1;
}
grep '^PASS:' "$LOGS/rml-lifetime.log"

echo 'Checking failure-only guest lookup caller, syscall/thread context, mapping and bounded memory diagnostics; host compilation may take a few minutes.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" CC="$CLOCK_CC" \
  bash tools/test_guest_lookup.sh) > "$LOGS/guest-lookup.log" 2>&1; then
  echo 'STOP: guest lookup provenance regressions failed.' >&2
  tail -100 "$LOGS/guest-lookup.log"; exit 1
fi
for marker in \
  'PASS: exact generated indirect caller, signed shift/relocation and unload consumers extracted' \
  'PASS: actual generated caller and decoded syscall provenance, active/deleted/shifted/overlapping mappings, bounded raw words and journal rollover; successful one-evaluation dispatch has no diagnostic I/O' \
  'PASS: exact cb896c3 fails all three provenance assertions; full runtime missing lookup remains fatal without ordinary teardown on Android; desktop behavior preserved'; do
  grep -Fq "$marker" "$LOGS/guest-lookup.log" || { echo "STOP: missing lookup result: $marker" >&2; exit 1; }
done
if grep -Eq '^SKIP:|^FAIL:|Traceback|runtime error:|AddressSanitizer|UndefinedBehaviorSanitizer' "$LOGS/guest-lookup.log"; then
  echo 'STOP: lookup suite includes a skip/failure or sanitizer error.' >&2; exit 1
fi
grep '^PASS:' "$LOGS/guest-lookup.log"

if ! (cd "$SRC" && CXX="$AUDIO_CXX" bash tools/test_android_guest_failure.sh) > "$LOGS/guest-failure.log" 2>&1; then
  echo 'STOP: primary guest-failure/quick-exit regressions failed.' >&2
  tail -100 "$LOGS/guest-failure.log"; exit 1
fi
grep -Fq 'PASS: exact parent source executes ordinary-exit callbacks and lacks private primary-failure records; real runtime quick_exit executes in current Android cases' "$LOGS/guest-failure.log" || {
  echo 'STOP: exact-parent guest-failure control missing.' >&2; exit 1;
}
grep '^PASS:' "$LOGS/guest-failure.log"
if ! (cd "$SRC" && CXX="$AUDIO_CXX" bash tools/test_android_surface_lifecycle.sh) > "$LOGS/lifecycle.log" 2>&1; then
  echo 'STOP: surface/picker lifecycle regressions failed.' >&2
  tail -100 "$LOGS/lifecycle.log"; exit 1
fi
grep '^PASS:' "$LOGS/lifecycle.log"

echo 'Checking copied source ownership, explicit generations and guest clocks; host compilation may take a few minutes.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" CC="$CLOCK_CC" \
  bash tools/test_correction_diagnostics.sh) > "$LOGS/correction-observer.log" 2>&1; then
  echo 'STOP: correction ownership/marker/clock/protocol controls failed.' >&2
  tail -100 "$LOGS/correction-observer.log"
  exit 1
fi
for marker in \
  'PASS: config/generated hook parity in exact core/overlay function scopes; 16 observer hooks plus existing frame/root/bone boundaries' \
  'PASS: all six generated C files retain exact parent instruction bodies with trace undefined/0' \
  'PASS: owned bind/local/output copies, exact matrix/command join, reuse/ambiguity/expiry/capacity, read-only RAM/registers, caller clocks/end/reset/overflow, ROM provenance, late bursts' \
  'PASS: real diagnostic writer, bounded records, complete authored words and observed clock values/ROM header (synthetic RAM)' \
  'PASS: source/consumer joins, incomplete snapshots remain incomplete, PID/TID clock windows, UTF-16, torn-data/mixed-process/duplicate/binding controls'; do
  grep -Fq "$marker" "$LOGS/correction-observer.log" || {
    echo "STOP: missing correction control result: $marker" >&2; exit 1;
  }
done
[[ "$(grep -Fc 'PASS: correction observer compiled out' "$LOGS/correction-observer.log")" == 2 ]] || {
  echo 'STOP: Android OFF and desktop selector-ON compile-out controls are required.' >&2; exit 1;
}
grep '^PASS:' "$LOGS/correction-observer.log"
if ! (cd "$SRC" && CXX="$AUDIO_CXX" SIMD_CXX="$RENDER_SIMD_CXX" \
  bash tools/test_geometry_capture.sh) > "$LOGS/geometry-capture.log" 2>&1; then
  echo 'STOP: retained device-input production replay controls failed.' >&2
  tail -100 "$LOGS/geometry-capture.log"
  exit 1
fi
[[ "$(grep -Fc 'PASS: 3 captured triangles, cold/seeded-previous production TransformProcessor,' "$LOGS/geometry-capture.log")" == 2 ]] || {
  echo 'STOP: both scalar and SIMD retained device-input replays are required.' >&2; exit 1;
}
grep -Fq 'PASS: UTF-16 input; torn/duplicate/mixed-process/owner/flags/NaN/absent controls; corrupted matrix and altered AUTO skip fail the actual consumer' "$LOGS/geometry-capture.log" || {
  echo 'STOP: captured-input corruption controls are absent.' >&2; exit 1;
}
grep '^PASS:' "$LOGS/geometry-capture.log"
if ! (cd "$SRC" && CC="$CLOCK_CC" bash tools/test_demo_pacing.sh) > "$LOGS/demo-pacing.log" 2>&1; then
  echo 'STOP: production demo pacing regressions failed.' >&2
  tail -100 "$LOGS/demo-pacing.log"; exit 1
fi
grep -Fq 'PASS: exact-parent missing pacing and altered wait controls fail; desktop, ordinary timing, input and dt semantics retained' "$LOGS/demo-pacing.log" || {
  echo 'STOP: production pacing negative controls were not verified.' >&2; exit 1;
}
grep '^PASS:' "$LOGS/demo-pacing.log"
if ! (cd "$SRC" && CXX="$AUDIO_CXX" python3 tools/test_demo_render_cadence.py) > "$LOGS/demo-render-cadence.log" 2>&1; then
  echo 'STOP: native demo renderer-cadence regressions failed.' >&2
  tail -100 "$LOGS/demo-render-cadence.log"; exit 1
fi
grep -Fq 'PASS: exact 8bcaad0 command-emission control fails the replay renderer-duration requirement' "$LOGS/demo-render-cadence.log" || {
  echo 'STOP: exact-parent renderer-duration control was not verified.' >&2; exit 1;
}
grep '^PASS:' "$LOGS/demo-render-cadence.log"
if ! (cd "$SRC" && CC="$CLOCK_CC" bash tools/test_demo_replay_clock.sh) > "$LOGS/demo-clock.log" 2>&1; then
  echo 'STOP: generated demo-clock controls failed.' >&2
  tail -100 "$LOGS/demo-clock.log"
  exit 1
fi
grep -Fq 'PASS: changed recorded-span byte, double-consumption and doubled game-timer controls fail the actual generated consumers' "$LOGS/demo-clock.log" || {
  echo 'STOP: generated-clock negative controls are absent.' >&2; exit 1;
}
grep '^PASS:' "$LOGS/demo-clock.log"
echo 'Checking the geometry observer, exact-base/off controls and replay; host compilation may take a few minutes.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" SIMD_CXX="$RENDER_SIMD_CXX" TOOIE_CORRECTION_TEST=1 \
  TOOIE_GEOMETRY_TEST_BUILD_DIR="$RUN_ROOT/geometry-observer-host" \
  bash tools/test_geometry_diagnostics.sh) > "$LOGS/geometry-observer.log" 2>&1; then
  echo 'STOP: consumed-geometry observer/replay/base/off/SIMD checks failed.' >&2
  tail -100 "$LOGS/geometry-observer.log"
  exit 1
fi
[[ "$(grep -Fc 'PASS: real matcher/TransformProcessor, synthetic cross-bone AUTO ambiguity versus explicit identity control,' "$LOGS/geometry-observer.log")" == 7 ]] || {
  echo 'STOP: all six scalar geometry modes and the independent SIMD fixture are required.' >&2; exit 1;
}
for marker in \
  'PASS: actual source-linked consumer delivery; exact 3286c6f lacks required observer; consumed geometry/velocities byte-identical to parent and trace OFF' \
  'PASS: bounded complete records, PID/TID/workload/sample provenance; exact 98f909c lacks hook; consumed matrices/velocities byte-identical with trace on/off and base' \
  'PASS: replay reconstructs consumed collapse/identity pairs, UTF-16 Windows logs, missing-trace and torn-record controls' \
  'PASS: independent SIMD consumed-geometry observer fixture (scalar build carries ASan/UBSan)'; do
  grep -Fq "$marker" "$LOGS/geometry-observer.log" || {
    echo "STOP: missing observer/replay control result: $marker" >&2; exit 1;
  }
done
grep '^PASS:' "$LOGS/geometry-observer.log"
if ! (cd "$SRC" && CXX="$AUDIO_CXX" SIMD_CXX="$RENDER_SIMD_CXX" \
  bash tools/test_render_interpolation.sh --baseline) > "$LOGS/render-interpolation.log" 2>&1; then
  echo 'STOP: geometry interpolation/base/SIMD regressions failed.' >&2
  tail -100 "$LOGS/render-interpolation.log"
  exit 1
fi
grep -Fq 'PASS: exact 59d3ad0 base fails all four cold/reset/mode/independent-warm-rotation geometry controls' \
  "$LOGS/render-interpolation.log" || { echo 'STOP: four exact-base geometry controls missing.' >&2; exit 1; }
[[ "$(grep -Fc 'PASS: actual matcher/rigid-body consumed geometry' "$LOGS/render-interpolation.log")" == 2 ]] || {
  echo 'STOP: both scalar sanitizer and host SIMD geometry results are required.' >&2; exit 1;
}
grep '^PASS:' "$LOGS/render-interpolation.log"
if ! (cd "$SRC" && CXX="$AUDIO_CXX" bash tools/test_frame_protocol.sh --baseline) > "$LOGS/frame-protocol.log" 2>&1; then
  echo 'STOP: actual-source frame ownership/source-rate regressions failed.' >&2
  tail -100 "$LOGS/frame-protocol.log"
  exit 1
fi
grep -Fq 'PASS: 16 actual-source producer/consumer cases:' "$LOGS/frame-protocol.log" || {
  echo 'STOP: expected 16 actual-source ownership cases missing.' >&2; exit 1;
}
for marker in \
  'PASS: 32 actual-source Android outer-queue cases, sustained arrivals/costs, bounded image progress and explicit surface skips' \
  'PASS: independent outer decision reports on 96 consecutive surface skips' \
  'PASS: actual Android diagnostic writer reports skipped outer decisions, PID/TID, stage units and current totals separately from peak context' \
  'unchanged desktop outer-queue cases and retirement notifications' \
  'PASS: exact 36c61d8 rendering source (identical in b12ac59) fails sustained-load progress control'; do
  grep -Fq "$marker" "$LOGS/frame-protocol.log" || {
    echo "STOP: missing progress/diagnostic control: $marker" >&2; exit 1;
  }
done
grep '^PASS:' "$LOGS/frame-protocol.log"
echo 'Checking 303 matching frames and two exact-base operation controls; host compilation may take a few minutes.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" \
  TOOIE_MATCH_TEST_BUILD_DIR="$RUN_ROOT/matching-equivalence-host" \
  bash tools/test_matching_equivalence.sh) > "$LOGS/matching-equivalence.log" 2>&1; then
  echo 'STOP: exact-base matching equivalence or operation controls failed.' >&2
  tail -100 "$LOGS/matching-equivalence.log"
  exit 1
fi
[[ "$(grep -Fc 'PASS: 303 full-matcher frames,' "$LOGS/matching-equivalence.log")" == 2 ]] || {
  echo 'STOP: both optimized-source and exact-base 303-frame results are required.' >&2; exit 1;
}
for marker in \
  'PASS: byte-identical ordered candidate sets and consumed geometry/tile/look-at/velocity/history output against exact fc0a221' \
  'PASS: exact fc0a221 fails both repeated-call and distinct-range/TMEM 704x704 operation controls; optimized source passes without an arbitrary matching cap'; do
  grep -Fq "$marker" "$LOGS/matching-equivalence.log" || {
    echo "STOP: missing matching equivalence/control result: $marker" >&2; exit 1;
  }
done
grep '^PASS:' "$LOGS/matching-equivalence.log"
echo 'Host timing observations are retained in matching-equivalence.log; Odin performance remains unverified.'
if ! (cd "$SRC" && CXX="$AUDIO_CXX" bash tools/test_matching_cost.sh) > "$LOGS/matching-cost.log" 2>&1; then
  echo 'STOP: actual matcher phase/consumption fixture failed.' >&2
  tail -100 "$LOGS/matching-cost.log"
  exit 1
fi
grep -Fq 'PASS: full production matcher phase coverage, repeated compatible calls, consumed transform/tile output; host timings are not Odin timings' \
  "$LOGS/matching-cost.log" || { echo 'STOP: matching fixture result missing.' >&2; exit 1; }
grep '^PASS:' "$LOGS/matching-cost.log"
if ! (cd "$SRC" && CXX="$AUDIO_CXX" TOOIE_JDK="$AUDIO_JDK" \
  bash tools/test_audio_runtime.sh --baseline) > "$LOGS/audio-runtime.log" 2>&1; then
  echo 'STOP: audio/timing/window regressions failed.' >&2
  tail -100 "$LOGS/audio-runtime.log"
  exit 1
fi
for marker in \
  'PASS: stereo order/gain, prefix, queue duration, complete-frame decimation' \
  'dummy backend open/convert/queue/close; no hardware audio claim' \
  'PASS: three tested-base audio controls reproduce prefix, output-rate accounting and fragmented stereo-frame defects' \
  'PASS: independent 60/120-Hz diagnostic windows' \
  'PASS: unchanged extents suppressed; resize/inset/return transitions observed; bounded log budget'; do
  grep -Fq "$marker" "$LOGS/audio-runtime.log" || { echo "STOP: missing audio/timing/window result: $marker" >&2; exit 1; }
done
grep '^PASS:' "$LOGS/audio-runtime.log"
if ! (cd "$SRC" && bash tools/test_android_vulkan_clear.sh) > "$LOGS/regression.log" 2>&1; then
  echo 'STOP: regression tests failed.' >&2
  tail -100 "$LOGS/regression.log"
  exit 1
fi
for mode in proposal legacy-transfer load-op-control barrier-control fallback-comparison legacy-skip-comparison production; do
  if ! grep -q "^PASS: $mode recording" "$LOGS/regression.log"; then
    echo "STOP: expected successful regression mode missing: $mode" >&2
    tail -80 "$LOGS/regression.log"
    exit 1
  fi
done
grep -q 'five-second timeout' "$LOGS/regression.log" || {
  echo 'STOP: expected timeout-control result absent' >&2; exit 1;
}
grep '^PASS:' "$LOGS/regression.log" | tail -12
if ! (cd "$SRC" && bash tools/test_android_vulkan_descriptors.sh) > "$LOGS/descriptor-recorder.log" 2>&1; then
  echo 'STOP: descriptor recorder regressions failed.' >&2
  tail -100 "$LOGS/descriptor-recorder.log"
  exit 1
fi
grep -Fq 'PASS: 55 invalid descriptor cases stop before unsafe Vulkan calls, with exact reasons' "$LOGS/descriptor-recorder.log" || {
  echo 'STOP: expected descriptor recorder result missing' >&2; exit 1;
}
tail -8 "$LOGS/descriptor-recorder.log"

# Retain the extra production clipping and rejection controls with the exact flags.
PLUME_HOST="$SRC/lib/rt64/src/contrib/plume"
NORMAL_RECORDER="$RUN_ROOT/normal-clear-recorder"
NORMAL_RECORDS="$LOGS/normal-clear-records"
mkdir "$NORMAL_RECORDS"
read -r -a NORMAL_CXX_ARGS <<< "$TIMING_CXX_FLAGS"
if ! "${CXX:-clang++}" -std=c++17 -D__ANDROID__ -DNDEBUG "${NORMAL_CXX_ARGS[@]}" \
  -I"$SRC/tests/android_headers" -I"$PLUME_HOST" \
  -I"$PLUME_HOST/contrib/volk" -I"$PLUME_HOST/contrib/Vulkan-Headers/include" \
  -I"$PLUME_HOST/contrib/VulkanMemoryAllocator/include" \
  "$PLUME_HOST/plume_vulkan.cpp" "$SRC/tests/android_vulkan_clear.cpp" \
  -ldl -o "$NORMAL_RECORDER" > "$LOGS/normal-clear-compile.log" 2>&1; then
  echo 'STOP: exact production recorder compilation failed.' >&2
  tail -100 "$LOGS/normal-clear-compile.log"
  exit 1
fi
if ! APP_FOLDER_PATH="$NORMAL_RECORDS" "$NORMAL_RECORDER" > "$LOGS/normal-clear-recorder.log" 2>&1; then
  echo 'STOP: exact production clear recording failed.' >&2
  tail -100 "$LOGS/normal-clear-recorder.log"
  exit 1
fi
for marker in \
  'PASS: clipped/empty/mixed rectangles' \
  'PASS: clipping domain fbExtent=320x241 imageExtent=320x241 viewMip=0 samples=2' \
  'PASS: clipping domain fbExtent=320x241 imageExtent=640x482 viewMip=1 samples=1'; do
  grep -Fq "$marker" "$LOGS/normal-clear-recorder.log" || { echo "STOP: missing exact production result: $marker" >&2; exit 1; }
done
grep -Fq 'path=scissored-draw' "$NORMAL_RECORDS/banjotooi-debug.log" || { echo 'STOP: normal rectangle draw path absent.' >&2; exit 1; }
grep -Fq 'path=transfer ' "$NORMAL_RECORDS/banjotooi-debug.log" || { echo 'STOP: normal full transfer path absent.' >&2; exit 1; }
if grep -Eq 'path=load-op-control|attachment-fallback|Comparison only:' "$NORMAL_RECORDS/banjotooi-debug.log"; then
  echo 'STOP: exact production recorder selected a diagnostic control.' >&2
  exit 1
fi
for reason in unsupported-draw-format null-attachment-view outside-unsafe-layout inverted-x; do
  expected="$reason"
  [[ "$reason" == 'outside-unsafe-layout' ]] && expected='layout-not-color-write'
  [[ "$reason" == 'inverted-x' ]] && expected='malformed-rectangle'
  if APP_FOLDER_PATH="$NORMAL_RECORDS" "$NORMAL_RECORDER" "--draw-reject=$reason" > "$LOGS/normal-reject-$reason.log" 2>&1; then
    echo "STOP: exact production accepted $reason." >&2
    exit 1
  else
    status=$?
    [[ "$status" == 1 ]] || { cat "$LOGS/normal-reject-$reason.log"; exit 1; }
  fi
  grep -Fq "reason=$expected" "$LOGS/normal-reject-$reason.log" || { cat "$LOGS/normal-reject-$reason.log"; exit 1; }
done
echo 'PASS: exact production clear mode, partial MSAA/view clipping, full transfers and four rejection controls.'

echo 'Software Vulkan/GPU tile lifecycle/pixel readbacks are not run by this builder; Codex results are separate.'

printf '\n=== 3/7 RESTORE PROVEN BUILD DEPENDENCIES ===\n'
ZSTD_DEST="$SRC/lib/rt64/src/contrib/zstd/build/cmake"
mkdir -p "$ZSTD_DEST"
rsync -a --ignore-existing "$CACHE/zstd/" "$ZSTD_DEST/"
DXC_DEST="$SRC/lib/rt64/src/contrib/dxc/lib/x64"
mkdir -p "$DXC_DEST"
for file_name in libdxcompiler.so libdxil.so; do
  if [[ ! -e "$DXC_DEST/$file_name" ]]; then
    cp "$CACHE/dxc/$file_name" "$DXC_DEST/$file_name"
  fi
done
HOST_DEST="$SRC/build-host-tools/file_to_c"
mkdir -p "$(dirname "$HOST_DEST")"
if [[ ! -e "$HOST_DEST" ]]; then
  cp "$CACHE/host/file_to_c" "$HOST_DEST"
fi
[[ -x "$HOST_DEST" ]] || { echo 'STOP: host generator is not executable' >&2; exit 1; }

printf '\n=== 4/7 CONFIGURE AND BUILD ANDROID ARM64 ===\n'
if ! cmake -S "$SRC" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$SDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DTOOIE_ANDROID_RUNTIME=ON \
  -DTOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE:BOOL=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=ON \
  -DCMAKE_C_FLAGS="$TIMING_CXX_FLAGS" \
  -DCMAKE_CXX_FLAGS="$TIMING_CXX_FLAGS" \
  -DSDL2_DIR="$SDL/lib/cmake/SDL2" \
  -DFreetype_DIR="$FT/lib/cmake/freetype" \
  -DFREETYPE_LIBRARY="$FT/lib/libfreetype.a" \
  -DFREETYPE_LIBRARY_RELEASE="$FT/lib/libfreetype.a" \
  -DFREETYPE_INCLUDE_DIR_ft2build="$FT/include/freetype2" \
  -DFREETYPE_INCLUDE_DIR_freetype2="$FT/include/freetype2" \
  > "$LOGS/configure.log" 2>&1; then
  echo 'STOP: CMake configuration failed.' >&2
  tail -100 "$LOGS/configure.log"
  exit 1
fi
# Verify the actual application, audio, VI, presentation, config and Plume commands.
python3 - "$BUILD" "$SRC" <<'VERIFY_GAMEPLAY_CONFIGURATION'
from pathlib import Path
import hashlib, json, re, shlex, subprocess, sys

build, source = map(Path, sys.argv[1:])
profile = '-DTOOIE_ANDROID_TIMING_TRACE=1 -DTOOIE_ANDROID_CORRECTION_TRACE=0 -DTOOIE_ANDROID_GEOMETRY_TRACE=0 -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=0'
required = {
    'TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE:BOOL=OFF',
    'CMAKE_EXPORT_COMPILE_COMMANDS:BOOL=ON',
    'CMAKE_BUILD_TYPE:STRING=Release',
    'CMAKE_C_FLAGS:STRING=' + profile,
    'CMAKE_CXX_FLAGS:STRING=' + profile,
}
cache = set(build.joinpath('CMakeCache.txt').read_text().splitlines())
if not required.issubset(cache):
    raise SystemExit('STOP: CMake cache does not match the reviewed independent-timing global C/C++ configuration.')
entries = json.loads(build.joinpath('compile_commands.json').read_text())
targets = (
    'lib/rt64/src/contrib/plume/plume_vulkan.cpp',
    'lib/rt64/src/hle/rt64_application.cpp', 'src/main.cpp',
    'lib/N64ModernRuntime/ultramodern/src/events.cpp',
    'lib/rt64/src/hle/rt64_present_queue.cpp', 'lib/rt64/src/hle/rt64_state.cpp',
    'lib/RecompFrontend/recompui/src/renderer/rt64_render_context.cpp',
    'lib/rt64/src/common/rt64_math.cpp', 'lib/rt64/src/hle/rt64_game_frame.cpp',
    'lib/rt64/src/hle/rt64_rigid_body.cpp', 'lib/rt64/src/hle/rt64_workload_queue.cpp',
    'lib/rt64/src/render/rt64_raster_shader_cache.cpp',
    'lib/rt64/src/render/rt64_transform_processor.cpp',
    'lib/rt64/src/hle/rt64_rsp.cpp', 'lib/rt64/src/hle/rt64_interpreter.cpp',
    'lib/rt64/src/gbi/rt64_gbi_extended.cpp', 'lib/rt64/src/hle/rt64_workload.cpp',
    'src/recomp_api.cpp',
    'RecompiledFuncs/funcs_0.c', 'RecompiledFuncs/funcs_1.c', 'RecompiledFuncs/funcs_2.c',
    'RecompiledFuncs/funcs_24.c', 'RecompiledFuncs/funcs_25.c',
    'RecompiledFuncs/funcs_30.c', 'RecompiledFuncs/funcs_77.c',
    'RecompiledFuncs/funcs_32.c', 'RecompiledFuncs/funcs_35.c', 'RecompiledFuncs/funcs_36.c', 'RecompiledFuncs/funcs_44.c', 'RecompiledFuncs/funcs_45.c',
    'lib/RecompFrontend/recompui/src/util/file.cpp',
    'lib/RecompFrontend/recompui/src/util/android_file_dialog.cpp',
    'lib/RecompFrontend/recompui/src/composites/android_mod_installer.cpp',
    'lib/RecompFrontend/recompui/src/composites/ui_mod_installer.cpp',
    'lib/RecompFrontend/recompui/src/composites/ui_mod_menu.cpp',
    'lib/RecompFrontend/recompui/src/base/ui_state.cpp',
    'lib/RecompFrontend/recompui/lib/RmlUi/Source/Core/Element.cpp',
    'lib/RecompFrontend/recompui/lib/RmlUi/Source/Core/ElementDocument.cpp',
    'lib/N64ModernRuntime/librecomp/src/overlays.cpp',
    'lib/N64ModernRuntime/librecomp/src/recomp.cpp',
)
resolved = []
for entry in entries:
    file = Path(entry['file'])
    if not file.is_absolute():
        file = Path(entry['directory']) / file
    resolved.append((file.resolve(), entry))
for relative in targets:
    if sum(file == source.joinpath(relative).resolve() for file, entry in resolved) != 1:
        raise SystemExit('STOP: expected exactly one compile command for ' + relative)

expected = {'TOOIE_ANDROID_TIMING_TRACE':'1','TOOIE_ANDROID_CORRECTION_TRACE':'0','TOOIE_ANDROID_GEOMETRY_TRACE':'0','TOOIE_VULKAN_QUEUE_ISOLATION_TEST':'0'}
disabled = ('TOOIE_ANDROID_GEOMETRY_TRACE', 'TOOIE_ANDROID_CORRECTION_TRACE',
            'TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE')
forbidden = ('TOOIE_VULKAN_TRANSFER_CLEAR_TEST', 'TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK',
             'TOOIE_VULKAN_CLEAR_BARRIER_TEST')
audited = []
for file, entry in resolved:
    suffix = file.suffix
    language = 'C' if suffix == '.c' else 'C++' if suffix.lower() in ('.cc', '.cpp', '.cxx') or suffix == '.C' else None
    if language is None:
        continue
    arguments = entry.get('arguments') or shlex.split(entry['command'])
    definitions, undefinitions = [], []
    for index, argument in enumerate(arguments):
        if argument.startswith('@'):
            raise SystemExit('STOP: unexpanded compiler response file prevents profile verification: ' + str(file))
        if argument.startswith(('-Wp,', '--define-macro', '--undefine-macro')) and any(name in argument for name in (*expected, *disabled, *forbidden, 'TOOIE_TIMING_TRACE_ENABLED', 'TOOIE_GUEST_CLOCK_TRACE_ENABLED', 'TOOIE_CORRECTION_TRACE_ENABLED', 'TOOIE_ANDROID_SKELETAL_INTERPOLATION', 'TOOIE_ANDROID_ACTOR_INTERPOLATION', 'TOOIE_ACTOR_POSE_ENABLED', 'TOOIE_SKELETAL_POSE_ENABLED')):
            raise SystemExit('STOP: wrapped compiler selector prevents unambiguous profile verification: ' + str(file))
        if argument in ('-D', '-U') and index + 1 < len(arguments):
            (definitions if argument == '-D' else undefinitions).append(arguments[index + 1])
        elif argument.startswith('-D'):
            definitions.append(argument[2:])
        elif argument.startswith('-U'):
            undefinitions.append(argument[2:])
    for name, value in expected.items():
        actual = [d for d in definitions if d == name or d.startswith(name + '=')]
        if actual != [name + '=' + value]:
            raise SystemExit('STOP: missing or conflicting actual compiler definition for ' + name + ' in ' + str(file))
        if name in undefinitions:
            raise SystemExit('STOP: compiler must not undefine ' + name + ' in ' + str(file))
    for name in forbidden:
        if any(d == name or d.startswith(name + '=') for d in definitions):
            raise SystemExit('STOP: diagnostic selector must be undefined in production: ' + name + ' in ' + str(file))
    if not any(re.fullmatch(r'--target=aarch64(?:-none|-unknown)?-linux-android24', arg) for arg in arguments):
        raise SystemExit('STOP: compile command does not identify the required Android ARM64/API24 target: ' + str(file))
    for name in disabled:
        actual = [d for d in definitions if d == name or d.startswith(name + '=')]
        if actual not in ([], [name + '=0']) or (actual and name in undefinitions):
            raise SystemExit('STOP: diagnostic observer/probe enabled or ambiguous: ' + name + ' in ' + str(file))
    for name in ('TOOIE_ANDROID_SKELETAL_INTERPOLATION','TOOIE_ANDROID_ACTOR_INTERPOLATION'):
        policy = [d for d in definitions if d == name or d.startswith(name+'=')]
        if policy not in ([], [name+'=1']) or name in undefinitions:
            raise SystemExit('STOP: compiler command disables/conflicts with hierarchy policy '+name+': '+str(file))
    for name in ('TOOIE_SKELETAL_POSE_ENABLED','TOOIE_ACTOR_POSE_ENABLED','TOOIE_TIMING_TRACE_ENABLED','TOOIE_GUEST_CLOCK_TRACE_ENABLED','TOOIE_CORRECTION_TRACE_ENABLED'):
        if any(d==name or d.startswith(name+'=') for d in definitions) or name in undefinitions:
            raise SystemExit('STOP: derived hierarchy policy must come from its header: '+name)
    audited.append({'file': str(file), 'language': language})
counts = {language: sum(row['language'] == language for row in audited) for language in ('C', 'C++')}
if not all(counts.values()):
    raise SystemExit('STOP: both actual C and C++ command sets must be audited.')

# Prove the effective header-derived policy in the actual producer/runtime/RSP/
# matcher/consumer/generated compile environments, rather than trusting CMake text.
policy_units=(
    'src/recomp_api.cpp', 'RecompiledFuncs/funcs_25.c',
    'RecompiledFuncs/funcs_32.c', 'RecompiledFuncs/funcs_35.c', 'RecompiledFuncs/funcs_36.c',
    'lib/N64ModernRuntime/ultramodern/src/events.cpp',
    'lib/rt64/src/gbi/rt64_gbi_extended.cpp', 'lib/rt64/src/hle/rt64_rsp.cpp',
    'lib/rt64/src/hle/rt64_workload.cpp', 'lib/rt64/src/hle/rt64_game_frame.cpp',
    'lib/rt64/src/render/rt64_transform_processor.cpp',
)
policy_checks=[]
for relative in policy_units:
    entry=next(e for p,e in resolved if p==source.joinpath(relative).resolve())
    original=entry.get('arguments') or shlex.split(entry['command'])
    arguments=[];i=0
    while i<len(original):
        arg=original[i]
        if arg in ('-o','-MF','-MT','-MQ','-MJ'):
            i+=2;continue
        if arg in ('-c','-MD','-MMD','-MP','-MG'):
            i+=1;continue
        if any(arg.startswith(x) and len(arg)>len(x) for x in ('-o','-MF','-MT','-MQ','-MJ')):
            i+=1;continue
        arguments.append(arg);i+=1
    result=subprocess.run([*arguments,'-E','-dM'],cwd=entry['directory'],capture_output=True,text=True,timeout=120)
    if result.returncode:
        raise SystemExit('STOP: actual policy preprocessing failed for '+relative+'\n'+result.stderr[-8000:])
    macros=dict(re.findall(r'^#define[ \t]+(\w+)[ \t]+([^\r\n]*)$',result.stdout,re.M))
    enabled=('TOOIE_ANDROID_SKELETAL_INTERPOLATION','TOOIE_SKELETAL_POSE_ENABLED',
             'TOOIE_ANDROID_ACTOR_INTERPOLATION','TOOIE_ACTOR_POSE_ENABLED')
    if any(macros.get(name)!='1' for name in enabled) or macros.get('TOOIE_ACTOR_POLICY')!='"validated-marker-hierarchy"':
        raise SystemExit('STOP: authored player/actor hierarchy is not effectively enabled in '+relative)
    if relative == 'src/recomp_api.cpp':
        preprocessed=subprocess.run([*arguments,'-E','-P'],cwd=entry['directory'],capture_output=True,text=True,timeout=120)
        if preprocessed.returncode:
            raise SystemExit('STOP: actual renderer-cadence preprocessing failed\n'+preprocessed.stderr[-8000:])
        bodies=re.findall(r'void tooie_rt64_begin_frame\([^;{}]*\)\s*\{.*?\n\}',preprocessed.stdout,re.S)
        expected_divisor='constuint32_tdivisor=tooie::demo::renderInterval(rdram,read_word(rdram,kViRetraceInterval));'
        if len(bodies)!=1 or expected_divisor not in re.sub(r'\s+','',bodies[0]):
            raise SystemExit('STOP: actual ARM64 begin-frame body lacks the accepted replay renderer interval.')
        build.joinpath('demo-render-cadence-compile-profile.json').write_text(json.dumps({
            'file':relative,'profile':profile,'accepted_replay_interval':True,
            'preprocessed_body_sha256':hashlib.sha256(bodies[0].encode()).hexdigest(),
        },indent=2)+'\n')
        print('PASS: actual ARM64 begin-frame source-rate command borrows the validated accepted replay interval.')
    policy_checks.append({'file':relative,**{name:1 for name in enabled},
                          'TOOIE_ACTOR_POLICY':'validated-marker-hierarchy'})
build.joinpath('skeletal-policy-profile.json').write_text(json.dumps(policy_checks,indent=2)+'\n')
print('PASS: all eleven actual producer/generated/runtime/RSP/workload/matcher/consumer environments enable player and validated-marker actor hierarchy.')

build.joinpath('timing-compile-profile.json').write_text(json.dumps({
    'profile': profile, 'required_targets': list(targets), 'counts': counts, 'commands': audited,
}, indent=2) + '\n')
print('PASS: all %d C and %d C++ ARM64/API24 commands: timing ON, geometry/correction/probe OFF, queue isolation=0; clear selectors undefined; %d required units include mod installer/JNI/UI and lifetime/guest-failure code.' % (counts['C'], counts['C++'], len(targets)))
VERIFY_GAMEPLAY_CONFIGURATION
python3 - "$BUILD" "$SRC" <<'VERIFY_LOOKUP_CONFIGURATION'
from pathlib import Path
import json, re, shlex, subprocess, sys

build, source = map(Path, sys.argv[1:])
entries = json.loads(build.joinpath('compile_commands.json').read_text())
units = (
    'src/recomp_api.cpp',
    'lib/N64ModernRuntime/librecomp/src/overlays.cpp',
    'lib/N64ModernRuntime/librecomp/src/recomp.cpp',
    'RecompiledFuncs/funcs_2.c', 'RecompiledFuncs/funcs_25.c',
    'RecompiledFuncs/funcs_32.c', 'RecompiledFuncs/funcs_35.c',
    'RecompiledFuncs/funcs_36.c', 'RecompiledFuncs/funcs_44.c', 'RecompiledFuncs/funcs_45.c',
)
profile = []
for relative in units:
    matches = []
    for entry in entries:
        file = Path(entry['file'])
        if not file.is_absolute():
            file = Path(entry['directory']) / file
        if file.resolve() == source.joinpath(relative).resolve():
            matches.append(entry)
    if len(matches) != 1:
        raise SystemExit('STOP: expected one lookup-provenance compile command for ' + relative)
    entry = matches[0]
    original = entry.get('arguments') or shlex.split(entry['command'])
    arguments = []
    i = 0
    while i < len(original):
        arg = original[i]
        if arg in ('-o', '-MF', '-MT', '-MQ', '-MJ'):
            i += 2
            continue
        if arg in ('-c', '-MD', '-MMD', '-MP', '-MG'):
            i += 1
            continue
        if any(arg.startswith(x) and len(arg) > len(x) for x in ('-o', '-MF', '-MT', '-MQ', '-MJ')):
            i += 1
            continue
        arguments.append(arg)
        i += 1
    result = subprocess.run([*arguments, '-E', '-dM'], cwd=entry['directory'],
                            capture_output=True, text=True, timeout=120)
    if result.returncode:
        raise SystemExit('STOP: actual lookup preprocessing failed for ' + relative + '\n' + result.stderr[-8000:])
    if not re.search(r'^#define __ANDROID__ 1$', result.stdout, re.M):
        raise SystemExit('STOP: Android lookup provenance compiled out in ' + relative)
    row = {'file': relative, 'android': 1}
    if relative.startswith('RecompiledFuncs/'):
        expected = 'tooie_lookup_func_at((int32_t)(val),rdram,ctx,__func__,__LINE__)'
        definitions = re.findall(r'^#define LOOKUP_FUNC\(val\)[ \t]+([^\r\n]*)$', result.stdout, re.M)
        if len(definitions) != 1 or re.sub(r'\s+', '', definitions[0]) != expected:
            raise SystemExit('STOP: generated lookup lacks the exact target/RAM/register/caller/line route in ' + relative)
        row['lookup'] = 'contextual-target-once'
    profile.append(row)
build.joinpath('lookup-provenance-profile.json').write_text(json.dumps(profile, indent=2) + '\n')
print('PASS: ten actual Android compile environments retain failure provenance; seven generated units route target once with RAM, registers, caller and source line.')
VERIFY_LOOKUP_CONFIGURATION
python3 - "$BUILD" "$SRC" <<'VERIFY_TIMING_CONFIGURATION'
from pathlib import Path
import json,re,shlex,subprocess,sys

build,source=map(Path,sys.argv[1:])
entries=json.loads((build/'compile_commands.json').read_text())
units=('src/recomp_api.cpp','lib/rt64/src/hle/rt64_state.cpp','lib/rt64/src/hle/rt64_present_queue.cpp',
       'RecompiledFuncs/funcs_0.c','RecompiledFuncs/funcs_1.c','RecompiledFuncs/funcs_24.c','RecompiledFuncs/funcs_77.c',
       'lib/N64ModernRuntime/ultramodern/src/events.cpp',
       'lib/RecompFrontend/recompui/src/renderer/rt64_render_context.cpp')
expected={'__ANDROID__':'1','TOOIE_ANDROID_TIMING_TRACE':'1','TOOIE_TIMING_TRACE_ENABLED':'1',
          'TOOIE_GUEST_CLOCK_TRACE_ENABLED':'1','TOOIE_ANDROID_CORRECTION_TRACE':'0',
          'TOOIE_CORRECTION_TRACE_ENABLED':'0','TOOIE_ANDROID_GEOMETRY_TRACE':'0',
          'TOOIE_VULKAN_QUEUE_ISOLATION_TEST':'0'}
profile=[]
full_sync_profile=None
task_profiles=[]

def compiled_scope(text,pattern,label):
    matches=list(re.finditer(pattern,text))
    if len(matches)!=1:raise SystemExit('STOP: ambiguous/missing compiled '+label)
    start=matches[0].end();depth=1
    # Strip comments/strings before callers pass text here, so braces in literal
    # diagnostics cannot affect the compiled function's brace boundary.
    for end in range(start,len(text)):
        depth+=(text[end]=='{')-(text[end]=='}')
        if depth==0:return text[start:end]
    raise SystemExit('STOP: unterminated compiled '+label)
def once(text,pattern,label):
    if len(re.findall(pattern,text))!=1:raise SystemExit('STOP: compiled task diagnostic missing/repeated: '+label)

for relative in units:
    matches=[]
    for entry in entries:
        file=Path(entry['file'])
        if not file.is_absolute():file=Path(entry['directory'])/file
        if file.resolve()==source.joinpath(relative).resolve():matches.append(entry)
    if len(matches)!=1:raise SystemExit('STOP: expected exactly one timing compile command: '+relative)
    entry=matches[0];original=entry.get('arguments') or shlex.split(entry['command']);arguments=[];i=0
    while i<len(original):
        arg=original[i]
        if arg in ('-o','-MF','-MT','-MQ','-MJ'):i+=2;continue
        if arg in ('-c','-MD','-MMD','-MP','-MG'):i+=1;continue
        if any(arg.startswith(x) and len(arg)>len(x) for x in ('-o','-MF','-MT','-MQ','-MJ')):i+=1;continue
        arguments.append(arg);i+=1
    result=subprocess.run([*arguments,'-E','-dM'],cwd=entry['directory'],capture_output=True,text=True,timeout=120)
    if result.returncode:raise SystemExit('STOP: timing preprocessing failed: '+relative+'\n'+result.stderr[-8000:])
    macros={}
    for name,value in re.findall(r'^#define[ \t]+(\w+)[ \t]+([^\r\n]*)$',result.stdout,re.M):
        if name in expected and name in macros:raise SystemExit('STOP: duplicate effective timing macro: '+name)
        macros[name]=value
    if any(macros.get(name)!=value for name,value in expected.items()):
        raise SystemExit('STOP: effective independent timing profile mismatch: '+relative)
    row={'file':relative,**expected}
    if relative.startswith('RecompiledFuncs/'):
        routes=re.findall(r'^#define LOOKUP_FUNC\(val\)[ \t]+([^\r\n]*)$',result.stdout,re.M)
        if len(routes)!=1 or re.sub(r'\s+','',routes[0])!='tooie_lookup_func_at((int32_t)(val),rdram,ctx,__func__,__LINE__)':
            raise SystemExit('STOP: timing generated consumer lost contextual lookup: '+relative)
        row['lookup']='contextual-target-once'

    if relative=='lib/rt64/src/hle/rt64_state.cpp':
        compiled=subprocess.run([*arguments,'-E','-P'],cwd=entry['directory'],capture_output=True,text=True,timeout=120)
        if compiled.returncode:
            raise SystemExit('STOP: full-sync producer preprocessing failed:\n'+compiled.stderr[-8000:])
        text=compiled.stdout
        start=re.search(r'\bvoid\s+State::fullSync\s*\(\s*\)\s*\{',text)
        end=re.search(r'\bvoid\s+State::updateScreen\s*\(',text[start.end():]) if start else None
        if not start or not end or len(re.findall(r'\bclass\s+FullSyncTrace\b',text))!=1:
            raise SystemExit('STOP: actual ARM64 producer source lacks the enabled full-sync observer.')
        scope=text[start.end():start.end()+end.start()]
        if len(re.findall(r'\btooie::diagnostics::FullSyncTrace\s+fullSyncTrace\s*;',scope))!=1:
            raise SystemExit('STOP: full-sync observer is not instantiated once in the production function.')
        hooks=('prepared','advanced','selection','candidate','published')
        for hook in hooks:
            if len(re.findall(r'\bfullSyncTrace\s*\.\s*'+hook+r'\s*\(',scope))!=1:
                raise SystemExit('STOP: actual ARM64 full-sync boundary missing or repeated: '+hook)
        full_sync_profile={'file':relative,'observer':'FullSyncTrace','hooks':list(hooks),
                           'compiled_function':'State::fullSync','proof':'actual-ARM64-preprocessed-body',
                           'macros':expected,'source_tree':'@@TREE@@'}


    if relative in ('lib/N64ModernRuntime/ultramodern/src/events.cpp','lib/RecompFrontend/recompui/src/renderer/rt64_render_context.cpp'):
        compiled=subprocess.run([*arguments,'-E','-P'],cwd=entry['directory'],capture_output=True,text=True,timeout=120)
        if compiled.returncode:raise SystemExit('STOP: task delivery preprocessing failed: '+relative+'\n'+compiled.stderr[-8000:])
        # Only C++ syntax matters below. Literal contents are already pinned by
        # the source manifest and independently checked in packaged libmain.so.
        text=re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'','""',compiled.stdout)
        if relative.endswith('/events.cpp'):
            action=compiled_scope(text,r'\bstruct\s+SpTaskAction\s*\{','SpTaskAction')
            once(action,r'\btooie::diagnostics::GfxTaskStamp\s+timing\s*;','immutable action stamp')
            submit=compiled_scope(text,r'\bvoid\s+ultramodern::submit_rsp_task\s*\([^)]*\)\s*\{','submit_rsp_task')
            dispatch=compiled_scope(text,r'\bvoid\s+gfx_thread_func\s*\([^)]*\)\s*\{','gfx_thread_func')
            once(submit,r'\btooie::diagnostics::GfxSubmitTrace\s+submitTrace\s*\(','submit observer')
            once(submit,r'\baction\s*\.\s*timing\s*=\s*submitTrace\s*\.\s*packet\s*\(\s*\)','owned queue stamp')
            once(dispatch,r'\btooie::diagnostics::GfxDispatchTrace\s+taskTrace\s*\(','dispatch observer')
            for method in ('rendererBegin','rendererEnd'):
                once(dispatch,r'\btaskTrace\s*\.\s*'+method+r'\s*\(\s*\)',method)
            begin=re.search(r'\btaskTrace\s*\.\s*rendererBegin\s*\(',dispatch).start()
            send=re.search(r'\brenderer_context\s*->\s*send_dl\s*\(',dispatch)
            end=re.search(r'\btaskTrace\s*\.\s*rendererEnd\s*\(',dispatch).start()
            if not send or not begin<send.start()<end:raise SystemExit('STOP: compiled dispatch boundary ordering differs.')
            verified=['SpTaskAction.timing','submit_rsp_task:submitTrace/packet','gfx_thread_func:taskTrace/rendererBegin/send_dl/rendererEnd']
        else:
            send=compiled_scope(text,r'\bvoid\s+renderer::RT64Context::send_dl\s*\([^)]*\)\s*\{','RT64Context::send_dl')
            once(send,r'\btooie::diagnostics::DisplayListTrace\s+trace\s*\(','display-list observer')
            positions=[]
            for boundary in range(3):
                pattern=r'\btrace\s*\.\s*boundary\s*\(\s*'+str(boundary)+r'\s*\)'
                once(send,pattern,'display-list boundary '+str(boundary));positions.append(re.search(pattern,send).start())
            if positions!=sorted(positions):raise SystemExit('STOP: compiled display-list boundaries reordered.')
            verified=['RT64Context::send_dl:DisplayListTrace/boundary0/boundary1/boundary2']
        task_profiles.append({'file':relative,'proof':'actual-ARM64-preprocessed-body','verified':verified,
                              'source_tree':'@@TREE@@','macros':expected})

    profile.append(row)
if full_sync_profile is None:raise SystemExit('STOP: no production full-sync compiler proof.')
(build/'full-sync-policy-profile.json').write_text(json.dumps(full_sync_profile,indent=2)+'\n')
if len(task_profiles)!=2:raise SystemExit('STOP: both actual task-delivery compiler proofs are required.')
(build/'task-delivery-policy-profile.json').write_text(json.dumps(task_profiles,indent=2)+'\n')
(build/'timing-policy-profile.json').write_text(json.dumps(profile,indent=2)+'\n')
print('PASS: nine actual native/generated compile environments enable independent clocks/frame sync and presentation input; correction/geometry capture OFF; four generated timing consumers retain contextual lookup.')
print('PASS: actual ARM64 State::fullSync body instantiates the enabled producer observer and all five boundaries exactly once; compiler proof saved.')
print('PASS: actual ARM64 runtime/renderer bodies retain the immutable task stamp, submit/dispatch observers and ordered display-list boundaries; compiler proof saved.')
VERIFY_TIMING_CONFIGURATION
if ! cmake --build "$BUILD" --target BanjoTooieRecompiled --parallel 2 > "$LOGS/build.log" 2>&1; then
  echo 'STOP: ARM64 compilation failed.' >&2
  tail -100 "$LOGS/build.log"
  exit 1
fi
LIB="$BUILD/libmain.so"
[[ -f "$LIB" ]] || { echo 'STOP: libmain.so is missing' >&2; exit 1; }
"$READELF" -h "$LIB" | grep -q 'AArch64' || { echo 'STOP: not AArch64' >&2; exit 1; }
SOURCE_BUILD_ID=$("$READELF" -n "$LIB" | awk '/Build ID:/ {print $3; exit}')
[[ -n "$SOURCE_BUILD_ID" ]] || { echo 'STOP: missing source ELF Build ID' >&2; exit 1; }
printf 'ARM64 compilation passed. ELF Build ID: %s\n' "$SOURCE_BUILD_ID"

printf '\n=== 5/7 ASSEMBLE APK IN SEPARATE ANDROID PROJECT ===\n'
mkdir -p "$PACKAGE"
rsync -a --delete --exclude='build/' --exclude='.gradle/' "$SRC/android/" "$PACKAGE/"
JNI="$PACKAGE/app/src/main/jniLibs/arm64-v8a"
mkdir -p "$JNI"
cp "$CACHE/android/libSDL2.so" "$JNI/libSDL2.so"
cp "$LIB" "$JNI/libmain.so"
cmp -s "$LIB" "$JNI/libmain.so" || { echo 'STOP: JNI copy mismatch' >&2; exit 1; }
if [[ -d "$CACHE/assets" ]]; then
  mkdir -p "$PACKAGE/app/src/main/assets"
  rsync -a --ignore-existing "$CACHE/assets/" "$PACKAGE/app/src/main/assets/"
fi
MANIFEST="$PACKAGE/app/src/main/AndroidManifest.xml"
[[ -f "$MANIFEST" ]] || { echo 'STOP: Android manifest missing' >&2; exit 1; }
python3 - "$MANIFEST" "$PACKAGE/app/build.gradle" <<'VERIFY_GAMEPLAY_PACKAGE_IDENTITY'
from pathlib import Path
import re, sys, xml.etree.ElementTree as ET

path = Path(sys.argv[1])
gradle = Path(sys.argv[2]).read_text()
expected = 'io.github.banjotooierecompiled'
identifiers = re.findall(r"(?m)^\s*applicationId\s+['\"]([^'\"]+)['\"]\s*$", gradle)
namespaces = re.findall(r"(?m)^\s*namespace\s+['\"]([^'\"]+)['\"]\s*$", gradle)
if identifiers != [expected] or namespaces != [expected]:
    raise SystemExit('STOP: unexpected or ambiguous Gradle application/namespace identity.')
tree = ET.parse(path)
root = tree.getroot()
if root.get('package') not in (None, expected):
    raise SystemExit('STOP: unexpected Android package identity.')
app = root.find('application')
if app is None:
    raise SystemExit('STOP: application element missing.')
android = '{http://schemas.android.com/apk/res/android}'
activities = [activity for activity in app.findall('activity') if activity.get(android + 'name') in (expected + '.BanjoTooieSDLActivity', '.BanjoTooieSDLActivity')]
if len(activities) != 1:
    raise SystemExit('STOP: expected exactly one established launch activity.')
resources = path.parent / 'res/values/strings.xml'
names = [element.text for element in ET.parse(resources).getroot().findall('string') if element.get('name') == 'app_name']
if names != ['Tooie Recompiled']:
    raise SystemExit('STOP: approved Tooie Recompiled app name missing or ambiguous.')
if app.get(android + 'icon') != '@mipmap/ic_launcher' or app.get(android + 'roundIcon') != '@mipmap/ic_launcher_round':
    raise SystemExit('STOP: approved normal/round icon references missing.')
for element in (app, activities[0]):
    if element.get(android + 'label') != '@string/app_name':
        raise SystemExit('STOP: unexpected app/activity label resource.')
print('PASS: Tooie Recompiled app/activity labels, round icon references and established Android package/activity identity.')
VERIFY_GAMEPLAY_PACKAGE_IDENTITY
if ! (cd "$PACKAGE" && gradle --build-cache assembleDebug) > "$LOGS/gradle.log" 2>&1; then
  echo 'STOP: APK packaging failed.' >&2
  tail -100 "$LOGS/gradle.log"
  exit 1
fi
APK="$PACKAGE/app/build/outputs/apk/debug/app-debug.apk"
[[ -f "$APK" ]] || { echo 'STOP: APK missing' >&2; exit 1; }
unzip -tq "$APK" > "$LOGS/zip-check.log" || { echo 'STOP: invalid APK archive' >&2; exit 1; }

printf '\n=== 6/7 VERIFY PACKAGED ARM64 AND ELF BUILD ID ===\n'
EXTRACT="$RUN_ROOT/verify-jni"
mkdir "$EXTRACT"
unzip -p "$APK" 'lib/arm64-v8a/libmain.so' > "$EXTRACT/libmain.so"
unzip -p "$APK" 'lib/arm64-v8a/libSDL2.so' > "$EXTRACT/libSDL2.so"
[[ -s "$EXTRACT/libmain.so" && -s "$EXTRACT/libSDL2.so" ]] || { echo 'STOP: missing packaged native libraries' >&2; exit 1; }
"$READELF" -h "$EXTRACT/libmain.so" | grep -q 'AArch64'
"$READELF" -h "$EXTRACT/libSDL2.so" | grep -q 'AArch64'
PACKAGED_BUILD_ID=$("$READELF" -n "$EXTRACT/libmain.so" | awk '/Build ID:/ {print $3; exit}')
[[ -n "$PACKAGED_BUILD_ID" && "$PACKAGED_BUILD_ID" == "$SOURCE_BUILD_ID" ]] || {
  echo "STOP: ELF Build ID mismatch: original=$SOURCE_BUILD_ID packaged=$PACKAGED_BUILD_ID" >&2; exit 1;
}
# Gradle may strip symbols. Prove exact bytes with the lexical llvm-strip alias.
python3 - "$LIB" "$CACHE/android/libSDL2.so" "$EXTRACT" "$SDK" "$LOGS" <<'VERIFY_NATIVE_PACKAGING'
from pathlib import Path
import hashlib, json, os, subprocess, sys
STATE = {'events': []}

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()

def compare_native(source, packaged, strip_tools, work):
    """Exact whole-file proof; never normalize arbitrary differences away."""
    source, packaged, work = map(Path, (source, packaged, work))
    source_hash, packaged_hash = sha256(source), sha256(packaged)
    record = {
        'library': packaged.name, 'source': str(source), 'packaged': str(packaged),
        'source_size': source.stat().st_size, 'packaged_size': packaged.stat().st_size,
        'source_sha256': source_hash, 'packaged_sha256': packaged_hash,
        'attempts': [],
    }
    STATE['events'].append(record)
    if source_hash == packaged_hash and source.stat().st_size == packaged.stat().st_size:
        record['comparison'] = 'exact unmodified input'
        return record
    for index, tool in enumerate(strip_tools):
        expected = work / (packaged.name + '.strip-' + str(index))
        result = subprocess.run([str(tool), '--strip-unneeded', '-o', str(expected), str(source)],
                                capture_output=True, text=True, errors='replace', timeout=30)
        attempt = {'tool': str(tool), 'returncode': result.returncode, 'stderr': result.stderr}
        record['attempts'].append(attempt)
        if result.returncode or not expected.is_file():
            continue
        attempt['expected_sha256'] = sha256(expected)
        attempt['expected_size'] = expected.stat().st_size
        require(sha256(source) == source_hash, 'Original library changed during verification.')
        if attempt['expected_sha256'] == packaged_hash and attempt['expected_size'] == packaged.stat().st_size:
            record['comparison'] = 'exact NDK --strip-unneeded output'
            record['strip_tool'] = str(tool)
            return record
    raise RuntimeError(packaged.name + ' does not exactly match its source or reproduced NDK symbol stripping; export blocked.')

def installed_strip_tools(home, preferred_ndk):
    roots = [home / 'Android/Sdk']
    for name in ('ANDROID_SDK_ROOT', 'ANDROID_HOME'):
        if os.environ.get(name):
            roots.append(Path(os.environ[name]))
    candidates = [preferred_ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip']
    for sdk in roots:
        if sdk.is_dir():
            candidates.extend(sorted(sdk.glob('ndk/*/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip')))
    found, resolved = [], set()
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK) and candidate.resolve() not in resolved:
            # llvm-strip may be a symlink to LLVM's multicall llvm-objcopy.
            # Its invocation name selects strip mode. Resolve only for deduplication.
            resolved.add(candidate.resolve())
            found.append(candidate.absolute())
    require(found, 'No installed NDK llvm-strip found; cannot prove the packaging transformation.')
    return found

original, sdl, extracted, ndk, logs = map(Path, sys.argv[1:])
work = extracted / 'strip-proof'
work.mkdir()
tools = installed_strip_tools(Path.home(), ndk)
try:
    for source, name in ((original, 'libmain.so'), (sdl, 'libSDL2.so')):
        record = compare_native(source, extracted / name, tools, work)
        print('PASS: ' + name + ' matches ' + record['comparison'] + '.')
finally:
    with (logs / 'native-packaging-verification.json').open('x', encoding='utf-8') as output:
        json.dump(STATE['events'], output, indent=2)
        output.write('\n')
VERIFY_NATIVE_PACKAGING
printf 'Verified matching ELF Build ID: %s\n' "$PACKAGED_BUILD_ID"
python3 - "$EXTRACT/libmain.so" "$READELF" "$APK" <<'VERIFY_PACKAGED_GAMEPLAY'
from pathlib import Path
import hashlib, subprocess, sys, zipfile

library, readelf, apk = sys.argv[1:]
data = Path(library).read_bytes()
markers = [
    b'Descriptor pool device:', b'Descriptor pool policy:', b'Descriptor pool retired:',
    b'adreno-740-proprietary-declared-bound', b'Descriptor allocation binding:',
    b'reservedType=', b'Descriptor allocation ABI:', b'Descriptor allocation arguments:',
    b'descriptor-array-element-not-allocated', b'descriptor-pool-update-after-bind-limit',
    b'GPU tile buffer:', b'initialized-empty', b'gpu-tile-buffer-not-prepared',
    b'gpu-tile-buffer-range-too-small',
    b'scissored-draw', b'Clear draw:', b'Partial color clear rejected: reason=',
    b'COLOR->TRANSFER_DST; clear mip=0 layer=0; TRANSFER_DST->COLOR; no clear render pass',
]
if any(marker not in data for marker in markers):
    raise SystemExit('STOP: expected pool policy, GPU tile lifecycle or descriptor safeguards missing from packaged native code.')
for marker in (
    b'Descriptor allocation probe start:', b'Descriptor allocation probe complete:',
    b'production-path control begin', b'production-path control retired',
    b'intentional diagnostic stop',
):
    if marker in data:
        raise SystemExit('STOP: early-exit probe code is present in the timing diagnostic library.')
for marker in (b'load-op-control', b'load-op D barrier only', b'Comparison only: explicit attachment-clear fallback enabled', b'DIAG D: swapchain vkCmdClearColorImage recorded'):
    if marker in data:
        raise SystemExit('STOP: a diagnostic clear control is present in the production library.')
# These are format fragments: the domain is substituted into Runtime %s.
for marker in (
    b'Runtime %s: mono_us=', b'event=open sdl_runtime=', b'sdl_revision=',
    b'event=rate input_hz=', b'sdl_queue_us=', b'hardware_pending_excluded=1',
    b'conversion-size-overflow', b'invalid-converted-frame-range', b'queue-failed',
    b'event=effective-graphics-config values=', b'vi_iterations=',
    b'vi_message_requests_total=', b'native_present_calls=', b'interpolation_frames=',
):
    if marker not in data:
        raise SystemExit('STOP: packaged runtime diagnostic is missing: ' + marker.decode())
for marker in (
    b'workload-stages', b'render-stages', b'present-stages', b'vulkan-build', b'shader-build',
    b'stage_units=n/sum_us/max_us/end_mono_us', b'previous_log_us=',
    b'player_mapped=', b'duplicate_player_ids=', b'invalid_decomposition_pairs=',
    b'projection_mismatch_total=', b'gpu_ns=',
):
    if marker not in data:
        raise SystemExit('STOP: packaged rendering diagnostic is missing: ' + marker.decode())
for marker in (
    b'present-queue', b'cursor_wait', b'select_write=', b'ready_write=',
    b'decisions_total=', b'current_%s', b'decisions=%llu backlog=%llu',
    b'matching-stages', b'logical_call_pairs=', b'transform_pairs=',
    b'expansion_visits=', b'tile_lookups=', b'input_calls=',
    b'scene_candidates', b'velocity_prepare',
):
    if marker not in data:
        raise SystemExit('STOP: packaged progress/matching diagnostic is missing: ' + marker.decode())
for marker in (
    b'identity=unknown edge_metric=endpoint-envelope-departure-not-error',
    b'correction-source', b'correction-data', b'correction-link',
    b'correction-sample', b'correction-consumer-cost', b'correction-budget',
    b'layout recursion: depth=',
):
    if marker in data:
        raise SystemExit('STOP: geometry/correction-only diagnostic code is present in the timing diagnostic library: ' + marker.decode())

for marker in (b'skeletal-source', b'skeletal-rejected', b'policy=authored-hierarchy',
               b'npc_policy=validated-marker-hierarchy', b'action=current-submitted-pose', b'current_pose_fallback=',
               b'evaluated_pairs=', b'rejected_owner=', b'changed_builder_output='):
    if marker not in data:
        raise SystemExit('STOP: packaged authored player-hierarchy component missing: '+marker.decode())


for marker in (b'actor-source', b'actor-draw', b'actor_applied=', b'player_applied=',
               b'identity=marker-constructor-generation', b'array_stride=156',
               b'marker=0x', b'generation=', b'callback=0x',
               b'owned-hierarchy', b'unsupported-current-pose'):
    if marker not in data:
        raise SystemExit('STOP: packaged actor ownership/hierarchy component missing: '+marker.decode())

for marker in (
    b'Android mod picker: opening document selection', b'openModPicker',
    b'Could not register texture-pack support.', b'rt64.json',
    b'The ZIP bundle contains no .nrm mods or .rtz texture packs.',
    b'targets a different game. Select a compatible Tooie mod.',
    b'This mod requires a native library that has not been validated for Android ARM64.',
    b'The archive contains an unsafe entry path.',
    b'An existing mod could not be restored. Its backup was retained in ',
    b'guest-fatal', b'action=terminate-without-static-destructors',
):
    if marker not in data:
        raise SystemExit('STOP: packaged Android mod/primary-failure component missing: ' + marker.decode())

for marker in (
    b'lookup-failure', b'lookup-registers', b'lookup-neighbors', b'lookup-words',
    b'lookup-map', b'lookup-section', b'lookup-guest-table', b'lookup-guest-overlay', b'lookup-event',
    b'caller=%.160s source_line=%u', b'guest_ra_is_raw=1',
    b'thunk=0x%08X overlay=0x%08X entry_offset=0x%X auto_unload=%u',
    b'readable_words=%u', b'loaded_reported=%zu loaded_omitted=%zu',
    b'declared_exact=%u map_present=%u', b'entries_scanned=%u omitted=%u limit=64',
    b'computed_code=0x%llX code_in_rdram=%u native_table_index=%zu',
    b'sequence=%llu operation=%s', b'run_thread_function', b'recomp_syscall_handler',
):
    if marker not in data:
        raise SystemExit('STOP: packaged failure-only lookup provenance missing: ' + marker.decode())


for marker in (
    b'correction-rom', b'configured_usa_hash=00F70DE5F2D70EA2',
    b'correction-clock', b'correction-clock-event', b'correction-clock-bucket',
    b'guest-frame-sync', b'counters=guest_VI timing=observer_only',
    b'max_start_us=%llu max_end_us=%llu',
    b'before_pending=%u before_swap_ack=%u divisor=%u barrier=%u after_elapsed=%d after_pending=%u after_swap_ack=%u',
    b'window_start_us=%llu window_end_us=%llu elapsed_us=%llu timers=%llu',
    b'replay_result=%d recorded_span=%u elapsed_span=%d applied_span=%d',
    b'caller=%u mode_bucket=%u events=%llu records=%llu',
    b'present-input', b'cursor_lock', b'cursor_condition',
    b'vi_flags_known=%u memory_flags_known=%u',
    b'entry_gap_max_us=%llu entry_gap_end_us=%llu publish_gap_max_us=%llu publish_gap_end_us=%llu',
    b'gaps=lifetime_max result_codes=repeat/early/unchanged/published',
):
    if marker not in data:
        raise SystemExit('STOP: packaged independent timing attribution missing: '+marker.decode())


for marker in (
    b'full-sync', b'publish_reset',
    b'result=%u pairs=%u candidates=%u history=%u workload=%llu present=%llu',
    b'disabled=%llu empty=%llu no_candidate=%llu no_history_match=%llu published=%llu',
    b'gaps=lifetime_and_report_window result_codes=disabled/empty/no_candidate/no_history/published',
):
    if marker not in data:
        raise SystemExit('STOP: packaged full-sync producer attribution missing: '+marker.decode())


for marker in (
    b'entry_gap_window_max_us=%llu entry_gap_window_end_us=%llu',
    b'present=%llu task=%llu correlation_valid=%u',
    b'gfx-submit', b'gfx-dispatch', b'display-list', b'enqueue', b'submit_age',
    b'texture_actions', b'rsp_reset', b'ucode', b'interpret',
    b'task=%llu entry_us=%llu submit_us=%llu dl=0x%X bytes=%u',
    b'task=%llu submit_us=%llu dispatch_us=%llu dl=0x%X bytes=%u',
    b'task=%llu dl=0x%X bytes=%u correlation_valid=%u',
    b'calls=%llu invalid=%llu entry_gap_max_us=%llu entry_gap_end_us=%llu',
):
    if marker not in data:raise SystemExit('STOP: packaged task/window-gap diagnostic missing: '+marker.decode())

symbols = subprocess.run([readelf, '--dyn-syms', '--wide', library], capture_output=True, text=True, check=True).stdout
pacing_rows = [line for line in symbols.splitlines() if line.split() and line.split()[-1] == 'tooie_demo_pacing']
if not any(' UND ' not in (' ' + row + ' ') for row in pacing_rows):
    raise SystemExit('STOP: production demo pacing bridge is not defined in the packaged native library.')
for suffix in ('nativeFileDialogResult', 'nativeModFileDialogResult', 'nativePresentationLifecycle'):
    name = 'Java_io_github_banjotooierecompiled_BanjoTooieSDLActivity_' + suffix
    rows = [line for line in symbols.splitlines() if line.split() and line.split()[-1] == name]
    if not any(' UND ' not in (' ' + row + ' ') for row in rows):
        raise SystemExit('STOP: a required native JNI bridge is not exported: ' + suffix)
with zipfile.ZipFile(apk) as archive:
    names = archive.namelist()
    if len(names) != len(set(names)):
        raise SystemExit('STOP: duplicate APK ZIP entries.')
    for entry in ('lib/arm64-v8a/libmain.so', 'lib/arm64-v8a/libSDL2.so', 'AndroidManifest.xml', 'resources.arsc'):
        if names.count(entry) != 1:
            raise SystemExit('STOP: required APK member missing: ' + entry)
    for name,digest in (
        ('menu.png','3c125d58ad008e837f3c7dc335133d03bb9aae19086ffbde90d8a71daa52c707'),
        ('menu-recompiled.png','e76b97de0c685566db2505e85d42052a520c8f31237cbfdbfe405a1706295269'),
        ('launcher-layout.json','b242f7244ce35c3eb656e30a48def2ece22c4c7a7ee2b38c8deb25e4b9d79d55')):
        member='assets/program/assets/'+name
        if names.count(member)!=1 or hashlib.sha256(archive.read(member)).hexdigest()!=digest:
            raise SystemExit('STOP: packaged approved launcher asset differs: '+name)
    for marker in (b'?/tooie/launcher-recompiled',b'menu-recompiled.png',b'LauncherArtwork'):
        if marker not in data:
            raise SystemExit('STOP: compiled native launcher layer registration is missing.')
    dex = b''.join(archive.read(name) for name in archive.namelist() if name.startswith('classes') and name.endswith('.dex'))
    for marker in (b'RomImporter', b'ModImporter', b'openModPicker', b'nativeModFileDialogResult', b'mod-selection-', b'The mod archive exceeds the 1 GiB import limit.', b'openRomPicker', b'nativeFileDialogResult', b'nativePresentationLifecycle', b'WindowSnapshotLog', b'Runtime window:', b'mono_uptime_ms=', b'insets_ltrb=', b'sequence='):
        if marker not in dex:
            raise SystemExit('STOP: a required Java/JNI component is missing from packaged DEX.')
print('PASS: exact TOOIE artwork; authored player/actor hierarchy and lifetime diagnostics, Android mod importer/installer/texture registration, primary guest-failure and bounded lookup provenance records, all three JNI exports; retained gameplay safeguards, DEX; task-delivery/display-list and full-sync window-gap diagnostics ON; independent clocks/frame-sync/input/split cursor waits retained; geometry/correction capture and intentional probe-exit controls absent.')
VERIFY_PACKAGED_GAMEPLAY

python3 - "$APK" "$AAPT2" "$SRC" "$LOGS" <<'VERIFY_PACKAGED_BRANDING'
from pathlib import Path
import hashlib, json, re, struct, subprocess, sys, zipfile, zlib

apk, aapt2, source, logs = map(Path, sys.argv[1:])
expected_package = 'io.github.banjotooierecompiled'
expected_activity = expected_package + '.BanjoTooieSDLActivity'

def require(condition, message):
    if not condition:
        raise SystemExit('STOP: packaged branding: ' + message)

def dump(label, *arguments):
    result = subprocess.run([str(aapt2), 'dump', *arguments], capture_output=True,
                            text=True, errors='replace', timeout=30)
    logs.joinpath('aapt2-' + label + '.txt').write_text(result.stdout + result.stderr)
    require(result.returncode == 0, 'aapt2 ' + label + ' failed.')
    return result.stdout

def xml_nodes(text):
    nodes, stack = [], []
    for line in text.splitlines():
        match = re.match(r'^(\s*)E: (\S+)', line)
        if match:
            indent = len(match[1])
            while stack and stack[-1]['indent'] >= indent:
                stack.pop()
            node = {'tag': match[2], 'indent': indent, 'attributes': {},
                    'parent': stack[-1] if stack else None}
            nodes.append(node); stack.append(node)
        elif stack:
            match = re.match(r'\s*A: (.+?)=(.*)$', line)
            if match:
                name = match[1].split('(')[0].rsplit(':', 1)[-1]
                value = match[2].split(' (Raw:', 1)[0]
                require(name not in stack[-1]['attributes'], 'duplicate XML attribute: ' + name)
                stack[-1]['attributes'][name] = value
    return nodes

def png_pixels(data):
    """Decode noninterlaced PNG to visible RGBA, including AAPT palette packing.

    RGB under alpha=0 has no visible meaning and can be optimized by AAPT.
    Every nonzero-alpha channel and every alpha byte must match exactly.
    """
    require(data[:8] == b'\x89PNG\r\n\x1a\n', 'invalid icon PNG signature.')
    cursor, chunks, compressed = 8, {}, bytearray()
    while cursor < len(data):
        require(cursor + 12 <= len(data), 'truncated PNG chunk.')
        size = struct.unpack_from('>I', data, cursor)[0]
        tag = data[cursor+4:cursor+8]
        payload = data[cursor+8:cursor+8+size]
        require(len(payload) == size and cursor+12+size <= len(data), 'truncated PNG data.')
        crc = struct.unpack_from('>I', data, cursor+8+size)[0]
        require(zlib.crc32(tag + payload) & 0xffffffff == crc, 'invalid PNG CRC.')
        if tag == b'IDAT': compressed += payload
        else: chunks[tag] = payload
        cursor += 12 + size
        if tag == b'IEND': break
    require(b'IHDR' in chunks and b'IEND' in chunks, 'incomplete PNG.')
    width, height, depth, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', chunks[b'IHDR'])
    channels = {0:1, 2:3, 3:1, 4:2, 6:4}.get(color)
    require(channels and depth in (1,2,4,8) and (depth == 8 or color in (0,3)), 'unsupported PNG format.')
    require(0 < width <= 2048 and 0 < height <= 2048 and (compression, filtering, interlace) == (0,0,0), 'unsupported icon size/interlace.')
    stride = (width * channels * depth + 7)//8
    bpp = max(1, (channels * depth + 7)//8)
    inflater = zlib.decompressobj()
    raw = inflater.decompress(bytes(compressed), height * (stride + 1) + 1)
    require(inflater.eof and not inflater.unconsumed_tail and len(raw) == height * (stride + 1), 'wrong PNG decoded size.')
    previous, rgba = bytearray(stride), bytearray()
    palette, transparency = chunks.get(b'PLTE', b''), chunks.get(b'tRNS', b'')
    for y in range(height):
        offset = y * (stride + 1)
        filter_type, row = raw[offset], bytearray(raw[offset+1:offset+1+stride])
        require(filter_type in range(5), 'unknown PNG row filter.')
        for index in range(stride):
            left = row[index-bpp] if index >= bpp else 0
            above = previous[index]
            corner = previous[index-bpp] if index >= bpp else 0
            if filter_type == 1: prediction = left
            elif filter_type == 2: prediction = above
            elif filter_type == 3: prediction = (left + above)//2
            elif filter_type == 4:
                p = left + above - corner
                distances = (abs(p-left), abs(p-above), abs(p-corner))
                prediction = (left, above, corner)[distances.index(min(distances))]
            else: prediction = 0
            row[index] = (row[index] + prediction) & 255
        previous = row
        for x in range(width):
            if depth == 8: sample = row[x*channels:(x+1)*channels]
            else: sample = [(row[x*depth//8] >> (8-depth-(x*depth)%8)) & ((1<<depth)-1)]
            if color == 6: r,g,b,a = sample
            elif color == 2:
                r,g,b = sample
                a = 0 if len(transparency) == 6 and (r,g,b) == struct.unpack('>HHH', transparency) else 255
            elif color == 3:
                p = sample[0]
                require(3*p+3 <= len(palette), 'palette index out of range.')
                r,g,b = palette[3*p:3*p+3]
                a = transparency[p] if p < len(transparency) else 255
            elif color == 4: r,a = sample; g=b=r
            else:
                r=g=b=sample[0] * 255//((1<<depth)-1)
                a = 0 if len(transparency) == 2 and sample[0] == struct.unpack('>H',transparency)[0] else 255
            rgba.extend((r,g,b,a) if a else (0,0,0,0))
    return width, height, bytes(rgba)

badging = dump('badging', 'badging', str(apk))
package = re.findall(r"^package: name='([^']+)'", badging, re.M)
require(package == [expected_package], 'unexpected application ID.')
require(re.findall(r"^application-label:'([^']+)'", badging, re.M) == ['Tooie Recompiled'], 'application label differs.')
launchers = re.findall(r"^launchable-activity: name='([^']+)'\s+label='([^']*)'", badging, re.M)
require(launchers == [(expected_activity, 'Tooie Recompiled')], 'launcher activity or its resolved label differs.')
require("minSdkVersion:'24'" in badging and "targetSdkVersion:'35'" in badging, 'SDK levels differ.')
resources = dump('resources', 'resources', str(apk))
table, ids, current = {}, {}, None
for line in resources.splitlines():
    match = re.match(r'\s*resource (0x[0-9a-fA-F]+) (\S+)', line)
    if match:
        current = match[2]
        require(current not in table, 'duplicate resource: ' + current)
        ids[current], table[current] = match[1].lower(), []
    elif current:
        match = re.match(r'\s+\(([^)]*)\) (.*)', line)
        if match: table[current].append((match[1], match[2]))
for name in ('string/app_name', 'color/ic_launcher_background', 'mipmap/ic_launcher', 'mipmap/ic_launcher_round', 'mipmap/ic_launcher_foreground'):
    require(name in table, 'missing compiled resource: ' + name)
require(table['string/app_name'] and all(value == '"Tooie Recompiled"' for _, value in table['string/app_name']), 'compiled label values differ.')
require(table['color/ic_launcher_background'] == [('', '#ff000536')], 'adaptive background color differs.')
nodes = xml_nodes(dump('manifest', 'xmltree', str(apk), '--file', 'AndroidManifest.xml'))
applications = [node for node in nodes if node['tag'] == 'application']
require(len(applications) == 1, 'ambiguous application manifest.')
app = applications[0]
activities = [node for node in nodes if node['tag'] == 'activity' and node['parent'] is app and node['attributes'].get('name') == '"'+expected_activity+'"']
require(len(activities) == 1, 'expected launch activity absent in compiled manifest.')
for node in (app, activities[0]):
    require(node['attributes'].get('label') == '@'+ids['string/app_name'], 'compiled app/activity label reference differs.')
for attribute, name in (('icon','mipmap/ic_launcher'), ('roundIcon','mipmap/ic_launcher_round')):
    require(app['attributes'].get(attribute) == '@'+ids[name], 'compiled '+attribute+' reference differs.')

records = []
with zipfile.ZipFile(apk) as archive:
    names = archive.namelist()
    require(len(names) == len(set(names)), 'duplicate ZIP members.')
    native = [name.split('/')[1] for name in names if name.startswith('lib/') and name.endswith('.so')]
    require(native and set(native) == {'arm64-v8a'}, 'unexpected or missing packaged ABI.')
    for resource in ('ic_launcher', 'ic_launcher_round', 'ic_launcher_foreground'):
        expected = {'mdpi':48,'hdpi':72,'xhdpi':96,'xxhdpi':144,'xxxhdpi':192}
        seen_png, seen_xml = set(), set()
        for config, value in table['mipmap/'+resource]:
            match = re.fullmatch(r'\(file\) (\S+) type=(PNG|XML)', value)
            require(match is not None, 'unexpected icon resource value.')
            member, kind = match.groups()
            require(names.count(member) == 1, 'missing icon member: '+member)
            if kind == 'PNG':
                density = config.split('-')[0]
                require(density in expected and density not in seen_png, 'unexpected/duplicate icon density.')
                seen_png.add(density)
                original = source/'android/app/src/main/res'/('mipmap-'+density)/(resource+'.png')
                actual = png_pixels(archive.read(member))
                require(actual == png_pixels(original.read_bytes()), 'visible PNG pixels/dimensions differ: '+member)
                if resource != 'ic_launcher_foreground':
                    require(actual[:2] == (expected[density],expected[density]), 'legacy icon size differs.')
                    require(actual[2][3] == 0 and actual[2][actual[0]*4-1] == 0, 'round legacy icon lost transparent corners.')
                records.append({'resource':resource, 'configuration':config, 'member':member,
                                'width':actual[0], 'height':actual[1], 'visible_rgba_sha256':hashlib.sha256(actual[2]).hexdigest()})
            else:
                require(resource != 'ic_launcher_foreground' and config in ('anydpi-v26','anydpi-v33') and config not in seen_xml, 'unexpected adaptive icon configuration.')
                seen_xml.add(config)
                adaptive = xml_nodes(dump(resource+'-'+config, 'xmltree', str(apk), '--file', member))
                require([node['tag'] for node in adaptive] == ['adaptive-icon','background','foreground'], 'adaptive icon layer structure differs.')
                require(adaptive[1]['attributes'].get('drawable') == '@'+ids['color/ic_launcher_background'], 'adaptive background reference differs.')
                require(adaptive[2]['attributes'].get('drawable') == '@'+ids['mipmap/ic_launcher_foreground'], 'adaptive foreground reference differs.')
        require(seen_png == set(expected), 'one or more icon densities missing.')
        require(seen_xml == (set() if resource == 'ic_launcher_foreground' else {'anydpi-v26','anydpi-v33'}), 'adaptive icon versions missing.')
require(len(records) == 15, 'expected fifteen PNG resource comparisons.')
logs.joinpath('packaged-branding-verification.json').write_text(json.dumps({
    'package':expected_package,'activity':expected_activity,'label':'Tooie Recompiled',
    'adaptive_background':'#000536','icon_pngs':records,
    'comparison':'exact visible RGBA and dimensions; RGB under alpha=0 normalized',
}, indent=2)+'\n')
print('PASS: final APK compiled labels/identity, all 15 round-BT icon PNGs and four adaptive icon layers match approved source.')
VERIFY_PACKAGED_BRANDING

# Build and packaging may add untracked outputs but must not edit reviewed files.
git -C "$SRC" diff --exit-code --quiet --
git -C "$SRC" diff --cached --exit-code --quiet --

PACKAGED_SDL_SHA=$(sha256sum "$EXTRACT/libSDL2.so" | awk '{print $1}')
PACKAGED_SDL_BUILD_ID=$("$READELF" -n "$EXTRACT/libSDL2.so" | awk '/Build ID:/ {print $3; exit}')
PACKAGED_SDL_BUILD_ID="${PACKAGED_SDL_BUILD_ID:-unavailable}"

printf '\n=== 7/7 EXPORT IMMUTABLE APK AND BUILD REPORT ===\n'
SHORT_SHA="${FULL_SHA:0:12}"
OUTPUT="$WINDOWS_DOWNLOADS/Tooie-${SHORT_SHA}-Demo-Render-Launcher-Timing-Debug.apk"
REPORT="$WINDOWS_DOWNLOADS/Tooie-${SHORT_SHA}-Demo-Render-Launcher-Timing-Debug-build-report.txt"
if [[ -e "$OUTPUT" || -e "$REPORT" ]]; then
  echo 'STOP: output APK or report already exists; nothing will be overwritten.' >&2
  exit 1
fi
python3 - "$APK" "$OUTPUT" <<'EXPORT_GAMEPLAY_APK'
from pathlib import Path
import shutil, sys
with Path(sys.argv[1]).open('rb') as src, Path(sys.argv[2]).open('xb') as dst:
    shutil.copyfileobj(src, dst)
EXPORT_GAMEPLAY_APK
cmp -s "$APK" "$OUTPUT" || { echo 'STOP: APK copy verification failed' >&2; exit 1; }
APK_SHA=$(sha256sum "$OUTPUT" | awk '{print $1}')

(set -o noclobber; cat > "$REPORT" <<EOF
Tooie Recompiled Android verified task-delivery-timing build report
Source commit: $FULL_SHA
Source branch: $BRANCH
Source parent: $ACTUAL_PARENT
Source tree: @@TREE@@
Engineering revision: @@ENGINEERING@@
Player implementation: b679975a352bf2f63b650f78845a2bf1eb5f8d9c
Mod/branding tested source: 82a6641c228d71756222bc0da96b0f159e51abd4
Approved icon/name source: e1fcf938dcc24bb9865b743f900d107a9de26d9d
Approved title source: @@BRANDING@@
Integration: exact disjoint 107-file engineering and 69-file mod/branding/host delta from ffd520a; no implementation edits
App name: Tooie Recompiled (compiled application/activity labels verified)
Branding: approved round BT/Jiggy icon, legacy/round/foreground visible RGBA and adaptive references verified in final APK
Branding audit: $LOGS/packaged-branding-verification.json
Wordmark asset SHA-256: e76b97de0c685566db2505e85d42052a520c8f31237cbfdbfe405a1706295269 (both source copies and exact APK member)
Launcher layout SHA-256: b242f7244ce35c3eb656e30a48def2ece22c4c7a7ee2b38c8deb25e4b9d79d55 (both source copies and exact APK member)
Launcher artwork: exact approved Jiggy background/red TOOIE- and separate blue -RECOMPILED, shared cover coordinates and live five-action menu; device appearance unverified
Launcher host checks: $LOGS/launcher-artwork.log
Menu asset SHA-256: 3c125d58ad008e837f3c7dc335133d03bb9aae19086ffbde90d8a71daa52c707 (both source copies and exact APK member)
Icon master SHA-256: 4b6db92f4ad3280c10131b5cec745508b19257a93ab1b2615279813ebca06784
Build mode: INDEPENDENT TASK DELIVERY TIMING DIAGNOSTIC DEBUG
Native CMake build type: Release
Android packaging variant: Debug (existing isolated Gradle workflow; no release/key configuration changes)
Descriptor allocation probe: OFF
Geometry trace: OFF
Correction geometry capture: OFF
Guest-clock/frame-sync trace: ON (independent TOOIE_ANDROID_TIMING_TRACE=1 in all native C/C++ commands)
Presentation-input/split-wait trace: ON (same timing profile)
Full-sync producer trace: ON (same timing profile; prepare/advance/publish_reset, eligibility counts, separate lifetime/report-window entry gaps and copied task correlation)
Task delivery/display-list trace: ON (same timing profile; gfx-submit/gfx-dispatch/display-list, immutable IDs/timestamps, nested TLS correlation)
Task timing scope: submit age includes enqueue/observation overhead; interpreter includes full-sync; entry gaps include previous work/logging; independent/nested maxima are not additive
Producer diagnostics scope: observer only; queued work, candidate ordering, publication predicates and ownership waits unchanged
Timing behavior: accepted replay spans extend existing guest VI waits and describe renderer source intervals; ordinary title/gameplay and global VI rate unchanged
Demo cadence policy: accepted recorded-VI minimum in existing guest waits and Android renderer source interval
Demo renderer cadence: accepted replay interval in the existing Android source-rate command; exact-parent emission failure and actual ARM64 preprocessed body verified
Demo renderer cadence logs: $LOGS/demo-render-cadence.log and $BUILD/demo-render-cadence-compile-profile.json
Timing measurement scope: includes bounded observer/logging overhead; not the normal observer-OFF performance baseline
Player hierarchy: ON by Android default; eleven actual C/C++ environments verify player/actor switches and enabled macros equal 1
Player hierarchy scope: validated animated player slots; owned pose/schema/occupant/frame metadata; exact submitted endpoints; invalid recognized history uses current pose; standard actor policy separately validated
Player hierarchy validation: native hooks/task/RSP/workload/matcher/consumer fixture, GCC ASan/UBSan, independent Clang SIMD and exact-60c0c89 failing control passed; actual Odin appearance/overhead unverified
Hierarchy host coverage: six committed player/constructed-actor triangle cases in scalar/SIMD plus rigid actor reordering and native lifetime controls; external 192 cases were separate Codex validation, not captured NPC poses
Player hierarchy logs: $LOGS/skeletal-pose.log and $LOGS/hierarchy-proposal.log
Player hierarchy compiler audit: $BUILD/skeletal-policy-profile.json
Actor hierarchy: ON by Android default; validated marker-constructor generation, canonical movable actor-array ownership and owned authored pose
Actor fallback: unsupported registered bones retain exact submitted matrices; custom/sprite/particle paths unchanged
Actor ownership results: $LOGS/actor-ownership.log (actual generated constructor/draw/compaction/lifetime hooks, parent-failure and actor-OFF/desktop instruction-body controls)
C flags: $TIMING_CXX_FLAGS
C++ flags: $TIMING_CXX_FLAGS
Actual compiler profile audit: $BUILD/timing-compile-profile.json
Queue isolation: 0; transfer/fallback/barrier selectors UNDEFINED
Transfer clear policy: production full transfer and scissored partial draw
ARM64 API level: 24
ELF Build ID: $PACKAGED_BUILD_ID
APK SHA-256: $APK_SHA
APK: $OUTPUT
Logs: $LOGS
Native packaging verification: $LOGS/native-packaging-verification.json
Mod host results: $LOGS/mod-support.log (Java/SDK/JVM/JNI, POSIX miniz prototypes, production manifest/config/archive/rollback/registration and exact-base controls rerun)
Mod packaged checks: ModImporter/openModPicker/nativeModFileDialogResult in DEX, actual JNI export, archive validation/rollback and texture startup code in libmain.so
UI lifetime results: $LOGS/rml-lifetime.log (strict cstdint first include, actual RmlUi and exact-parent failing controls)
Guest fatal results: $LOGS/guest-failure.log
Lookup provenance: Android failure-only caller/line/raw registers, decoded syscall/thread entry, native/guest mappings, bounded words and recent load/unload journal
Lookup fatal handling: primary guest-fatal before detail records; strict failure/quick-exit retained; no guessed/stubbed function or timing correction
Lookup host results: $LOGS/guest-lookup.log (38 controls including three exact-cb896c3 provenance failures; constructed tables/RAM, not the Odin trigger)
Lookup compiler audit: $BUILD/lookup-provenance-profile.json (ten actual Android environments, seven contextual generated LOOKUP_FUNC routes)
Full lookup stderr trace: not enabled by this builder; opt-in host regression control is separate
First-person transition: corrections retained; entrance/boss success observed on prior b3401dd hardware run; this timing-diagnostic APK remains untested on Odin
First-person consumer: exact complete normalized function SHA-256 997edfb876fe4a765e4dce7d7ede0bbbdcf9cbbe55bbb610c4bfec3f6fcb2412
First-person host results: $LOGS/jump-table-analysis.log; $LOGS/first-person-dispatch.log; $LOGS/first-person-table.log (20 generator controls; 513 Android + 513 desktop consumer/reference scenarios, parent and mutation failures; controlled peripheral callees, not gameplay)
Four-consumer source profile: $LOGS/bsfirstp-source-profile.json (four complete function hashes, ordered ROM targets, scoped 306-function audit)
Four-consumer host results: $LOGS/bsfirstp-dispatches.log (468 Android + 468 desktop full consumer/reference scenarios; four exact-parent failures and eight mutations; controlled peripheral callees, not gameplay)
Four-consumer ROM excerpt SHA-256: 7bc7cd52c090d8da384833f342c9f9bfca6803515ebf72dbe0fd4d6f94612acd (23 slots and 2512 verified instruction bytes; no full ROM bundled)
Guest timing host results: $LOGS/guest-timing-trace.log (565 complete consumer/profile scenarios, retained 1105 sparse events per profile, ordinary-state digest equivalence and explicit replay-wait expectations and parent/frame/replay controls)
Presentation input host results: $LOGS/presentation-input-trace.log (40 actual State::updateScreen cases and unchanged exact-parent publication/RAM controls)
Timing compiler audit: $BUILD/timing-policy-profile.json (nine actual environments, four generated contextual lookup routes)
Producer compiler audit: $BUILD/full-sync-policy-profile.json (actual ARM64 preprocessed State::fullSync body, observer and five boundaries)
Producer host results: $LOGS/full-sync-trace.log (50 publication-tail cases across exact-parent/default/zero/timing/desktop; startup-masked exact-parent window control, missing-boundary and unsafe-candidate controls; constructed queues/clock, not device scheduling)
Step 115 captured timing results: $LOGS/step115-captured-timing.log (633 sparse details: 249 ordinary, 336 replay and 48 terminal through actual generated consumers in five profiles; 560 full consumer/profile scenarios, parent and mutation controls)
Step 115 fixture SHA-256: 1019de60047286e9a5c6723e25d945ff0df624b73c9f9e31bfd2f060f28698e5 (236987 committed LF bytes; no external archive required)
Task compiler audit: $BUILD/task-delivery-policy-profile.json (actual ARM64 runtime and frontend function bodies, private action stamp and ordered boundaries)
Task host results: $LOGS/gfx-task-trace.log (40 actual-source cases, five profiles, queue/skeletal lifetime/cross-TU and exact-parent/mutation controls; adapted services/clocks, not Odin scheduling)
Step 118 captured timing results: $LOGS/step118-captured-timing.log (569 records: 243 ordinary, 278 replay and 48 terminal; all captured divisors retained, five profiles and 565 complete consumer/profile scenarios)
Step 118 fixture SHA-256: 523ffe51c04051d383aa0891f8e0d2a2341378862e51b736128740cbc52bde49 (212256 committed LF bytes; external evidence-archive validation NOT RUN by this builder)
Clock attribution limit: encoded replay wait retained; renderer duration corrected in source; this revision remains untested on Odin
First-person assembler: $LOGS/first-person-assembler.json (actual compiler/version, verified MIPS words; private clang-19 alias for the retained test script)
First-person compile audit: funcs_44.c and funcs_45.c required in the normal ARM64 command database and effective Android contextual-lookup preprocessing
Generator: host library/emitter built Release; analyzer and consumer adapters retain their source suite sanitizer settings; no full ROM bundled; independent timing observer enabled separately
Lifecycle results: $LOGS/lifecycle.log
Geometry results: $LOGS/render-interpolation.log and $LOGS/geometry-capture.log
Observer regression results: $LOGS/correction-observer.log and $LOGS/geometry-observer.log (host controls; final APK geometry/correction capture OFF and independent timing ON)
Generated clock results: $LOGS/demo-clock.log (all 1,105 retained sparse details and generated-consumer controls; no device-speed correction)
Frame ownership/progress results: $LOGS/frame-protocol.log
Matching results: $LOGS/matching-equivalence.log and $LOGS/matching-cost.log
Audio/timing/window results: $LOGS/audio-runtime.log
Host SDL2 pkg-config version: $(pkg-config --modversion sdl2)
Host audio compiler: $AUDIO_CXX
Host SIMD compiler: $RENDER_SIMD_CXX
Host SDK jar: $ANDROID_JAR
Resource verification tool: $AAPT2
Clear modes: six comparison plus exact production; timeout control passed
Exact production recorder: clipping, partial MSAA/view clear, full transfers and four rejection controls
Descriptor recorder: 55 rejection cases; reservation/count/overlap and five-second safeguards retained
Software Vulkan, GPU tile lifecycle and pixel readbacks: NOT RUN by this builder (external Codex results are separate)
Packaged SDL2 SHA-256: $PACKAGED_SDL_SHA
Packaged SDL2 Build ID: $PACKAGED_SDL_BUILD_ID
Hardware gameplay, Android provider behavior, code-mod execution and rendered texture replacement with this APK: NOT YET TESTED
Outstanding NPC stretching, demo cadence, audio timeouts, sustained presentation, temple/jigsaw and full-game coverage: UNRESOLVED; global pacing unchanged
Automatic update checking: planned only; not implemented here
Existing package/activity: io.github.banjotooierecompiled / .BanjoTooieSDLActivity (unchanged)
Odin: NOT installed; NOT launched; no ADB commands used
App data, settings and existing PC build folders/artifacts: preserved
EOF
)
printf '\nSUCCESS: VERIFIED TOOIE RECOMPILED DEMO-RENDER-LAUNCHER APK READY (NOT INSTALLED)\n'
printf 'Commit: %s\n' "$FULL_SHA"
printf 'APK: %s\n' "$OUTPUT"
printf 'Build ID: %s\n' "$PACKAGED_BUILD_ID"
printf 'SHA-256: %s\n' "$APK_SHA"
printf 'Report: %s\n' "$REPORT"
echo 'Task delivery/display-list, full-sync window gaps and independent guest-clock/frame-sync/input timing ON; geometry/correction capture and allocation probe OFF; player/actor hierarchy ON; clear selectors undefined.'
echo 'Mod support, selected round BT icon, Tooie Recompiled name and red TOOIE / blue RECOMPILED title verified.'
echo 'Retained first-person switch corrections passed host tests; prior b3401dd entrance/boss success is recorded. This diagnostic APK remains untested on Odin.'
echo 'Replay wait correction passed host controls; Odin appearance and ordinary-title motion remain unverified. Observer overhead is included; global VI rate and safeguards are unchanged.'
echo 'No ADB commands were used. Odin unchanged.'
