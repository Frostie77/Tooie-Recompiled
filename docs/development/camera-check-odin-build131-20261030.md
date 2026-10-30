# Build-131 bounded camera capture — 30 October 2026

The observer worked and its temporary request was removed. This is a verified
CPU camera capture, **not a camera fix or an aligned reproduction of the visible
shake**. Frostie is unsure whether the shake happened within the bounded burst
and has kept the game open for another pass.

## Capture verification

- Installed source `aa277b14262c951b395a39d4345845e6ecf481d4`, native version
  `0.1.0-ci.131.1+gaa277b14262c`, main Build ID
  `2ac5aeb2745839ca5ac5e25b11eb29f0d06da09f`, original game PID **21801**.
  The installed APK identity matches Frostie's local preparation/update result.
- Share ZIP `Tooie-aa277b14262c-Installed-Debug-Odin-20261030-222833-06d4b8a5-Share-20261030-223240-9efc3fee.zip`:
  **2,493,176 bytes**, SHA-256
  `e02e955913f6b9b348f500fba6d9bcd440612f30a20dc50fadb21426ec5d51a9`.
  All **184** unique flat members pass CRC. All **180 verbatim sources** match
  their lengths/hashes; two omitted historical logs are local-only and their
  contents are not independently verified.
- Complete NewLog: **6,097,602 bytes**, SHA-256
  `f4ffb26640ace932fc2107994ca15666f4bd26c64e78fa48362d5ca41f431ddb`.
  All **53 Runtime exports** reconstruct, including the separately derived four
  Java window records. All **13,766 native Runtime records** identify PID 21801.
- Three actual journal inputs reconcile: `playing`, one event, `save`. There
  are no notes/orphans. The sole event is marker 1,
  `bb47afd420924600968bdc13fddadc1c`, literal description
  **`narrow passage - camera trace Displa`**, host receipt
  **22:31:19.527 AEDT**. Preserve its missing final `y` rather than relabelling it.
  Delivery is acknowledged. The native **1920×1080** PNG and foreground/PID
  receipts verify; the image has been viewed and locates the rocky/timber
  passage with Banjo. A still cannot establish motion or its timing.
- The declared gameplay window is **22:31:03.585077–22:32:21.926866 AEDT**,
  **78.341789 seconds**, ending on `save`. Fifteen periodic checks and final
  collection retain the original PID. No nonzero command status or original
  fatal/guest-fatal/lookup-failure/skeletal-rejected record is captured.
  All **113** Vulkan-build result fields are zero. This is not broad stability
  or long-play character acceptance.
- Camera enable and removal are acknowledged, with no cleanup error. The
  request hash matches `tooie-camera-trace-v1\n`, the native burst nonce, and
  its final newline. No manual private-file cleanup is needed.

## What the burst actually covers

Nonce `879d682251dc4d8491324f3b84f8d179` emits one begin and one end. The end
reports **step-limit**, **1,440 steps**, **11,520 matrix records**, **12,961
records before end**, **3,250,048 payload bytes**, and **zero dropped records**.
These counts and payload bytes reconstruct exactly from the appended log.
Every step has all eight distinct matrix kinds, finite values, matching
workload/nonce/PID/TID, and increasing step timestamps. Actual duration is
**7.767172 seconds**, not 12 seconds.

Three tagged transforms are processed for each observed camera output:

| Transform | Observations | Verified relationship |
|---|---:|---|
| 1 | 480 | Main invertible view sequence, scene 0/projection 0 |
| 3 | 480 | Exact duplicate of transform 1's eight matrices, selecting previous transform 1 |
| 2 | 480 | Separate scene; view upper-left 3×3 has rank 2 |

The budget counts processed tagged projections, not unique cameras or displayed
frames. Thus the 1,440-step cap expires sooner with three projections. Do not
infer a frame rate from that counter. The singular third transform also means
role `0x1000` is insufficient by itself to justify rigid interpolation for every
tagged matrix; no such policy change is included.

The main sequence covers **234 consecutive workloads, 3897–4130**, with 36
distinct submitted views and 35 changed input transitions. Changes occur only
between about **2.834 and 3.967 seconds** after the first step; the inputs are
constant before and after. The data show one pitch excursion, approximately
**−4.920° to +13.912°**, already present in the submitted views. This does not
establish whether player input or automatic camera/collision adjustment caused it.
Controller values and the guest collision decision are not recorded.

Every selected main previous view exactly equals the previous workload's
submitted view. Roles, viewport/scissors and aspect scale remain stable; no
viewport bypass or debugger-camera activation occurs. The consumed matrices
follow the existing component blend. Maximum absolute component residuals
against the logged inputs/weight are about **0.000128** for views and
**0.00000198** for adjusted projections; the combined-matrix product residual
is about **0.000124**, consistent with single-precision serialization/arithmetic.
These checks use immutable parsed inputs to avoid changing a previous matrix
while normalizing it for analysis.

The largest selected rotation change is about **2.398° per input update**.
The smallest consumed/input column-length ratio is **0.999832**, around a
**0.017%** contraction. This is far smaller than the synthetic large-turn
examples; it does not establish the cause of the reported visible shake or
justify switching all cameras to decomposed interpolation.

## Timing and next discriminator

The effective config is Display/Expand, MSAA 2×, WindowIntegerScale, filtering
AntiAliasedPixelScaling. Display's native target is **120**, while nearby
five-second present windows deliver roughly **60.0–62.6 presents/s**, with
maximum gaps around **24.1–24.3 ms**. The recorded render target is 2134×1205.
The stored `refreshRateTarget=60` is not Display's effective native target.
GPU wait dominates the recorded render-stage time. These are overlapping
diagnostic windows, not synchronized GPU frame readbacks or a clean benchmark.
The trace reports 111,591 μs total per-step emission cost (about 1.44% of its
window), maximum 450 μs per step; polling and begin/end cost are excluded.
The prior Manual-60 result still prevents assigning the entire report to
missing Display's 120 target alone.

The marker and screenshot follow the bounded burst; no aligned motion video is
supplied. Asked whether the shake occurred in the first eight seconds, Frostie
answered **“I'm not sure.”** An additional pass on the installed build is useful:
reach the problem section first, have `playing` ready, begin walking before
pressing Enter, and keep moving immediately after the armed message. The
existing request was removed and each new recorder session uses a fresh nonce.
Retain whether the shake occurred in the first few seconds, rather than assuming
the final screenshot shows a traced frame. No rebuild is needed for that repeat.

If the captured movement includes the shake, the next investigation is the
game-supplied adjustment/input/collision path plus a separate render-load
control. If the visible event occurred later, capture it before selecting a
camera policy correction. No accepted HUD/water/logo/Jiggy/menu test needs
repeating for this question. See the [machine-readable verification](evidence/camera-check-build131-20261030.json).
