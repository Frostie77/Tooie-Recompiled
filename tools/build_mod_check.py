#!/usr/bin/env python3
"""Build the ROM-free Tooie code-mod example with the vendored mod tool."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True,
                        help="New or empty output directory")
    parser.add_argument("--mod-tool", type=Path, help="Existing matching RecompModTool")
    parser.add_argument("--tool-build-dir", type=Path, help="Reuse a vendored CMake tool build")
    parser.add_argument("--clang", default="clang")
    parser.add_argument("--linker", default="ld.lld")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise SystemExit("STOP: output is not empty; choose a new output directory.")
    for name in (args.clang, args.linker, "zip"):
        if not shutil.which(name):
            raise SystemExit(f"STOP: required tool not found: {name}")
    output.mkdir(parents=True, exist_ok=True)
    commands = []

    def run(command, cwd=None):
        command = [str(value) for value in command]
        commands.append(command)
        subprocess.run(command, cwd=cwd, check=True, timeout=600)

    if args.mod_tool:
        mod_tool = args.mod_tool.resolve()
    else:
        build = args.tool_build_dir.resolve() if args.tool_build_dir else output / "tool-build"
        run(["cmake", "-S", root / "lib/N64ModernRuntime/N64Recomp", "-B", build,
             "-DCMAKE_BUILD_TYPE=Release"])
        run(["cmake", "--build", build, "--target", "RecompModTool", "-j", str(args.jobs)])
        mod_tool = build / "RecompModTool"
    if not mod_tool.is_file():
        raise SystemExit("STOP: RecompModTool was not produced.")

    work = output / "source"
    shutil.copytree(root / "examples/mods/tooie_mod_check", work)
    (work / "build").mkdir()
    run([args.clang, "-target", "mips", "-mips2", "-mabi=32", "-O2", "-G0",
         "-mno-abicalls", "-mno-odd-spreg", "-mno-check-zero-division",
         "-fomit-frame-pointer", "-fno-builtin", "-funsigned-char", "-ffunction-sections",
         "-fdata-sections", "-nostdinc", "-Iinclude", "-c", "mod.c", "-o", "build/mod.o"], work)
    run([args.linker, "-nostdlib", "-T", "mod.ld", "--unresolved-symbols=ignore-all",
         "--emit-relocs", "-e", "0", "--no-nmagic", "--gc-sections", "build/mod.o",
         "-o", "build/mod.elf"], work)
    run([mod_tool, work / "mod.toml", output], work)
    archive_path = output / "tooie_mod_check.nrm"
    with zipfile.ZipFile(archive_path) as archive:
        if archive.testzip() is not None:
            raise SystemExit("STOP: generated archive failed its CRC check.")
        manifest = json.loads(archive.read("mod.json"))
        if manifest["game_id"] != "bt" or manifest["id"] != "tooie_mod_check":
            raise SystemExit("STOP: unexpected generated mod identity.")
        symbols = archive.read("mod_syms.bin")
        if not symbols.startswith(b"N64RSYMS") or not archive.read("mod_binary.bin"):
            raise SystemExit("STOP: generated archive has no executable mod payload.")
    try:
        revision = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    except subprocess.CalledProcessError:
        revision = "unavailable"
    report = {"source_commit": revision, "source_may_be_modified": True,
              "game_id": "bt", "mod_id": "tooie_mod_check", "commands": commands,
              "mod_tool_sha256": hashlib.sha256(mod_tool.read_bytes()).hexdigest(),
              "archive_sha256": hashlib.sha256(archive_path.read_bytes()).hexdigest(),
              "archive_bytes": archive_path.stat().st_size,
              "source_files": {str(p.relative_to(work)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in sorted(work.rglob("*")) if p.is_file() and "build" not in p.relative_to(work).parts},
              "device_tested": False}
    (output / "tooie_mod_check-build-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"BUILT: {archive_path}\nSHA-256: {report['archive_sha256']}\nOdin execution is not yet verified.")


if __name__ == "__main__":
    main()
