#!/usr/bin/env python3
"""Test the saved setting and complete retained guest input consumers, no ROM/device."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import tomllib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    root = args.root.resolve()
    if args.build_dir:
        args.build_dir.mkdir(parents=True, exist_ok=True)
    build = Path(tempfile.mkdtemp(prefix="tooie-aiming-", dir=args.build_dir))
    environment = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0", "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}

    def run(command, label, expected=0):
        result = subprocess.run([str(v) for v in command], cwd=root, env=environment,
                                capture_output=True, text=True, timeout=180)
        output = result.stdout + result.stderr
        (build / (label + ".log")).write_text(output)
        if result.returncode != expected or "runtime error:" in output or "ERROR: AddressSanitizer" in output:
            raise RuntimeError(f"{label}: exit {result.returncode}, expected {expected}\n{output[-10000:]}\nLogs: {build}")
        if expected and "aiming failure line" not in output:
            raise RuntimeError(f"{label}: missing specific negative-control assertion\n{output}\nLogs: {build}")
        for line in output.splitlines():
            if line.startswith("PASS:"):
                print(line, flush=True)

    hooks = [
        ("func_808000B4_baeggaim", 0x80800100),
        ("func_808002A4_bafpctrl", 0x80800320),
        ("func_80804848_bsfirstp", 0x8080491C),
        ("func_80804848_bsfirstp", 0x808049D4),
    ]
    cfg = tomllib.loads((root / "banjotooie.us.toml").read_text())
    selected = [h for h in cfg["patches"]["hook"] if "tooie_aiming_invert(" in h["text"]]
    assert [(h["func"], h["before_vram"]) for h in selected] == hooks
    fixture = json.loads((root / "tests/fixtures/aiming-consumers-parent.json").read_text())
    bodies = {}
    call = "    tooie_aiming_invert(rdram, ctx);\n"
    for name, row in fixture["functions"].items():
        source = (root / row["file"]).read_text()
        matches = re.findall(r"^RECOMP_FUNC void " + name + r"\(.*?(?=^RECOMP_FUNC |\Z)", source, re.M | re.S)
        assert len(matches) == 1, name
        body = matches[0]
        assert hashlib.sha256(body.replace(call, "").encode()).hexdigest() == row["sha256"], name
        bodies[name] = body
    for name, pc in hooks:
        assert f"tooie_aiming_invert(rdram, ctx);\n    // 0x{pc:08X}:" in bodies[name]
    assert sum(body.count(call) for body in bodies.values()) == 4
    assert call + "    // 0x80804904:" not in bodies["func_80804848_bsfirstp"]
    main = (root / "src/main.cpp").read_text()
    assert main.index("create_general_tab(general_options)") < main.index("add_aiming_option(general_config)") < main.index("recompui::config::finalize()")
    assert "${CMAKE_SOURCE_DIR}/src/aiming_controls.cpp" in (root / "CMakeLists.txt").read_text()
    print("PASS: four maintained generator hooks at verified Y consumers; walking Y/getters and all retained instruction bodies match the exact parent", flush=True)

    source = "".join(bodies.values())
    parent_names = {name for name, _ in hooks}
    parent = "".join(bodies[name].replace(call, "") for name in sorted(parent_names))
    for name in sorted(parent_names):
        parent = re.sub(r"\b" + name + r"\b", "parent_" + name, parent)
    all_source = source + parent
    calls = set(re.findall(r"\b(\w+)\(rdram,\s*ctx\)", all_source))
    names = set(re.findall(r"^RECOMP_FUNC void (\w+)\(", all_source, re.M))
    header = '#include "tooie_recomp.h"\n' + "".join(f"void {name}(uint8_t*,recomp_context*);\n" for name in sorted(calls | names))
    profiles = {
        "current": source,
        "missing-hooks": source.replace(call, ""),
        "walking-inverted": source.replace("    // 0x80804904:", call + "    // 0x80804904:"),
        "horizontal-inverted": source.replace("    // 0x80800344:", call + "    // 0x80800344:"),
    }
    runtime = root / "lib/N64ModernRuntime"
    (build / "miniz_export.h").write_text("#pragma once\n#define MINIZ_EXPORT\n")
    includes = [build, root / "src", root / "include", root / "lib/N64Recomp/include",
                runtime / "librecomp/include", runtime / "librecomp/include/librecomp",
                runtime / "N64Recomp/include", runtime / "ultramodern/include",
                runtime / "thirdparty", runtime / "thirdparty/concurrentqueue", runtime / "thirdparty/miniz"]
    flags = ["-O1", "-g1", "-fwrapv", "-fno-pie", "-fno-omit-frame-pointer", "-fsanitize=address,undefined",
             "-ffunction-sections", "-fdata-sections", *["-I" + str(p) for p in includes]]
    objects = []
    for index, path in enumerate([runtime / "librecomp/src/config.cpp", runtime / "librecomp/src/config_option.cpp",
                                  runtime / "librecomp/src/files.cpp", root / "src/aiming_controls.cpp", root / "tests/aiming_controls.cpp"]):
        target = build / f"common-{index}.o"
        run([args.cxx, "-std=c++20", *flags, "-c", path, "-o", target], f"compile-common-{index}")
        objects.append(target)
    for profile, body in profiles.items():
        generated = build / (profile + ".c"); generated.write_text(header + body + parent)
        target = build / (profile + ".o")
        run([args.cc, "-std=c11", "-D__ANDROID__", "-Werror=implicit-function-declaration", *flags, "-c", generated, "-o", target], "compile-" + profile)
        binary = build / profile
        run([args.cxx, "-no-pie", "-fsanitize=address,undefined", "-Wl,--gc-sections", *objects, target, "-pthread", "-o", binary], "link-" + profile)
        run([binary, build / (profile + "-config")], "execute-" + profile, expected=0 if profile == "current" else 23)
    # Config can read true successfully yet fail to publish it to the input
    # thread. This control specifically disables the production load callback.
    aiming = (root / "src/aiming_controls.cpp").read_text()
    before = "context != recomp::config::OptionChangeContext::Temporary"
    assert aiming.count(before) == 1
    bad = build / "load-callback.cpp"
    bad.write_text(aiming.replace(before, before + " && context != recomp::config::OptionChangeContext::Load"))
    run([args.cxx, "-std=c++20", *flags, "-c", bad, "-o", build / "bad-load.o"], "compile-load-control")
    control_objects = [obj for i, obj in enumerate(objects) if i != 3] + [build / "bad-load.o", build / "current.o"]
    run([args.cxx, "-no-pie", "-fsanitize=address,undefined", "-Wl,--gc-sections", *control_objects, "-pthread", "-o", build / "bad-load"], "link-load-control")
    run([build / "bad-load", build / "bad-load-config"], "execute-load-control", expected=23)
    print("PASS: missing-hook, first-person walking, horizontal-axis and config-load mutation controls fail for the expected assertions", flush=True)


if __name__ == "__main__":
    main()
