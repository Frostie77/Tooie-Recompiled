#!/usr/bin/env python3
"""Validate a single-function generator delta; optionally apply it.

Usage: check_first_person_dispatch.py REPO ORIGINAL.inc GENERATED.inc [--apply]
The normal mode also verifies that committed production output matches.
"""
from pathlib import Path
import hashlib
import json
import re
import struct
import sys


def expected_body(root, original):
    table = json.loads((root/'tests/fixtures/bsfirstp_table_us.json').read_text())
    targets = [int(v,16) for v in table['raw_offsets']]
    if len(targets)!=47 or any(v&3 or not 0x5994<=v<0x5F48 for v in targets):
        raise ValueError('invalid USA intra-function target list')
    if hashlib.sha256(struct.pack('>47I',*targets)).hexdigest()!='57810e0d7399ae57ef9f0e261ea4c315875400ee67c8ff91174e20751fcd2cd3':
        raise ValueError('USA table SHA-256 mismatch')
    old='    LOOKUP_FUNC(ctx->r15)(rdram, ctx);\n    return;'
    if original.count(old)!=1:raise ValueError('original dispatch preimage differs')
    switch='    switch (jr_addend_808059F8 >> 2) {\n'+''.join('        case %d: goto L_%08X; break;\n'%(i,0x80800000+v) for i,v in enumerate(targets))+'        default: switch_error(__func__, 0x808059F8, 0x8080A494);\n    }'
    result=original.replace('    ctx->r1 = ADD32(ctx->r1, ctx->r15);','    gpr jr_addend_808059F8 = ctx->r15;\n    ctx->r1 = ADD32(ctx->r1, ctx->r15);',1).replace('ctx->r15 = MEM_W(ctx->r1, (int16_t)RELOC_LO16(108, 0XA494));','ctx->r15 = ADD32(ctx->r1, (int16_t)RELOC_LO16(108, 0XA494));').replace(old,switch)
    for target in sorted(set(targets)):
        pc=0x80800000+target;label=f'L_{pc:08X}:'
        if label+'\n' in result:continue
        pattern=rf'^    // 0x{pc:08X}: '
        matches=list(re.finditer(pattern,result,re.M))
        if len(matches)!=1:raise ValueError(f'ambiguous/missing case instruction {pc:08X}')
        i=matches[0].start();result=result[:i]+label+'\n'+result[i:]
    return result


def main():
    root, oldpath, newpath=map(Path,sys.argv[1:4])
    old,new=oldpath.read_text(),newpath.read_text()
    if new.strip()!=expected_body(root,old).strip():raise ValueError('unexpected generator delta: preserve all case/epilogue instructions')
    path=root/'RecompiledFuncs/funcs_44.c';data=path.read_bytes()
    text=data.decode().replace('\r\n','\n')
    match=re.search(r'^RECOMP_FUNC void bsfirstp_entrypoint_26\(.*?(?=^RECOMP_FUNC )',text,re.M|re.S)
    if not match:raise ValueError('production consumer absent')
    if '--apply' in sys.argv:
        if match[0].strip() not in (old.strip(),new.strip()):raise ValueError('production consumer contains unrelated edits')
        # This file is CRLF; preserve every byte outside the selected function.
        replacement=(new.rstrip()+'\n').replace('\n','\r\n').encode()
        original=match[0].replace('\n','\r\n').encode()
        if data.count(original)!=1:raise ValueError('production newline/preimage mismatch')
        path.write_bytes(data.replace(original,replacement,1))
    else:
        if match[0].strip()!=new.strip():raise ValueError('committed production consumer is stale; regenerate/apply explicitly')
    print('PASS: complete USA switch matches real emitter and committed consumer; only switch/addend/load and required case labels changed')


if __name__=='__main__':main()
