"""Read-only Step 118 evidence analyzer, supplied in the reviewed handoff.

Adapted from the reviewed Step 115 analyzer for the exact Step 117 capture; not an independent timing baseline.
Standard library; no execution or extraction.

Review this source before running. Input is the ORIGINAL Step 117 evidence ZIP,
or the Step 118 handoff ZIP containing it. Output is deterministic JSON on stdout.
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

NAME='Tooie-49b98b49c6f2-Producer-Timing-Odin-20261020-173425-c8793498-Evidence.zip'
EXPECTED_SHA='3bb6456e15c87922a71a268c9fa66a5db08ef7e271ee0db576735435656a9760'
EXPECTED_APK='f2dd7990496bb9caa6f413851ddd305df588808560d8089e7aeae16de9270cff'
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
    require(len(raw)==23506810 and digest(raw)==EXPECTED_SHA,'Original evidence identity mismatch')
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        require(z.testzip() is None,'ZIP CRC failure')
        names=z.namelist()
        require(len(names)==len(set(names))==308,'Unexpected duplicate/member count')
        require(all('/' not in n and '\\' not in n and n.startswith(PREFIX+'-') for n in names),'Unexpected member path')
        def read(suffix):return z.read(PREFIX+'-'+suffix+'.txt')
        def text(suffix):return read(suffix).decode('utf-8',errors='replace').strip()
        pid=int(text('Original-Launch-PID'))
        require(pid==26855 and text('Final-PIDs')==str(pid),'Unexpected original/final PID')
        identity=dict(line.split('=',1) for line in text('Identity').splitlines())
        require(identity['commit']=='49b98b49c6f2753a232debdab8aef249483ca152' and identity['hash']==EXPECTED_APK,'Unexpected build identity')
        require(text('Installed-APK-SHA256').split()[0]==EXPECTED_APK,'Recorded installed hash mismatch')
        require(identity['engineering_ancestor']=='121787042b50b3250036bc6013b501c737e4d4ac' and identity['build_id']=='eaf1b57693973eed5d75f46f1dab130468803027','Unexpected engineering/native identity')
        require('Build mode: INDEPENDENT PRODUCER TIMING DIAGNOSTIC DEBUG' in text('Build-Report'),'Unexpected saved build profile')
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
        marker_lines={json.loads(l.split('TooieUserMark: ',1)[1])['id']:l for l in combined if 'TooieUserMark: ' in l}
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

        producers=records('full-sync')
        require(len(producers)==75,'Unexpected producer record count')
        # The long publication gap is safely inside this producer reporting
        # interval. Its extrema cover completed calls, not exact per-call traces.
        enclosing=[r for r in producers if int(r['mono_us'])-int(r['elapsed_us']) < end-gap-1000 and int(r['mono_us'])>end+1000]
        require(len(enclosing)==1,'No unique producer reporting interval enclosing the complete gap')
        producer=enclosing[0]
        previous=producers[producers.index(producer)-1]
        require(int(previous['mono_us'])<end-gap and int(producer['mono_us'])>end,'Producer reports do not bracket the gap')
        # This lifetime maximum is from startup; it must not be assigned to a
        # later demo transition just because it repeats on later log lines.
        lifetime=max(producers,key=lambda r:int(r['entry_gap_max_us']))
        producer_deltas={key:int(producer[key])-int(previous[key]) for key in ('current_disabled','empty','no_candidate','no_history_match','published')}
        require(sum(producer_deltas.values())==int(producer['events']),'Producer cumulative counters do not reconcile')
        def nonstartup_maximum(domain,key):
            rs=[r for r in records(domain) if int(r.get('elapsed_us','0'))>0]
            best=max(rs,key=lambda r:stage(r,key)['max_us'])
            return {'stage':stage(best,key),'report_mono_us':int(best['mono_us'])}
        producer_report={
            'count':len(producers),'enclosing_report':producer,'preceding_report':previous,
            'completed_calls_in_enclosing_window':int(producer['events']),
            'enclosing_window_result_count_deltas':producer_deltas,
            'enclosing_stages':{key:stage(producer,key) for key in ('prepare','advance','publish_reset')},
            'largest_completed_call_in_enclosing_window_us':int(producer['event_max_us']),
            'gap_exceeds_every_completed_producer_call_in_enclosing_window':gap>int(producer['event_max_us']),
            'lifetime_entry_gap_max_us':int(lifetime['entry_gap_max_us']),
            'lifetime_entry_gap_end_us':int(lifetime['entry_gap_end_us']),
            'nonstartup_stage_maxima':{key:nonstartup_maximum('full-sync',key) for key in ('prepare','advance','publish_reset')},
            'last_cumulative_counts':{key:int(producers[-1][key]) for key in ('current_disabled','empty','no_candidate','no_history_match','published')},
            'inference':'The 841862us publication gap is not a comparably long completed State::fullSync call in its enclosing window (largest 48425us). The producer entry-gap lifetime maximum is an earlier startup event and obscures later inter-entry gaps. The uninstrumented work before/between fullSync calls, task delivery and scheduling still require attribution; no exact cause is established.',
            'scope':'Published in FullSyncTrace means that its updateScreen call returned; the present-input counters separately observe queue publication. Stage maxima and event peak context can belong to different calls. Do not sum maxima.',
        }
        stage_peaks={domain:{key:nonstartup_maximum(domain,key) for key in keys} for domain,keys in {
            'present-queue':('cursor_wait','cursor_condition','cursor_lock','producer_wait','dispatch'),
            'workload-stages':('matching','render','present_wait'),
            'render-stages':('interpolation','record','gpu_wait','worker_lock'),
            'present-stages':('present','gpu_wait','target_wait'),
        }.items()}
        for marker in marks:
            marker['seconds_after_demo_start']=marker['host_read_mono_seconds']-windows[0]['host_start_monotonic_seconds']
            marker['matching_device_logcat_line']=marker_lines[marker['id']]
        marks_short=[{k:r[k] for k in ('sequence','marker','description','host_read_wall','host_read_mono_seconds','phase_at_read','seconds_after_demo_start','matching_device_logcat_line')} for r in marks]

        return {
            'archive':{'name':NAME,'bytes':len(raw),'sha256':digest(raw),'members':len(names),'uncompressed_bytes':sum(i.file_size for i in z.infolist()),'crc_verified':True},
            'recorded_build_identity':identity,
            'binary_scope':'APK/native binaries were not supplied for independent inspection; identity is from the collector and saved installed-hash command.',
            'delta':{s:{'bytes':len(data),'sha256':digest(data)} for s,data in (('Baseline',baseline),('FullLog',full),('NewLog',new))},
            'delta_exact':True,
            'capture':{'original_pid':pid,'final_pid':pid,'fresh_command_statuses':len(statuses),'fresh_command_failures':0,'timed_pid_samples':len(samples),'windows':windows,'gameplay_windows':0,'decision':text('Demo-01-Capture-Decision'),'user_observation':text('User-Demo-Observation')},
            'markers':marks_short,
            'marker_delivery':{'device_log_matches':len(device_marks),'host_acknowledged':len(deliveries),'maximum_host_handling_delay_seconds':max(deliveries[r['id']]['host_handled_mono_seconds']-r['host_read_mono_seconds'] for r in marks)},
            'runtime_counts':{k:len(v) for k,v in sorted(domains.items())},
            'clocks':{'groups':{k:clock_stats(v) for k,v in groups.items()},'previously_requested_demo_maps_335_278_314':clock_stats([c for c in groups['successful_replay'] if int(c['map']) in (335,278,314)]),'rows':clocks,'bucket_rows':records('correction-clock-bucket'),'sparse_event_count':len(events),'scope':'Ratios cover complete fresh reporting windows, including time outside the 300-second observation. Window-end map is not proof of an unmixed map. Pure classifications use recorded bucket/reset/end flags; sparse details do not enumerate every input.'},
            'publication_gap':{'duration_us':gap,'start_us':end-gap,'end_us':end,'first_report_mono_us':int(publication['mono_us']),'largest_entry_gap_us':max(int(r['entry_gap_max_us']) for r in inputs),'last_result_counts':{k:int(inputs[-1][k]) for k in ('current_repeated','early_ignored','unchanged','published')},'source_record':publication},
            'post_startup_queue_peak':{'report':wait,'cursor_wait':stage(wait,'cursor_wait'),'cursor_condition':stage(wait,'cursor_condition'),'cursor_lock_window_max':stage(wait,'cursor_lock'),'largest_initial_cursor_lock_us':max(stage(r,'cursor_lock')['max_us'] for r in queues),'condition_end_minus_publication_us':stage(wait,'cursor_condition')['end_us']-end,'scope':'Excludes the elapsed_us=0 startup report. Condition time includes mutex reacquisition; nested stages and independent maxima are not additive.'},
            'full_sync_producer':producer_report,
            'nonstartup_stage_peaks':stage_peaks,
            'guest_frame_sync':{'records':len(frames),'largest_observed_call_us':int(frame_max['max_us']),'largest_record':frame_max,'near_publication_gap_sparse_inputs':around,'sparse_event_rows':events,'scope':'Peak snapshots belong to the maximum-duration call in each reporting window. after_elapsed is not every frame return.'},
            'runtime_rates_entire_fresh_capture':{'vi_intervals':len(vi),'vi_seconds':vi_seconds,'vi_iterations':sum(int(r['vi_iterations']) for r in vi),'vi_iterations_per_second':sum(int(r['vi_iterations']) for r in vi)/vi_seconds,'speed_multipliers':sorted({r['speed_multiplier'] for r in vi}),'native_present_intervals':len(present),'native_present_seconds':present_seconds,'native_present_calls':sum(int(r['native_present_calls']) for r in present),'native_presents_per_second':sum(int(r['native_present_calls']) for r in present)/present_seconds,'largest_native_present_gap_us':max(int(r['gap_max_us']) for r in present),'scope':'Launcher/intro/demo/prompt intervals are included. These counters do not establish intended game/demo cadence.'},
            'audio':{'open':[r for r in audio if r.get('event')=='open'],'software_queue_windows':len(audio_windows),'software_errors_sum':sum(int(r['errors']) for r in audio_windows),'maximum_sdl_queue_us':max(int(r['sdl_queue_us']) for r in audio_windows),'opensles_mutex_warnings':warnings,'scope':'No audio recording; stop/debug messages or nonfatal warnings do not establish audible dropouts.'},
            'fresh_fatal_logcat_lines':fatal,
            'limitations':['No intended retail or identified working-PC demo landmark baseline. Measured ratios do not prescribe a correction factor.','Independent timing observation includes bounded observer/logging overhead.','No manual normal gameplay window or first-person/boss retest in this capture.','No new Mod import/execution/texture-output validation.','CPU publication is not GPU completion or display scanout; the long wait still needs upstream attribution.','Host and Odin wall clocks differ; marker IDs link matching records, but receipt time is not exact bug onset.'],
        }

if __name__=='__main__':
    require(len(sys.argv)==2,'Usage: python3 analyze_timing_evidence.py EVIDENCE_OR_HANDOFF.zip')
    print(json.dumps(analyze(sys.argv[1]),indent=2,sort_keys=True))
