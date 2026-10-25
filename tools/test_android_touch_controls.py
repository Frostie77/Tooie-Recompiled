#!/usr/bin/env python3
"""Check touch geometry, actual native input merging/JNI, and full Android Java compilation."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--android-jar",type=Path,required=True)
    parser.add_argument("--jni-include",type=Path)
    args=parser.parse_args()
    if not args.android_jar.is_file():parser.error("Android SDK android.jar is required")
    java=Path(shutil.which("java")).resolve()
    javac=[shutil.which("javac")] if shutil.which("javac") else [str(java),"com.sun.tools.javac.Main"]
    jni=args.jni_include or Path(os.environ.get("JAVA_HOME",java.parent.parent))/"include"
    if not (jni/"jni.h").is_file():parser.error("Provide JDK development headers with --jni-include")
    cxx=os.environ.get("CXX","g++")
    def run(command,**options):return subprocess.run([str(v) for v in command],check=True,**options)
    with tempfile.TemporaryDirectory(prefix="tooie-touch-") as folder:
        work=Path(folder)
        source=ROOT/"android/app/src/main/java"
        run([*javac,"--release","17","-cp",args.android_jar,"-d",work/"android",*sorted(source.rglob("*.java"))])
        model=source/"io/github/banjotooierecompiled/TouchControllerModel.java"
        run([*javac,"--release","17","-d",work/"model",model,*sorted((ROOT/"tests/java_touch").rglob("*.java"))])
        run([java,"-ea","-cp",work/"model","io.github.banjotooierecompiled.TouchControllerModelTest"])
        run([cxx,"-std=c++20","-O1","-g","-pthread","-fsanitize=address,undefined","-fno-omit-frame-pointer","-no-pie",
             "-I"+str(ROOT/"tests/android_touch_headers"),ROOT/"tests/android_touch_input.cpp",ROOT/"src/android_touch_input.cpp","-o",work/"native"])
        run([work/"native"],env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0",UBSAN_OPTIONS="halt_on_error=1"))
        run([*javac,"--release","17","-d",work/"jni",*sorted((ROOT/"tests/java_touch_jni").rglob("*.java"))])
        run([cxx,"-std=c++20","-O1","-g","-pthread","-fPIC","-shared",
             "-I"+str(jni),"-I"+str(jni/"linux"),"-I"+str(ROOT/"tests/android_touch_headers"),
             ROOT/"src/android_touch_jni.cpp",ROOT/"src/android_touch_input.cpp",ROOT/"tests/android_touch_jni.cpp",
             "-o",work/"libtooie_touch_jni.so"])
        result=run([java,"-ea","-Xcheck:jni","-Djava.library.path="+str(work),"-cp",work/"jni",
                    "io.github.banjotooierecompiled.TouchNativeTest"],text=True,capture_output=True)
        print(result.stdout,end="")
        if "WARNING in native method" in result.stdout+result.stderr:raise RuntimeError("JNI checker reported a warning: "+result.stderr)
        print("PASS: complete Android Activity/View Java compile; native physical-input passthrough and coherent touch-state checks")


if __name__=="__main__":main()
