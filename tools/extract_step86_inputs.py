#!/usr/bin/env python3
"""Recreate numeric regression fixtures from the original Step 86 NewLog.

No ROM, APK or device access. Reject another capture rather than relabel its
provenance, and never overwrite different existing output files.
"""
import argparse
import hashlib
from pathlib import Path
from replay_correction_trace import replay

RAW_SHA256='b332320429ddfa1ca389f609cddec829252ec0c2b55a1b991d21e641aa58afe4'
SAMPLES={9223372036854805092,9223372036854793415,9223372036854827803}
OUTPUTS={
    'odin-step86-hierarchy.txt':'400b89118feb428da08399c85f23c83386da1b90adf9812041a00cdae0953589',
    'odin-step86-clocks.txt':'4d10ad126f79d7675dfee1941432bb02c32170d9d18bffd041a7e411d33aa679',
}

def extract(path):
    raw=Path(path).read_bytes()
    if hashlib.sha256(raw).hexdigest()!=RAW_SHA256:raise ValueError('not the original Step 86 NewLog')
    joined=replay(path);ids=set();builders=set();models=set()
    for g in joined['triangles']:
        if g['sample'] not in SAMPLES or g['category']!='player':continue
        for corner in g['ownership']:
            for endpoint in ['previous','current']:
                o=corner[endpoint]
                if o['status']!='copied-bone-inputs':raise ValueError('missing copied endpoint')
                ids.add(int(o['binding']['id']));models.add(int(o['model']['id']));builders.add(o['builder_id'])
    if len(builders)!=6:raise ValueError('missing retained builder pairs')
    ids|=models|builders
    lines=raw.splitlines(keepends=True)
    def records():
        for line in lines:
            text=line.decode('utf-8')
            yield line,text,dict(w.split('=',1) for w in text.split() if '=' in w)
    for line,text,f in records():
        if 'Runtime correction-source:' in text and f.get('kind')=='bone-data' and int(f['builder']) in builders:ids.add(int(f['id']))
    hierarchy=[];clocks=[]
    for line,text,f in records():
        keep=('Runtime correction-source:' in text or 'Runtime correction-data:' in text) and int(f.get('id','0')) in ids
        keep|=any(k in text for k in ['Runtime geometry:','Runtime correction-link:','Runtime correction-sample:']) and int(f.get('sample','0')) in SAMPLES
        if keep:hierarchy.append(line)
        if any(k in text for k in ['Runtime correction-clock:','Runtime correction-clock-bucket:','Runtime correction-clock-event:','Runtime correction-rom:']):clocks.append(line)
    return {'odin-step86-hierarchy.txt':b''.join(hierarchy),'odin-step86-clocks.txt':b''.join(clocks)}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
    outputs=extract(a.log)
    # Validate all outputs before creating or replacing anything.
    for name,data in outputs.items():
        if hashlib.sha256(data).hexdigest()!=OUTPUTS[name]:raise ValueError('fixture hash mismatch: '+name)
        target=a.output/name
        if target.exists() and target.read_bytes()!=data:raise ValueError('different existing output: '+str(target))
    a.output.mkdir(parents=True,exist_ok=True)
    for name,data in outputs.items():
        (a.output/name).write_bytes(data);print(f'{name}: {len(data)} bytes, SHA-256 {OUTPUTS[name]}')

if __name__=='__main__':main()
