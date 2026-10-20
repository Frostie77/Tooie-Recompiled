"""Verify and analyze the exact Step 111 handoff ZIP; stdout only, no extraction."""
# Retains the inspected handoff's analysis calculations; replaces its local-file
# reconstruction workflow with verified in-memory ZIP input. No attached code
# is executed. The retained analysis comparison is a reproducibility check.
from pathlib import Path
import collections,datetime as dt,hashlib,io,json,re,statistics,sys,zipfile
from pathlib import PurePosixPath

PREFIX='Tooie-b3401ddb7294-First-Person-Switches-Odin-20261020-004627-318d5f3b'
PID=29701
ARCHIVE_SHA256='e7cec81cb8c02e28e923307a520907674f152979fec1db30c91facacf55b1313'
def sha(b):return hashlib.sha256(b).hexdigest()
def checked_zip(data):
    z=zipfile.ZipFile(io.BytesIO(data))
    infos=z.infolist()
    assert len(infos)==len({i.filename for i in infos})
    assert sum(i.file_size for i in infos)<256*1024*1024
    for i in infos:
        p=PurePosixPath(i.filename)
        assert not p.is_absolute() and '..' not in p.parts and '\\' not in i.filename
    assert z.testzip() is None
    return z
assert len(sys.argv)==2,'usage: analyze_step111_evidence.py HANDOFF.zip'
raw=Path(sys.argv[1]).read_bytes();assert sha(raw)==ARCHIVE_SHA256,'handoff archive identity mismatch'
outer=checked_zip(raw)
manifest=json.loads(outer.read('MANIFEST.json'))
for item in manifest['files']:
    value=outer.read(item['path']);assert len(value)==item['bytes'] and sha(value)==item['sha256']
index_bytes=outer.read('upload/'+PREFIX+'-Upload-Index.txt');index=json.loads(index_bytes)
archives={}
for number in (4,9,10,11,12):
    spec=next(p for p in index['parts'] if p['part_number']==number)
    value=outer.read('upload/'+spec['name']);assert len(value)==spec['size_bytes'] and sha(value)==spec['sha256']
    z=checked_zip(value);archives[number]=z
    expected={c['member'] for s in index['sources'] for c in s['chunks'] if c['part_number']==number}
    assert set(z.namelist())==expected
    for source in index['sources']:
        for c in source['chunks']:
            if c['part_number']==number:
                value=z.read(c['member']);assert len(value)==c['length'] and sha(value)==c['sha256']
files={}
for source in index['sources']:
    if not all(c['part_number'] in archives for c in source['chunks']):continue
    value=bytearray()
    for c in source['chunks']:
        assert c['source_offset']==len(value)
        value.extend(archives[c['part_number']].read(c['member']))
    assert len(value)==source['size_bytes'] and sha(value)==source['sha256']
    name=source['name'].removeprefix(PREFIX+'-')
    assert name==PurePosixPath(name).name and name not in files
    files[name]=bytes(value)
assert len(files)==689
expected_analysis=json.loads(outer.read('analysis.json'))
def read(name):return files[name].decode(errors='replace')
def fields(text):return dict(re.findall(r'\b([A-Za-z_][A-Za-z_0-9]*)=([^\s]+)',text))
def rows(domain):
    lines=read('NewLog.txt').splitlines()
    return [(line,fields(line)) for line in lines if line.startswith('Runtime '+domain+':') and re.search(r'\bpid='+str(PID)+r'\b',line)]
def wallus(value):return round(dt.datetime.fromisoformat(value).timestamp()*1e6)
loglines=read('Logcat.txt').splitlines()
logre=re.compile(r'^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+) ([+-]\d{4})\s+(\d+)\s+(\d+) ([VDIWEF]) (.*?): (.*)$')
parsed=[]
for line in loglines:
    m=logre.match(line)
    if m:
        stamp=dt.datetime.strptime(m[1]+' '+m[2],'%Y-%m-%d %H:%M:%S.%f %z')
        parsed.append({'wall_us':round(stamp.timestamp()*1e6),'wall':stamp.isoformat(),
                       'pid':int(m[3]),'tid':int(m[4]),'level':m[5],'tag':m[6].strip(),'message':m[7],'line':line})
anchors=[p for p in parsed if p['pid']==PID and 'Runtime window: mono_uptime_ms=' in p['message']]
offsets=[p['wall_us']-int(fields(p['message'])['mono_uptime_ms'])*1000 for p in anchors]
offset=statistics.median(offsets)
def native(wall):return wallus(wall)-offset
def wall_of(mono):return dt.datetime.fromtimestamp((float(mono)+offset)/1e6,dt.timezone(dt.timedelta(hours=11))).isoformat()
windows=[json.loads(read(f'Gameplay-{i:02d}-Window.txt')) for i in range(1,4)]
markers=[json.loads(x) for x in read('User-Markers.txt').splitlines() if json.loads(x).get('kind')=='event']
device_markers=[p for p in parsed if p['tag']=='TooieUserMark' and PREFIX in p['message']]
for marker in markers:
    matches=[p for p in device_markers if json.loads(p['message'])['id']==marker['id']]
    assert len(matches)==1
    marker['device_logcat_wall']=matches[0]['wall']
    marker['device_logcat_native_us_estimate']=matches[0]['wall_us']-offset
    marker['receipt_to_device_log_ms']=(matches[0]['wall_us']-wallus(marker['host_read_wall']))/1000

def inside(row):
    if 'elapsed_us' not in row or int(row['elapsed_us'])<=0:return False
    end=int(row['mono_us']);start=end-int(row['elapsed_us'])
    return any(native(w['host_start'])+1e6<=start and end<=native(w['host_end'])-1e6 for w in windows)
def complete(domain):return [(line,row) for line,row in rows(domain) if inside(row)]
def stages(domain):
    result={};samples=complete(domain)
    for line,row in samples:
        for name,n,total,peak,end in re.findall(r'\b(\w+)=(\d+)/(\d+)/(\d+)/(\d+)',line):
            entry=result.setdefault(name,{'operations':0,'sum_us':0,'max_us':0,'max_end_mono_us':0})
            entry['operations']+=int(n);entry['sum_us']+=int(total)
            if int(peak)>entry['max_us']:entry['max_us']=int(peak);entry['max_end_mono_us']=int(end)
    for entry in result.values():
        entry['mean_us']=entry['sum_us']/entry['operations'] if entry['operations'] else None
        if entry['max_end_mono_us']:entry['approx_max_wall']=wall_of(entry['max_end_mono_us'])
    return {'intervals':len(samples),'stages':result,
            'max_outer_event_us':max(int(row['event_max_us']) for _,row in samples)}

presents=complete('present');assert presents
frames=sum(int(row['native_present_calls']) for _,row in presents)
seconds=sum(int(row['elapsed_us']) for _,row in presents)/1e6
rates=[int(row['native_present_calls'])*1e6/int(row['elapsed_us']) for _,row in presents]
max_gap=max(presents,key=lambda x:int(x[1]['gap_max_us']))
vi=complete('vi');audio=[(line,row) for line,row in complete('audio') if row.get('event')=='window']
def weighted(samples,count):return sum(int(row[count]) for _,row in samples)*1e6/sum(int(row['elapsed_us']) for _,row in samples)

app=[p for p in parsed if p['pid']==PID]
timeouts=[p for p in app if 'pthread_mutex_lock_timeout_np returned 110' in p['message']]
stops=[p for p in app if p['tag']=='AudioTrack' and re.search(r'\bstop\(',p['message'])]
crash_re=re.compile(r'Fatal signal|SIGSEGV|SIGABRT|FATAL EXCEPTION|ANR in io.github.banjotooierecompiled|Process io.github.banjotooierecompiled.*has died|Killing .*29701|am_crash|am_anr',re.I)
crashes=[p for p in parsed if crash_re.search(p['line'])]
fatal_re=re.compile(r'guest-fatal|lookup-failure|fence timeout|Transfer clear rejected:|VK_ERROR(?:_DEVICE_LOST)?|Descriptor rejected:|Partial color clear rejected:|Fatal signal|SIGSEGV|SIGABRT|assertion.*fail',re.I)
native_failures=[line for line in read('NewLog.txt').splitlines() if fatal_re.search(line)]
pidfiles=[n for n in files if re.fullmatch(r'Gameplay-.*-PIDs-.*\.txt',n) and not n.endswith(('-status.txt','-error.txt'))]
assert pidfiles and all(read(p).split()==[str(PID),'TOOIE_PIDOF_RC=0'] for p in pidfiles)
assert all(w['completed_requested_window'] and not w['original_process_unavailable'] and not w['live_reader_ended'] for w in windows)
assert read('Final-PIDs.txt').strip()==str(PID)

full=next(s for s in index['sources'] if s['name'].endswith('-FullLog.txt'))
baseline=next(s for s in index['sources'] if s['name'].endswith('-Baseline.txt'))
new=next(s for s in index['sources'] if s['name'].endswith('-NewLog.txt'))
last=full['chunks'][-1]
tail=archives[last['part_number']].read(last['member'])
newdata=files['NewLog.txt']
assert baseline['size_bytes']+len(newdata)==full['size_bytes']
assert tail[baseline['size_bytes']-last['source_offset']:]==newdata
assert sha(newdata)==new['sha256']

def phase_range(lo,hi):
    ps=[(line,row) for line,row in rows('present') if int(row['elapsed_us'])>0 and lo<=int(row['mono_us'])-int(row['elapsed_us']) and int(row['mono_us'])<=hi]
    return {'complete_intervals':len(ps),'covered_seconds':sum(int(row['elapsed_us']) for _,row in ps)/1e6,
            'weighted_native_present_per_s':weighted(ps,'native_present_calls') if ps else None,
            'median_native_present_per_s':statistics.median(int(row['native_present_calls'])*1e6/int(row['elapsed_us']) for _,row in ps) if ps else None,
            'min_native_present_per_s':min((int(row['native_present_calls'])*1e6/int(row['elapsed_us']) for _,row in ps),default=None),
            'max_native_present_gap_us':max((int(row['gap_max_us']) for _,row in ps),default=None)}
first=next(x for x in markers if x['marker']==9)['device_logcat_native_us_estimate']
loaded=next(x for x in markers if x['marker']==10)['device_logcat_native_us_estimate']
phases={
    'gameplay_before_entering_marker':phase_range(native(windows[0]['host_start'])+1e6,first),
    'between_entry_and_loaded_markers':phase_range(first,loaded),
    'loaded_marker_to_end_first_window':phase_range(loaded,native(windows[0]['host_end'])-1e6),
    'second_window':phase_range(native(windows[1]['host_start'])+1e6,native(windows[1]['host_end'])-1e6),
    'third_window':phase_range(native(windows[2]['host_start'])+1e6,native(windows[2]['host_end'])-1e6),
}
actor=collections.Counter(row.get('result') for _,row in rows('actor-draw'))
skeletal=rows('skeletal')
summary={
 'capture':PREFIX,'original_pid':PID,
 'identity':dict(line.split('=',1) for line in read('Identity.txt').splitlines() if '=' in line),
 'integrity':{'index_sha256':sha(index_bytes),'provided_parts':[4,9,10,11,12],
     'all_provided_part_and_chunk_hashes_verified':True,'complete_reconstructed_sources':len(files),
     'baseline_bytes':baseline['size_bytes'],'new_bytes':len(newdata),'full_bytes':full['size_bytes'],
     'new_sha256':sha(newdata),'new_exact_full_log_tail_comparison':True,
     'full_log_last_chunk_offset':last['source_offset'],'full_log_last_chunk_sha256':last['sha256'],
     'baseline_prefix_independently_reconstructed':False,
     'logcat_bytes':len(files['Logcat.txt']),'logcat_sha256':sha(files['Logcat.txt'])},
 'windows':windows,'completed_observer_seconds':sum(w['measured_observer_seconds'] for w in windows),
 'pid_samples':len(pidfiles),'final_pid':int(read('Final-PIDs.txt')),
 'markers':markers,'user_observation':read('User-Gameplay-Observation.txt').strip(),
 'clock_alignment':{'anchors':len(anchors),'median_wall_minus_uptime_us':offset,'startup_offset_spread_us':max(offsets)-min(offsets),
     'scope':'Approximate native-to-device-wall mapping uses four startup Java uptime-ms anchors; no later drift bound. Gameplay interval selection uses host-reported wall windows with a one-second guard at each edge. Marker phase ranges use device Logcat times. Report intervals are selected only when fully inside a range.'},
 'runtime_counts':dict(collections.Counter(re.findall(r'^Runtime ([^:]+):',read('NewLog.txt'),re.M))),
 'native_tagged_pids':dict(collections.Counter(re.findall(r'\bpid=(\d+)',read('NewLog.txt')))),
 'present':{'complete_intervals':len(presents),'interval_seconds':seconds,'native_present_calls':frames,
    'weighted_native_present_per_s':frames/seconds,'median_interval_rate':statistics.median(rates),
    'min_interval_rate':min(rates),'max_interval_rate':max(rates),'max_present_gap_us':int(max_gap[1]['gap_max_us']),
    'max_gap_report_wall':wall_of(max_gap[1]['mono_us']),
    'target_hz':sorted({int(row['target_hz']) for _,row in presents}),
    'source_hz':sorted({int(row['vi_original_hz']) for _,row in presents}),
    'under_60_intervals':sum(x<60 for x in rates),'under_100_intervals':sum(x<100 for x in rates)},
 'phases':phases,
 'vi':{'intervals':len(vi),'weighted_iterations_per_s':weighted(vi,'vi_iterations'),
       'speed_multiplier_values':sorted({row['speed_multiplier'] for _,row in vi}),
       'scope':'VI scheduling and logged configured speed multiplier do not establish guest demo/intro clock correctness.'},
 'audio':{'backend_open':[line for line,row in rows('audio') if row.get('event')=='open'],
      'complete_intervals':len(audio),'weighted_queue_calls_per_s':weighted(audio,'queue_calls'),
      'api_error_windows':sum(int(row['errors'])!=0 for _,row in audio),
      'max_snapshot_sdl_queue_us':max(int(row['sdl_queue_us']) for _,row in audio),
      'max_interval_sdl_queued_bytes':max(int(row['queued_max_bytes']) for _,row in audio),
      'decimation_max':max(int(row['decimation_max']) for _,row in audio),
      'opensl_mutex_timeouts':timeouts,'audiotrack_stop_debug_lines':len(stops),
      'scope':'SDL software queue excludes hardware pending audio; AudioTrack stops and zero queue polls do not prove audible dropouts. Audio content was not recorded.'},
 'stages':{domain:stages(domain) for domain in ('render-stages','present-stages','workload-stages','matching-stages','present-queue')},
 'configuration':json.loads(read('Runtime-config.txt').split('values=',1)[1]),
 'actor':{'sampled_draw_results':dict(actor),
      'sampled_skeletal_rejection_reasons':dict(collections.Counter(row.get('reason') for _,row in rows('skeletal-rejected'))),
      'reported_actor_applied_sum':sum(int(row['actor_applied']) for _,row in skeletal),
      'reported_player_applied_sum':sum(int(row['player_applied']) for _,row in skeletal),
      'reported_current_pose_fallback_sum':sum(int(row['current_pose_fallback']) for _,row in skeletal),
      'scope':'Bounded sampled diagnostics and interval operations are not unique actors, persistent identities or proof of correct appearance. Guards retain current submitted pose on rejected ownership/history.'},
 'native_failure_matches':native_failures,'logcat_crash_matches':crashes,
 'presentation_wait_source':{
     'commit':'b3401ddb72942f99958828ffc27723fb049d50bd',
     'path':'lib/rt64/src/hle/rt64_present_queue.cpp',
     'git_blob_sha':'15b1e939120ae20931261646681a53030e0c2216',
     'scope':'cursor_wait begins before acquisition of cursorMutex and includes cursorCondition.wait until writeCursor != threadCursor or shutdown. It does not identify why the cursor advance, notification or mutex acquisition was delayed.',
     'reviewed_lines':'570-599'},
 'limitations':['This run verifies the observed first-person entrance and user-reported boss fight, not every first-person state, switch slot or full game.',
                'No captured guest-clock observer records in this normal observer-OFF APK; demo/intro speed multiplier and intended cadence cannot be established from native presentation/VI rates.',
                'Rendering counts are native presentation calls, not guest simulation FPS or a display pixel/readback measurement.',
                'No causal before/after performance comparison: content and path differ from the previous crash capture.',
                'No mod import/executable-mod/texture replacement verification during this run.']}
assert summary==expected_analysis,'independent recomputation differs from retained handoff analysis'
print(json.dumps(summary,indent=2))
