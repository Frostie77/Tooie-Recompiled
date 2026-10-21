#!/usr/bin/env python3
"""Prepare four bounded bsfirstp consumers from the Step 108 USA ROM excerpts.

Normal mode only creates test/generator artifacts in the requested directory.
--check verifies generated output against production; --apply additionally
updates just the four original/already-correct bodies after validating all four.
"""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import sys

PARENT = '0516264894203cc3b0ffd377bfeffbd8134dde8e'
FIXTURE_SHA = '7bc7cd52c090d8da384833f342c9f9bfca6803515ebf72dbe0fd4d6f94612acd'


def audit(source):
    """Scoped candidate scan; exact-parent positive control prevents false green."""
    functions=[];candidates=[]
    for m in re.finditer(r'^RECOMP_FUNC void (\w+)\(.*?(?=^RECOMP_FUNC |\Z)',source,re.M|re.S):
        name,body=m[1],m[0]
        if not (name.endswith('_bsfirstp') or name.startswith('bsfirstp_entrypoint_')):continue
        functions.append(name)
        for reg,off in re.findall(r'ctx->r(\d+) = MEM_W\(ctx->r1, \(int16_t\)RELOC_LO16\(108, 0X([0-9A-F]+)\)\);',body):
            if int(off,16)&0x8000 and 'sltiu' in body and 'LOOKUP_FUNC(ctx->r'+reg+')' in body:
                candidates.append(name)
    return functions,candidates


def load(root):
    raw = (root/'tests/fixtures/bsfirstp_dispatches_us.json').read_bytes()
    if hashlib.sha256(raw).hexdigest()!=FIXTURE_SHA:raise ValueError('Step 108 USA fixture hash mismatch')
    fixture = json.loads(raw)
    for t in fixture['tables']:
        targets=[int(v,16) for v in t['raw_offsets']]
        start,end=[int(t[k],16) for k in ('function_start_offset','function_end_offset_exclusive')]
        code=struct.pack('>'+str(len(t['code_words_be']))+'I',*[int(v,16) for v in t['code_words_be']])
        if len(targets)!=t['count'] or any(v&3 or not start<=v<end for v in targets):raise ValueError('invalid intra-function targets')
        if hashlib.sha256(struct.pack('>'+str(len(targets))+'I',*targets)).hexdigest()!=t['table_sha256']:raise ValueError('table hash differs')
        if len(code)!=end-start or hashlib.sha256(code).hexdigest()!=t['code_sha256']:raise ValueError('code hash differs')
        gate=int(t['code_words_be'][(int(t['jr_link_address'],16)-0x80800000-start)//4-6],16)
        if gate>>26!=0xB or gate&0xFFFF!=t['count']:raise ValueError('ROM range gate differs from table count')
    return fixture


def labels(body,targets):
    for target in sorted(set(targets)):
        pc=0x80800000+target;label=f'L_{pc:08X}:'
        if label+'\n' in body:continue
        matches=list(re.finditer(rf'^    // 0x{pc:08X}: ',body,re.M))
        if len(matches)!=1:raise ValueError(f'ambiguous/missing case instruction {pc:08X}')
        i=matches[0].start();body=body[:i]+label+'\n'+body[i:]
    return body


def transform(original,t,reference=False):
    jr=int(t['jr_link_address'],16);offset=int(t['table_offset'],16)
    targets=[int(v,16) for v in t['raw_offsets']]
    m=re.search(r'    LOOKUP_FUNC\(ctx->r(\d+)\)\(rdram, ctx\);\n    return;',original)
    if not m or len(re.findall(r'LOOKUP_FUNC',original))!=1:raise ValueError('indirect dispatch preimage differs')
    reg=m[1]
    if reference:
        switch=f'    switch ((uint32_t)ctx->r{reg} - (uint32_t)section_addresses[108]) {{\n'+''.join(f'        case 0x{v:X}: goto L_{0x80800000+v:08X};\n' for v in sorted(set(targets)))
        body=original.replace(t['function']+'(',t['function']+'_reference(',1)
    else:
        switch=f'    switch (jr_addend_{jr:08X} >> 2) {{\n'+''.join(f'        case {i}: goto L_{0x80800000+v:08X}; break;\n' for i,v in enumerate(targets))
        body=original
        # Capture the byte index at this exact addu, not an earlier use of at.
        pat=rf'(    // 0x{jr-8:08X}: [^\n]+\n)(    ctx->r1 = ADD32\(ctx->r1, ctx->r{reg}\);)'
        body,n=re.subn(pat,rf'\g<1>    gpr jr_addend_{jr:08X} = ctx->r{reg};\n\g<2>',body)
        if n!=1:raise ValueError('index-preservation site differs')
        old=f'ctx->r{reg} = MEM_W(ctx->r1, (int16_t)RELOC_LO16(108, 0X{offset:X}));'
        if body.count(old)!=1:raise ValueError('table load site differs')
        body=body.replace(old,old.replace('MEM_W','ADD32'))
    switch+=f'        default: switch_error(__func__, 0x{jr:08X}, 0x{0x80800000+offset:08X});\n    }}'
    return labels(body.replace(m[0],switch),targets)


def prepare(root,out,fixture):
    out.mkdir(parents=True,exist_ok=True)
    sources={f:subprocess.check_output(['git','-C',str(root),'show',PARENT+':RecompiledFuncs/'+f],text=True) for f in ['funcs_44.c','funcs_45.c']}
    metadata=(root/'RecompiledFuncs/recomp_overlays.inl').read_text()
    needed=set();originals=[]
    for t in fixture['tables']:
        matches=[(f,m[0]) for f,s in sources.items() if (m:=re.search(r'^RECOMP_FUNC void '+t['function']+r'\(.*?(?=^RECOMP_FUNC |\Z)',s,re.M|re.S))]
        if len(matches)!=1:raise ValueError('parent consumer is not unique')
        f,body=matches[0]
        if hashlib.sha256(body.encode()).hexdigest()!=t['function_normalized_lf_sha256']:raise ValueError('parent consumer hash differs')
        originals.append((f,body));needed.update(re.findall(r'\b(\w+)\(rdram, ctx\)',body));needed.add(t['function'])
        (out/(t['function']+'-original.inc')).write_text(body)
        (out/(t['function']+'-reference.inc')).write_text(transform(body,t,True))
    lines=['context.sections.resize(855);context.rom.resize(0x1F4BE90+0xA5B0+4);std::vector<size_t> selected;']
    for rom,ram,size,index in re.findall(r'\{ \.rom_addr = (0x[\dA-F]+), \.ram_addr = (0x[\dA-F]+), \.size = (0x[\dA-F]+),.*?\.index = (\d+) \}',metadata):
        if int(index) not in (2,3,108):continue
        lines.append(f'context.sections[{index}].ram_addr={ram};context.sections[{index}].rom_addr={rom};context.sections[{index}].size={size};context.sections[{index}].relocatable='+('true;' if int(index)>=4 else 'false;'))
    names={t['function'] for t in fixture['tables']}
    resolved=set()
    for index,block in re.findall(r'static FuncEntry section_(\d+)_\w+_funcs\[\] = \{(.*?)\};',metadata,re.S):
        for name,off,size in re.findall(r'\.func = (\w+), \.offset = (0x[\dA-F]+), \.rom_size = (0x[\dA-F]+)',block):
            if name not in needed:continue
            if int(index) not in (2,3,108):raise ValueError('callee requires additional section metadata')
            resolved.add(name)
            if name in names:lines.append('selected.push_back(context.functions.size());')
            lines.extend([f'context.functions_by_vram[context.sections[{index}].ram_addr+{off}].push_back(context.functions.size());',f'context.functions.emplace_back(context.sections[{index}].ram_addr+{off},context.sections[{index}].rom_addr+{off},std::vector<uint32_t>{{0}},"{name}",{index});'])
    if resolved!=needed:raise ValueError('callee metadata is incomplete')
    block=re.search(r'static RelocEntry section_108_\w+_relocs\[\] = \{(.*?)\};',metadata,re.S)[1]
    for off,target,section,kind in re.findall(r'\.offset = (0x[\dA-F]+), \.target_section_offset = (0x[\dA-F]+), \.target_section = (\d+), \.type = (\w+)',block):
        if any(int(t['function_start_offset'],16)<=int(off,16)<int(t['function_end_offset_exclusive'],16) for t in fixture['tables']):
            lines.append(f'context.sections[108].relocs.push_back({{0x80800000+{off},{target},0,{section},N64Recomp::RelocType::{kind},false}});')
    # Populate exactly the supplied data/code words. Reject overlapping inputs
    # that disagree; table slots may also appear as referenced initialized data.
    words={}
    def put(off,value):
        if off in words and words[off]!=value:raise ValueError('inconsistent overlapping USA words')
        words[off]=value
    for t in fixture['tables']:
        start=int(t['function_start_offset'],16)
        for i,v in enumerate(t['code_words_be']):put(start+i*4,int(v,16))
        for i,v in enumerate(t['raw_offsets']):put(int(t['table_offset'],16)+i*4,int(v,16))
        for d in t['referenced_initialized_data_words']:put(int(d['offset'],16),int(d['word_be'],16))
    lines.append('const uint32_t words[][2]={'+','.join('{'+hex(k)+','+hex(v)+'}' for k,v in sorted(words.items()))+'};')
    lines.append('for(auto &w:words){uint32_t v=byteswap(w[1]);std::memcpy(context.rom.data()+0x1F4BE90+w[0],&v,4);}')
    for t in fixture['tables']:
        lines.append('for(size_t i:selected)if(context.functions[i].name=="'+t['function']+'"){auto &f=context.functions[i];f.words.resize('+str(len(t['code_words_be']))+');std::memcpy(f.words.data(),context.rom.data()+f.rom,f.words.size()*4);}')
    ends=''.join('if(context.functions[i].name=="'+t['function']+'")context.rom.resize(0x1F4BE90+'+str(int(t['table_offset'],16)+4*t['count'])+');' for t in fixture['tables'])
    (out/'generate.cpp').write_text('''#include "recompiler/context.h"
#include "rabbitizer.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
int main(int argc,char **argv){if(argc!=2)return 2;
RabbitizerConfig_Cfg.pseudos.pseudoMove=false;RabbitizerConfig_Cfg.pseudos.pseudoBeqz=false;
RabbitizerConfig_Cfg.pseudos.pseudoBnez=false;RabbitizerConfig_Cfg.pseudos.pseudoNot=false;RabbitizerConfig_Cfg.pseudos.pseudoBal=false;
N64Recomp::Context context{};
'''+ '\n'.join(lines)+'''
std::vector<std::vector<uint32_t>> statics(855);const auto supplied=context.rom;
for(size_t i:selected){context.rom=supplied;
// The ROM excerpt ends at this table's verified instruction range gate. Do not
// fabricate an unknown following data word as a table-terminating sentinel.
'''+ends+'''
std::ostringstream output;if(!N64Recomp::recompile_function(context,i,output,statics,false))return 1;
std::ofstream(std::filesystem::path(argv[1])/(context.functions[i].name+"-fixed.inc"))<<output.str();}
}
''')
    (out/'callees.inc').write_text('\n'.join('extern "C" void '+n+'(uint8_t*r,recomp_context*c){call("'+n+'",r,c);}' for n in sorted(needed-names)))
    configs=[]
    for t in fixture['tables']:
        code=[int(v,16) for v in t['code_words_be']];frame=(-code[0])&0xFFFF
        reg=int(re.search(r'LOOKUP_FUNC\(ctx->r(\d+)\)',originals[len(configs)][1])[1])
        configs.append('{"'+t['function']+'",'+t['function']+','+t['function']+'_reference,'+hex(frame)+','+str(reg)+','+hex(int(t['table_offset'],16))+',{'+','.join('0x'+v for v in t['raw_offsets'])+'}}')
    (out/'configs.inc').write_text(',\n'.join(configs))
    (out/'data.inc').write_text(','.join('{'+hex(int(d['offset'],16))+','+hex(int(d['word_be'],16))+'}' for t in fixture['tables'] for d in t['referenced_initialized_data_words'] if int(d['offset'],16)!=int(t['table_offset'],16)))
    return originals


def check(root,out,fixture,apply):
    edits={}
    for t in fixture['tables']:
        name=t['function'];old=(out/(name+'-original.inc')).read_text();new=(out/(name+'-fixed.inc')).read_text()
        if new.strip()!=transform(old,t).strip():raise ValueError('unexpected real generator delta: '+name)
        found=[]
        for f in ['funcs_44.c','funcs_45.c']:
            p=root/'RecompiledFuncs'/f;data=edits.get(p,p.read_bytes());text=data.decode().replace('\r\n','\n')
            m=re.search(r'^RECOMP_FUNC void '+name+r'\(.*?(?=^RECOMP_FUNC |\Z)',text,re.M|re.S)
            if m:found.append((p,data,m[0]))
        if len(found)!=1:raise ValueError('production function is not unique')
        p,data,current=found[0]
        if apply:
            if current.strip() not in (old.strip(),new.strip()):raise ValueError('unrelated production edits')
            pre=current.replace('\n','\r\n').encode();post=(new.rstrip()+'\n').replace('\n','\r\n').encode()
            if data.count(pre)!=1:raise ValueError('CRLF/preimage differs')
            edits[p]=data.replace(pre,post,1)
        elif current.strip()!=new.strip():raise ValueError('production output is stale: '+name)
    # Nothing is written until every input and delta has passed validation.
    for p,data in edits.items():p.write_bytes(data)
    print('PASS: all four real generator outputs match only permitted switch/index/load/label deltas and production consumers')


if __name__=='__main__':
    root,out=map(Path,sys.argv[1:3]);fixture=load(root)
    if '--check' in sys.argv or '--apply' in sys.argv:check(root,out,fixture,'--apply' in sys.argv)
    else:prepare(root,out,fixture)
