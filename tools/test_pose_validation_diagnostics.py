#!/usr/bin/env python3
"""Check exact producer validation reasons without changing pose policy."""
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    root=Path(__file__).resolve().parents[1]
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    with tempfile.TemporaryDirectory(prefix='tooie-pose-validation-') as folder:
        build=Path(folder);shadow=build/'shadow';shadow.mkdir()
        source=(root/'src/actor_pose_guest.h').read_text()
        assert source.count('list ? source.lastRejection : "none"')==1
        (shadow/'actor_pose_guest.h').write_text(source.replace('list ? source.lastRejection : "none"','"unclassified"'))
        for label,extra,expected in [('current',[],0),('missing-reason',['-I'+str(shadow)],86)]:
            exe=build/label
            subprocess.run([os.environ.get('CXX','g++'),'-std=c++20','-O1','-D__ANDROID__',
                '-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',*extra,'-I'+str(root/'src'),
                str(root/'tests/pose_validation_diagnostics.cpp'),'-o',str(exe)],check=True)
            result=subprocess.run([str(exe),str(build/(label+'-data'))],env=env,capture_output=True,text=True)
            if result.returncode!=expected or 'runtime error:' in result.stderr or 'ERROR: AddressSanitizer' in result.stderr:
                raise RuntimeError(result.stdout+result.stderr)
            if expected:
                assert 'pose diagnostic failure:' in result.stderr
            else: print(result.stdout.strip())
        print('PASS: dropping the actual validation reason fails the actor-log assertion; no renderer policy changes.')


if __name__=='__main__':main()
