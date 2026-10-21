#!/usr/bin/env python3
"""Verify the approved layers, mirrored bytes, layout and registration route."""
from pathlib import Path
import hashlib
import json
import os
import shlex
import struct
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]

def main():
    manifest=json.loads((ROOT/'assets/launcher-layout.json').read_text())
    for name,digest,size in (
        ('menu.png','3c125d58ad008e837f3c7dc335133d03bb9aae19086ffbde90d8a71daa52c707',(1672,941)),
        ('menu-recompiled.png','e76b97de0c685566db2505e85d42052a520c8f31237cbfdbfe405a1706295269',(2203,714))):
        data=(ROOT/'assets'/name).read_bytes()
        assert data[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',data[16:24])==size
        assert hashlib.sha256(data).hexdigest()==digest, name
        assert (ROOT/'android/app/src/main/assets/program/assets'/name).read_bytes()==data
    assert (ROOT/'assets/launcher-layout.json').read_bytes()==(ROOT/'android/app/src/main/assets/program/assets/launcher-layout.json').read_bytes()
    assert manifest['wordmark']=={'asset':'recompiled-wordmark.png','left_percent':7,'top_percent':43.5,'width_percent':74,'height':'preserve source aspect ratio','rotation_degrees':-4,'transform_origin':'center'}
    source=(ROOT/'src/main.cpp').read_text()
    assert 'queue_image_from_bytes_file(' in source
    assert 'context.create_element<Wallpaper>(this, "menu.png", kWallpaperImage);' in source
    assert 'context.create_element<Wallpaper>(this, "menu-recompiled.png", kLauncherWordmark);' in source
    assert 'create_element<LauncherArtwork>(background, menu)' in source
    assert 'title->set_rotation(tooie::launcher::titleRotation)' in source
    assert 'set_pointer_events(recompui::PointerEvents::None)' in source
    for label in ('add_start_game_or_load_rom_option','add_setup_controls_option','add_settings_option','add_mods_option','add_exit_option'):
        assert label in source
    with tempfile.TemporaryDirectory(prefix='tooie-launcher-layout-') as temp:
        binary=Path(temp)/'layout'
        subprocess.run([*shlex.split(os.environ.get('CXX','g++')),'-std=c++20','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(ROOT/'src'),str(ROOT/'tests/launcher_layout.cpp'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
    print('PASS: exact approved launcher PNG layers, Android mirrors, registration route and preserved menu actions; no device-rendering claim')

if __name__=='__main__':main()
