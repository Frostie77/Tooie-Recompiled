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
    parser.add_argument("--sdl-include", type=Path, default=Path("/usr/include/SDL2"))
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
        ("func_808000B4_baeggaim", 0x80800100, "tooie_aiming_invert(rdram, ctx, MEM_W(0x28, ctx->r29));"),
        ("func_808002A4_bafpctrl", 0x80800320, "tooie_aiming_invert(rdram, ctx, ctx->r16);"),
        ("bafly_entrypoint_10", 0x80800590, "tooie_flight_invert(rdram, ctx, MEM_W(0x20, ctx->r29));"),
    ]
    cfg = tomllib.loads((root / "banjotooie.us.toml").read_text())
    selected = [h for h in cfg["patches"]["hook"] if re.search(r"tooie_(?:aiming|flight)_invert\(", h["text"])]
    assert [(h["func"], h["before_vram"], h["text"]) for h in selected] == hooks
    fixture = json.loads((root / "tests/fixtures/aiming-consumers-parent.json").read_text())
    bodies = {}
    hook_line = r"^    tooie_(?:aiming|flight)_invert\([^\n]*\);\n"
    def unhook(body):
        return re.sub(hook_line, "", body, flags=re.M)
    for name, row in fixture["functions"].items():
        source = (root / row["file"]).read_text()
        matches = re.findall(r"^RECOMP_FUNC void " + name + r"\(.*?(?=^RECOMP_FUNC |\Z)", source, re.M | re.S)
        assert len(matches) == 1, name
        body = matches[0]
        assert hashlib.sha256(unhook(body).encode()).hexdigest() == row["sha256"], name
        bodies[name] = body
    for name, pc, call in hooks:
        assert f"{call}\n    // 0x{pc:08X}:" in bodies[name]
    assert len(re.findall(hook_line, "".join(bodies.values()), flags=re.M)) == 3
    assert "tooie_aiming_invert(" not in bodies["func_80804848_bsfirstp"]
    # Permanent entry stubs used by the policy are authored dispatch identities.
    for file, prefix, first, count in [("funcs_3.c", "_bsbfly", 0x80082950, 10),
                                       ("funcs_6.c", "_bsfirstp", 0x80084288, 38)]:
        generated = (root / "RecompiledFuncs" / file).read_text()
        for index in range(count):
            body = re.search(r"RECOMP_FUNC void " + prefix + "_entrypoint_" + str(index) + r"\(.*?(?=RECOMP_FUNC |\Z)", generated, re.S).group()
            assert f"// 0x{first + index * 8:08X}:" in body
    main = (root / "src/main.cpp").read_text()
    assert main.index("create_general_tab(general_options)") < main.index("add_aiming_option(general_config)") < main.index("recompui::config::finalize()")
    assert ".on_init_callback =" in main and "tooie::input::initialize_controls(rdram);" in main
    assert ".get_input = tooie::input::read_controller_input," in main
    assert "tooie::input::read_controller_input(player, buttons, x, y)" in (root / "src/android_touch_input.cpp").read_text()
    assert "${CMAKE_SOURCE_DIR}/src/aiming_controls.cpp" in (root / "CMakeLists.txt").read_text()
    print("PASS: three maintained Y hooks, authored flight/FPS dispatch identities, and every retained instruction body match the exact parent", flush=True)

    source = "".join(bodies.values())
    parent_names = {name for name, _, _ in hooks} | {"func_80804848_bsfirstp"}
    parent = "".join(unhook(bodies[name]) for name in sorted(parent_names))
    for name in sorted(parent_names):
        parent = re.sub(r"\b" + name + r"\b", "parent_" + name, parent)
    all_source = source + parent
    calls = set(re.findall(r"\b(\w+)\(rdram,\s*ctx\)", all_source))
    names = set(re.findall(r"^RECOMP_FUNC void (\w+)\(", all_source, re.M))
    header = '#include "tooie_recomp.h"\n' + "".join(f"void {name}(uint8_t*,recomp_context*);\n" for name in sorted(calls | names))
    aiming_call = "    tooie_aiming_invert(rdram, ctx, ctx->r16);\n"
    flight_call = "    tooie_flight_invert(rdram, ctx, MEM_W(0x20, ctx->r29));\n"
    profiles = {
        "current": source,
        "missing-hooks": unhook(source),
        "rifle-inverted": source.replace("    // 0x8080491C:",
            "    tooie_flight_invert(rdram, ctx, MEM_W(0x40, ctx->r29));\n    // 0x8080491C:"),
        "walking-inverted": source.replace("    // 0x80804904:", "    ctx->f0.fl = -ctx->f0.fl;\n    // 0x80804904:"),
        "horizontal-inverted": source.replace("    // 0x80800344:", aiming_call + "    // 0x80800344:"),
        "flight-steering-missing": source.replace(flight_call, ""),
    }
    # Force the rifle mutation to invert regardless of context so the complete
    # unchanged FPS consumer must catch it, rather than relying on policy gates.
    profiles["rifle-inverted"] = profiles["rifle-inverted"].replace(
        "    tooie_flight_invert(rdram, ctx, MEM_W(0x40, ctx->r29));", "    ctx->f0.fl = -ctx->f0.fl;")
    runtime = root / "lib/N64ModernRuntime"
    (build / "miniz_export.h").write_text("#pragma once\n#define MINIZ_EXPORT\n")
    includes = [build, root / "src", root / "include", root / "lib/N64Recomp/include",
                runtime / "librecomp/include", runtime / "librecomp/include/librecomp",
                runtime / "N64Recomp/include", runtime / "ultramodern/include",
                runtime / "thirdparty", runtime / "thirdparty/concurrentqueue", runtime / "thirdparty/miniz",
                root / "lib/RecompFrontend/recompinput/include", root / "lib/RecompFrontend/lib/GamepadMotionHelpers",
                root / "lib/rt64/src/contrib", args.sdl_include]
    flags = ["-O1", "-g1", "-fwrapv", "-fno-pie", "-fno-omit-frame-pointer", "-fsanitize=address,undefined",
             "-ffunction-sections", "-fdata-sections", *["-I" + str(p) for p in includes]]
    input_state = (root / "lib/RecompFrontend/recompinput/src/input_state.cpp").read_text()
    reader_source = '#include <cmath>\n#include "recompinput/input_state.h"\nnamespace recompui::config::general { double get_joystick_deadzone(); }\n'
    for signature in ["float recompinput::get_input_analog(int controller_num, const std::span<const InputField> fields)",
                      "bool recompinput::get_input_digital(int controller_num, const std::span<const InputField> fields)",
                      "void recompinput::apply_joystick_deadzone(float x_in, float y_in, float* x_out, float* y_out)"]:
        start = input_state.index(signature)
        end = input_state.index("\n}", start) + 2
        reader_source += input_state[start:end] + "\n"
    readers = build / "input-readers.cpp"
    readers.write_text(reader_source)
    objects = []
    for index, path in enumerate([runtime / "librecomp/src/config.cpp", runtime / "librecomp/src/config_option.cpp",
                                  runtime / "librecomp/src/files.cpp", root / "src/aiming_controls.cpp", root / "tests/aiming_controls.cpp",
                                  root / "lib/RecompFrontend/recompinput/src/profiles.cpp", readers]):
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
    # The old broad preference inverted the airborne cursor while flight pitch
    # remained original. Replay that prior policy through the actual consumers.
    old_policy = re.sub(r'extern "C" void tooie_aiming_invert\(.*', '''extern "C" void tooie_aiming_invert(uint8_t*, recomp_context* ctx, uint32_t) {
    invert_sample(ctx, aiming_invert.load(std::memory_order_relaxed));
}
extern "C" void tooie_flight_invert(uint8_t*, recomp_context*, uint32_t) {}
''', aiming, flags=re.S)
    bad_policy = build / "prior-policy.cpp"
    bad_policy.write_text(old_policy)
    run([args.cxx, "-std=c++20", *flags, "-c", bad_policy, "-o", build / "prior-policy.o"], "compile-prior-policy")
    policy_objects = [obj for i, obj in enumerate(objects) if i != 3] + [build / "prior-policy.o", build / "current.o"]
    run([args.cxx, "-no-pie", "-fsanitize=address,undefined", "-Wl,--gc-sections", *policy_objects, "-pthread", "-o", build / "prior-policy"], "link-prior-policy")
    run([build / "prior-policy", build / "prior-policy-config"], "execute-prior-policy", expected=23)
    # Falsify a missing swap and the tempting one-way replacement that moves aim
    # to the right stick but leaves its former C-button role there too.
    mappings = (root / "lib/RecompFrontend/recompinput/src/profiles.cpp").read_text()
    for label, modified in [
        ("swap-missing", mappings.replace("if (swap_sticks) for", "if (false) for")),
        ("swap-one-way", mappings.replace("replacement = SDL_CONTROLLER_AXIS_LEFTX", "replacement = SDL_CONTROLLER_AXIS_RIGHTX")
                                 .replace("replacement = SDL_CONTROLLER_AXIS_LEFTY", "replacement = SDL_CONTROLLER_AXIS_RIGHTY")),
    ]:
        changed_profile = build / (label + ".cpp")
        changed_profile.write_text(modified.replace('#include "./json.h"', '#include "' + str(root / "lib/RecompFrontend/recompinput/src/json.h") + '"'))
        obj = build / (label + ".o")
        run([args.cxx, "-std=c++20", *flags, "-c", changed_profile, "-o", obj], "compile-" + label)
        linked = [item for i, item in enumerate(objects) if i != 5] + [obj, build / "current.o"]
        run([args.cxx, "-no-pie", "-fsanitize=address,undefined", "-Wl,--gc-sections", *linked, "-pthread", "-o", build / label], "link-" + label)
        run([build / label, build / (label + "-config")], "execute-" + label, expected=23)
    print("PASS: prior broad aiming policy, missing hooks/flight steering/swap, rifle/walking/horizontal inversion, one-way swap and broken load callback each fail the expected assertion", flush=True)


if __name__ == "__main__":
    main()
