#!/usr/bin/env python3
"""Probe dialogue transition decisions in retained generated game functions.

The controller, spline evaluator, clamp/copy/rate helpers and guest memory ABI
are real code. Camera/object services are spies; spline time advancement and
overlay constants are controlled inputs. This is not a ROM or MD-002 replay.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile


FUNCTIONS = {
    "func_80800000_gcdialogcamera": "RecompiledFuncs/funcs_74.c",
    "ncpoddialog_entrypoint_4": "RecompiledFuncs/funcs_79.c",
    "ncpoddialog_entrypoint_9": "RecompiledFuncs/funcs_79.c",
    "func_808015CC_ncpoddialog": "RecompiledFuncs/funcs_79.c",
    "mlspline_entrypoint_4": "RecompiledFuncs/funcs_78.c",
    "func_800F10B4": "RecompiledFuncs/funcs_33.c",
    "func_800F0D50": "RecompiledFuncs/funcs_33.c",
    "func_800EE7F8": "RecompiledFuncs/funcs_32.c",
    "func_800D8FF8": "RecompiledFuncs/funcs_30.c",
}


def extract(root):
    cache = {}
    bodies = {}
    for name, file in FUNCTIONS.items():
        if file not in cache:
            cache[file] = (root / file).read_text()
        matches = re.findall(r"^RECOMP_FUNC void " + re.escape(name)
                             + r"\(.*?(?=^RECOMP_FUNC |\Z)", cache[file], re.M | re.S)
        if len(matches) != 1:
            raise RuntimeError("Missing or ambiguous generated function: " + name)
        bodies[name] = matches[0]
    return bodies


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    root = args.root.resolve()
    bodies = extract(root)
    controller = "func_808015CC_ncpoddialog"
    condition = "if (ctx->r14 != ctx->r1) {"
    dirty = "MEM_B(0X216, ctx->r4) = ctx->r12;"
    if bodies[controller].count(condition) != 1 or bodies["ncpoddialog_entrypoint_4"].count(dirty) != 1:
        raise RuntimeError("Transition/parameter-control anchors changed; review the probe")
    source = "".join(bodies.values())
    calls = set(re.findall(r"\b(\w+)\(rdram,\s*ctx\)", source))
    header = '#include "tooie_recomp.h"\n' + "".join(
        f"void {name}(uint8_t*, recomp_context*);\n" for name in sorted(calls | bodies.keys()))
    profiles = {
        "current": source,
        "force-gradual": source.replace(bodies[controller], bodies[controller].replace(condition, "if (1) {")),
        "force-instant": source.replace(bodies[controller], bodies[controller].replace(condition, "if (0) {")),
        "ignore-parameter-change": source.replace(dirty, "/* mutation: omit dirty notification */"),
    }
    environment = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0:abort_on_error=1",
                   "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}

    def run(command, expected=0, failure_case=None):
        result = subprocess.run([str(x) for x in command], cwd=root, env=environment,
                                capture_output=True, text=True, timeout=120)
        output = result.stdout + result.stderr
        if (result.returncode != expected or "runtime error:" in output
                or "ERROR: AddressSanitizer" in output
                or (failure_case and f"dialogue camera probe failure: {failure_case}:" not in output)):
            raise RuntimeError(f"exit {result.returncode}, expected {expected}: {command[0]}\n{output[-10000:]}")
        if expected == 0:
            for line in output.splitlines():
                if line.startswith("PASS:"):
                    print(line, flush=True)

    with tempfile.TemporaryDirectory(prefix="tooie-dialogue-camera-") as temporary:
        build = Path(temporary)
        includes = [root / "include", root / "src",
                    root / "lib/N64ModernRuntime/N64Recomp/include"]
        flags = ["-O1", "-g1", "-fwrapv", "-fno-strict-aliasing", "-fno-pie",
                 "-fno-omit-frame-pointer", "-fsanitize=address,undefined",
                 *["-I" + str(p) for p in includes]]
        harness = build / "harness.o"
        run([args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", *flags,
             "-c", root / "tests/dialogue_camera_probe.cpp", "-o", harness])
        failures = {"force-gradual": "instant",
                    "force-instant": "gradual-0",
                    "ignore-parameter-change": "changed-parameters"}
        for profile, body in profiles.items():
            generated = build / (profile + ".c")
            generated.write_text(header + body)
            obj = build / (profile + ".o")
            exe = build / profile
            run([args.cc, "-std=c11", "-Werror=implicit-function-declaration", *flags,
                 "-c", generated, "-o", obj])
            run([args.cxx, "-no-pie", "-fsanitize=address,undefined", harness, obj, "-o", exe])
            run([exe], expected=0 if profile == "current" else 23,
                failure_case=failures.get(profile))
    print("PASS: forced-instant, forced-gradual and missing-parameter-notification controls fail at their expected cases.", flush=True)
    fingerprint = hashlib.sha256(source.encode()).hexdigest()
    print(f"PASS: {len(bodies)} retained generated functions exercised; extracted-source SHA256 {fingerprint}.", flush=True)
    print("Scope: controlled guest states and constants; time advancement/object services are spies. MD-002 cause remains unconfirmed.", flush=True)


if __name__ == "__main__":
    main()
