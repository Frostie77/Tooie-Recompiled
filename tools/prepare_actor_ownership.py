#!/usr/bin/env python3
"""Extract exact generated actor consumers and check hook/config parity."""
from pathlib import Path
import re
import os
import subprocess
import sys
import tomllib
from lookup_source import without_lookup_lines

root, destination = map(Path, sys.argv[1:])
functions = {}
for source in (root / "RecompiledFuncs").glob("funcs_*.c"):
    text = source.read_text()
    for match in re.finditer(r"^RECOMP_FUNC void (\w+)\(.*?(?=^RECOMP_FUNC |\Z)", text, re.M | re.S):
        name = match[1]
        assert name not in functions, name
        functions[name] = match[0]

config = tomllib.loads((root / "banjotooie.us.toml").read_text())
hooks = [h for h in config["patches"]["hook"] if "tooie_rt64_actor_" in h["text"]]
assert len(hooks) == 6
for hook in hooks:
    body = functions[hook["func"]]
    call = re.search(r"tooie_rt64_actor_\w+\([^;]+;", hook["text"])[0]
    assert "#if TOOIE_ACTOR_POSE_ENABLED" in hook["text"]
    assert body.split(call, 1)[1].lstrip().startswith("#endif\n    // 0x%08X:" % hook["before_vram"])

selected = ["func_80107C2C", "func_80101808", "func_80101870", "func_801018A4",
            "func_801018D8", "func_801027F4", "func_800DF47C", "func_800EC340",
            "func_800EC360", "func_800EC368", "func_801066C0", "func_800B28CC",
            "func_800B2928", "func_800B2EA0", "func_801065E0", "func_80106608"]
adapters = {"func_80106668": "actor_test_allocate", "func_800EBED4": "actor_test_marker",
            "func_8001BBC8": "actor_test_move", "func_800B2D70": "actor_test_pool",
            "func_800DE448": "actor_test_render", "aligned4_memcpy": "actor_test_move"}
callees = set()
for name in selected:
    callees.update(re.findall(r"\b([A-Za-z_]\w*)\(rdram, ctx\);", functions[name]))
output = ['#include "tooie_recomp.h"', '#include "funcs.h"', '#include <stdlib.h>', 'void do_break(uint32_t vram) {(void)vram;abort();}']
for name in adapters.values():
    output.append(f"void {name}(uint8_t*,recomp_context*);")
for name in sorted(callees - set(selected)):
    body = adapters[name] + "(rdram,ctx);" if name in adapters else "(void)rdram;ctx->r2=0;"
    output.append(f"RECOMP_FUNC void {name}(uint8_t *rdram,recomp_context *ctx) {{{body}}}")
output.extend(functions[name] for name in selected)
destination.write_text("\n".join(output))
print("PASS: six actor hook/config boundaries; exact generated constructor, draw wrappers and array/lifetime consumers extracted")

# With the feature disabled, ordinary generated instruction bodies must remain
# byte-for-byte equivalent after preprocessing with today's shared declarations.
for source in ["funcs_32.c", "funcs_35.c", "funcs_36.c"]:
    parent = destination.parent / ("parent-" + source)
    parent.write_bytes(subprocess.check_output(["git", "-C", str(root), "show", "ffd520a6ac40ed92cedfbc6026f8bda51cdefcb6:RecompiledFuncs/" + source]))
    for profile in [["-D__ANDROID__", "-DTOOIE_ANDROID_ACTOR_INTERPOLATION=0"], ["-DTOOIE_ANDROID_ACTOR_INTERPOLATION=1"]]:
        args = [os.environ.get("CC", "gcc"), "-E", "-P", "-DNDEBUG", *profile,
                "-I"+str(root/"include"), "-I"+str(root/"RecompiledFuncs"), "-I"+str(root/"lib/N64Recomp/include")]
        old = subprocess.check_output([*args, str(parent)], text=True)
        now = subprocess.check_output([*args, str(root/"RecompiledFuncs"/source)], text=True)
        # New diagnostic __LINE__ arguments reflect physical source positions,
        # including disabled hook lines. Preserve every guest operation/target.
        assert re.sub(r"\s+", " ", without_lookup_lines(old)) == re.sub(r"\s+", " ", without_lookup_lines(now)), source
print("PASS: all three generated instruction bodies unchanged with actors OFF on Android and ON-but-compiled-out on desktop")
