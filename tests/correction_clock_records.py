#!/usr/bin/env python3
"""Validate retained clocks and export exact sampled spans to a native fixture.

Input payloads/gate tables are not captured. Native tests construct those
explicitly; sparse detail records cannot replay complete aggregate windows.
"""
from pathlib import Path
import hashlib
import sys

root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))
from replay_correction_trace import replay, read

def main():
    path=Path(sys.argv[1]);out=Path(sys.argv[2]);data=path.read_bytes()
    assert hashlib.sha256(data).hexdigest()=='4d10ad126f79d7675dfee1941432bb02c32170d9d18bffd041a7e411d33aa679','not the retained Step 86 clock fixture'
    parsed=replay(path);windows=parsed['clock_windows'];events=[]
    for line in read(path).splitlines():
        if 'Runtime correction-clock-event:' not in line:continue
        f=dict(w.split('=',1) for w in line.split() if '=' in w)
        assert int(f['pid'])==parsed['pid'] and f['site']=='0x80012458'
        assert f['mode'] in ('0/0','2/2') and f['reset']=='0'
        mode=int(f['mode'].split('/')[0]);before,after=map(int,f['index'].split('/'));counts=list(map(int,f['count'].split('/')))
        assert counts[0]==counts[1] and f['map'].split('/')[0]==f['map'].split('/')[1]
        result=int(f['replay_result']);span=int(f['recorded_span'])
        assert mode==0 and result==-1 or mode==2 and result in (0,1)
        events.append((mode,int(f['elapsed_span']),span,int(f['applied_span']),int(f['delta_bits'],16),before,after,counts[0],result))
    pure=[];ordinary=[]
    for w in windows:
        c=w['clock'];b=w['buckets']
        assert int(c['pid'])==parsed['pid'] and int(c['invalid'])==0
        assert c['constants_bits']=='3D4CCCCE/3C23D70A/3C888889/3C888889' and c['tv_type']=='1'
        if w['startup_or_short'] or len(b)!=1:continue
        x=b[0]
        if x['caller']=='0' and x['mode_bucket']=='2' and all(int(x[k])==0 for k in ('ends','resets','transitions')) and int(c['attract_reset_calls'])==0 and int(x['records'])==int(x['events'])==int(x['index_advances']):pure.append(w)
        if x['caller']=='0' and x['mode_bucket']=='0' and int(x['records'])==0:ordinary.append(w)
    assert len(windows)==135 and sum(len(w['buckets']) for w in windows)==140 and len(events)==1105 and len(pure)==10 and len(ordinary)==120
    total=lambda records,key:sum(float(w['clock'][key]) for w in records)
    elapsed=total(pure,'elapsed_us')/1e6
    ratio=total(pure,'timer_delta_sum')/elapsed;attract=total(pure,'attract_delta_sum')/elapsed
    ordinary_ratio=total(ordinary,'timer_delta_sum')/(total(ordinary,'elapsed_us')/1e6)
    assert abs(elapsed-50.116535)<1e-6 and abs(ratio-1.383110)<1e-6 and abs(attract-1.460925)<1e-6
    assert abs(ordinary_ratio-1.000415)<1e-6
    header='struct captured_clock { unsigned mode; int elapsed; unsigned span,applied,bits,index,next,count; int result; };\n'
    header+='static const struct captured_clock captured_clocks[]={\n'
    header+=''.join('{%s},\n'%','.join(str(x) for x in e) for e in events)+'};\n'
    out.write_text(header)
    print(f'PASS: PID {parsed["pid"]}; 135 windows/140 buckets/1105 sparse details; 10 pure replay windows {elapsed:.6f}s timer/wall={ratio:.6f} attract/wall={attract:.6f}; 120 ordinary timer/wall={ordinary_ratio:.6f}; intended cadence unverified')

if __name__=='__main__':main()
