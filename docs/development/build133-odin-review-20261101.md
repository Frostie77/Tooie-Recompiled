# Build 133 Odin review — 1 November 2026

Verified the uploaded
`Tooie-7ba1260eb907-Installed-Debug-Odin-20261101-221007-c8a1fd89-Share-20261101-224837-ccf98fa9.zip`,
SHA-256 `38d6c98a18a5c199fab7483b73820d36a5168a492e17a41671e0f3f055f7ce5d`.
ZIP integrity, unique entries and all 1,092 stored manifest items verify. Two
historical whole-log sources remain local-only; the complete fresh delta is
included. Source `7ba1260eb907fd866b93228dcb772333877f9787`, APK
`0284717b36bd377637c97baccb15eebf213f7f937f180ade3ab2d6269afb9aa5`,
native Build ID `c6d47ee43417792ea9120f26e609849ea5fed586` match the prepared
build-133 identity. The attached game process is PID 21608.

## Complete event review

| Actual sequence / marker | Description | Review |
|---|---|---|
| 1 / 1 | narrow passage - camera fix | Native image retained. Frostie reports the camera is fixed. |
| 2 / 2 | dialogue and open-area camera | Native image retained. Frostie reports the camera looks good after both checks. |
| 3 / 7 | fixed ratio clipping - 21:9 | Letterboxed wide image shows missing side geometry, with HUD values 36 and 46 visible. |
| 4 / 8 | Expand visibility comparison | Same general location, narrower field of view and intact-looking nearby scenery; values 36 and 46 remain visible. This is not a pixel-registered same-frame comparison. |

All four actual event images were inspected. There are no separate markers
3–6 in this archive; do not invent them or confuse sequence 3/4 with marker 3/4.
Native effective-config records include all four fixed targets (16:9, 16:10,
3:2, 21:9), plus Original/Expand. Frostie manually accepts HUD values and timers
throughout, while explicitly failing 3D visibility in every fixed preset.
HUD acceptance is partly his manual report, rather than timer screenshots for
each target. The source visibility gate explains the new-preset omission; see
the [correction](wide-visibility-transition-fix-20261101.md).

The 22:16:55.904632–22:43:24.104458 AEDT gameplay window ends on the requested
save, with the same PID present in all 308 periodic assessments and final read.
No current-build native fatal is captured. These facts support a **focused
camera pass** with Frostie's observations; still images do not prove movement
smoothness or full-game performance. Keep the accepted long-play character
report separate and closed unless it recurs.

## Chunked recorder device acceptance

Both transfer receipts are verified and their ranges/counts reconcile:

| Snapshot | Native bytes | Chunks | SHA-256 |
|---|---:|---:|---|
| Baseline | 103,773,962 | 25 | `cd8219ad2bcff2af48c1415e2048c860364149d5fa1fd130ce78cb31e6f6b813` |
| FullLog | 108,436,122 | 26 | `debfb1bc877fea148b095b0c14c2c9f2c47b7e5da538bf8feb2d0a0e4703f3cb` |

The included NewLog is exactly 4,662,160 bytes, equal to the size difference,
and the collector reports an exact native prefix/delta match. Both snapshots
retain their successful statuses and receipts; no manual recovery was needed.
The absent whole historical snapshots cannot be independently rehashed from
this Share ZIP. This is device evidence for the new chunked collection path,
in addition to its host failure controls.

## Separate transition footage

`IMG_8726.MOV`, SHA-256
`5b4b0bd0ae40e8b9dd52660fe96a702ffb7f5311bbbbaddc6418b1139d77c342`,
is a 15.283333-second phone video showing two complete doorway transitions.
Frostie identifies the setting as Expand. Residual Jiggy-mask pieces are still
visible around 14.5 seconds and disappear by about 14.6 seconds, before smoothly
clearing the edges. The clip is separate from the earlier marker capture; no
native-present synchronization or exact frame-time claim is made. The
[transition draw-scale candidate](wide-visibility-transition-fix-20261101.md)
addresses wide-screen coverage without modifying transition timing/rotation.

Frostie explicitly parks the screen-recording exit observation. Use the
supplied phone footage; do not make new crash diagnostics a prerequisite for
these two graphics corrections.
