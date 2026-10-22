# Android RT64 texture archive compatibility

The Odin rejected GhostlyDark's BT Reloaded v0.1.1 with
`invalid CEN header (bad compression method)`. The selected download is intact:
both the PC and Odin SHA-256 match the author's release metadata. This is an
importer compatibility failure, before installation, rather than evidence that
the texture pack was corrupted.

## Exact input and reproduced failures

- Author: GhostlyDark; [BT Reloaded downloads](https://evilgames.eu/texture-packs/bt-reloaded.htm).
- File: `bt-reloaded-v0.1.1-rt64.rtz`, 37,474,752 bytes.
- SHA-256: `0f896281240ac22c4b1fa10e83757eb6d393fbbbe2121c81a2c38fbc960cdde9`.
- Directory: 2,180 files, comprising 2,176 Zstandard/method-93 entries, three
  Deflate entries and one stored entry; 94,635,315 decoded bytes in total.
- `mod.json` is Deflate-compressed. `rt64.json` and 2,175 DDS files use
  Zstandard. The manifest's ID is `BT-Reloaded` and its game is `bt`.

Four controls use the exact `a44bd5c36222c2c0f6a73e9a77718359f4d88e87`
parent sources, with source-blob checks:

1. The production Java copier rejects the real pack with the reported CEN
   error. Host OpenJDK also includes `: 93` in the message.
2. After bypassing only that copy-layer restriction, the old native installer
   rejects method-93 entries using miniz's stored/Deflate-only support flag.
3. A separate Deflate-only fixture with a hyphenated texture ID exposes the
   installer's unrelated C-identifier restriction.
4. The old runtime mod reader cannot read Zstandard metadata, even though
   RT64's own archive filesystem already supports its compression.

## Change

Java remains a bounded, cancellable provider-to-private-storage copy. It does
not interpret archive compression or extract entries. Native inspection owns
ZIP structure, entry-count and size limits, paths, entry types, manifests,
game/version compatibility and installation transactions. Nonempty malformed
data can reach that validator, but cannot be committed as a valid mod.

The runtime mod reader adds method-93 extraction through miniz's existing raw
compressed-data API and Zstandard. It caps compressed and decoded entries at
256 MiB, checks local/central method and flag agreement, and requires the
decoded size and ZIP CRC to match. Valid concatenated Zstandard frames use the
same whole-buffer decoder as RT64. Unsupported methods, encryption, patched
entries, malformed streams and checksum failures remain rejected.

Android inspection checks each Zstandard entry after the existing count,
per-entry and aggregate bounds. Every `.rtz` must have a readable JSON-object
`rt64.json`, including archives that also provide `mod.json`. Native-library
rejection, private staging, confirmation, cancellation and rollback remain in
place. The outer `.zip` bundle format still supports stored/Deflate entries;
an `.rtz` inside such a bundle can use Zstandard internally.

RTZ is registered for texture content only. Its IDs may include ASCII letters,
digits, spaces, underscores, hyphens and periods, bounded to one private config
filename component; empty IDs, `.`/`..`, path separators, colons and NUL are
rejected. Executable `.nrm` IDs retain the existing C-identifier rules. The
author's ID and archive bytes are preserved.

The app reuses the pinned static Zstandard target already built for RT64.
The frontend explicitly links librecomp so that its archive consumer receives
that target's exported include path and linkage. The first CI run passed host
regressions but exposed the missing frontend dependency during ARM64
compilation (`zstd.h` not found); this dependency correction addresses that
specific failure. A subsequent full build is still a separate gate.
Standalone runtime CMake consumers and the host mod-runtime fixture use the
system Zstandard CMake package. Android CI installs `libzstd-dev` for host
checks. There is no new downloaded app library or Java compression dependency.

## Reproduction and validation

On a Linux host with JDK/JNI headers, C++ tools and `libzstd-dev`:

```bash
python3 tools/test_android_mod_support.py --compression-base-root /path/to/exact-a44bd5c-source --rt64-pack /path/to/bt-reloaded-v0.1.1-rt64.rtz
```

The external pack is optional. The test pins its full size and SHA-256 before
reading it. It does not download, modify, bundle or redistribute the pack.
Without that argument, generated mixed-method fixtures exercise the same
consumers. A runtime-only host can provide `--zstd-include` and `--zstd-link`.

Coverage includes the actual Java copier, real JVM/JNI route, transaction
failures, the production archive/manifest/config installer, method-93 metadata
with and without manifests, empty assets, concatenated frames and hyphenated
texture IDs. Rejections cover malformed/truncated directories, bad CRC/size,
truncated/trailing compressed data, local-header disagreement, unsupported
methods, encryption/patch flags, missing/invalid databases, unsafe IDs and the
retained path/native-library/game/version/limit cases.

The actual downloaded pack passes native installation without changing its
bytes. The production runtime reader and unmodified RT64 filesystem return
identical decoded bytes for all 2,180 files, including all 2,176 Zstandard
entries. These checks run under ASan/UBSan; they do not decode DDS on a GPU or
establish Odin texture appearance.

The local environment provides OpenJDK 17.0.20's compiler module and Linux
Zstandard runtime; the test uses the vendored Zstandard header and official
OpenJDK 17 JNI headers. Actual Activity/Android SDK compilation is not claimed
by that local run. The full Android CI is the separate compile/link/packaging
gate for this revision.

## Remaining acceptance

The installed Odin APK is still the earlier `386d58e4946f` build. These source
changes do not update the device. A rebuilt, identity-verified APK must import
the original pack through Mods and visibly change a supported UI surface,
then show the expected disabled comparison with graphics settings retained.
The pack's author lists current coverage as UI only.

The previously recorded GOLD/BLUE/persistence/disabled executable-example
checks belong to the earlier source tree. The next build also includes the
[saved Aiming Invert preference](android-aiming-invert-20261022.md); its hardware
acceptance is pending. The archive correction changes no timing, rendering,
controller mapping, save handling, Vulkan policy or five-second runtime guard.
No release or full-game compatibility claim follows from archive acceptance.
