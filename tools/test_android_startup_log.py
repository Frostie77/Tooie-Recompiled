#!/usr/bin/env python3
"""Exercise the actual exporter/provider through a host file consumer; compile against the SDK.

Small Android adapters record the Intent/URI contract, not Android permission
enforcement or Binder dispatch. Those still require an actual device share.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--android-jar',type=Path,required=True)
    args=parser.parse_args()
    java=Path(shutil.which('java')).resolve()
    javac=[shutil.which('javac')] if shutil.which('javac') else [str(java),'com.sun.tools.javac.Main']
    source=ROOT/'android/app/src/main/java/io/github/banjotooierecompiled'
    production=[source/'StartupLog.java',source/'StartupLogProvider.java']
    def run(command):subprocess.run([str(v) for v in command],check=True)
    with tempfile.TemporaryDirectory(prefix='tooie-startup-log-') as folder:
        work=Path(folder)
        run([*javac,'--release','17','-cp',args.android_jar,'-d',work/'sdk',*production])
        run([*javac,'--release','17','-cp',args.android_jar,'-d',work/'host',*production,
             *sorted((ROOT/'tests/java_startup_log').rglob('*.java'))])
        run([java,'-ea','-cp',str(work/'host')+os.pathsep+str(args.android_jar),
             'io.github.banjotooierecompiled.StartupLogTest',work/'files',work/'cache'])
        # Reintroduce the failed transport in the actual provider helper. The
        # same consumer test must reject the large inline-text request.
        control=work/'control/StartupLogProvider.java';control.parent.mkdir()
        text=production[1].read_text()
        old='.putExtra(Intent.EXTRA_STREAM,uri)'
        assert text.count(old)==1
        control.write_text(text.replace(old,'.putExtra(Intent.EXTRA_TEXT,"x".repeat(256*1024))'))
        run([*javac,'--release','17','-cp',args.android_jar,'-d',work/'negative',production[0],control,
             *sorted((ROOT/'tests/java_startup_log').rglob('*.java'))])
        negative=subprocess.run([str(v) for v in [java,'-ea','-cp',str(work/'negative')+os.pathsep+str(args.android_jar),
            'io.github.banjotooierecompiled.StartupLogTest',work/'negative-files',work/'negative-cache']],capture_output=True,text=True)
        if negative.returncode==0 or 'java.lang.AssertionError' not in negative.stderr:
            raise RuntimeError('Inline-text negative control did not fail as expected: '+negative.stderr)
        print('PASS: reintroduced 256-KiB inline-intent negative control is rejected by the same consumer test')
    print('PASS: startup-log SDK compilation; Android URI-grant/share-sheet acceptance remains a device check')


if __name__=='__main__':main()
