#!/usr/bin/env python3
"""Real driver ZIP/store Java and native-consumer tests; no device or system driver changes."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import urllib.request
import warnings
import zipfile

ROOT=Path(__file__).resolve().parents[1]
JSON_URL="https://repo.maven.apache.org/maven2/org/json/json/20240303/json-20240303.jar"
JSON_SHA="3cf6cd6892e32e2b4c1c39e0f52f5248a2f5b37646fdfbb79a66b46b618414ed"

def fixtures(root):
    root.mkdir()
    elf=bytearray(64);elf[:6]=b"\x7fELF\x02\x01";struct.pack_into("<HH",elf,16,3,183)
    metadata={"schemaVersion":1,"name":"Mesa Turnip fixture","vendor":"mesa","driverVersion":"fixture","minApi":27,"libraryName":"vulkan.adreno.so"}
    def package(name,changes=None,entries=None,raw_meta=None):
        data={"meta.json":raw_meta if raw_meta is not None else json.dumps(metadata| (changes or {})),"vulkan.adreno.so":bytes(elf)}
        data.update(entries or {})
        with zipfile.ZipFile(root/name,"w",zipfile.ZIP_DEFLATED) as bundle:
            for key,value in data.items():bundle.writestr(key,value)
    package("good.zip")
    for name,changes in {
        "api":{"minApi":99},"float-api":{"minApi":27.0},"schema":{"schemaVersion":2},
        "bool-schema":{"schemaVersion":True},"vendor":{"vendor":"Qualcomm"},"name":{"name":"different"},
        "library":{"libraryName":"../escape.so"},"missing-library":{"libraryName":"missing.so"}
    }.items():package("bad-"+name+".zip",changes)
    for name,entries in {"path":{"../escape.so":bytes(elf)},"nested":{"sub/library.so":bytes(elf)},
                         "x86":{"vulkan.adreno.so":bytes(elf[:18])+b"\x3e\0"+bytes(elf[20:])},
                         "elf":{"vulkan.adreno.so":b"not a library"},"duplicate-dot":{"other..so":bytes(elf)}}.items():
        package("bad-"+name+".zip",entries=entries)
    for name,text in {"duplicate-meta":json.dumps(metadata)[:-1]+',"minApi":27}',
                      "deep":'{"nested":'+"["*40+"0"+"]"*40+"}","metadata-size":" "*16385,
                      "trailing":json.dumps(metadata)+" trailing"}.items():package("bad-"+name+".zip",raw_meta=text)
    (root/"bad-not-zip.zip").write_bytes(b"not a ZIP")
    for kind in ("link","encrypted","declared-size","duplicate-entry","crc"):
        package("bad-"+kind+".zip")
        p=root/("bad-"+kind+".zip")
        if kind=="duplicate-entry":
            with warnings.catch_warnings(),zipfile.ZipFile(p,"a") as bundle:
                warnings.simplefilter("ignore",UserWarning);bundle.writestr("meta.json",json.dumps(metadata))
            continue
        data=bytearray(p.read_bytes());cursor=data.find(b"PK\x01\x02")
        if kind=="link":struct.pack_into("I",data,cursor+38,0o120777<<16)
        elif kind=="encrypted":struct.pack_into("H",data,cursor+8,1)
        elif kind=="declared-size":struct.pack_into("I",data,cursor+24,128*1024*1024+1)
        elif kind=="crc":struct.pack_into("I",data,cursor+16,123)
        p.write_bytes(data)

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--android-jar",type=Path,required=True);parser.add_argument("--json-jar",type=Path)
    args=parser.parse_args()
    java=Path(shutil.which("java")).resolve();javac=[shutil.which("javac")] if shutil.which("javac") else [str(java),"com.sun.tools.javac.Main"]
    def run(cmd,**kw):return subprocess.run([str(x) for x in cmd],check=True,**kw)
    with tempfile.TemporaryDirectory(prefix="tooie-driver-picker-") as temp:
        area=Path(temp);jar=args.json_jar or area/"json.jar"
        if not jar.is_file():
            with urllib.request.urlopen(JSON_URL,timeout=30) as response:data=response.read(1024*1024)
            if hashlib.sha256(data).hexdigest()!=JSON_SHA:raise ValueError("Pinned host JSON library differs")
            jar.write_bytes(data)
        if hashlib.sha256(jar.read_bytes()).hexdigest()!=JSON_SHA:raise ValueError("Pinned host JSON library differs")
        source=ROOT/"android/app/src/main/java/io/github/banjotooierecompiled"
        run([*javac,"--release","17","-cp",args.android_jar,"-d",area/"classes",*[source/n for n in ("DriverJson.java","DriverPackage.java","DriverStore.java")],
             *sorted((ROOT/"tests/java_driver_picker").rglob("*.java"))])
        fixtures(area/"fixtures")
        result=run([java,"-ea","-cp",str(area/"classes")+os.pathsep+str(jar),"io.github.banjotooierecompiled.DriverStoreTest",area/"fixtures",area/"files"],capture_output=True,text=True)
        print(result.stdout,end="")
        run([os.environ.get("CXX","g++"),"-std=c++20","-O1","-g","-fsanitize=address,undefined","-fno-omit-frame-pointer","-no-pie",ROOT/"tests/android_driver_picker_selection.cpp","-o",area/"consumer"])
        run([area/"consumer",area/"files/vulkan-driver"],env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0","UBSAN_OPTIONS":"halt_on_error=1"})

if __name__=="__main__":main()
