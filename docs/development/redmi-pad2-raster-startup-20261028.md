# Redmi Pad 2 optional compose pipeline — 28 October 2026

Status: candidate host controls pass; Android build and reporter acceptance pending.
Issue #26 stays open. Frostie handles replies, merges and issue closure.

## New report and exact log

[5ejfkhj's reply](https://github.com/Frostie77/Tooie-Recompiled/issues/26#issuecomment-6449622217),
created 27 October 17:56:16 UTC / 28 October 04:56:16 AEDT, says:

> Its here, it finally opens but crashes after pressing start

The [attached startup log](https://github.com/user-attachments/files/34034870/Tooie-startup-787471a3360e47ee9244605190c8cfcc.txt)
was also uploaded directly by Frostie. Both copies are exactly **119,352 bytes**,
SHA-256 `1a8c19e2f1f8c24f3449ca1ad0adcb41fc4841ea6c410831e1c2033355219d30`,
1,250 lines. The original report identifies Redmi Pad 2 / Helio G100-Ultra /
Mali-G57 MC2. Android version and exact APK/source identity remain unspecified.
Do not infer them from the release timing or the device's usual specifications.

The export selects **System Default**, driver `Mali-G57 MC2`, driver ID 9,
version `0x0D801000`, information
`v1.r54p1-09bet0.ffb9e377f5d7c95fe2471f3c463d2515`. Vulkan instance/device and
initial UI pipeline setup succeed. Surface RGBA8 selection succeeds; descriptor
layouts/allocations shown succeed. All **10** native starts end with the same
preflight rejection:

```text
Graphics pipeline rejected: reason=attachment-blend-unsupported attachment=0 format=109 optimal=0xDC83 required=0x180
```

PIDs are 21905, 22421, 22841, 23266, 24130, 24231, 24232, 25035, 25512 and
25730. Rejections are at lines 134, 258, 382, 506, 630, 754, 878, 1002, 1126
and 1250. The final record is the rejection. No different failure or native
crash stack appears. The historical `menu_ready=true` / `accepted=true` header
does not prove that the native game launcher or gameplay completed this run.
The reporter's word “start” does not identify which screen's button was pressed.

## Source and candidate

The vendored Vulkan definitions identify format 109 as
`VK_FORMAT_R32G32B32A32_SFLOAT`. The reported mask contains color attachment
support (0x80), but lacks color attachment blending (0x100); the required mask
is 0x180. The Android backend logs and exits before unsafe pipeline creation.
This is a specific format-usage incompatibility, not a demonstrated missing
Vulkan-device feature or descriptor-allocation failure.

`ShaderLibrary::setupCommonShaders` unconditionally creates the ray-tracing
compose pipeline with AlphaBlend and R32G32B32A32_FLOAT, despite its sole
consumer being `FramebufferRenderer::submitRaytracingScene`. The candidate
creates this optional pipeline only under the same `RT_ENABLED` build boundary
and a ray-tracing-capable device. It leaves raster shaders, presentation formats,
precision/blend descriptors for real RT use and Vulkan format validation intact.
No GPU vendor blacklist, new setting, custom driver or shared input change.

The isolated branch starts from main `1a1e194a749d9d80cdfcaf3d5bb2559b74dccfcb`.
It does not include the unaccepted limb interpolation experiments from PR #41.
Its CI profile produces a separately identified **GPU Test** app with touch and
driver-picker controls, rather than replacing an existing public/Debug app.

## Verification and reporter test

`python3 tools/test_optional_raytracing_compose.py` executes the actual compose
setup block, exact capability retrieval and real descriptor/capability types
against recording stand-ins. All **eight** compiled-RT/device-capability/format
cases pass with ASan/UBSan: unavailable RT performs no optional layout/shader/
pipeline creation; supported RT retains its full descriptor; an actually needed
unsupported blend target still rejects. Restoring unconditional creation fails
on the recorded refusal. The exact main-parent source independently fails the
same raster-only check with `--baseline`. Python and whitespace checks pass.
This is not a full ShaderLibrary startup, real shader binary or GPU test.
CI runs this regression with the retained Vulkan checks; ARM64 compilation and
packaging remain pending. No test APK has been installed or accepted yet.

When the separately verified build is ready, retain the release app, install
only its GPU Test APK and leave **System Default** selected. Test Start/return to
game, then native launcher/own USA ROM/gameplay if reached, touch and sound,
closing/reopening, and Share startup log whether it works or fails. Record the
screen reached and the exact new export; further failures need their own cause.

Frostie's latest reply offered to find a suitable driver. No compatible Mali
driver download has been verified. Test this concrete app-side candidate with
the existing system driver first; do not promise a replacement driver or assume
the Poco/Adreno Turnip result applies. The log now confirms that the startup-log
sharing path produced a useful attachment on this device; gameplay stays open.
