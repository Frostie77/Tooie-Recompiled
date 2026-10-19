#!/usr/bin/env python3
"""Pinned Step 100 analysis: JSON on stdout; no extraction or device operations."""
from pathlib import Path
from collections import Counter, defaultdict
import datetime as dt
import hashlib
import json
import re
import statistics
import sys
import zipfile

STEM='Tooie-eea5be64d606-Actor-Hierarchy-Odin-20261019-133554-d3c17123'
ZIP_SHA='36d42f2cbafab96e94f5c0c7e50ef540f1183d3c21f4d6fd73e89369453c75d0'
APK_SHA='980819ccda3419ce4386c54d304b6ef43bfd6932936e3abb5f5f3da0479bf75f'
PID='8002'
TZ=dt.timezone(dt.timedelta(hours=11))
if len(sys.argv)!=2: raise SystemExit('usage: analyze_step100_evidence.py EVIDENCE.zip')
archive=Path(sys.argv[1])

def fields(text):return dict(re.findall(r'(?:^|\s)([\w-]+)=([^\s]+)',text))
def parse(text):
    head,_,tail=text.partition(' stage_units=')
    d=fields(head)
    d['stages']={k:tuple(map(int,v)) for k,*v in re.findall(r'\b([a-z_]+)=(\d+)/(\d+)/(\d+)/(\d+)\b',tail)}
    d['raw']=text
    return d
def epoch(text):return round(dt.datetime.strptime(text,'%Y-%m-%d %H:%M:%S.%f').replace(tzinfo=TZ).timestamp()*1e6)

assert hashlib.sha256(archive.read_bytes()).hexdigest()==ZIP_SHA,'Wrong evidence archive'
with zipfile.ZipFile(archive) as z:
    assert len(z.namelist())==len(set(z.namelist()))==493
    assert z.testzip() is None
    for name in z.namelist():
        assert not Path(name).is_absolute() and '..' not in Path(name).parts and '\\' not in name
        assert name.startswith(STEM+'-')
    def get(s):return z.read(STEM+'-'+s+'.txt')
    def text(s):return get(s).decode(errors='replace')
    baseline,new,full=[get(n) for n in ('Baseline','NewLog','FullLog')]
    assert full==baseline+new
    assert text('Original-Launch-PID').strip()==PID
    report={}
    for line in text('Build-Report').splitlines():
        key,sep,value=line.partition(': ')
        if sep:
            assert key not in report,key
            report[key]=value
    expected={'Source commit':'eea5be64d60630830dd21caac7251bd8684372b8',
              'Source branch':'build/android-actor-hierarchy-mods-20261019-cb896c3',
              'Source parent':'cb896c3866fdd8dade1b8a1d12f43872ac516d39 c9f5efc17df77be6a53e4f475a227092bb7e9c9f',
              'Source tree':'132e24d1938cbafde751a70f780cda068300c465',
              'Engineering revision':'cb896c3866fdd8dade1b8a1d12f43872ac516d39',
              'ELF Build ID':'6b76fa9a085c80d52c1b2b73aa1be2c5f7e875a1',
              'APK SHA-256':APK_SHA,'Build mode':'NORMAL GAMEPLAY ACTOR HIERARCHY DEBUG',
              'Descriptor allocation probe':'OFF','Geometry trace':'OFF','Correction-input/guest-clock trace':'OFF',
              'C flags':'-DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=0','C++ flags':'-DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=0',
              'Menu asset SHA-256':'87b0fdf84f30e5bff5516e6acfc7fa812713fdae2f4a4fb22b68333b16b89f26 (both source copies and exact APK member)',
              'Icon master SHA-256':'4b6db92f4ad3280c10131b5cec745508b19257a93ab1b2615279813ebca06784',
              'Actor hierarchy':'ON by Android default; validated marker-constructor generation, canonical movable actor-array ownership and owned authored pose'}
    for k,v in expected.items():assert report[k]==v,k
    assert text('Installed-APK-SHA256').split()[0]==APK_SHA
    pids=Counter(re.findall(r'\bpid=(\d+)\b',new.decode(errors='replace')))
    assert set(pids)=={PID}
    raw=new.decode(errors='replace').splitlines()
    domains=('audio','vi','present','config','workload-stages','render-stages','present-stages',
             'vulkan-build','shader-build','interpolation','present-queue','matching-stages',
             'guest-fatal','skeletal-source','skeletal-rejected','skeletal','actor-source','actor-draw')
    records={}
    for domain in domains:
        chosen=[l for l in raw if l.startswith('Runtime '+domain+':') and re.search(r'\bpid='+PID+r'\b',l)]
        assert chosen==text('Runtime-'+domain).splitlines() if chosen else not get('Runtime-'+domain).strip(),domain
        records[domain]=[parse(l) for l in chosen]
    anchors=[]
    for line in text('Runtime-window').splitlines():
        stamp=' '.join(line.split()[:2])
        anchors.append(epoch(stamp)-int(fields(line)['mono_uptime_ms'])*1000)
    offset=int(statistics.median(anchors))
    def wall(mono):return dt.datetime.fromtimestamp((mono+offset)/1e6,TZ).isoformat(timespec='microseconds')
    def mono_device(stamp):return round(dt.datetime.strptime(stamp.strip(),'%Y-%m-%d %H:%M:%S').replace(tzinfo=TZ).timestamp()*1e6)-offset
    assert len(records['guest-fatal'])==1
    fatal=records['guest-fatal'][0]
    assert fatal['reason']=='missing-function' and fatal['guest_address']=='0x801F38E0'
    fatal_mono=int(fatal['mono_us'])
    output={'archive':{'name':archive.name,'bytes':archive.stat().st_size,'sha256':ZIP_SHA,'entries':493,'crc':'pass'},
            'source':expected,'log_bytes':{'baseline':len(baseline),'new':len(new),'full':len(full)},
            'new_log_sha256':hashlib.sha256(new).hexdigest(),'private_log_pids':dict(pids),
            'original_pid':PID,'final_pids':text('Final-PIDs').strip(),'user_observation':text('User-Gameplay-Observation').strip(),
            'domain_counts':{k:len(v) for k,v in records.items()},
            'clock_alignment':{'anchors':len(anchors),'median_device_wall_minus_native_us':offset,
                               'anchor_spread_us':max(anchors)-min(anchors),
                               'scope':'Approximate native monotonic to device wall via Java uptime anchors. Timed boundary reads are whole seconds. Host and device clocks are kept separate.'},
            'fatal':{**{k:v for k,v in fatal.items() if k!='stages'},'approx_device_wall':wall(fatal_mono)},
            'process_history':text('Process-History').splitlines(),'windows':{},'markers':[]}
    for label in ('Gameplay-01','Gameplay-02'):
        metadata=json.loads(text(label+'-Window'))
        start=mono_device(text(label+'-Start-Device-Time'))
        observed_end=mono_device(text(label+'-End-Device-Time'))
        end=min(observed_end,fatal_mono)
        window={'metadata':metadata,'device_start':text(label+'-Start-Device-Time').strip(),
                'device_observed_end':text(label+'-End-Device-Time').strip(),
                'native_bounds_us':[start,end],'analyzed_until':'fatal record' if end==fatal_mono else 'window end',
                'rates':{},'counters':{},'stage_maxima':{}}
        for domain,count in (('vi','vi_iterations'),('present','native_present_calls'),('audio','queue_calls')):
            rows=[r for r in records[domain] if int(r.get('elapsed_us',0))>0
                  and start<=int(r['mono_us'])-int(r['elapsed_us']) and int(r['mono_us'])<=end]
            rates=[int(r[count])*1e6/int(r['elapsed_us']) for r in rows]
            d={'reports':len(rows),'covered_seconds':sum(int(r['elapsed_us']) for r in rows)/1e6,
               'min_hz':min(rates),'max_hz':max(rates),'median_hz':statistics.median(rates),
               'weighted_hz':sum(int(r[count]) for r in rows)*1e6/sum(int(r['elapsed_us']) for r in rows),
               'max_reported_gap_us':max(int(r['gap_max_us']) for r in rows)}
            peak=max(rows,key=lambda r:int(r['gap_max_us']))
            d['max_gap_report']={'approx_device_window_start':wall(int(peak['mono_us'])-int(peak['elapsed_us'])),
                                 'approx_device_window_end':wall(int(peak['mono_us'])),'raw':peak['raw']}
            if domain=='vi':d['speed_multipliers']=sorted(set(r['speed_multiplier'] for r in rows))
            if domain=='present':
                d['source_hz']=dict(Counter(r['vi_original_hz'] for r in rows))
                d['target_hz']=sorted(set(r['target_hz'] for r in rows))
            if domain=='audio':
                d['errors_values']=sorted(set(r['errors'] for r in rows))
                d['decimation_max']=sorted(set(r['decimation_max'] for r in rows))
                d['max_sdl_queue_us']=max(int(r['sdl_queue_us']) for r in rows)
            window['rates'][domain]=d
        for domain,names in (
            ('actor-source',('draws','admitted','roots','skeletal_bones','unsupported_bones')),
            ('skeletal-source',('builders','published','rejected_owner','missing_builder','schema_mismatch','changed_builder_output','invalid_copy')),
            ('skeletal',('applied','current_pose_fallback','evaluated_pairs','actor_applied','player_applied'))):
            rows=[r for prev,r in zip(records[domain],records[domain][1:]) if start<=int(prev['mono_us']) and int(r['mono_us'])<=end]
            window['counters'][domain]={'complete_intervals':len(rows),'sums':{n:sum(int(r[n]) for r in rows) for n in names},
                                       'scope':'Resetting counters over contained reporting intervals: operations, not unique actors or visible correctness.'}
        for domain in ('matching-stages','workload-stages','render-stages','present-stages','vulkan-build','shader-build'):
            peaks={}
            for row in records[domain]:
                for stage,(n,total,maximum,event_end) in row['stages'].items():
                    if start<=event_end<=end and (stage not in peaks or maximum>peaks[stage]['max_us']):
                        peaks[stage]={'max_us':maximum,'end_mono_us':event_end,'approx_device_wall':wall(event_end),'raw':row['raw']}
            window['stage_maxima'][domain]=peaks
        samples=[]
        for name in z.namelist():
            if re.search('-'+label+r'-PIDs-\d+\.txt$',name):
                content=z.read(name).decode().strip().splitlines()
                payload=next((v for v in content if v and not v.startswith('TOOIE_')),'')
                samples.append(payload)
        window['pid_samples']=dict(Counter(samples))
        output['windows'][label]=window

    rx=re.compile(r'^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+) ([+-]\d+)\s+(\d+)\s+(\d+)\s+([VDIWEF])\s+([^:]+):\s?(.*)$')
    lc=text('Logcat').splitlines()
    rows=[{'wall':m[1],'zone':m[2],'pid':m[3],'tid':m[4],'level':m[5],'tag':m[6].strip(),'message':m[7],'raw':line}
          for line in lc if (m:=rx.match(line))]
    marker_rows=[r for r in rows if r['tag']=='TooieUserMark']
    events=[json.loads(line) for line in text('User-Markers').splitlines()]
    registered=[r for r in events if r['kind']=='event']
    assert len(marker_rows)==len(registered)==3
    for event in registered:
        match=[r for r in marker_rows if json.loads(r['message'])['id']==event['id']]
        assert len(match)==1
        logged=json.loads(match[0]['message'])
        assert logged['original_game_pid']==int(PID) and logged['capture']==STEM
        assert logged['description']==event['description'] and logged['marker']==event['marker']
        delivery=next(r['metadata'] for r in events if r.get('event_id')==event['id'])
        native_event=epoch(match[0]['wall'])-offset
        output['markers'].append({**event,'delivery':delivery['delivery'],'device_wall':match[0]['wall'],
                                  'device_log_pid':match[0]['pid'],'approx_native_us':native_event,
                                  'seconds_before_fatal':(fatal_mono-native_event)/1e6,
                                  'description_scope':'User observation/intent; not a model-ID join or exact onset.'})
    output['raw_input_journal']=[json.loads(line) for line in text('Live-Input-Journal').splitlines()]
    app=[r for r in rows if r['pid']==PID]
    output['opensl_mutex_timeouts']=[r for r in app if 'pthread_mutex_lock_timeout_np returned' in r['message']]
    output['audiotrack_stop_count']=sum(r['tag']=='AudioTrack' and r['message'].startswith('stop(') for r in app)
    output['app_signal_or_java_fatal_records']=[r for r in app if re.search(r'Fatal signal|FATAL EXCEPTION|SIGSEGV|SIGABRT|VK_ERROR_DEVICE_LOST',r['message'])]
    output['native_guard_failure_records']=[l for l in raw if re.search(r'Descriptor rejected:|Clear.*rejected:|VK_ERROR_|gpu-tile-buffer-(?:not-prepared|range-too-small)',l)]
    assert all(int(r['applied'])==int(r['actor_applied'])+int(r['player_applied']) for r in records['skeletal'])
    output['actor_draw_results']=dict(Counter(r['result'] for r in records['actor-draw']))
    output['actor_policy_values']=sorted(set(r['npc_policy'] for r in records['skeletal']))
    tracked=defaultdict(set)
    for r in records['actor-draw']:tracked[(r['marker'],r['generation'],r['callback'],r['model'])].add(r['actor'])
    moved={str(k):sorted(v) for k,v in tracked.items() if len(v)>1}
    output['actor_identity_groups_with_multiple_observed_addresses']=moved
    output['skeletal_rejected']={'count':len(records['skeletal-rejected']),
                                'reasons':dict(Counter(r['reason'] for r in records['skeletal-rejected'])),
                                'players':dict(Counter(r['player'] for r in records['skeletal-rejected'])),
                                'actions':sorted(set(r['action'] for r in records['skeletal-rejected']))}
    output['stale_collector_scope_sentence']='NPC policy and global pacing are unchanged' in text('Capture-Scope')
    output['stale_scope_correction']='The NPC-policy phrase is stale collector prose. Build profile and every skeletal policy record enable validated-marker-hierarchy. Global pacing is unchanged.'
    relevant={}
    for event in output['markers']:
        center=event['approx_native_us']
        selected=[l for l in raw if l.startswith('Runtime ') and (m:=re.search(r'\bmono_us=(\d+)',l)) and abs(int(m[1])-center)<=5_000_000]
        relevant['marker-'+str(event['marker'])]=selected
    relevant['fatal-tail']=raw[-45:]
    output['marker_contexts']=relevant

print(json.dumps(output,indent=2))
