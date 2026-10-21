#!/usr/bin/env python3
"""Run actual transform-upload and staging-copy methods with an in-memory API.

This checks CPU source selection and byte copies, not GPU rendering or ownership.
An optional exact-parent source file can be supplied without fetching any data.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def method(source, signature):
    start = source.rfind("\n", 0, source.index(signature)) + 1
    brace = source.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-source", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source_path = root / "lib/rt64/src/render/rt64_transform_processor.cpp"
    source = source_path.read_text()
    upload = method(source, "void TransformProcessor::upload(")
    staging = (root / "lib/rt64/src/render/rt64_buffer_uploader.cpp").read_text()
    copies = "\n".join(method(staging, signature) for signature in (
        "bool BufferUploader::Upload::valid(", "void BufferUploader::threadUpload("))
    compiler = shlex.split(os.environ.get("CXX", "g++"))
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",
               UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    with tempfile.TemporaryDirectory(prefix="tooie-transform-upload-") as directory:
        temp = Path(directory)

        def build(body, name):
            (temp / "production-upload.inc").write_text(body + "\n" + copies)
            binary = temp / name
            subprocess.run([*compiler, "-std=c++17", "-O1", "-g", "-fsanitize=address,undefined",
                            "-fno-omit-frame-pointer", "-fno-pie", "-no-pie", "-I", str(temp),
                            str(root / "tests/transform_upload.cpp"), "-o", str(binary)], check=True)
            return binary

        binary = build(upload, "current")
        subprocess.run([str(binary)], env=env, check=True)
        old_guard = "const bool prevFrameValid = (p.prevFrame != nullptr);"
        guard = re.compile(r"const bool prevFrameValid = .*?;")
        assert len(guard.findall(upload)) == 1
        controls = [("history-only", guard.sub(old_guard, upload)),
                    ("always-current", guard.sub("const bool prevFrameValid = false;", upload))]
        if args.baseline_source:
            controls.append(("exact-parent", method(args.baseline_source.read_text(),
                                                    "void TransformProcessor::upload(")))
        for name, body in controls:
            control = build(body, name)
            result = subprocess.run([str(control)], env=env, capture_output=True, text=True)
            expected = ("FAIL: matched-history: interpolated world bytes" if name == "always-current"
                        else "FAIL: new-workload: current world transform bytes")
            assert result.returncode == 1 and expected in result.stderr, (name, result.returncode, result.stderr)
            if name != "always-current":
                growth = subprocess.run([str(control), "growth"], env=env, capture_output=True, text=True)
                assert growth.returncode != 0 and "AddressSanitizer: heap-buffer-overflow" in growth.stderr, growth.stderr
            print(f"PASS: {name} fails the independent byte check" +
                  (" and grown-source ASan control" if name != "always-current" else ""), flush=True)


if __name__ == "__main__":
    main()
