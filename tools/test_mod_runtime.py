#!/usr/bin/env python3
"""Execute the example through real archive/config/mod/JIT/event consumers.

UI imports are recording doubles, not a claim of Android UI rendering.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

BASE = "26d41b13a8350182a0018578fbcbb150eee7470c"


def boot_consumer(source):
    start = source.index('extern "C" void tooie_boot_register_core2(')
    end = source.index("\n}", start) + 2
    constants = "\n".join(re.findall(r"^constexpr .* kCore2\w+ = .*;$", source, re.M))
    assert len(constants.splitlines()) == 3
    return '#include "mod_runtime.h"\n' + constants + "\n" + source[start:end] + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, help="Retain this new or existing test build")
    parser.add_argument("--mod-output", type=Path, help="New or empty directory for the example .nrm")
    parser.add_argument("--clang", default=shutil.which("clang") or shutil.which("clang-18"))
    parser.add_argument("--linker", default=shutil.which("ld.lld") or shutil.which("ld.lld-18"))
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if not args.clang or not args.linker:
        raise SystemExit("STOP: install a Clang MIPS compiler and LLD (Ubuntu: clang-18 lld-18).")
    root = Path(__file__).resolve().parents[1]
    temporary = None if args.build_dir else tempfile.TemporaryDirectory(prefix="tooie-mod-runtime-")
    build = args.build_dir.resolve() if args.build_dir else Path(temporary.name)
    build.mkdir(parents=True, exist_ok=True)
    records = Path(tempfile.mkdtemp(prefix="run-", dir=build))
    sequence = 0
    env = os.environ.copy()
    env.setdefault("ASAN_OPTIONS", "detect_leaks=0")
    env.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")

    def run(command, expected=0):
        nonlocal sequence
        sequence += 1
        result = subprocess.run([str(x) for x in command], cwd=root, env=env,
                                capture_output=True, text=True, timeout=600)
        output = result.stdout + result.stderr
        (records / f"check-{sequence:02d}.log").write_text(output)
        if result.returncode != expected or "runtime error:" in output or "ERROR: AddressSanitizer" in output:
            raise RuntimeError(f"Command failed ({result.returncode}): {command}\n{output[-10000:]}")
        for line in output.splitlines():
            if line.startswith(("PASS:", "BUILT:")):
                print(line, flush=True)
        return output

    main_source = (root / "src/main.cpp").read_text()
    assert main_source.index("tooie::register_mod_events();") < main_source.index("recomp::start(configuration)")
    api_source = (root / "src/recomp_api.cpp").read_text()
    boot = boot_consumer(api_source)
    assert boot.index("load_overlays(") < boot.index("tooie::dispatch_mod_init(")
    generated = (root / "RecompiledFuncs/funcs_0.c").read_text()
    assert generated.count("tooie_boot_register_core2(rdram, ctx);") == 1
    runtime = (root / "lib/N64ModernRuntime/librecomp/src/recomp.cpp").read_text()
    startup = runtime[runtime.index("bool wait_for_game_started("):]
    assert startup.index("mod_context->load_mods(") < startup.index("recomp::init_heap(") < startup.index("game_entry.entrypoint(")
    (build / "version.cpp").write_text('#include <array>\n#include <charconv>\n#include "librecomp/game.hpp"\n' +
        runtime[runtime.index("bool recomp::Version::from_string("):runtime.index("\nconst std::array<uint8_t, 4> first_rom_bytes")])
    boot_file = build / "boot-consumer.cpp"
    boot_file.write_text(boot)
    run(["cmake", "-S", root / "tests/mod_runtime", "-B", build,
         "-DCMAKE_BUILD_TYPE=Debug", "-DTOOIE_MOD_TEST_SANITIZERS=ON"])
    build_command = ["cmake", "--build", build, "--target", "mod_runtime", "RecompModTool", "-j", args.jobs]
    run(build_command)
    mod_output = args.mod_output.resolve() if args.mod_output else records / "example"
    run(["python3", root / "tools/build_mod_check.py", "--output", mod_output,
         "--mod-tool", build / "recomp/RecompModTool", "--clang", args.clang, "--linker", args.linker])
    mods = records / "mods"
    mods.mkdir(exist_ok=True)
    shutil.copyfile(mod_output / "tooie_mod_check.nrm", mods / "tooie_mod_check.nrm")
    test = build / "mod_runtime"
    run([test, mods, records / "state-current", "current"])
    run([test, mods, records / "state-no-event", "parent"])

    parent_command = ["git", "-C", root, "show", BASE + ":src/recomp_api.cpp"]
    parent_result = subprocess.run([str(x) for x in parent_command], capture_output=True, text=True)
    if parent_result.returncode:
        run(["git", "fetch", "--depth=1", "origin", BASE])
        parent_result = subprocess.run([str(x) for x in parent_command], capture_output=True, text=True, check=True)
    parent_boot = boot_consumer(parent_result.stdout)
    assert "dispatch_mod_init" not in parent_boot
    parent_main = subprocess.check_output(["git", "-C", str(root), "show", BASE + ":src/main.cpp"], text=True)
    assert "register_mod_events" not in parent_main
    try:
        boot_file.write_text(parent_boot)
        run(build_command)
        output = run([test, mods, records / "state-parent-boot", "current"], expected=2)
        assert "FAIL: exactly one visible badge when enabled" in output
    finally:
        boot_file.write_text(boot)
        run(build_command)
    print("PASS: exact parent core2 consumer fails execution assertion; current consumer dispatches after core mapping", flush=True)
    print("PASS: ASan/UBSan production mod loading and callback execution; actual rendering and ARM64 execution await Odin", flush=True)
    if temporary:
        temporary.cleanup()


if __name__ == "__main__":
    main()
