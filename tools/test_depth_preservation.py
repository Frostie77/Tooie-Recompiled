#!/usr/bin/env python3
"""Replay the recorded decal/water sequence through the real RT64 depth selector."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def balanced(source, start):
    first = source.index('{', start)
    level = 1
    end = first + 1
    while level:
        level += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    renderer = (ROOT / 'lib/rt64/src/render/rt64_framebuffer_renderer.cpp').read_text()
    first = renderer.index('const interop::OtherMode otherMode = triangles.shaderDesc.otherMode;')
    end = renderer.index('if (previousScissor != triangles.scissor)', first)
    selector = renderer[first:end]
    header = (ROOT / 'lib/rt64/src/shared/rt64_other_mode.h').read_text()
    mode = balanced(header, header.index('struct OtherMode')) + ';'
    fixture = json.loads((ROOT / 'tests/fixtures/odin-water-depth-20261029.json').read_text())
    assert [int(c['call']) for c in fixture['calls']] == list(range(138, 148))
    rows = []
    for call in fixture['calls']:
        high, low = call['other_mode'].split('/')
        rows.append('{' + ','.join([call['call'], low, high, call['z_compare'],
                                  call['z_write'], call['z_mode']]) + '}')
    source = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "shared/rt64_f3d_defines.h"
using uint = uint32_t;
#define constmethod const
namespace interop { @@MODE@@ }
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAILED line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
struct Recorded { unsigned call, low, high, compare, write, zmode; };
int main() {
    const Recorded calls[] = { @@ROWS@@ };
    bool depthState = false;
    unsigned readRequests = 0, writeRequests = 0, readOnlyTests = 0;
    auto switchToDepthRead = [&]() { depthState = true; ++readRequests; };
    auto switchToDepthWrite = [&]() { depthState = false; ++writeRequests; };
    struct { struct { interop::OtherMode otherMode; } shaderDesc; } triangles;
    for (const auto &call : calls) {
        triangles.shaderDesc.otherMode = {call.low, call.high};
        CHECK(triangles.shaderDesc.otherMode.zCmp() == bool(call.compare));
        CHECK(triangles.shaderDesc.otherMode.zUpd() == bool(call.write));
        CHECK(triangles.shaderDesc.otherMode.zMode() == call.zmode);
        @@SELECTOR@@
        if (call.call == 138) CHECK(!depthState);
        if (call.call == 139) CHECK(depthState && call.zmode == ZMODE_DEC);
        if (call.call >= 140) {
            CHECK(depthState && call.compare && !call.write && call.zmode != ZMODE_DEC);
            ++readOnlyTests;
        }
    }
    CHECK(readRequests == 1 && writeRequests == 1 && readOnlyTests == 8);
    std::puts("PASS: ten consecutive captured calls through actual OtherMode and RT64 depth selection; eight later depth comparisons remain in the read-only pass. No GPU/device execution.");
}
'''.replace('@@MODE@@', mode).replace('@@ROWS@@', ',\n'.join(rows)).replace('@@SELECTOR@@', selector)
    compiler = os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('C++ compiler required')
    with tempfile.TemporaryDirectory(prefix='tooie-depth-selector-') as directory:
        directory = Path(directory)
        path = directory / 'consumer.cpp'
        path.write_text(source)
        binary = directory / 'consumer'
        subprocess.run([compiler, '-std=c++17', '-I' + str(ROOT / 'lib/rt64/src'),
                        str(path), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
