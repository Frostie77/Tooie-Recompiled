#!/usr/bin/env python3
"""Exercise production Android update selection, HTTP controls, JNI and notice policy.

No ROM, device or live update endpoint is used. The real Activity/SDL Java is
also compiled against the Android SDK. The pinned JSON jar is host-test-only;
Android supplies org.json, so the app gains no dependency.
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import urllib.request
from pathlib import Path

JSON_SHA256 = "3cf6cd6892e32e2b4c1c39e0f52f5248a2f5b37646fdfbb79a66b46b618414ed"
JSON_URL = "https://repo.maven.apache.org/maven2/org/json/json/20240303/json-20240303.jar"


def run(command):
    subprocess.run([str(value) for value in command], check=True)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--android-jar", type=Path, default=Path(os.environ.get("ANDROID_HOME", "/missing-sdk")) / "platforms/android-36/android.jar")
    parser.add_argument("--jni-include", type=Path)
    parser.add_argument("--json-jar", type=Path)
    parser.add_argument("--build-dir", type=Path, default=root / ".ci-android/update-tests")
    args = parser.parse_args()
    if not args.android_jar.is_file():
        parser.error("Provide --android-jar from the Android API 36 SDK")
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    jar = args.json_jar or build / "json-20240303.jar"
    if not jar.is_file():
        with urllib.request.urlopen(JSON_URL, timeout=30) as response:
            content = response.read(1024 * 1024)
        if hashlib.sha256(content).hexdigest() != JSON_SHA256:
            raise ValueError("Host JSON dependency hash differs")
        jar.write_bytes(content)
    if hashlib.sha256(jar.read_bytes()).hexdigest() != JSON_SHA256:
        raise ValueError("Host JSON dependency hash differs")
    java = Path(shutil.which("java") or "java").resolve()
    compiler = shutil.which("javac")
    if compiler:
        javac = [compiler]
    else:
        helper = build / "CompileJava.java"
        helper.write_text('import javax.tools.ToolProvider; public class CompileJava { public static void main(String[] a) { var c=ToolProvider.getSystemJavaCompiler(); if(c==null) throw new IllegalStateException("JDK compiler unavailable"); System.exit(c.run(null,System.out,System.err,a)); }}')
        javac = [java, "--source", "17", helper]
    source = root / "android/app/src/main/java"
    helpers = source / "io/github/banjotooierecompiled"
    tests = root / "tests/java_updates/io/github/banjotooierecompiled"
    # Compile against Android's checked JSONException/API surface; execute the
    # same bytecode against real org.json instead of the SDK's stub methods.
    run([*javac, "--release", "17", "-cp", args.android_jar, "-d", build / "classes",
         *[helpers / name for name in ("UpdateManifest.java", "UpdateClient.java", "UpdateHttpTransport.java")],
         *sorted(tests.glob("*.java"))])
    for name in ("UpdateClientTest", "UpdateTransportTest"):
        run([java, "-ea", "-cp", str(build / "classes") + os.pathsep + str(jar),
             "io.github.banjotooierecompiled." + name, root / "updates/android-stable.json"])
    jni = args.jni_include or Path(os.environ.get("JAVA_HOME", java.parent.parent)) / "include"
    if not (jni / "jni.h").is_file():
        parser.error("JNI development headers missing; provide --jni-include from JDK 17")
    cxx = os.environ.get("CXX", "g++")
    run([cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-fPIC", "-shared", "-D__ANDROID__",
         "-I" + str(jni), "-I" + str(jni / "linux"), "-I" + str(root / "src"),
         "-I" + str(root / "tests/android_picker_headers"), root / "src/android_updates_jni.cpp",
         root / "tests/android_updates_jni.cpp", "-o", build / "libtooie_updates_test.so"])
    jni_result = subprocess.run([str(java), "-ea", "-Xcheck:jni", "-Djava.library.path=" + str(build),
        "-cp", str(build / "classes"), "io.github.banjotooierecompiled.UpdateJniTest"], text=True, capture_output=True)
    print(jni_result.stdout, end="")
    if jni_result.returncode or "WARNING in native method" in jni_result.stdout + jni_result.stderr:
        raise RuntimeError("JNI regression/warning: " + jni_result.stderr)
    run([cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-I" + str(root / "src"),
         root / "tests/android_update_notice.cpp", "-o", build / "notice-policy"])
    run([build / "notice-policy"])
    print("PASS: automatic notices defer during gameplay, other modals and lost focus; Later suppresses duplicates for that version.")
    run([*javac, "--release", "17", "-cp", args.android_jar, "-d", build / "android-classes", *sorted(source.rglob("*.java"))])
    print("PASS: real Activity, update/cache adapters, import helpers and SDL Java compile against Android SDK 36.")


if __name__ == "__main__":
    main()
