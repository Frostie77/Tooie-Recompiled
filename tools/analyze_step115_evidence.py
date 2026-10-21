"""Read-only Step 115 evidence analyzer, supplied in the reviewed handoff.

Calculations retained from that source; not an independent timing baseline.
Standard library; no execution or extraction.

Review this source before running. Input is the ORIGINAL Step 114 evidence ZIP,
or the Step 115 handoff ZIP containing it. Output is deterministic JSON on stdout.
"""
from pathlib import Path
import collections
import hashlib
import io
import json
import math
import re
import sys
import zipfile

NAME='Tooie-7a4a8432bb7f-Timing-Launch-Recovery-Odin-20261020-145054-1e8945ce-Evidence.zip'
EXPECTED_SHA='3f30a38c6d18d6dfc84e76fceb1332755eca36f2bacf264b78388d3761e90cd1'
EXPECTED_APK='c6b4b4688f85021c86c0de1439fe2e91f58ea0fd5781d063b39912613ce3393f'
PREFIX=NAME.removesuffix('-Evidence.zip')

def require(ok,message):
    if not ok: raise ValueError(message)

def digest(data): return hashlib.sha256(data).hexdigest()
def fields(line): return dict(re.findall(r'\b([a-z][a-z0-9_]*)=([^ ]+)',line))
def stage(record,key):
    n,total,maximum,end=map(int,record[key].split('/'))
    return {'n':n,'sum_us':total,'max_us':maximum,'end_us':end,'start_us':end-maximum}

def analyze(path):
    raw=Path(path).read_bytes()
    if digest(raw)!=EXPECTED_SHA:
        with zipfile.ZipFile(io.BytesIO(raw)) as outer:
            require(outer.namelist().count(NAME)==1,'Not the pinned original evidence or its handoff')
            manifest=json.loads(outer.read('MANIFEST.json'))
            for name,entry in manifest['payloads'].items():
                data=outer.read(name)
                require(len(data)==entry['bytes'] and digest(data)==entry['sha256'],'Handoff payload identity mismatch: '+name)
            raw=outer.read(NAME)
    require(len(raw)==23936925 and digest(raw)==EXPECTED_SHA,'Original evidence identity mismatch')
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        require(z.testzip() is None,'ZIP CRC failure')
        names=z.namelist()
        require(len(names)==len(set(names))==312,'Unexpected duplicate/member count')
        require(all('/' not in n and '\\' not in n and n.startswith(PREFIX+'-') for n in names),'Unexpected member path')
        def read(suffix):return z.read(PREFIX+'-'+suffix+'.txt')
        def text(suffix):return read(suffix).decode('utf-8',errors='replace').strip()
        pid=int(text('Original-Launch-PID'))
        require(pid==15060 and text('Final-PIDs')==str(pid),'Unexpected original/final PID')
        identity=dict(line.split('=',1) for line in text('Identity').splitlines())
        require(identity['commit']=='7a4a8432bb7fd108c01bdd9d20da44ee1ca81e16' and identity['hash']==EXPECTED_APK,'Unexpected build identity')
        require(text('Installed-APK-SHA256').split()[0]==EXPECTED_APK,'Recorded installed hash mismatch')
        baseline,full,new=map(read,('Baseline','FullLog','NewLog'))
        require(full==baseline+new,'FullLog is not exact Baseline + NewLog')
        statuses={n.removeprefix(PREFIX+'-'):z.read(n).decode(errors='replace') for n in names if n.endswith('-status.txt') and '-Previous-' not in n and z.read(n).startswith(b'returncode=')}
        require(all(v.splitlines()[0]=='returncode=0' for v in statuses.values()),'Fresh command failure')
        windows=[json.loads(z.read(n)) for n in names if re.fullmatch(re.escape(PREFIX)+r'-(Demo|Gameplay)-\d+-Window.txt',n)]
        require(len(windows)==1 and windows[0]['completed_requested_window'] and windows[0]['requested_seconds']==300,'Unexpected timed window')
        samples=[read(n.removeprefix(PREFIX+'-').removesuffix('.txt')).decode().split() for n in names if re.fullmatch(re.escape(PREFIX)+r'-Demo-01-PIDs-\d{3}.txt',n)]
        require(len(samples)==58 and all(s==[str(pid),'TOOIE_PIDOF_RC=0'] for s in samples),'Timed PID samples differ')
        marker_rows=[json.loads(l) for l in text('User-Markers').splitlines()]
        marks=[r for r in marker_rows if r['kind']=='event']
        deliveries={r['event_id']:r['metadata'] for r in marker_rows if r['kind']=='delivery'}
        require([r['marker'] for r in marks]==list(range(1,11)),'Missing/out-of-order markers')
        require(all(deliveries[r['id']]['delivery']=='command-acknowledged' for r in marks),'Unconfirmed marker delivery')
        logcat=text('Logcat').splitlines()
        combined=list(dict.fromkeys(logcat+text('Logcat-Dump').splitlines()))
        device_marks=[json.loads(l.split('TooieUserMark: ',1)[1]) for l in combined if 'TooieUserMark: ' in l]
        require({r['id'] for r in device_marks}=={r['id'] for r in marks},'Host/device markers do not match')
        runtime=[l for l in new.decode(errors='replace').splitlines() if re.match(r'Runtime [^:]+:',l) and re.search(r'\bpid='+str(pid)+r'\b',l)]
        domains=collections.defaultdict(list)
        for line in runtime: domains[line.split(':',1)[0].removeprefix('Runtime ')].append(line)
        def records(domain):return [fields(l) for l in domains[domain]]
        for domain in domains:
            if PREFIX+'-Runtime-'+domain+'.txt' in names:
                require(read('Runtime-'+domain)==('\n'.join(domains[domain])+'\n').encode(),'Derived extraction differs: '+domain)
        clocks=records('correction-clock')
        buckets=collections.defaultdict(list)
        for r in records('correction-clock-bucket'):buckets[int(r['window_end_us'])].append(r)
        for index,c in enumerate(clocks):
            require(int(c['elapsed_us'])==int(c['window_end_us'])-int(c['window_start_us']),'Clock window duration mismatch')
            if index: require(c['window_start_us']==clocks[index-1]['window_end_us'],'Noncontiguous clock windows')
            require(int(c['invalid'])==0,'Invalid clock observation')
        groups={'ordinary':[],'successful_replay':[],'mixed_or_transition':[],'initial_partial':[]}
        for c in clocks:
            bs=buckets[int(c['window_end_us'])]
            clean=all(all(int(b[k])==0 for k in ('ends','resets','transitions')) for b in bs)
            ordinary=bs and clean and all(int(b['mode_bucket'])==0 and int(b['records'])==0 for b in bs) and int(c['attract_calls'])==0 and int(c['attract_reset_calls'])==0
            replay=bs and clean and all(int(b['caller'])==0 and int(b['mode_bucket'])==2 and int(b['records'])==int(b['events'])==int(b['index_advances']) for b in bs) and int(c['attract_calls'])>0 and int(c['attract_reset_calls'])==0
            kind='initial_partial' if int(c['elapsed_us'])<1000000 else 'ordinary' if ordinary else 'successful_replay' if replay else 'mixed_or_transition'
            groups[kind].append(c)
        def clock_stats(cs):
            duration=sum(int(c['elapsed_us']) for c in cs)/1e6
            timer=math.fsum(float(c['timer_delta_sum']) for c in cs)
            attract=math.fsum(float(c['attract_delta_sum']) for c in cs)
            rates=[float(c['timer_delta_sum'])*1e6/int(c['elapsed_us']) for c in cs]
            return {'windows':len(cs),'elapsed_seconds':duration,'timer_guest_seconds':timer,'timer_guest_seconds_per_wall_second':timer/duration if duration else None,'attract_guest_seconds':attract,'attract_guest_seconds_per_wall_second':attract/duration if duration else None,'timer_ratio_min':min(rates) if rates else None,'timer_ratio_max':max(rates) if rates else None,'window_end_maps':dict(collections.Counter(int(c['map']) for c in cs)),'window_end_us':[int(c['window_end_us']) for c in cs]}
        inputs=records('present-input')
        publication=max(inputs,key=lambda r:int(r['publish_gap_max_us']))
        gap=int(publication['publish_gap_max_us']);end=int(publication['publish_gap_end_us'])
        queues=records('present-queue')
        observed=[r for r in queues if int(r['elapsed_us'])>0]
        wait=max(observed,key=lambda r:stage(r,'cursor_wait')['max_us'])
        frames=records('guest-frame-sync')
        require(all(int(r['invalid'])==0 for r in frames),'Invalid frame-sync observation')
        frame_max=max(frames,key=lambda r:int(r['max_us']))
        events=records('correction-clock-event')
        around=[r for r in events if end-gap-1000000<=int(r['mono_us'])<=end+200000]
        vi=records('vi');present=records('present')
        vi_seconds=sum(int(r['elapsed_us']) for r in vi)/1e6
        present_seconds=sum(int(r['elapsed_us']) for r in present)/1e6
        audio=records('audio');audio_windows=[r for r in audio if r.get('event')=='window']
        warnings=[l for l in combined if re.search(r'\b'+str(pid)+r'\b',l) and re.search(r'pthread_mutex_lock_timeout_np',l)]
        fatal=[l for l in combined if re.search(r'Fatal signal|FATAL EXCEPTION|SIGSEGV|Runtime guest-fatal:|Runtime lookup-failure:',l)]
        require(not domains.get('guest-fatal') and not domains.get('lookup-failure'),'Fresh guest fatal/lookup failure')
        marks_short=[{k:r[k] for k in ('sequence','marker','description','host_read_wall','host_read_mono_seconds','phase_at_read')} for r in marks]
        return {
            'archive':{'name':NAME,'bytes':len(raw),'sha256':digest(raw),'members':len(names),'uncompressed_bytes':sum(i.file_size for i in z.infolist()),'crc_verified':True},
            'recorded_build_identity':identity,
            'binary_scope':'APK/native binaries were not supplied for independent inspection; identity is from the collector and saved installed-hash command.',
            'delta':{s:{'bytes':len(data),'sha256':digest(data)} for s,data in (('Baseline',baseline),('FullLog',full),('NewLog',new))},
            'delta_exact':True,
            'capture':{'original_pid':pid,'final_pid':pid,'fresh_command_statuses':len(statuses),'fresh_command_failures':0,'timed_pid_samples':len(samples),'windows':windows,'gameplay_windows':0,'decision':text('Demo-01-Capture-Decision'),'user_observation':text('User-Demo-Observation'),'previous_evidence_prefix':text('Previous-Evidence-Prefix')},
            'markers':marks_short,
            'marker_delivery':{'device_log_matches':len(device_marks),'host_acknowledged':len(deliveries),'maximum_host_handling_delay_seconds':max(deliveries[r['id']]['host_handled_mono_seconds']-r['host_read_mono_seconds'] for r in marks)},
            'runtime_counts':{k:len(v) for k,v in sorted(domains.items())},
            'clocks':{'groups':{k:clock_stats(v) for k,v in groups.items()},'previously_requested_demo_maps_335_278_314':clock_stats([c for c in groups['successful_replay'] if int(c['map']) in (335,278,314)]),'rows':clocks,'bucket_rows':records('correction-clock-bucket'),'sparse_event_count':len(events),'scope':'Ratios cover complete fresh reporting windows, including time outside the 300-second observation. Window-end map is not proof of an unmixed map. Pure classifications use recorded bucket/reset/end flags; sparse details do not enumerate every input.'},
            'publication_gap':{'duration_us':gap,'start_us':end-gap,'end_us':end,'first_report_mono_us':int(publication['mono_us']),'largest_entry_gap_us':max(int(r['entry_gap_max_us']) for r in inputs),'last_result_counts':{k:int(inputs[-1][k]) for k in ('current_repeated','early_ignored','unchanged','published')},'source_record':publication},
            'post_startup_queue_peak':{'report':wait,'cursor_wait':stage(wait,'cursor_wait'),'cursor_condition':stage(wait,'cursor_condition'),'cursor_lock_window_max':stage(wait,'cursor_lock'),'largest_initial_cursor_lock_us':max(stage(r,'cursor_lock')['max_us'] for r in queues),'condition_end_minus_publication_us':stage(wait,'cursor_condition')['end_us']-end,'scope':'Excludes the elapsed_us=0 startup report. Condition time includes mutex reacquisition; nested stages and independent maxima are not additive.'},
            'guest_frame_sync':{'records':len(frames),'largest_observed_call_us':int(frame_max['max_us']),'largest_record':frame_max,'near_publication_gap_sparse_inputs':around,'scope':'Peak snapshots belong to the maximum-duration call in each reporting window. after_elapsed is not every frame return.'},
            'runtime_rates_entire_fresh_capture':{'vi_intervals':len(vi),'vi_seconds':vi_seconds,'vi_iterations':sum(int(r['vi_iterations']) for r in vi),'vi_iterations_per_second':sum(int(r['vi_iterations']) for r in vi)/vi_seconds,'speed_multipliers':sorted({r['speed_multiplier'] for r in vi}),'native_present_intervals':len(present),'native_present_seconds':present_seconds,'native_present_calls':sum(int(r['native_present_calls']) for r in present),'native_presents_per_second':sum(int(r['native_present_calls']) for r in present)/present_seconds,'largest_native_present_gap_us':max(int(r['gap_max_us']) for r in present),'scope':'Launcher/intro/demo/prompt intervals are included. These counters do not establish intended game/demo cadence.'},
            'audio':{'open':[r for r in audio if r.get('event')=='open'],'software_queue_windows':len(audio_windows),'software_errors_sum':sum(int(r['errors']) for r in audio_windows),'maximum_sdl_queue_us':max(int(r['sdl_queue_us']) for r in audio_windows),'opensles_mutex_warnings':warnings,'scope':'No audio recording; stop/debug messages or nonfatal warnings do not establish audible dropouts.'},
            'fresh_fatal_logcat_lines':fatal,
            'limitations':['No intended retail or identified working-PC demo landmark baseline. Measured ratios do not prescribe a correction factor.','Independent timing observation includes bounded observer/logging overhead.','No manual normal gameplay window or first-person/boss retest in this capture.','No new Mod import/execution/texture-output validation.','CPU publication is not GPU completion or display scanout; the long wait still needs upstream attribution.'],
        }

if __name__=='__main__':
    require(len(sys.argv)==2,'Usage: python3 analyze_timing_evidence.py EVIDENCE_OR_HANDOFF.zip')
    print(json.dumps(analyze(sys.argv[1]),indent=2,sort_keys=True))
