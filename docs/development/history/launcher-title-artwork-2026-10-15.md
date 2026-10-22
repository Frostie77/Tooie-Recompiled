# TOOIE launcher title artwork

The upper blue/purple title now reads **TOOIE**. The red **RECOMPILED** line remains below it, with the same lettering style and colour scheme. This uses the built-in image-editing tool with the original launcher wallpaper as its reference.

Updated assets:

- `assets/menu.png` (desktop / native build assets)
- `android/app/src/main/assets/program/assets/menu.png` (Android APK source assets)

Both copies are identical RGB PNGs, **1669 × 942** pixels. The previous image was 1360 × 768. The launcher already uses proportional `cover` scaling, so no layout or menu-button code changes are needed. The full bitmap was generated from the original image; it preserves the composition rather than being a pixel-exact patch outside the lettering.

Source asset Git blob: `61ff9b22d7d72c0fede7186a6a9e714f9a20b6e5` at tested commit `59d3ad0aca6eea957552219b83672fb82fe8a3b2`.

Replacement PNG SHA-256: `e7dc6ec776a14b09ca5108a9f279e7d0ac5e464d908e57a817a3534e7716bde6`.

Android's Activity copies packaged program assets on creation and overwrites the program file. These files do not change the ROM/save directory. The guarded builder copies repository Android assets before filling missing files from its dependency cache, so a cached old wallpaper will not replace this tracked image.

This is a separate artwork commit based on the tested development revision. Include it alongside the rendering fix when preparing the next APK; preserve newer engineering work. No APK build or device operation was performed for this artwork change.

Generation prompt (built-in imagegen):

> Use case: text-localization. Edit target: the attached original 1360 x 768 launcher wallpaper (not a photograph). Make one precise branding edit: replace the entire upper blue/purple title 'BANJO 2' with the exact five-letter word 'TOOIE' (T O O I E), centered above the lower line, fitting the upper title's existing area. Match the original chunky, irregular, playful Banjo-style cartoon lettering, slight varied letter angles, deep blue-purple fill with lighter round spots, thick bright yellow/gold outline, beveled golden side shading and soft white outer glow. Preserve the original title's overall visual weight and footprint. CRITICAL INVARIANTS: keep the red lower word 'RECOMPILED' exactly where it is, with the identical lettering, texture, size, gold outline and appearance. Keep Mumbo, Banjo, Kazooie, their faces and bodies, the feather, cloudy blue background, framing, colors and all pixels outside the upper title edit unchanged. Preserve the original canvas dimensions and layout. No menu buttons, no extra words, no 'BANJO', no numeral 2, no watermarks, no redesign of the rest of the wallpaper. Output a finished replacement wallpaper.
