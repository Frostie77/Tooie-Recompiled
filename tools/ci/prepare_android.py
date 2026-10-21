#!/usr/bin/env python3
"""Restore omitted build tools; never replace vendored runtime source or use a ROM."""
import hashlib
import json
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path


def prepare(source, work):
    lock = json.loads((source / "tools/ci/android-dependencies.json").read_text())
    downloads = work / "downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    extracted = {}
    for name, dependency in lock.items():
        archive = downloads / (name + ".tar.gz")
        if not archive.exists():
            subprocess.run(["curl", "--fail", "--location", "--silent", "--show-error",
                            "--connect-timeout", "30", "--max-time", "300",
                            dependency["url"], "--output", str(archive)], check=True)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != dependency["sha256"]:
            raise ValueError(f"{name}: SHA-256 mismatch; remove the failed download before retrying")
        destination = work / "sources" / name
        destination.mkdir(parents=True, exist_ok=True)
        with tarfile.open(archive) as package:
            package.extractall(destination, filter="data")
        extracted[name] = destination
        print(f"Verified {name}: {dependency['sha256']}", flush=True)

    # The native SDL and packaged Java bridge must come from the same release.
    sdl = next(extracted["sdl"].iterdir())
    for java in (sdl / "android-project/app/src/main/java/org/libsdl/app").glob("*.java"):
        packaged = source / "android/app/src/main/java/org/libsdl/app" / java.name
        if java.read_bytes().replace(b"\r\n", b"\n") != packaged.read_bytes().replace(b"\r\n", b"\n"):
            raise ValueError(f"SDL Java/native release mismatch: {java.name}")

    # Only CMake recipes are absent from the snapshot. Keep its zstd C sources.
    zstd = next(extracted["zstd-cmake"].iterdir())
    destination = source / "lib/rt64/src/contrib/zstd/build/cmake"
    if destination.exists():
        raise ValueError("zstd CMake destination already exists; use a clean CI checkout")
    shutil.copytree(zstd / "build/cmake", destination)
    dxc = extracted["dxc"]
    target = source / "lib/rt64/src/contrib/dxc"
    (target / "lib/x64").mkdir(parents=True, exist_ok=True)
    # Install the executable and its libraries as one versioned toolset.
    shutil.copy2(dxc / "bin/dxc", target / "bin/x64/dxc-linux")
    (target / "bin/x64/dxc-linux").chmod(0o755)
    for name in ("libdxcompiler.so", "libdxil.so"):
        shutil.copy2(dxc / "lib" / name, target / "lib/x64" / name)
    (work / "logs/dependencies.json").write_text(json.dumps(lock, indent=2) + "\n")


if __name__ == "__main__":
    prepare(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
