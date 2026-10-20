#!/usr/bin/env python3
"""Reproduce two existing Android mod-import gaps without running the game.

Targets the cancellation stub present at 5db2d7f. This is an investigation
reproducer, not a claim of working mod installation or an Android UI test.
"""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def run(command, expected=0):
    result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    if result.returncode != expected:
        raise RuntimeError(f"Unexpected exit {result.returncode}: {command}\n{result.stdout}{result.stderr}")
    return {"exit": result.returncode, "stdout": result.stdout.strip()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--java", default="java", help="JDK with source-file execution support")
    args = parser.parse_args()
    source = (args.root / "lib/RecompFrontend/recompui/src/util/file.cpp").read_text()
    start = source.index("    static void perform_file_dialog_operation_multiple(")
    end = source.index("\n    std::filesystem::path file::get_app_folder_path()", start)
    function = source[start:end].strip()
    anchor = "callback(false, std::list<std::filesystem::path>{});"
    if function.count(anchor) != 1:
        raise RuntimeError("The cancellation stub changed; inspect and update or retire this reproducer.")
    prefix = "#include <filesystem>\n#include <functional>\n#include <list>\n#include <cstdio>\n#define __ANDROID__ 1\n"
    harness = r'''
int main() {
    bool called = false, selected = false; size_t count = 0;
    perform_file_dialog_operation_multiple([&](bool ok, const std::list<std::filesystem::path>& paths) {
        called = true; selected = ok; count = paths.size();
    });
    std::printf("called=%d selected=%d paths=%zu\n", called, selected, count);
    return called && selected && count > 0 ? 0 : 2;
}
'''
    java_source = (args.root / "android/app/src/main/java/io/github/banjotooierecompiled/RomImporter.java").read_text()
    split = java_source.index("final class RomImporter")
    java_harness = r'''
public class RomImportAudit {
    public static void main(String[] args) throws Exception {
        java.nio.file.Path directory = java.nio.file.Files.createTempDirectory("tooie-import-audit-");
        try {
            java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
            try (java.util.zip.ZipOutputStream zip = new java.util.zip.ZipOutputStream(bytes)) {
                zip.putNextEntry(new java.util.zip.ZipEntry("mod.json"));
                zip.write("{}".getBytes(java.nio.charset.StandardCharsets.UTF_8));
                zip.closeEntry();
            }
            byte[] archive = bytes.toByteArray();
            RomImporter.Result zipped = RomImporter.select(
                () -> new java.io.ByteArrayInputStream(archive), directory.toFile(), () -> false);
            byte[] header = new byte[64];
            header[0] = (byte)0x80; header[1] = 0x37; header[2] = 0x12; header[3] = 0x40;
            RomImporter.Result control = RomImporter.select(
                () -> new java.io.ByteArrayInputStream(header), directory.toFile(), () -> false);
            if (zipped.status != 2 || zipped.error == null || !zipped.error.contains("uncompressed N64 ROM") || control.status != 0)
                throw new AssertionError("Unexpected importer result");
            System.out.println("ZIP status=" + zipped.status + "; synthetic N64-header control status=" + control.status);
            System.out.println("Java helper only: the synthetic header is not a validated game ROM.");
        } finally {
            try (java.util.stream.Stream<java.nio.file.Path> files = java.nio.file.Files.walk(directory)) {
                for (java.nio.file.Path file : files.sorted(java.util.Comparator.reverseOrder()).toList())
                    java.nio.file.Files.deleteIfExists(file);
            }
        }
    }
}
'''
    report = {}
    with tempfile.TemporaryDirectory(prefix="tooie-mod-audit-") as temporary:
        directory = Path(temporary)
        for name, body, expected in (
            ("actual", function, 2),
            ("positive-control", function.replace(anchor, 'callback(true, std::list<std::filesystem::path>{"control.nrm"});'), 0),
        ):
            cpp = directory / (name + ".cpp")
            executable = directory / name
            cpp.write_text(prefix + body + harness)
            run([args.compiler, "-std=c++20", "-Wall", "-Wextra", str(cpp), "-o", str(executable)])
            report[name] = run([str(executable)], expected)
        java_file = directory / "RomImportAudit.java"
        java_file.write_text(java_source[:split] + java_harness + java_source[split:])
        report["rom-importer"] = run([args.java, "--source", "17", str(java_file)])
    print(json.dumps(report, indent=2))
    print("CONFIRMED: Android mod selection cancels; ROM importing rejects ZIP bytes. No device operation.")


if __name__ == "__main__":
    main()
