# Signed build 135 checkpoint — 2 November 2026

[CI 135](https://github.com/Frostie77/Tooie-Recompiled/actions/runs/41314069033)
passes at main source `a17ba3f9cbaff2635d6136f2327a0cf5a9e10e3d`, version
1.2.0 / Android code 6. The unsigned-candidate ZIP matches GitHub SHA-256
`ccf09b6c8e26b69de55b1f0b4900f90e731fc20c39a339de7d12a838bd7244ac`.
All seven bundled checksum entries, source-matched signing helpers/identities,
the verified Debug binding, seven native libraries/Build IDs, Java/JNI bridges,
32 source assets and 15 launcher icons verify. Release resource paths are
shortened; four icons normalize RGB only under zero alpha. Their alpha and
visible colours match the source. Do not require Debug resource paths or hidden
transparent RGB bytes when checking optimized release icons.

Frostie's local signing helper reports the expected protected certificate and
complete unsigned payload verified. Signed APK SHA-256:
`63d9581f9240120fe3781f64389d4c225360a2aca36f444ed397093806b5288b`.
The replacement command first requires installed release code 5 and verifies
both source/staged APK hashes. Frostie reports `adb install -r` Success.

## Manual device results

Frostie reports importing his Debug game's saved data into the new Release app
worked easily. He then saved and quit at 21:9, reloaded, and confirmed that
21:9 remained selected. This accepts Debug → Release save transfer on Odin and
the selected fixed-ratio preference surviving save/quit/reload.

These are manual results without a new diagnostic capture. Pre-import release
save/settings/mod retention is not separately confirmed by the import result.
Other devices, transfer directions and test-app channels retain their scope.

## Requested settings follow-up

Before publication, clarify Game Aspect Ratio versus HUD Placement. Put Expand
first in the game-ratio choices, Display first in Framerate and Auto first in
Resolution. HUD choices are
Auto, Classic 4:3 and 16:9. Auto follows the selected game image and becomes the
default only when no HUD preference is stored. Keep existing enum values and
JSON keys so saved HUD, framerate and aspect preferences are preserved. Launcher
and settings menus continue filling the display; rendering/placement math is
unchanged. This requires one replacement candidate and a brief settings check,
with the accepted graphics/control results retained.

The final host config/renderer suite passes with ASan/UBSan: 576 resolution
cases, 216 ratio/HUD/window/scale cases, all three legacy HUD preference
save/reload checks and the missing-preference Auto default. Existing failing
consumer controls still fail as intended. The UI consumer maps radio positions
through stable option values, so presentation order does not renumber settings.

The installed preview is already code 6. The replacement remains the unpublished
1.2.0/code 6 candidate, using the same protected signer. Bind signing to public
v1.1.0/code 5 as before; the next install guard must account for the verified
code-6 preview already on Odin rather than requiring code 5 again.
