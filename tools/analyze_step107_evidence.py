#!/usr/bin/env python3
"""Pinned read-only Step 107 evidence reader; JSON stdout, no device/extraction."""
from pathlib import Path, PurePosixPath
from collections import Counter
import datetime as dt
import hashlib
import json
import re
import statistics
import sys
import zipfile

SHA='00bd8ac3b07ba30a9d2063e437b46ec6b6b4054afba303e506e54e5b7ce2fb9f'
PID='24004'
STEM='Tooie-08b6400c781b-First-Person-Odin-20261019-222326-e4f1c0b8'
ZONE=dt.timezone(dt.timedelta(hours=11))


def fields(line):return dict(re.findall(r'\b([\w-]+)=([^\s]+)',line))


def analyze(path):
    raw=path.read_bytes();assert len(raw)==20606083 and hashlib.sha256(raw).hexdigest()==SHA
    with zipfile.ZipFile(path) as z:
        names=z.namelist();assert len(names)==len(set(names))==211 and z.testzip() is None
        assert all(not PurePosixPath(n).is_absolute() and '..' not in PurePosixPath(n).parts and '\\' not in n for n in names)
        def data(s):return z.read(STEM+'-'+s+'.txt')
        def text(s):return data(s).decode('utf-8')
        baseline,new,full=(data(s) for s in ['Baseline','NewLog','FullLog'])
        assert full==baseline+new
        assert [len(b) for b in [baseline,new,full]]==[68138440,2144354,70282794]
        assert hashlib.sha256(baseline).hexdigest()=='1942c388e153b623bc9dafc1da9d03e34ece8709fcd957d9981e46823ce05b81'
        assert hashlib.sha256(full).hexdigest()=='a2ce0f94d471f98f72c7db77ca6d242466b2486e98922326db274e5625d90403'
        assert hashlib.sha256(new).hexdigest()=='62aad3b8198ca9454c944c2244971aed077551a9999af00a6b0a7a6114528340'
        pids=Counter(re.findall(rb'\bpid=(\d+)\b',new));assert pids=={b'24004':11273}
        identity=dict(line.split('=',1) for line in text('Identity').splitlines() if '=' in line)
        assert identity['commit']==identity['native_source_commit']=='08b6400c781b2e081ec0ea874272b214ae86f836'
        assert identity['engineering_ancestor']=='0516264894203cc3b0ffd377bfeffbd8134dde8e'
        assert text('Installed-APK-SHA256').split()[0]==identity['hash']=='254fd0357edfe73bef57bfccfa1a44799285b9c2df1620e855eca85665f3a6ec'
        assert identity['build_id']=='123dbafc131464fefa8236f93fb24a97d3cacef0'
        assert text('Original-Launch-PID').strip()==PID and not text('Final-PIDs').strip()
        lines=new.decode().splitlines()
        lookup=[l for l in lines if re.match(r'^Runtime (?:guest-fatal|lookup-[a-z-]+):',l)]
        assert len(lookup)==124 and lookup==text('Guest-Lookup-Diagnostics').splitlines()
        assert all(fields(l)['pid']==PID and fields(l)['tid']=='24241' for l in lookup)
        fatal=fields(lookup[0]);assert lookup[0].startswith('Runtime guest-fatal:')
        assert fatal['guest_address']=='0x801EEA14' and fatal['context']=='func_80800F14_bsfirstp'
        section=[fields(l) for l in lookup if 'lookup-section:' in l and fields(l)['contains']=='1']
        assert len(section)==1 and section[0]['loaded_ram']=='0x801EDA70' and section[0]['target_offset']=='0xFA4'
        assert section[0]['declared_exact']==section[0]['map_present']=='0'
        history=text('Process-History');assert 'Process 24004 exited cleanly (1)' in history
        assert re.findall(r'Start proc (\d+):io.github.banjotooierecompiled',history)==[PID]
        # Derive wall/monotonic alignment from actual Java records, not the
        # handoff's calculated timestamp or its reported means.
        anchors=[]
        for line in text('Logcat').splitlines():
            if 'BanjoTooieSDL: Runtime window:' not in line or ' pid='+PID+' ' not in line:continue
            f=fields(line)
            stamp=re.match(r'^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+ [+-]\d{4})',line)
            if stamp and 'mono_uptime_ms' in f:
                wall=dt.datetime.strptime(stamp[1],'%Y-%m-%d %H:%M:%S.%f %z')
                anchors.append(round(wall.timestamp()*1e6)-int(f['mono_uptime_ms'])*1000)
        # The collector may retain duplicates; deduplicate complete anchors.
        offsets=sorted(set(anchors));offset=int(statistics.median(offsets))
        assert len(offsets)==9 and max(offsets)-min(offsets)==850
        start=dt.datetime.strptime(text('Gameplay-01-Start-Device-Time').strip(),'%Y-%m-%d %H:%M:%S').replace(tzinfo=ZONE)
        lower=round(start.timestamp()*1e6)-offset+1000000;upper=int(fatal['mono_us'])
        assert lower==528805796600
        report={}
        for domain in ['present','vi','audio','workload-stages','render-stages','present-stages','matching-stages','present-queue']:
            rows=[fields(l) for l in lines if l.startswith('Runtime '+domain+':')]
            rows=[f for f in rows if int(f.get('elapsed_us',0))>0 and lower<=int(f['mono_us'])-int(f['elapsed_us']) and int(f['mono_us'])<=upper]
            sums={}
            for f in rows:
                for name,value in f.items():
                    if re.fullmatch(r'\d+/\d+/\d+/\d+',value):
                        n,total,maximum,end=map(int,value.split('/'));s=sums.setdefault(name,[0,0,0]);s[0]+=n;s[1]+=total;s[2]=max(s[2],maximum)
            elapsed=sum(int(f['elapsed_us']) for f in rows)
            d={'intervals':len(rows),'seconds':elapsed/1e6,'stages':{k:{'count':n,'sum_us':total,'max_us':maximum,'mean_ms':total/n/1000 if n else None} for k,(n,total,maximum) in sums.items()}}
            count={'present':'native_present_calls','vi':'vi_iterations','audio':'queue_calls'}.get(domain)
            if count:
                rates=[int(f[count])*1e6/int(f['elapsed_us']) for f in rows]
                d.update({'count':sum(int(f[count]) for f in rows),'weighted_per_second':sum(int(f[count]) for f in rows)*1e6/elapsed,'median_per_second':statistics.median(rates),'min_per_second':min(rates),'max_per_second':max(rates),'max_gap_us':max(int(f['gap_max_us']) for f in rows)})
            report[domain]=d
        app=set()
        for suffix in ['Logcat','Logcat-Dump']:
            app.update(l for l in text(suffix).splitlines() if re.match(r'^\d{4}-\d\d-\d\d \S+ \S+\s+24004\s+',l))
        timeouts=sum('libOpenSLES' in l and 'pthread_mutex_lock_timeout_np' in l for l in app)
        stops=sum('AudioTrack' in l and ' stop(' in l for l in app)
        hardware=[l for l in app if re.search(r'Fatal signal|FATAL EXCEPTION|SIGSEGV|VK_ERROR_DEVICE_LOST',l)]
        guards=[l for l in lines if re.search(r'Descriptor rejected:|Partial color clear rejected:|Transfer clear rejected|VK_ERROR_DEVICE_LOST',l)]
        assert timeouts==5 and stops==19662 and not hardware and not guards
        return {'archive_sha256':SHA,'delta_bytes':[len(baseline),len(new),len(full)],'fresh_pid_tags':11273,'identity':identity,'fatal':fatal,'containing_section':section[0],'failure_counts':dict(Counter(re.match(r'Runtime ([^:]+):',l)[1] for l in lookup)),
                'clock':{'anchors':len(offsets),'offset_us':offset,'spread_us':850,'approx_fatal_wall':dt.datetime.fromtimestamp((upper+offset)/1e6,ZONE).isoformat(),'selection_lower_us':lower,'conservative_start_added_us':1000000},
                'window':json.loads(text('Gameplay-01-Window')),'metrics':report,'observations':{'opensl_mutex_timeouts':timeouts,'deduplicated_audio_track_stop_records_not_dropouts':stops,'app_native_java_device_fatal_records':len(hardware),'strict_gpu_guard_failures':len(guards)},
                'scope':'Original PID only; complete native intervals, no entrance marker or controlled scene/thermal baseline. Nested stages are not additive; presents are not simulation FPS. No device or performance-fix claim.'}


if __name__=='__main__':
    if len(sys.argv)!=2:raise SystemExit('usage: analyze_step107_evidence.py EVIDENCE.zip')
    print(json.dumps(analyze(Path(sys.argv[1])),indent=2))
