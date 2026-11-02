# v1.2.0 adds save management, more aspect ratios and separate flight controls, alongside camera, HUD and graphics fixes.

### Added

- **Save Management** under **Settings → General**: export/import base-game save backups and export an earlier retained save for recovery. Debug → Release save import has been tested on Odin 2 Portal. Backups contain game saves; ROMs, mods and app settings are separate. Use Save Management from the launcher after saving and exiting gameplay.
- **Invert Flight y-axis**: a separate setting for flight steering and airborne aiming, independent of stationary Breegull Blaster aiming.
- **Swap Joystick Aiming Controls**: swap the sticks during stationary aiming. Normal stick controls return when you leave aiming.
- **Four additional game aspect ratios** under **Settings → Graphics**: **16:9, 16:10, 3:2 and 21:9**, alongside Original and Expand.
- **Manual resolution scaling** with quarter-step choices for finer control over rendering resolution.
- Added compatibility with more Android devices.

### Changed

- Renamed **Aspect Ratio** to **Game Aspect Ratio**, with **Expand** first. The launcher and settings continue filling the display.
- HUD Placement choices are now **Auto**, **Classic 4:3** and **16:9**. Auto follows the selected game image; Classic 4:3 keeps the original, more central positions; 16:9 limits the HUD spread within wider images. Auto is the default for new settings, while existing saved choices are preserved.
- Put **Display** first in **Framerate** and **Auto** first in **Resolution**.
- Renamed **Aiming Invert** to **Invert Aiming y-axis**, preserving the existing preference.
- Moved the **Touch** settings tab between **Controls** and **Sound**.
- Clarified descriptions for flight, Breegull Blaster controls and Save Management.

### Fixed

- Camera shaking during turns in narrow areas.
- The screen shifting sideways when entering dialogue.
- Visible gaps between animated character limbs at Display framerate.
- Character distortion that could appear after extended play.
- Bouncing opening logos at Display framerate.
- Missing or misplaced HUD values, counters and countdown timers, including Jiggy totals, egg ammunition and feathers.
- HUD icon/value alignment and placement across the available aspect ratios.
- Scenery and characters disappearing near the edges of the new fixed aspect ratios.
- Rotating Jiggy transitions disappearing before the animation fully clears the screen edges.
- Missing water and stray lines/polygons in Jolly Roger’s Lagoon.
- Flight steering and airborne aiming responding to different inversion settings.
- Jiggy Challenge controls being affected by stationary aiming stick swapping.
- A Graphics-menu controller-navigation crash encountered around MSAA changes.

### Updating

Install this update over your existing **release app** to retain its data. The separate Debug app remains independent.

Custom Turnip drivers remain an option for compatible **Adreno** devices. **Mali** users should keep **System Default**.

A user-supplied supported USA ROM is still required.
