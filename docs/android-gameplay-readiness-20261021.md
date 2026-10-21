# Manual 60 Hz evidence and gameplay priority

21 October 2026, Australia/Hobart. Frostie's current instruction is that demo
stutters are lower priority provided they do not occur during gameplay. This
supersedes the earlier restriction on playable-area testing. Smooth responsive
normal play is the next acceptance check; demo stutter alone does not block it.
The snowy rendering defect remains open and must also be checked during play.

## Codex source repair and tested APK

Codex's engineering commit
[`e6823c6d3dd128035882da7ff39434cfbc432f14`](https://github.com/Frostie77/Tooie-Recompiled/commit/e6823c6d3dd128035882da7ff39434cfbc432f14)
restores missing matcher integration in main `6239e92`. Its report and diff
describe the container/finalization/test adapter already present in tested
`d168ed6bb91a560496d3701ea9e11759c6e1a4dc`. That is a source integration repair,
not an established Odin performance or visual fix. This workflow continues to
use the exact verified d168 APK and receipt, not a build from incomplete main.
Do not overwrite or merge Codex's active engineering branch as part of tooling.

The current evidence is a second independent Odin session, distinct from the
19:16 capture requested by Codex. If available, give Codex the older 26.18-MB ZIP
for comparison and the new evidence below for actual bug pictures.

## New evidence integrity

Prefix: `Tooie-d168ed6bb91a-Demo-Match-Storage-Odin-20261021-224422-c2547b05`.

| Field | Verified value |
| --- | --- |
| Original evidence ZIP | 42,760,811 bytes; 407 members; all CRCs pass |
| Original ZIP SHA-256 | `cb49a443feafe73898f6ad970bcb412a61dd99b403c521c2820a878aac31c804` |
| NewLog SHA-256 | `7b3b61bd2a880d3e3c3f39cd79540dc89edeb04fe5f2100851f20351695f1f7e` |
| Exact byte delta | 97,440,450-byte Baseline + 2,926,942-byte NewLog = 100,367,392-byte FullLog |
| Process | Original PID 6119; final PID 6119 |
| Source / APK SHA-256 | `d168ed6bb91a560496d3701ea9e11759c6e1a4dc` / `3fcaf283002c273eb4dd84be3947a17f0b484bd453085da908e1fb7a26751768` |
| ARM64 Build ID | `67d5b4343bcc3556e66e4f8afaccb60d2ec905bf` |
| Timed window | 22:46:25.001124–22:51:25.001606 AEDT host time; 300.000472846 seconds |

All fourteen supplied part ZIPs match the Upload Index sizes/hashes and pass
CRC checks. Every chunk's hash, size and contiguous source offset were checked;
all **407 reconstructed files match the original ZIP**. The small Handoff ZIP's
original-archive hash also matches. These are alternate representations of one
session, not fifteen tests.

For initial Codex analysis the new **Part013** contains the complete NewLog,
original-PID runtime summaries and raw Logcat chunks. **Part014** contains the
marker pictures and journals. Supply the **Upload Index** with them for the
chunk/member map, and the small **Handoff ZIP** for source/process metadata.
Full Baseline/FullLog reconstruction additionally needs the earlier parts.
Never treat omitted earlier parts as independently verified by Codex merely
because this report records their validation here.

## What this run establishes

The effective configuration changes from Display to **Manual, 60** before the
timed window; only refresh mode changes between its two config records. The
file contains 19 startup present reports targeting 120 and 80 later reports
targeting 60. Startup reports must not be mixed into the comparison. A rounded
device clock and Java window anchor were used to select only full five-second
reports inside a two-second boundary margin: all **58** selected reports target
60. They contain 15,993 native present calls over 290.611001 report seconds:
55.03 calls/s overall, median report rate 59.56/s. These are native present calls,
not physical screen scanout measurements or a controlled hardware speedup.

All nine fresh numbered markers were acknowledged and produced complete PNG
responses. Screenshot attempt durations are 1.087–1.328 seconds, including
foreground/PID/date reads and the screencap. Their timing metadata is retained.
They may perturb nearby CPU/USB timing and cannot prove the exact visual onset.
No runtime focus/pause transition occurs after the initial launch in this run.

**Pictures 006 and 007 visibly capture the rendering defect.** Picture 006
shows separated horizontal strips across the snowy sky/mountain view; picture
007 shows a long horizontal dark band across another snowy view. Picture 008
does not show the same large band. Picture 009 shows a player pose to compare,
but a still image alone does not establish its pose or animation as incorrect.
Do not assign a clear, texture, particle, interpolation or synchronization cause
without a distinguishable reproduction.

The final user description associates the early notes with stutters and later
notes with graphical/model issues. Events themselves are quick numeric markers
without descriptions; retain the final free-form observation as context rather
than inventing exact onset times. In the source reporting intervals containing
markers 6/7, native presentation averages 33.9 calls/s and render-thread CPU
GPU-wait averages 16.54 ms/call. The corresponding marker-9 values are 34.0/s
and 17.93 ms/call. CPU wait includes queued/scheduling work and is not an
exclusive GPU-pass duration; different domains' peaks may be different frames.

For clean main-loop clock windows with one mode bucket and no replay ends,
resets or transitions, ordinary map 314 has timer/wall ratio 1.0011 across nine
windows. Replay map 296 has ratio 0.9453 across ten windows; maps 278/314 have
0.9271/0.9119 across four/six windows respectively. These limited windows do
not indicate a universal fast simulation clock or establish individual NPC
motion fidelity. A global slowdown is not supported. The 60 Hz output target
has not resolved the reported demo stutters or visible snowy bands.

## Next gameplay session

The reviewed optional session flag is:

```bash
~/projects/tooie-tools/tooie test --gameplay-only
```

Run in **Ubuntu** after updating the workflow. It keeps the pinned d168 source,
verified APK/receipt and current settings. No APK rebuild is needed. The launcher
check still uses `ready`; after one manual Start Game, load the existing save
and type `playing` only once Banjo can move. The ten-minute gameplay timer then
starts. There is no timed demo or `watching` prompt in this selected mode.
Optional five-minute extensions, `save`, same-terminal numbered bug notes,
screenshots, exact deltas and original-PID checks remain.

Check walking/turning/jumping, nearby actors, responsive controls, ordinary area
transitions, and the snowy area if already accessible. Use areas available in the
save; no progression milestone is required. Keep Manual 60 and other graphics/
sound settings unchanged for this baseline. Record any visible lines, stretching,
hitch or input delay. No defect in normal gameplay is established by this
automatic-demo capture alone. The gameplay test must answer that directly.

The default test retains its demo-only scope. New host scenarios verify direct
gameplay, delayed loading/readiness, ten-minute timing, extension, and stops at
the launcher or loading prompt without an install/launch retry. No new native,
Java, shader, artwork, save-data or device operation is part of the tooling
change. Five-second native safeguards and game timing remain unchanged.
