#!/usr/bin/env python3
"""Temporarily enable build-110 scene inputs around the pinned Debug recorder.

The established signed APK must already be installed. This helper does not
install, launch, stop or send game input. Its only private app write is the
documented diagnostic request, removed after the recorder exits.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

SOURCE = "f37c1e2a10b76d9e75c7ded5468df5dd969cf486"
APK_HASH = "504e7320b6d1a371feab4dcebaac205303dd89dedb55d1dabc51474d09b99666"
CERT = "548bdb3c2702e830f61695c6d701da9c021a48af4b72566209fba0b518be2836"
PACKAGE = "io.github.banjotooierecompiled"
REQUEST = "files/data/render-trace-v1"
REQUEST_BYTES = b"tooie-render-trace-v1\n"
REQUEST_HASH = hashlib.sha256(REQUEST_BYTES).hexdigest()
TOOLS = {
    "capture_installed.py": "92985dff5ddb8e92784e1e36511b2d5ce4a6dc8d3f9015144a3930ee4d453981",
    "templates/collector.py": "b8ac627386c754d7b31657fb37976ae90c934dfd52b27c73cc7c4c8dead05f51",
    "tooie.py": "03b1004f4b0b07e4d31cfa428c408d3c13c2005cad54340372e6dfc316d32807",
}


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def device(args, *words, allow_missing=False):
    command = [str(args.adb), "-s", args.serial]
    if words[0] == "shell":
        # One remote shell command; run-as then gets an intact sh -c argument.
        command += ["shell", "-T", shlex.join(words[1:])]
    else:
        command += list(words)
    result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    if not (allow_missing and result.returncode == 1 and
            not result.stdout.strip() and not result.stderr.strip()):
        require(result.returncode == 0,
                "ADB command failed: " + (result.stderr or result.stdout).strip())
    return result.stdout.strip()


def verify_local(folder):
    apk = folder / ("Tooie-Touch-" + SOURCE[:12] + "-Local-Debug.apk")
    require(hashlib.sha256(apk.read_bytes()).hexdigest() == APK_HASH,
            "Local APK differs from the verified build-110 signed APK.")
    receipt = json.loads((folder / "touch-verification.json").read_text())
    report = json.loads((folder / "build-report.json").read_text())
    require(receipt["source_commit"] == report["source_commit"] == SOURCE and
            receipt["apk_sha256"] == APK_HASH and
            receipt["certificate_sha256"] == CERT and
            receipt["all_unsigned_payload_matches"] is True and
            receipt["package"] == report["package"] == PACKAGE,
            "The signing/build receipt differs from this diagnostic session.")
    capture = folder / "capture-tools"
    for relative, expected in TOOLS.items():
        require(hashlib.sha256((capture / relative).read_bytes()).hexdigest() == expected,
                "Pinned recorder changed: " + relative)
    return apk, capture


def verify_device(args):
    require(device(args, "get-state") == "device", "Expected Odin is unavailable.")
    require(device(args, "get-serialno") == args.serial, "Device serial differs.")
    installed = device(args, "shell", "pm", "path", PACKAGE).splitlines()
    require(len(installed) == 1 and re.fullmatch(
        r"package:/data/app/[A-Za-z0-9_./=+~\-]+/base\.apk", installed[0]),
        "Unexpected installed APK path.")
    measured = device(args, "shell", "sha256sum", installed[0][len("package:"):])
    require(measured.split() and measured.split()[0].lower() == APK_HASH,
            "Installed APK differs from the verified build-110 signed APK.")


def request_script(enable):
    shared = "set -eu\np=" + shlex.quote(REQUEST) + "\n"
    if enable:
        return shared + r'''
[ -d files/data ] || { printf '%s\n' 'Private data folder missing'; exit 44; }
if [ -e "$p" ] || [ -L "$p" ]; then
    printf '%s\n' 'A diagnostic request already exists; nothing overwritten'
    exit 45
fi
made=0
trap 'if [ "$made" = 1 ]; then rm -- "$p"; fi' 0
(umask 077; set -C; printf '%s\n' tooie-render-trace-v1 > "$p")
made=1
set -- $(sha256sum "$p")
[ "$1" = ''' + shlex.quote(REQUEST_HASH) + r''' ]
made=0
trap - 0
printf '%s\n' TOOIE_SCENE_REQUEST_ENABLED
'''
    return shared + r'''
[ ! -L "$p" ] || { printf '%s\n' 'Request is a link; nothing removed'; exit 46; }
if [ -e "$p" ]; then
    [ -f "$p" ] || exit 47
    set -- $(sha256sum "$p")
    [ "$1" = ''' + shlex.quote(REQUEST_HASH) + r''' ] || {
        printf '%s\n' 'Request contents differ; nothing removed'; exit 48;
    }
    rm -- "$p"
fi
[ ! -e "$p" ] && [ ! -L "$p" ]
printf '%s\n' TOOIE_SCENE_REQUEST_DISABLED
'''


def request(args, enable):
    expected = "TOOIE_SCENE_REQUEST_" + ("ENABLED" if enable else "DISABLED")
    actual = device(args, "shell", "run-as", PACKAGE, "sh", "-c", request_script(enable))
    require(actual == expected, "Diagnostic request readback was not confirmed.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--folder", type=Path, required=True)
    parser.add_argument("--adb", required=True)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--cleanup-only", action="store_true")
    args = parser.parse_args()
    require(re.fullmatch(r"[A-Za-z0-9_-]+", args.serial), "Invalid device serial.")
    apk, capture = verify_local(args.folder)
    verify_device(args)
    if args.cleanup_only:
        request(args, False)
        print("PASS: scene-input request is absent; new logging samples are disabled within five seconds.")
        return 0
    require(sys.stdin.isatty(), "Run this capture in the interactive Ubuntu terminal.")
    require(not device(args, "shell", "pidof", PACKAGE + ":game", allow_missing=True),
            "Close Debug first, then rerun this command. No app was stopped.")
    try:
        request(args, True)
    except (RuntimeError, OSError, subprocess.SubprocessError):
        print("Activation is not confirmed. If a request remains, reconnect Odin and use --cleanup-only.", file=sys.stderr)
        raise
    try:
        print("PASS: exact installed build-110 APK verified; temporary scene-input logging enabled.", flush=True)
        print("The request will be removed when you finish with save. Start Debug manually at the recorder prompt.", flush=True)
        command = [sys.executable, str(capture / "capture_installed.py"),
                   "--apk", str(apk), "--verification", str(args.folder / "touch-verification.json"),
                   "--build-report", str(args.folder / "build-report.json"),
                   "--build-tools", str(Path.home() / "Android/Sdk/build-tools/36.0.0"),
                   "--downloads", "/mnt/c/Users/Trent/Downloads",
                   "--expect-source", SOURCE, "--expect-sha256", APK_HASH, "--expect-cert", CERT,
                   "--adb", args.adb, "--serial", args.serial,
                   "--state", str(args.folder / "capture-state"), "--tool-revision", SOURCE]
        return subprocess.run(command, check=False).returncode
    finally:
        try:
            verify_device(args)
            request(args, False)
            print("PASS: temporary scene-input request removed; new logging samples stop within five seconds.", flush=True)
        except (RuntimeError, OSError, subprocess.SubprocessError) as error:
            print("STOP: diagnostic cleanup is not confirmed: " + str(error), file=sys.stderr)
            print("Reconnect Odin and rerun this helper with --cleanup-only.", file=sys.stderr)
            raise


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        raise SystemExit("STOP: " + str(error))
