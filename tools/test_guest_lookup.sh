#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import re,sys,subprocess
r,t=map(Path,sys.argv[1:]);functions={};origins={}
sys.path.insert(0,str(r/'tools'))
from lookup_source import without_lookup_lines
sample='tooie_lookup_func_at((int32_t)(ctx->r25+100), rdram, ctx, __func__, 123)'
assert without_lookup_lines(sample)==without_lookup_lines(sample[:-4]+'456)')
assert without_lookup_lines(sample)!=without_lookup_lines(sample.replace('r25','r24'))
assert without_lookup_lines(sample)!=without_lookup_lines(sample.replace('+100','+200'))
for file in ['funcs_2.c','funcs_32.c']:
 s=(r/'RecompiledFuncs'/file).read_text()
 for m in re.finditer(r'^RECOMP_FUNC void (\w+)\(.*?(?=^RECOMP_FUNC |\Z)',s,re.M|re.S):
  functions[m[1]]=m[0];origins[m[1]]=(s.count('\n',0,m.start())+1,file)
names=['func_800EC3C4','ovl_shift','func_80082088','func_800821FC','func_800822C0','ovl_unload','func_80082428']
(t/'generated.c').write_text('#include "tooie_recomp.h"\n#include "funcs.h"\nvoid __osPiGetAccess_recomp(uint8_t*,recomp_context*);\nvoid __osPiRelAccess_recomp(uint8_t*,recomp_context*);\n'+ '\n'.join('#line %d "RecompiledFuncs/%s"\n'%origins[n]+functions[n] for n in names))
(t/'caller-line').write_text(str(origins['func_800EC3C4'][0]+functions['func_800EC3C4'].splitlines().index('    LOOKUP_FUNC(ctx->r25)(rdram, ctx);')))
# Keep the actual registration wrappers and sentinel array, without linking
# every unrelated generated native function referenced by the full section table.
source=(r/'src/register_overlays.cpp').read_text();wrappers=[]
for name in ['load_overlay','unload_overlay']:
 start=source.index('bool tooie::'+name+'(');begin=source.index('{',start);end=begin+1;depth=1
 while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
 wrappers.append(source[start:end])
indices=re.search(r'static int overlay_sections_by_index\[\] = \{.*?\};',(r/'RecompiledFuncs/recomp_overlays.inl').read_text(),re.S)[0]
(t/'register.cpp').write_text('#include "register_overlays.hpp"\n#include "recomp.h"\nextern "C" void load_overlay_by_id(uint32_t,uint32_t);\nextern "C" void unload_overlay_by_id(uint32_t);\n#define ARRLEN(a) (sizeof(a)/sizeof((a)[0]))\n'+indices+'\n'+'\n'.join(wrappers))
assert 'LOOKUP_FUNC(ctx->r25)(rdram, ctx)' in functions['func_800EC3C4']
print('PASS: exact generated indirect caller, signed shift/relocation and unload consumers extracted')
(t/'parent').mkdir()
for source,destination in [('src/recomp_api.cpp','api.cpp'),('include/tooie_recomp.h','tooie_recomp.h'),('lib/N64ModernRuntime/librecomp/src/overlays.cpp','overlays.cpp')]:
 (t/'parent'/destination).write_bytes(subprocess.check_output(['git','-C',str(r),'show','cb896c3866fdd8dade1b8a1d12f43872ac516d39:'+source]))
for mode in ['current','parent']:
 source=(r/'lib/N64ModernRuntime/librecomp/src/recomp.cpp').read_text() if mode=='current' else subprocess.check_output(['git','-C',str(r),'show','cb896c3866fdd8dade1b8a1d12f43872ac516d39:lib/N64ModernRuntime/librecomp/src/recomp.cpp'],text=True)
 start=source.index('void run_thread_function(');begin=source.index('{',start);end=begin+1;depth=1
 while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
 (t/mode).mkdir(exist_ok=True)
 (t/mode/'guest-thread.inc').write_text('#line %d "lib/N64ModernRuntime/librecomp/src/recomp.cpp"\n'%(source.count('\n',0,start)+1)+source[start:end])
PY

rt="$r/lib/rt64/src"
flags=(-O1 -g -fwrapv -DNDEBUG -DHLSL_CPU -D__PRFCHWINTRIN_H -pthread -ffunction-sections -fdata-sections
 -I"$r/src" -I"$r/include" -I"$r/RecompiledFuncs" -I"$r/tests/android_headers" -I"$r/lib/N64ModernRuntime/N64Recomp/include" -I"$r/lib/N64Recomp/include"
 -I"$r/lib/N64ModernRuntime/librecomp/include" -I"$r/lib/N64ModernRuntime/librecomp/include/librecomp" -I"$r/lib/N64ModernRuntime/librecomp/src"
 -I"$r/lib/N64ModernRuntime/ultramodern/include" -I"$r/lib/N64ModernRuntime/thirdparty"
 -I"$r/lib/N64ModernRuntime/thirdparty/concurrentqueue" -I"$r/lib/N64Recomp/lib/fmt/include" -I"$r/lib/N64Recomp/lib/tomlplusplus"
 -I"$rt" -I"$rt/contrib" -I"$rt/contrib/hlslpp/include" -I"$rt/contrib/json/single_include" -I"$rt/contrib/plume"
 -I"$rt/contrib/dxc/inc" -I"$rt/contrib/mupen64plus-core/src/api" -I"$r/lib/rt64/include")
san=(-fsanitize=address,undefined -fno-omit-frame-pointer)
for mode in android desktop parent;do
 defs=();prefix=(-I"$t/current");api="$r/src/recomp_api.cpp";runtime="$r/lib/N64ModernRuntime/librecomp/src/overlays.cpp"
 if [ "$mode" != desktop ];then defs=(-D__ANDROID__);fi
 if [ "$mode" = parent ];then prefix=(-I"$t/parent");api="$t/parent/api.cpp";runtime="$t/parent/overlays.cpp";fi
 # The full parent runtime uses relative app-header paths; adjust includes only.
 if [ "$mode" = parent ];then
  python3 - "$runtime" "$r" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);p.write_text(p.read_text().replace('../../../../src/guest_failure.h',str(Path(sys.argv[2])/'src/guest_failure.h')))
PY
 fi
 "${CC:-gcc}" -std=gnu11 "${prefix[@]}" "${flags[@]}" "${defs[@]}" "${san[@]}" -c "$t/generated.c" -o "$t/$mode-generated.o"
 for pair in "$api:api" "$runtime:runtime" "$t/register.cpp:register";do
  "${CXX:-g++}" -std=c++20 "${prefix[@]}" "${flags[@]}" "${defs[@]}" "${san[@]}" -c "${pair%:*}" -o "$t/$mode-${pair##*:}.o"
 done
 "${CXX:-g++}" -std=c++20 "${prefix[@]}" "${flags[@]}" "${defs[@]}" "${san[@]}" "$r/tests/guest_lookup.cpp" "$r/lib/N64ModernRuntime/ultramodern/src/error_handling.cpp" "$t/$mode-generated.o" "$t/$mode-api.o" "$t/$mode-runtime.o" "$t/$mode-register.o" -Wl,--gc-sections -o "$t/$mode-test"
done
python3 - "$t" <<'PY'
from pathlib import Path
import os,re,subprocess,sys
t=Path(sys.argv[1]);cases=['good','dispatch-good','auto-unload','thread-good','generated','dispatch','thread','legacy','unloaded','overlap','bounds','journal','unaligned','outside','shifted-old','section-cap','ram-end']
def run(mode,case,trace=False):
 folder=t/(mode+'-'+case+('-trace' if trace else ''));folder.mkdir();marker=folder/'exit-marker'
 env=dict(os.environ,APP_FOLDER_PATH=str(folder),TEST_EXIT_MARKER=str(marker),ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',TOOIE_TRACE_LOOKUP='1' if trace else '0')
 p=subprocess.run([str(t/(mode+'-test')),case],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 good=case in cases[:4]
 assert p.returncode==(0 if good else 1),(mode,case,p.returncode,p.stdout)
 assert marker.exists()==(mode=='desktop' or good),(mode,case,'unsafe ordinary exit callbacks')
 file=folder/'banjotooi-debug.log';log=file.read_text() if file.exists() else ''
 if good or mode=='desktop':assert not log,(mode,case,log)
 if trace:assert '[lookup] 0x801F38E0 from rva' in p.stdout
 return log
for mode in ['android','desktop']:
 for case in cases:
  s=run(mode,case)
  if mode=='desktop' or case in cases[:4]:continue
  assert 'reason=missing-function' in s and 'pid=' in s and 'tid=' in s
  assert s.splitlines()[0].startswith('Runtime guest-fatal:'),(mode,case,'primary error must precede detailed inspection')
  assert all(len(l)<2048 for l in s.splitlines())
  if case=='generated':
   assert 'caller=func_800EC3C4 source_line='+ (t/'caller-line').read_text()+' ' in s and 't9=0x801F38E0' in s
   assert 'raw_ra=0x80111110' in s and 'guest_ra_is_raw=1' in s
  if case=='dispatch':assert 'caller=recomp_syscall_handler' in s and 'thunk=0x80082548 overlay=0x801F37C0 entry_offset=0x0 auto_unload=1' in s
  if case=='thread':assert 'caller=run_thread_function' in s and 'a0=0x80600000 a1=0x00000055' in s and 'raw_ra=0x00000000' in s
  if case=='legacy':assert 'caller=none source_line=0' in s and 'registers=0' in s
  if case not in ('unloaded','outside','shifted-old','ram-end'):assert 'containing_sections=1' in s
  if case=='overlap':assert 'declared_exact=1 map_present=0' in s and 'operation=unload-id' in s
  if case=='unloaded':assert 'containing_sections=0' in s and 'operation=unload-id rom=0x00123400 ram=0x801F3800' in s
  if case=='outside':assert 'containing_sections=0' in s and 'contains=0' in s
  if case=='ram-end':assert 'kind=target-raw start=0x80800000 words= readable_words=0' in s
  if case=='section-cap':assert 'containing_sections=1 loaded_reported=64 loaded_omitted=8' in s
  if case=='shifted-old':assert 'containing_sections=0' in s and 'operation=load rom=0x00123400 ram=0x801F4800' in s
  if case=='bounds':
   assert 'kind=stack-raw start=0x807FFFFC' in s and 'readable_words=1' in s
   assert 'kind=a0-raw start=0xFFFFFFFC words= readable_words=0' in s
   assert 'count=4294967295 entries_scanned=64 omitted=4294967231 limit=64' in s
  if case=='unaligned':assert 'kind=target-raw start=0x801F38E1 words= readable_words=0' in s
  if case=='journal':
   events=[l for l in s.splitlines() if l.startswith('Runtime lookup-event:')]
   assert len(events)==32 and 'sequence=11 ' in events[0] and 'sequence=42 ' in events[-1]
  if case not in ('bounds','legacy','unloaded'):assert 'header=0x801F37C0 readable=1 index=2 entry_count=2 computed_code=0x801F3800 code_in_rdram=1 native_table_index=1' in s
run('android','generated',True)
for case in ['generated','dispatch','thread']:
 s=run('parent',case)
 assert 'context=none' in s and 'Runtime lookup-failure:' not in s
 try:assert {'generated':'caller=func_800EC3C4','dispatch':'thunk=0x80082548','thread':'caller=run_thread_function'}[case] in s
 except AssertionError:pass
 else:raise AssertionError('exact parent unexpectedly retained failed caller provenance')
print('PASS: actual generated caller and decoded syscall provenance, active/deleted/shifted/overlapping mappings, bounded raw words and journal rollover; successful one-evaluation dispatch has no diagnostic I/O')
print('PASS: exact cb896c3 fails all three provenance assertions; full runtime missing lookup remains fatal without ordinary teardown on Android; desktop behavior preserved')
PY

# Opt-in controlled timing uses optimized, unsanitized full production objects.
# Sanitized correctness controls above remain mandatory; timings have no pass bar.
if [ "${TOOIE_LOOKUP_BENCHMARK:-0}" = 1 ];then
 for pair in "$r/src/recomp_api.cpp:api" "$r/lib/N64ModernRuntime/librecomp/src/overlays.cpp:runtime" "$t/register.cpp:register";do
  "${CXX:-g++}" -std=c++20 -I"$t/current" "${flags[@]}" -O2 -D__ANDROID__ -c "${pair%:*}" -o "$t/bench-${pair##*:}.o"
 done
 "${CC:-gcc}" -std=gnu11 -I"$t/current" "${flags[@]}" -O2 -D__ANDROID__ -c "$t/generated.c" -o "$t/bench-generated.o"
 "${CXX:-g++}" -std=c++20 -I"$t/current" "${flags[@]}" -O2 -D__ANDROID__ "$r/tests/guest_lookup_benchmark.cpp" \
  "$r/lib/N64ModernRuntime/ultramodern/src/error_handling.cpp" "$t/bench-api.o" "$t/bench-runtime.o" "$t/bench-register.o" "$t/bench-generated.o" -Wl,--gc-sections -o "$t/benchmark"
 mkdir "$t/benchmark-log"
 APP_FOLDER_PATH="$t/benchmark-log" TOOIE_TRACE_LOOKUP=0 "$t/benchmark"
 test ! -e "$t/benchmark-log/banjotooi-debug.log"
 test ! -e "$t/benchmark-log/banjotooierecompiled.log"
fi
