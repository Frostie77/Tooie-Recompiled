# Next Codex handoff: Tooie-Recompiled

Updated 17 October 2026. Start here, then read the linked investigations before
changing implementation. This branch combines selected Android branding,
the Banjo-Recomp comparison, Android mod import/texture registration and
update-readiness planning. The mod paths are host-tested; executable mods and
rendered replacements still need Odin verification. Production interpolation,
pacing and the update client are unchanged.

## Goal and completion requirements

Deliver a usable native Android Banjo-Tooie port for the AYN Odin 2 Portal,
with correct gameplay timing, responsive controls, faithful graphics and audio,
reliable ROM import and saves, and stable longer gameplay sessions. Launching
and loading a ROM is an intermediate milestone. Stretching, freezes/shunts,
input delay, temple-entry crashes, shadow/jigsaw glitches and demo pacing need
evidence-based resolution where still reproducible.

The user explicitly added **working mod support** to the finished-project
criteria. Use Mr Wiseguy's Banjo-Recomp workflow as the reference: install,
enable/disable, configure and order compatible mods through the UI. A visible
Mods button or successful archive copy does not establish working mods.

The user also requested future automatic update detection when the app opens.
See [android-update-readiness.md](android-update-readiness.md). Published
compatible Android builds are the update source; new source commits alone do
not constitute an installable update.

## Preserve the approved branding

| Item | Required result |
| --- | --- |
| Visible application name | Exactly **Tooie-Recompiled** |
| Android launcher icon | The selected custom blue B/red T, gold outlines and golden Jiggy on navy; **round**, not the rejected rounded-square option |
| In-game launcher artwork | Accepted **TOOIE** title; **RECOMPILED** and its font/colour scheme retained |
| Android package/activity | Retain `io.github.banjotooierecompiled` and the established `BanjoTooieSDLActivity` identity |

Read [android-launcher-icon.md](android-launcher-icon.md) for the selected
master, regeneration and actual AAPT2 validation. Both legacy launcher PNGs are
circular; Android controls the global adaptive-icon mask. No full APK or device
preview has validated these new launcher resources yet.

The accepted menu artwork is identical in `assets/menu.png` and
`android/app/src/main/assets/program/assets/menu.png`, SHA-256
`e7dc6ec776a14b09ca5108a9f279e7d0ac5e464d908e57a817a3534e7716bde6`.
The icon master SHA-256 is
`80e98fb563e23d1663dc185ee9e733f5b656f8157dcb85676a9d1d5b044a67ef`.

## Exact source history and integration

| Work | Revision / ancestry |
| --- | --- |
| Current inspected engineering head | `5db2d7f3719df6d196bf46a7a52a5e749d152956` on `fix/android-vulkan-transfer-proposal` |
| Initial Android icon wiring and accepted menu artwork | `05aca40d5f98891b442f20c6418fe6196e0c8f4e`, parent `5db2d7f` |
| Selected BT icon and exact display name | `5747f515c66a0b28cd524322497405c168fd028b`, parent `05aca40` |
| Mod audit and host reproducer | `79f97b7383749a9862dc14052ea37220fd198f10`, parent `5db2d7f` |

Parent `4a0a30912b4e54a31d00f64ed91f5bc8b2adcc37` joins the complete branding
chain and audit in one merge commit. This feature branch adds Android mod
support. Existing engineering, main, PR #1 and release refs were not moved. Inspect
the current engineering head before integrating so newer fixes are retained.
If integrating individual commits onto `5db2d7f`, include **both** `05aca40`
and `5747f5`: the latter alone omits the earlier unchanged icon wiring and menu
assets. The audit is documentation plus an investigation reproducer only.

## Mod support: implementation and device follow-up

Read [android-mod-support.md](android-mod-support.md) first. It implements a
mod-specific document picker, private staging, package/game/config validation,
confirmation and replacement rollback, plus texture startup registrations.
Normal `.nrm` mods, `.rtz` packs and ZIP bundles are accepted by that source
path; unverified native-library extensions are rejected. Actual normal code-mod
execution and texture output remain untested on Odin.

Run `python3 tools/test_android_mod_support.py` for the current host suite; pass
an Android SDK jar for actual Activity/SDL compilation and a pinned tested-base
checkout for previous-source controls. Reconfigure CMake to include the added
Android installer unit. Verify `ModImporter` and `nativeModFileDialogResult`
are present in the next APK.

The following items describe the **historical baseline**, explaining this
change; they are not claims that these exact source gaps still exist here.

Read [android-mod-support-audit.md](android-mod-support-audit.md), which pins
the inspected Banjo-Recomp and shared-framework revisions.

1. Android's `open_file_dialog_multiple` immediately calls its callback with
   failure and no paths. The Install Mods action uses that function. The host
   reproducer executes this actual branch and a successful callback control.
2. The current single-file JNI/Java path is a ROM importer, with N64-header
   validation and a ROM size limit. A valid ZIP is rejected. Add a separate
   mod-import purpose; preserve a safe package extension when staging `.nrm`
   and `.rtz` files for the existing installer.
3. Tooie's audited startup omits Banjo-Recomp's texture-content and `.rtz`
   container registrations. Existing renderer texture helpers alone do not
   connect the feature.
4. Adapt Android management actions, cancellation/lifecycle handling and
   package-replacement rollback. Provide Tooie-specific code/texture fixtures
   and prove output on ARM64. Shared package format does not make `bk` mods
   compatible with Tooie's `bt` target.

Run `python3 tools/audit_android_mod_support.py` from the pinned audit snapshot
with a C++20 compiler and Java 17 source-file support. Its successful exit means
the existing gaps and controls were reproduced, not that mods work. Update or
retire the investigation reproducer once its cancellation stub is corrected.

## Resume the unresolved crash, geometry and clock investigation

The latest engineering commits are:

- `79094de`: fixes a reproduced RmlUi document/child lifetime defect and adds a
  diagnostic guard if recursive layout dispatch survives.
- `6fe68e7`: reproduces observed clocks through generated replay/timer consumers.
- `5db2d7f`: retains primary guest failures and uses the existing Android
  `quick_exit` path to avoid unsafe global teardown.

Read [android-step86-investigation.md](android-step86-investigation.md).
These source changes do not establish the Odin temple-crash trigger, a
stretching correction or a pacing correction. Source tests and component
compilation are separate from hardware gameplay verification.

The latest relevant capture is the `f017d00` correction-input build, not this
handoff branch:

| Identity | Value |
| --- | --- |
| Built revision | `f017d00f943716ed38a8aa1197a54ba72d4293ae` |
| APK SHA-256 | `214c1d65990a66d6f1a00acdce3147cd5a14ebce82c4d98cd94b2ee189e76f96` |
| ARM64 Build ID | `818ea5de1d1c84734dfdb5b599de5aa2baf574a4` |
| Original PID | `19849`; later PID `21610` is a new process |
| First SIGSEGV | 17 October 2026, 11:28:30.121736, Australia/Hobart UTC+11 |

The observed stack repeatedly names `Rml::Element::IsLayoutDirty`; the retained
stack does not establish the initiating UI or temple-transition call. Later
destroyed-mutex messages follow the first fatal event. The user reports the
temple/first-person entry as the trigger, which still requires reproduction.

The capture reproduces 269 selected triangles. Exactly 96 have complete copied
bone inputs at both endpoints, all in the player category. NPC AUTO samples
have root-only/incomplete ownership; do not claim that their full bone
hierarchies were captured or that a player fix automatically fixes NPCs.

Ten pure replay windows span 50.116535 wall seconds, with duration-weighted game
timer/wall ratio 1.383110 and attract-clock/wall ratio 1.460925. Ordinary windows
inside active play span 546.933251 seconds with ratio 1.000518. These are
separate observed clocks, not proof of a universal 2x gameplay speed.

### Evidence to attach to the next Codex session

The exact private-log delta requested by Codex is:

`Tooie-f017d00f9437-Correction-Inputs-Odin-20261017-111632-6ba62e53-NewLog.txt`

- Size: **30,830,040 bytes**, below that environment's stated 32 MiB file limit.
- SHA-256: `b332320429ddfa1ca389f609cddec829252ec0c2b55a1b991d21e641aa58afe4`.
- It is the unchanged ZIP member, not a shortened or reformatted log.

Also attach the existing detailed coordinator handoff:
`Tooie-Step86-Codex-Crash-Geometry-Clock-Handoff-f017d00.md`.
The extracted crash-window file is
`Tooie-Step86-PID19849-Logcat-112825-112831.txt`.
These private capture files are separate attachments, not committed here.
This document preserves their identities but cannot substitute for their raw
matrix/clock records. If attachments are unavailable, say so before drawing
new conclusions from those records.

## Next work order and validation boundaries

1. Review the current source, existing failing controls and raw Step 86 inputs.
   Establish the remaining initiating crash path and actual authored
   hierarchy/pivot/clock behavior before choosing further corrections.
2. Integrate approved branding into the next intended APK source and verify the
   packaged labels, icon resources, menu hash, native Build ID and APK hash.
3. Verify the implemented mod-import/texture wiring with a known Tooie code mod
   and texture pack on Odin, including config/order/restart, replacement
   cancellation, failed providers, ROM selection and saves. Keep claims
   specific to the tested fixtures and device.
4. Build the update-release foundation before adding startup checks. Keep
   diagnostic APKs out of the stable update feed.

Preserve Vulkan guards, ownership/synchronization checks and five-second
timeouts. For normal production builds: allocation probe OFF and queue
isolation `0`; transfer, fallback and barrier diagnostic selectors remain
**undefined**, not forced to `0`. Observer-enabled capture profiles are
separate diagnostic builds and include copying/logging overhead.

Cloud full-APK linking still lacks established Android SDL2/FreeType prefixes
and required zstd sources. Local WSL builders have previously restored proven
dependencies and produced verified APKs. Do not describe a component build as
a complete packaged APK or a host/software-Vulkan pass as verified Odin play.

Use longer, single-session gameplay captures; the user found three minutes
insufficient and cannot play while holding a camera or run competing tests.
The last requested ten-minute active-play capture ended early on process loss.
Do not rerun a consumed one-install/one-launch script. Preserve app data and
avoid force-stop, uninstall, reset or settings changes outside the agreed test.
Main, PR #1 and releases remain separate from these isolated investigations.
