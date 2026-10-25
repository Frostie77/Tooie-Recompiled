#!/usr/bin/env python3
"""Verify a CI touch Debug ZIP, sign locally, and prepare a pinned capture launcher.

This helper never installs, launches, uninstalls or clears an app. The key stays
on the maintainer's PC. Only the existing Debug package/version is accepted.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import uuid
import zipfile

DEBUG_CERT="548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836"
TOOL_REVISION="f5cb8aa4185dc752095df9c33c71789264ea1401"


def require(ok,message):
    if not ok:raise ValueError(message)


def digest(data):return hashlib.sha256(data).hexdigest()


def payload(data):
    with zipfile.ZipFile(io.BytesIO(data)) as bundle:
        names=bundle.namelist();require(len(names)==len(set(names)),"Duplicate APK entries")
        signature=r"META-INF/(MANIFEST\.MF|[^/]+\.(SF|RSA|DSA|EC)|SIG-[^/]+)"
        return {n:digest(bundle.read(n)) for n in names if not n.endswith("/") and not re.fullmatch(signature,n,re.I)}


def load_candidate(path,expected_zip,source):
    require(re.fullmatch(r"[0-9a-f]{64}",expected_zip),"Invalid expected ZIP hash")
    require(re.fullmatch(r"[0-9a-f]{40}",source),"Invalid expected source")
    require(path.stat().st_size<=100*1024*1024,"Candidate ZIP exceeds size limit")
    archive=path.read_bytes();require(digest(archive)==expected_zip,"Candidate ZIP SHA-256 mismatch")
    with zipfile.ZipFile(io.BytesIO(archive)) as bundle:
        names=bundle.namelist();require(names and len(names)==len(set(names)),"Empty or duplicate candidate entries")
        require(all(n and Path(n).name==n and not any(c in n for c in ("/","\\",":")) and n not in (".","..") for n in names),"Unsafe candidate paths")
        require(sum(i.file_size for i in bundle.infolist())<=100*1024*1024,"Unexpected candidate size")
        report=json.loads(bundle.read("build-report.json"))
        require(report["source_commit"]==source and report["package"]=="io.github.banjotooierecompiled" and
                report["variant"]=="Touch-Debug-Candidate","Wrong source or candidate app identity")
        expected="Tooie-Recompiled-Android-arm64-"+source[:12]+"-Touch-Debug-Candidate.apk"
        require(report["apk"]==expected,"Unexpected candidate APK filename")
        apk=bundle.read(expected);require(digest(apk)==report["sha256"],"Candidate APK digest differs from build report")
        with zipfile.ZipFile(io.BytesIO(apk)) as app:
            require(len(app.namelist())==len(set(app.namelist())),"Duplicate APK entries")
            libs={n for n in app.namelist() if n.startswith("lib/") and n.endswith(".so")}
            require(libs=={"lib/arm64-v8a/libmain.so","lib/arm64-v8a/libSDL2.so"},"Wrong native library set")
            require(set(report["libraries"])=={"libmain.so","libSDL2.so"},"Incomplete native library report")
            for name,info in report["libraries"].items():require(digest(app.read("lib/arm64-v8a/"+name))==info["sha256"],"Native library differs: "+name)
            require(report["native_version"].encode() in app.read("lib/arm64-v8a/libmain.so"),"Native version stamp missing")
        tools={}
        require(set(report["capture_tools"])=={"capture_installed.py","collector.py","tooie.py"},"Capture tools are missing")
        for name,sha in report["capture_tools"].items():
            tools[name]=bundle.read(name);require(digest(tools[name])==sha,"Capture tool digest differs: "+name)
        return report,apk,tools


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zip",type=Path,required=True)
    parser.add_argument("--expect-zip-sha256",required=True)
    parser.add_argument("--expect-source",required=True)
    parser.add_argument("--expect-cert",default=DEBUG_CERT)
    parser.add_argument("--keystore",type=Path,default=Path.home()/".android/debug.keystore")
    parser.add_argument("--build-tools",type=Path,default=Path.home()/"Android/Sdk/build-tools/36.0.0")
    parser.add_argument("--output-root",type=Path,default=Path.home()/".local/share/tooie-recompiled/candidates")
    parser.add_argument("--downloads",type=Path,default=Path.home()/"Downloads")
    parser.add_argument("--adb",default="adb")
    parser.add_argument("--serial",required=True)
    args=parser.parse_args(argv)
    require(re.fullmatch(r"[0-9a-f]{64}",args.expect_cert),"Invalid expected Debug certificate")
    report,original,tools=load_candidate(args.zip,args.expect_zip_sha256,args.expect_source)
    # Export only the public certificate, before creating any signed output.
    cert=subprocess.check_output(["keytool","-exportcert","-keystore",str(args.keystore),"-storepass","android","-alias","androiddebugkey"],timeout=60)
    require(digest(cert)==args.expect_cert,"Local keystore is not the expected Debug signer")
    folder=args.output_root/("touch-"+args.expect_source[:12]+"-"+uuid.uuid4().hex[:8]);folder.mkdir(parents=True,mode=0o700)
    unsigned=folder/report["apk"];unsigned.write_bytes(original)
    (folder/"build-report.json").write_text(json.dumps(report,indent=2)+"\n")
    signed=folder/("Tooie-Touch-"+args.expect_source[:12]+"-Local-Debug.apk")
    subprocess.run([str(args.build_tools/"apksigner"),"sign","--ks",str(args.keystore),"--ks-key-alias","androiddebugkey",
                    "--ks-pass","pass:android","--key-pass","pass:android","--v4-signing-enabled","false","--out",str(signed),str(unsigned)],check=True,timeout=180)
    data=signed.read_bytes();require(payload(original)==payload(data),"Signed APK payload differs from the verified CI build")
    verified=subprocess.check_output([str(args.build_tools/"apksigner"),"verify","--verbose","--print-certs",str(signed)],text=True,timeout=180)
    certs=re.findall(r"^Signer #\d+ certificate SHA-256 digest: ([0-9a-fA-F:]+)\s*$",verified,re.M)
    require([c.replace(":","").lower() for c in certs]==[args.expect_cert],"Signed APK certificate differs")
    subprocess.run([str(args.build_tools/"zipalign"),"-c","4",str(signed)],check=True,timeout=60)
    badging=subprocess.check_output([str(args.build_tools/"aapt2"),"dump","badging",str(signed)],text=True,timeout=60)
    identity=re.search(r"^package: name='([^']+)' versionCode='(\d+)' versionName='([^']+)'",badging,re.M)
    require(identity and identity.groups()==("io.github.banjotooierecompiled","1","0.1.0-android") and "application-debuggable" in badging,"Wrong Debug package/version")
    receipt={"source_commit":args.expect_source,"apk_sha256":digest(data),"certificate_sha256":args.expect_cert,
             "package":identity[1],"version_code":1,"version_name":identity[3],"native_version":report["native_version"],
             "all_unsigned_payload_matches":True,"install_performed":False,"device_data_preservation_tested":False}
    verification=folder/"touch-verification.json";verification.write_text(json.dumps(receipt,indent=2)+"\n")
    capture=folder/"capture-tools";(capture/"templates").mkdir(parents=True)
    expected_tools={}
    for name,content in tools.items():
        path=capture/("templates/collector.py" if name=="collector.py" else name)
        path.write_bytes(content);path.chmod(0o400);expected_tools[str(path)]=digest(content)
    command=[sys.executable,str(capture/"capture_installed.py"),"--apk",str(signed),"--verification",str(verification),
             "--build-report",str(folder/"build-report.json"),"--build-tools",str(args.build_tools),"--downloads",str(args.downloads),
             "--expect-source",args.expect_source,"--expect-sha256",receipt["apk_sha256"],"--expect-cert",args.expect_cert,
             "--adb",args.adb,"--serial",args.serial,"--state",str(folder/"capture-state"),"--tool-revision",TOOL_REVISION]
    check="from pathlib import Path; import hashlib,sys; expected="+repr(expected_tools)+"; changed=any(hashlib.sha256(Path(p).read_bytes()).hexdigest()!=h for p,h in expected.items()); sys.exit('Capture tools changed; stop and reverify' if changed else 0)"
    launcher=folder/"capture-touch.sh";launcher.write_text("#!/bin/sh\n"+shlex.join([sys.executable,"-c",check])+" || exit 1\nexec "+shlex.join(command)+"\n");launcher.chmod(0o700)
    require(signed.read_bytes()==data,"Signed APK changed during verification")
    print("PASS: established Debug certificate, APK signature/alignment/version and complete CI payload verified.")
    print("APK:",signed);print("SHA256:",receipt["apk_sha256"])
    print("After the authorized adb install -r update, start capture with:",shlex.quote(str(launcher)))
    print("No installation or device-data changes were performed by this helper.")


if __name__=="__main__":
    try:main()
    except (ValueError,OSError,KeyError,subprocess.SubprocessError,zipfile.BadZipFile) as error:raise SystemExit("STOP: "+str(error))
