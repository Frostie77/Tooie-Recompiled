#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import subprocess,sys
r,t=map(Path,sys.argv[1:])
for mode in ['current','parent']:
 methods=[]
 for file,names in [('overlays.cpp',(['lookup_function'] if mode=='current' else [])+['get_function','unload_overlays']),('recomp.cpp',['switch_error','do_break'])]:
  path='lib/N64ModernRuntime/librecomp/src/'+file
  text=(r/path).read_text() if mode=='current' else subprocess.check_output(['git','-C',str(r),'show','01495d04a248bb263a0f0d18695c9cfc983cf2d5:'+path],text=True)
  for name in names:
   start=text.rfind('\n',0,text.index(' '+name+'('))+1;begin=text.index('{',start);depth=1;end=begin+1
   while depth:
    depth+=(text[end]=='{')-(text[end]=='}');end+=1
   methods.append(text[start:end])
 (t/mode).mkdir();(t/mode/'guest-failure-methods.inc').write_text('\n'.join(methods))
PY
flags=(-std=c++20 -O1 -g -DNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer
 -I"$r/src" -I"$r/lib/N64Recomp/include" -I"$r/lib/N64ModernRuntime/librecomp/include" -I"$r/lib/N64ModernRuntime/ultramodern/include")
for mode in android android-asserts desktop parent;do
 defs=();source=current
 if [ "$mode" != desktop ];then defs=(-D__ANDROID__);fi
 if [ "$mode" = parent ];then source=parent;fi
 if [ "$mode" = android-asserts ];then defs+=(-UNDEBUG);fi
 "${CXX:-g++}" "${flags[@]}" "${defs[@]}" -I"$t/$source" "$r/tests/guest_failure.cpp" "$r/lib/N64ModernRuntime/ultramodern/src/error_handling.cpp" -o "$t/$mode-test"
done
python3 - "$t" <<'PY'
from pathlib import Path
import os,signal,subprocess,sys
root=Path(sys.argv[1]);reasons={'missing':('missing-function','0x80801044'),'partial':('partial-overlay-unload','0x80801040'),'switch':('switch-case-out-of-bounds','0x80805678'),'break':('guest-break','0x80807890')}
for mode in ['android','android-asserts','desktop','parent']:
 for case in ['good',*reasons]:
  folder=root/(mode+'-'+case);folder.mkdir();marker=folder/'exit-marker'
  env=dict(os.environ,APP_FOLDER_PATH=str(folder),TEST_EXIT_MARKER=str(marker),ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
  p=subprocess.run([str(root/(mode+'-test')),case],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  assert p.returncode==(0 if case=='good' else (-signal.SIGABRT if mode=='android-asserts' else 1)),(mode,case,p.returncode,p.stdout)
  assert marker.exists()==(not mode.startswith('android') or case=='good'),(mode,case,'unexpected ordinary destructor/atexit execution')
  log=folder/'banjotooi-debug.log'
  if mode.startswith('android') and case!='good':
   text=log.read_text();reason,pc=reasons[case]
   assert 'reason='+reason in text and 'guest_address='+pc in text and 'pid=' in text and 'tid=' in text
   assert 'action=terminate-without-static-destructors' in text
   if case=='switch':assert 'context=test_guest_function' in text and 'detail0=0x80710000' in text
   if case=='partial':assert 'detail0=0x00000100 detail1=0x00123400' in text
  else:assert not log.exists(),(mode,case,'unexpected error record')
print('PASS: four actual guest fatal handlers preserve the primary reason/guest address/PID/TID and terminate without ordinary teardown on Android; mapped lookup/exact unload and desktop behavior unchanged; Android debug assertions remain fatal after primary logging')
print('PASS: exact parent source executes ordinary-exit callbacks and lacks private primary-failure records; real runtime quick_exit executes in current Android cases')
PY
