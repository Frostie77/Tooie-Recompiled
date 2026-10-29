#!/usr/bin/env python3
"""Exercise the optional HUD observer through complete retained guest consumers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import tomllib


def read_consumers(root):
    hooks = [("func_800FA508", 0x800FA5CC, "tooie_hud_component_begin"),
             ("func_800FA508", 0x800FA5D4, "tooie_hud_component_end"),
             ("scinfobar_entrypoint_24", 0x808007D0, "tooie_hud_component_callback")]
    cfg = tomllib.loads((root / "banjotooie.us.toml").read_text())
    selected = [h for h in cfg["patches"]["hook"] if "tooie_hud_component_" in h["text"]]
    expected = ["tooie_hud_component_begin(rdram, ctx);\ntooie_hud_placement_begin(rdram, ctx);",
                "tooie_hud_placement_end(rdram, ctx);\ntooie_hud_component_end(rdram, ctx);",
                "tooie_hud_component_callback(rdram, ctx);"]
    assert [(h["func"], h["before_vram"], h["text"].strip()) for h in selected] == [
        (name, pc, text) for (name, pc, _), text in zip(hooks, expected)]
    fixture = json.loads((root / "tests/fixtures/hud-consumers-parent.json").read_text())
    bodies = {}
    hook_line = r"^    tooie_hud_(?:component|placement)_\w+\(rdram, ctx\);\n"
    for name, row in fixture["functions"].items():
        body = re.search(r"^RECOMP_FUNC void " + name + r"\(.*?(?=^RECOMP_FUNC |\Z)",
                         (root / row["file"]).read_text(), re.M | re.S).group()
        assert hashlib.sha256(re.sub(hook_line, "", body, flags=re.M).encode()).hexdigest() == row["sha256"], name
        bodies[name] = body
    for (name, pc, _), text in zip(hooks, expected):
        assert "    " + text.replace("\n", "\n    ") + f"\n    // 0x{pc:08X}:" in bodies[name]
    assert "${CMAKE_SOURCE_DIR}/src/hud_diagnostics.cpp" in (root / "CMakeLists.txt").read_text()
    print("PASS: maintained HUD loop/callback hooks and every retained instruction match the exact parent", flush=True)
    return hooks, bodies, hook_line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    if args.build_dir:
        args.build_dir.mkdir(parents=True, exist_ok=True)
    build = Path(tempfile.mkdtemp(prefix="tooie-hud-", dir=args.build_dir))
    env = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0", "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}

    def run(command, label, expected=0):
        result = subprocess.run([str(x) for x in command], cwd=root, env=env, capture_output=True, text=True, timeout=180)
        output = result.stdout + result.stderr
        (build / (label + ".log")).write_text(output)
        if result.returncode != expected or "runtime error:" in output or "ERROR: AddressSanitizer" in output:
            raise RuntimeError(f"{label}: exit {result.returncode}, expected {expected}\n{output[-6000:]}\nLogs: {build}")
        if expected and "hud diagnostic failure" not in output:
            raise RuntimeError(f"Missing specific negative-control assertion: {label}\n{output}")
        for line in output.splitlines():
            if line.startswith("PASS:"):
                print(line, flush=True)

    hooks, bodies, hook_line = read_consumers(root)
    source = re.sub(r"^    tooie_hud_placement_\w+\(rdram, ctx\);\n", "", "".join(bodies.values()), flags=re.M)
    parent = re.sub(hook_line, "", source, flags=re.M)
    for name in bodies:
        parent = re.sub(r"\b" + name + r"\b", "parent_" + name, parent)
    calls = set(re.findall(r"\b(\w+)\(rdram,\s*ctx\)", source + parent))
    header = '#include "tooie_recomp.h"\n' + "".join(
        "void " + name + "(uint8_t*,recomp_context*);\n" for name in sorted(calls) if not name.startswith("tooie_hud_component_"))
    flags = ["-O1", "-g1", "-fwrapv", "-fno-pie", "-fno-omit-frame-pointer", "-fsanitize=address,undefined",
             "-I" + str(root / "include"), "-I" + str(root / "lib/N64ModernRuntime/N64Recomp/include")]
    cc, cxx = os.environ.get("CC", "gcc"), os.environ.get("CXX", "g++")
    profiles = {"current": source}
    for _, _, bridge in hooks:
        profiles["missing-" + bridge.removeprefix("tooie_hud_component_")] = source.replace("    " + bridge + "(rdram, ctx);\n", "")
    premature = source.replace("    tooie_hud_component_end(rdram, ctx);\n", "")
    profiles["premature-end"] = premature.replace("    _scinfobar_entrypoint_24(rdram, ctx);", "    tooie_hud_component_end(rdram, ctx);\n    _scinfobar_entrypoint_24(rdram, ctx);")
    for label, body in profiles.items():
        generated = build / (label + ".c"); generated.write_text(header + body + parent)
        obj, binary = build / (label + ".o"), build / label
        run([cc, "-std=c11", "-D__ANDROID__", "-Werror=implicit-function-declaration", *flags, "-c", generated, "-o", obj], "compile-" + label)
        run([cxx, "-std=c++17", "-D__ANDROID__", *flags, root / "tests/hud_diagnostics.cpp", obj, "-no-pie", "-o", binary], "link-" + label)
        run([binary, build / (label + "-data")], "execute-" + label, expected=0 if label == "current" else 61)
    binary = build / "non-android"
    run([cxx, "-std=c++17", *flags, root / "tests/hud_diagnostics.cpp", build / "current.o", "-no-pie", "-o", binary], "link-non-android")
    run([binary, build / "non-android-data", "--off-only"], "execute-non-android")
    print("PASS: removing begin/callback/end or ending before icon/text output fails the expected consumer assertion; observer never assigns HUD anchors or edits drawing inputs", flush=True)
    print("Scope: controlled HUD layout/drawing services; actual Odin component identities still require the opt-in capture.", flush=True)


if __name__ == "__main__":
    main()
