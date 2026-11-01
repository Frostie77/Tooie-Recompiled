# Build 134 Odin acceptance — 2 November 2026

Frostie reports that everything looks good after the requested fixed-ratio
visibility and Jiggy-transition checks. All four actual events and their native
screenshots in the uploaded capture were reviewed. Both reported defects have
focused Odin acceptance; no repeat graphics build is needed for these results.

## Verified identity and capture

Upload:
`Tooie-2e0962851d7a-Installed-Debug-Odin-20261102-001404-b681740d-Share-20261102-002513-a8ebdcae.zip`
(7,579,668 bytes), SHA-256
`ded289873fd9cb7d797e053566bee38dbdd6c90aaeece55cf9642c9780d104d1`.
ZIP CRCs, unique members and all 258 stored manifest items verify. Baseline and
FullLog remain local-only; the complete appended NewLog is included.

- [Android CI 134](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/41306559724)
  passed for head `6df356a32f9521389af1fca60f3b3e925789b30b`.
- Exact CI merge/package source: `2e0962851d7a03a829497f07b40b9f23fe9b3c99`;
  tree `fa03beec310b1907eab65b2f580748557509cf90`, matching the host-tested tree.
- Native version: `0.1.0-ci.134.1+g2e0962851d7a`.
- Locally signed installed Debug APK:
  `5b9cf695c54659ca22a800fe76800a00127089ed43860dc2b57ece7c46906053`.
- `libmain.so` Build ID: `0a306e49e70f9890df216a0f4c282b8e2a3c9e79`.

The downloaded CI ZIP matches its GitHub size and SHA-256
`eb56042a96537db06b9567309a104cb7c35b42eab7df5d507297daff9f37dc56`.
Independent checks cover APK/native hashes, seven AArch64 ELF Build IDs,
correction/JNI exports, executable Java markers, exact-source capture tools and
preparation helper, 32 source assets and 15 launcher icons. The captured build
report equals the independently verified CI report. Local SDK preparation passed
the established Debug certificate, signature, alignment, identity and unchanged
payload checks; Frostie's replacement installation returned Success.

## Complete event review

| Sequence / entered marker | Host receipt (AEDT) | Description | Review |
|---|---|---|---|
| 1 / 1 | 00:20:50.285 | transition fixed | Spiral Mountain gameplay after arrival; Frostie's manual transition pass. |
| 2 / 1 | 00:21:19.985 | transition good in Expand | Jinjo Village gameplay; explicit Expand transition pass. |
| 3 / 3 | 00:22:30.661 | transition good in all aspect ratios | Jinjo Village gameplay beside a house; manual acceptance across ratios. |
| 4 / 4 | 00:22:57.832 | no clipping in any aspect ratios | Nearby gameplay view with Banjo and surrounding scenery present; manual visibility acceptance across ratios. |

Marker 1 was entered twice. These are four separate events, identified by their
sequence and event ID; there is no entered marker 2. Every screenshot has a
successful status, original PID 1055 and the game activity in the foreground.
The still images show completed gameplay rather than the moving wipe; transition
smoothness/edge coverage is accepted on Frostie's explicit observations. They
are not frame-by-frame proof or a screenshot comparison for every preset.

Native effective settings record Expand, then 21:9, 3:2, 16:10, 16:9 and Expand
again. All use Display refresh, MSAA 2× and HUD Expand. The numeric fixed targets
are 2.3333333333333335, 1.5, 1.6 and 1.7777777777777777. The retained gameplay
window runs 00:20:35.733550–00:23:11.669615 AEDT and ends on the requested `save`.
PID 1055 remains present in all 30 periodic assessments and the final read.

## Collection integrity and remaining scope

| Snapshot | Native bytes | Chunks | Receipt SHA-256 |
|---|---:|---:|---|
| Baseline | 112,528,805 | 27 | `5a66ef9e52a6736e402e364dfd486fa1ef9ff8b355061a2e19acaf2b7bfd6b20` |
| FullLog | 114,907,052 | 28 | `e1418a239279d16b09698347039971c5d1d61ac94db868a5268d0cec67f86cb5` |

Both receipts report verified reads; chunk ranges/counts reconcile. The included
NewLog is 2,378,247 bytes, equal to the native size difference, with SHA-256
`a5e9b7435072d22cfad2fcf2b68c0da6b0a86a539d36b72abbe9128b8f48d303`.
The exact-delta status is retained. The absent whole historical snapshots cannot
be independently rehashed from this Share ZIP.

This closes the fixed-preset scenery/character clipping and premature wide
Jiggy-wipe disappearance checks. Retain the accepted camera, HUD, controls,
water and long-play results. This continuous-process capture does not establish
preset persistence after an app restart; fold that check into the next signed
release update/save/settings/reopen check. No new release version, signing
identity, public artifact or update feed is selected or published here.
