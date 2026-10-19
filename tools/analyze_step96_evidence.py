#!/usr/bin/env python3
# Read-only replay of the pinned Step 96 evidence analysis supplied with Step 98.
# No collector is imported, no archive member is executed/extracted, and no
# device command or filesystem mutation is performed. Counts are operations,
# rates are native calls, and timestamp alignment remains approximate.
from pathlib import Path
from collections import Counter
import datetime as dt
import hashlib
import json
import re
import statistics
import zipfile
import sys

stem='Tooie-e1fcf938dcc2-Player-Hierarchy-Icon-Odin-20261018-233409-26e252a3'
if len(sys.argv) != 2: raise SystemExit('usage: analyze_step96_evidence.py EVIDENCE.zip')
archive_path=Path(sys.argv[1])
assert hashlib.sha256(archive_path.read_bytes()).hexdigest()=='32721b0ec404042fcb816230de135c4ef49d4681b4195e81403e2ab410d4e529'
domains=('audio','vi','present','config','workload-stages','render-stages','present-stages','vulkan-build','shader-build','interpolation','present-queue','matching-stages','guest-fatal','skeletal-source','skeletal-rejected','skeletal')
pid='22851'
tz=dt.timezone(dt.timedelta(hours=11))
def fields(s): return dict(re.findall(r'(?:^|\s)([\w-]+)=([^\s]+)',s))
def parse(line):
    head,_,tail=line.partition(' stage_units=')
    d=fields(head)
    d['stages']={k:tuple(map(int,v)) for k,*v in re.findall(r'\b([a-z_]+)=(\d+)/(\d+)/(\d+)/(\d+)\b',tail)}
    d['raw']=line
    return d

with zipfile.ZipFile(archive_path) as z:
    assert z.testzip() is None
    assert len(z.namelist())==len(set(z.namelist()))==402
    def get(s): return z.read(stem+'-'+s+'.txt')
    new,base,full=[get(n) for n in ('NewLog','Baseline','FullLog')]
    assert full==base+new
    assert get('Original-Launch-PID').strip()==pid.encode()
    report=dict(l.split(': ',1) for l in get('Build-Report').decode().splitlines() if ': ' in l)
    expected={'Source commit':'e1fcf938dcc24bb9865b743f900d107a9de26d9d',
              'Engineering revision':'ffd520a6ac40ed92cedfbc6026f8bda51cdefcb6',
              'Player implementation':'b679975a352bf2f63b650f78845a2bf1eb5f8d9c',
              'APK SHA-256':'9f5027538621dd52c075d60ac740d56e97ab30efd509c0cf02416e9a6e711757',
              'ELF Build ID':'f21f96804e3f7d37673c91589cf5231f66807878',
              'C flags':'-DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=0',
              'C++ flags':'-DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=0'}
    for key,value in expected.items(): assert report[key]==value,key
    assert get('Installed-APK-SHA256').split()[0].decode()==report['APK SHA-256']
    raw=new.decode(errors='replace').splitlines()
    pids=Counter(re.findall(r'\bpid=(\d+)\b',new.decode(errors='replace')))
    assert set(pids)=={pid}
    records={}
    for domain in domains:
        chosen=[l for l in raw if l.startswith('Runtime '+domain+':') and re.search(r'\bpid='+pid+r'\b',l)]
        assert chosen==get('Runtime-'+domain).decode().splitlines() if chosen else not get('Runtime-'+domain).strip(),domain
        records[domain]=[parse(l) for l in chosen]
    window_lines=get('Runtime-window').decode().splitlines()
    offsets=[]
    for line in window_lines:
        stamp=dt.datetime.strptime(' '.join(line.split()[:3]),'%Y-%m-%d %H:%M:%S.%f %z')
        offsets.append(round(stamp.timestamp()*1e6)-int(fields(line)['mono_uptime_ms'])*1000)
    offset=int(statistics.median(offsets))
    def mono_stamp(s): return round(dt.datetime.strptime(s.strip(),'%Y-%m-%d %H:%M:%S').replace(tzinfo=tz).timestamp()*1e6)-offset
    def wall(v):return dt.datetime.fromtimestamp((v+offset)/1e6,tz).isoformat()
    interval=tuple(mono_stamp(get('Gameplay-01-'+s+'-Device-Time').decode()) for s in ('Start','End'))
    output={'zip_sha256':hashlib.sha256(archive_path.read_bytes()).hexdigest(),'zip_entries':402,
            'bytes':{'zip':archive_path.stat().st_size,'baseline':len(base),'full':len(full),'new':len(new)},
            'new_sha256':hashlib.sha256(new).hexdigest(),'private_log_pids':dict(pids),'report':report,
            'domain_counts':{d:len(v) for d,v in records.items()},
            'window':json.loads(get('Gameplay-01-Window')),
            'user_observation':get('User-Gameplay-Observation').decode(),
            'alignment':{'anchors':len(offsets),'median_offset_us':offset,'anchor_spread_us':max(offsets)-min(offsets),
                         'scope':'Approximate native-monotonic to device-wall mapping from Java window anchors; device boundary commands are whole-second precision. No frame-perfect screenshot or input correlation is claimed.'},
            'gameplay_device_bounds':list(map(wall,interval)), 'stages':{},'rates':{},'skeletal':{}}
    for domain in ('matching-stages','workload-stages','render-stages','present-stages','vulkan-build','shader-build'):
        maxima={}
        for record in records[domain]:
            for key,(n,total,maximum,end) in record['stages'].items():
                if interval[0]<=end<=interval[1] and (key not in maxima or maximum>maxima[key]['max_us']):
                    maxima[key]={'max_us':maximum,'end_mono_us':end,'end_wall_approx':wall(end),'raw':record['raw']}
        output['stages'][domain]=maxima
    for domain,count in (('vi','vi_iterations'),('present','native_present_calls'),('audio','queue_calls')):
        chosen=[r for r in records[domain] if 'elapsed_us' in r and int(r['elapsed_us'])>0
                and interval[0]<=int(r['mono_us'])-int(r['elapsed_us']) and int(r['mono_us'])<=interval[1]]
        rates=[int(r[count])*1e6/int(r['elapsed_us']) for r in chosen]
        out={'complete_reports':len(chosen),'total_report_seconds':sum(int(r['elapsed_us']) for r in chosen)/1e6,
             'min_per_second':min(rates),'max_per_second':max(rates),'median_per_second':statistics.median(rates),
             'weighted_per_second':sum(int(r[count]) for r in chosen)*1e6/sum(int(r['elapsed_us']) for r in chosen),
             'max_reported_gap_us':max(int(r['gap_max_us']) for r in chosen)}
        if domain=='vi':out['speed_multipliers']=sorted(set(r['speed_multiplier'] for r in chosen))
        if domain=='present':out['source_hz']=dict(Counter(r['vi_original_hz'] for r in chosen));out['target_hz']=sorted(set(r['target_hz'] for r in chosen))
        if domain=='audio':
            out['reported_error_values']=sorted(set(r['errors'] for r in chosen))
            out['decimation_max_values']=sorted(set(r['decimation_max'] for r in chosen))
            out['max_sdl_queue_us']=max(int(r['sdl_queue_us']) for r in chosen)
            out['zero_queue_polls_max']=max(int(r['zero_queue_polls']) for r in chosen)
        output['rates'][domain]=out
    for domain, names in (
        ('skeletal-source',('builders','published','rejected_owner','missing_builder','schema_mismatch','changed_builder_output','invalid_copy')),
        ('skeletal',('applied','current_pose_fallback','evaluated_pairs'))):
        all_records=records[domain]
        complete=[record for previous,record in zip(all_records,all_records[1:]) if interval[0]<=int(previous['mono_us']) and int(record['mono_us'])<=interval[1]]
        output['skeletal'][domain]={
            'all_report_count':len(all_records),'gameplay_complete_intervals':len(complete),
            'all_logged_sums':{key:sum(int(r[key]) for r in all_records) for key in names},
            'gameplay_complete_interval_sums':{key:sum(int(r[key]) for r in complete) for key in names},
            'npc_policies':sorted(set(r['npc_policy'] for r in all_records)),
            'scope':'Source and consumer counters reset after each emitted report (verified in ffd520a). Counts describe logged operations, not unique actors, frames or visible correctness. Gameplay sum excludes intervals crossing either timed boundary.',
        }
    rejected=records['skeletal-rejected']
    output['skeletal']['rejected']={
        'all_count':len(rejected),'reasons':dict(Counter(r['reason'] for r in rejected)),
        'players':dict(Counter(r['player'] for r in rejected)),
        'bone_counts':dict(Counter(r['bones'] for r in rejected)),
        'gameplay_count':sum(interval[0]<=int(r['mono_us'])<=interval[1] for r in rejected),
        'actions':sorted(set(r['action'] for r in rejected)),
        'scope':'Bounded reason records; not the count of every fallback transform.'}
    output['interpolation']={key:sorted(set(int(r[key]) for r in records['interpolation'])) for key in (
        'player_mapped','player_unmapped','duplicate_player_ids','invalid_decomposition_pairs','projection_mismatch_total')}
    output['skeletal']['samples']={d:[r['raw'] for r in records[d] if int(r.get('applied',r.get('published',0)))>0][:2] for d in ('skeletal-source','skeletal')}

    lc=get('Logcat').decode(errors='replace').splitlines()
    # Keep live Logcat and final dump distinct; count the live stream only.
    rx=re.compile(r'^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+) ([+-]\d+)\s+(\d+)\s+(\d+)\s+([VDIWEF])\s+([^:]+):\s?(.*)$')
    rows=[m.groups() for l in lc if (m:=rx.match(l))]
    app=[r for r in rows if r[2]==pid]
    audio_errors=[r for r in app if 'pthread_mutex_lock_timeout_np returned' in r[6]]
    output['opensl_mutex_timeouts']=[{'wall':r[0],'pid':r[2],'tid':r[3],'message':r[6]} for r in audio_errors]
    output['audiotrack_stop_records']=sum(r[5].strip()=='AudioTrack' and r[6].startswith('stop(') for r in app)
    output['fatal_app_records']=[r for r in app if re.search(r'Fatal signal|FATAL EXCEPTION|SIGSEGV|SIGABRT|guest-fatal|VK_ERROR_DEVICE_LOST',r[6],re.I)]
    output['private_failure_records']=[l for l in raw if re.search(r'Descriptor.*rejected|GPU.*not-prepared|Clear.*rejected|VK_ERROR|failed|timed? ?out|timeout|\bnan\b|\binfinity\b|fatal|exception',l,re.I) and not l.startswith('Runtime guest-fatal:')]
    for label in ('Final-PIDs','Process-History','Gameplay-01-Capture-Decision'):
        output[label]=get(label).decode()
    output['pid_samples']=Counter()
    for n in z.namelist():
        if re.search(r'-Gameplay-01-PIDs-\d+\.txt$',n):
            text=z.read(n).decode().strip()
            assert '22851' in text and 'TOOIE_PIDOF_RC=0' in text
            output['pid_samples']['original_alive']+=1

print(json.dumps(output,indent=2))
