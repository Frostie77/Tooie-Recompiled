"""Reassemble the exact parent consumer; regenerate using the Step 105 USA table.

Instructions come from retained generated source, not a downloaded ROM. All 47
table offsets come from the user's hash-identified USA ROM extraction.
"""
from pathlib import Path
import re
import hashlib
import json
import struct
import subprocess
import sys

root, out = map(Path, sys.argv[1:])
table = json.loads((root/'tests/fixtures/bsfirstp_table_us.json').read_text())
targets = [int(v,16) for v in table['raw_offsets']]
assert len(targets)==47 and all(not v&3 and 0x5994<=v<0x5F48 for v in targets)
assert hashlib.sha256(struct.pack('>47I',*targets)).hexdigest()==table['table_sha256']=='57810e0d7399ae57ef9f0e261ea4c315875400ee67c8ff91174e20751fcd2cd3'
source = subprocess.check_output(['git','-C',str(root),'show',
    '9fbd60f2455edd23a555df2beaa8fdce179716fa:RecompiledFuncs/funcs_44.c'],text=True)
body = re.search(r'^RECOMP_FUNC void bsfirstp_entrypoint_26\(.*?(?=^RECOMP_FUNC )', source, re.M|re.S)[0]
(out/'original.inc').write_text(body)
instructions = {}
for pc, instruction in re.findall(r'// 0x([0-9A-F]+): (.*)', body):
    pc = int(pc,16)
    assert pc not in instructions or instructions[pc] == instruction
    instructions[pc] = instruction
assert list(instructions) == list(range(0x80805994,0x80805F48,4))
(out/'consumer.s').write_text('.set noat\n.set noreorder\n.set mips3\n.text\n' +
    '\n'.join('L_%08X: %s' % pair for pair in instructions.items())+'\n')
subprocess.run(['clang-19','-target','mips-unknown-none','-mno-abicalls','-c',str(out/'consumer.s'),'-o',str(out/'consumer.o')],check=True)
elf = (out/'consumer.o').read_bytes()
offset = struct.unpack_from('>I',elf,32)[0]
size, count, names_index = struct.unpack_from('>HHH',elf,46)
headers = [struct.unpack_from('>10I',elf,offset+i*size) for i in range(count)]
names = elf[headers[names_index][4]:headers[names_index][4]+headers[names_index][5]]
text = [h for h in headers if names[h[0]:].split(b'\0',1)[0] == b'.text']
assert len(text)==1
words = elf[text[0][4]:text[0][4]+0x5B4]
assert len(words)==0x5B4
# Original object has section-relative branches and unrelocated overlay calls.
# These known words prevent assembler expansion/endian mistakes at this site.
assert struct.unpack_from('>5I',words,0x59EC-0x5994)==(0x3C010001,0x002F0821,0x8C2FA494,0x01E00008,0)
assert struct.unpack_from('>8I',words,0x5AE0-0x5994)==(0x0C001FBB,0x02002025,0x0C000697,0x02002025,0x240E0002,0x10000109,0xAFAE00EC,0x0C028D01)
metadata = (root/'RecompiledFuncs/recomp_overlays.inl').read_text()
needed = set(re.findall(r'\b(\w+)\(rdram, ctx\)',body)) | {'bsfirstp_entrypoint_26'}
lines = ['context.sections.resize(855); context.rom.resize(0x1F56400);']
for rom, ram, length, index in re.findall(r'\{ \.rom_addr = (0x[\dA-F]+), \.ram_addr = (0x[\dA-F]+), \.size = (0x[\dA-F]+),.*?\.index = (\d+) \}',metadata):
    if int(index) not in (3,108):continue
    lines.append(f'context.sections[{index}].ram_addr={ram}; context.sections[{index}].rom_addr={rom}; context.sections[{index}].size={length};context.sections[{index}].relocatable='+('true;' if int(index)==108 else 'false;'))
for index, block in re.findall(r'static FuncEntry section_(\d+)_\w+_funcs\[\] = \{(.*?)\};',metadata,re.S):
    if int(index) not in (3,108):continue
    for name, offset, length in re.findall(r'\.func = (\w+), \.offset = (0x[\dA-F]+), \.rom_size = (0x[\dA-F]+)',block):
        if name not in needed:continue
        if name=='bsfirstp_entrypoint_26':lines.append('size_t selected=context.functions.size();')
        lines.extend([f'context.functions_by_vram[context.sections[{index}].ram_addr+{offset}].push_back(context.functions.size());',
            f'context.functions.emplace_back(context.sections[{index}].ram_addr+{offset},context.sections[{index}].rom_addr+{offset},std::vector<uint32_t>{{0}},"{name}",{index});'])
block = re.search(r'static RelocEntry section_108_\w+_relocs\[\] = \{(.*?)\};',metadata,re.S)[1]
for offset, target, section, kind in re.findall(r'\.offset = (0x[\dA-F]+), \.target_section_offset = (0x[\dA-F]+), \.target_section = (\d+), \.type = (\w+)',block):
    if 0x5994<=int(offset,16)<0x5F48:
        lines.append(f'context.sections[108].relocs.push_back({{0x80800000+{offset},{target},0,{section},N64Recomp::RelocType::{kind},false}});')
lines.extend(['context.functions[selected].words={'+','.join(hex(struct.unpack('<I',words[j:j+4])[0]) for j in range(0,len(words),4))+'};',
    'std::memcpy(context.rom.data()+0x1F4BE90+0x5994,context.functions[selected].words.data(),0x5B4);'])
for i,target in enumerate(targets):lines.append(f'{{uint32_t value=byteswap({target});std::memcpy(context.rom.data()+0x1F4BE90+0xA494+{i*4},&value,4);}}')
(out/'generate.cpp').write_text('''#include "recompiler/context.h"
#include "rabbitizer.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
int main(int argc,char **argv){
 if(argc!=2)return 2;
 RabbitizerConfig_Cfg.pseudos.pseudoMove=false;
 RabbitizerConfig_Cfg.pseudos.pseudoBeqz=false;
 RabbitizerConfig_Cfg.pseudos.pseudoBnez=false;
 RabbitizerConfig_Cfg.pseudos.pseudoNot=false;
 RabbitizerConfig_Cfg.pseudos.pseudoBal=false;
 N64Recomp::Context context{};
'''+ '\n'.join(lines)+'''
 std::vector<std::vector<uint32_t>> statics(855);std::ostringstream output;
 if(!N64Recomp::recompile_function(context,selected,output,statics,false))return 1;
 std::ofstream(argv[1])<<output.str();
}
''')
# Peripheral callee adapters record arguments and control return/output branches.
(out/'callees.inc').write_text('\n'.join('extern "C" void '+name+'(uint8_t*r,recomp_context*c){call("'+name+'",r,c);}' for name in sorted(needed-{'bsfirstp_entrypoint_26'})))
(out/'targets.inc').write_text(','.join(hex(v) for v in targets))
