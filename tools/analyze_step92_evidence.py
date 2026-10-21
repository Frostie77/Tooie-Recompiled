#!/usr/bin/env python3
import zipfile,re,datetime,statistics,hashlib,json,sys
# Read-only analysis of the exact retained Step 92 archive. No archive command
# or instruction is executed, and no member is extracted to the filesystem.
if len(sys.argv)!=2: raise SystemExit('usage: analyze_step92_evidence.py EVIDENCE.zip')
p=sys.argv[1]
assert hashlib.sha256(open(p,'rb').read()).hexdigest()=='2e940f4460d84b8dc85fe80115fa9826ec399f30ba7dc7fc5cd1c8726ebe3aaf'
z=zipfile.ZipFile(p)
def read(suffix):
 hits=[n for n in z.namelist() if n.endswith('-'+suffix)];assert len(hits)==1;return z.read(hits[0]).decode('utf-8')
def stamp(s):return round(datetime.datetime.strptime(s,'%Y-%m-%d %H:%M:%S.%f %z').timestamp()*1e6)
anchors=[]
for line in read('Runtime-window.txt').splitlines():
 m=re.search(r'mono_uptime_ms=(\d+) pid=10832',line)
 if m:anchors.append(stamp(re.match(r'^(.+? [+-]\d{4}) ',line)[1])-int(m[1])*1000)
# Parse wall prefix rather than assuming a native counter shares Java's units.
assert len(anchors)==13
assert z.testzip() is None
assert len(z.namelist())==len(set(z.namelist()))==402
offset=statistics.median(anchors)
start=stamp('2026-10-18 11:12:10.000000 +1100')-offset;end=start+600000000
fresh=read('NewLog.txt')
assert hashlib.sha256(fresh.encode()).hexdigest()=='9b8dc58be9280db067db8f006bf7f88885f2c6c4e9ac697c858f01bc19f8fdb2'
pids=re.findall(r'\bpid=(\d+)',fresh);assert len(pids)==13618 and set(pids)=={'10832'}
assert read('FullLog.txt')==read('Baseline.txt')+fresh
def windows(domain):
 output=[]
 for line in read('Runtime-'+domain+'.txt').splitlines():
  fields=dict(re.findall(r'(\w+)=([^\s]+)',line))
  if fields.get('pid')!='10832' or 'elapsed_us' not in fields:continue
  now=int(fields['mono_us']);span=int(fields['elapsed_us'])
  if now-span>=start and now<=end:output.append(fields)
 return output
output={'identity':{'source_commit':'82a6641c228d71756222bc0da96b0f159e51abd4','pid':10832,'fresh_pid_fields':len(pids),'anchors':len(anchors),'wall_offset_spread_us':max(anchors)-min(anchors),'approximate_native_interval_us':[int(start),int(end)]}}
for domain in ['present','vi','present-queue','present-stages','workload-stages','matching-stages','render-stages','audio']:
 w=windows(domain);elapsed=sum(int(d['elapsed_us']) for d in w)
 result={'windows':len(w),'elapsed_seconds':elapsed/1e6}
 if domain in ['present','vi']:
  key='native_present_calls' if domain=='present' else 'vi_iterations'
  rate=[int(d[key])*1e6/int(d['elapsed_us']) for d in w]
  result.update(rate_min=min(rate),rate_median=statistics.median(rate),rate_max=max(rate),weighted_rate=sum(int(d[key]) for d in w)*1e6/elapsed)
  if domain=='present':result.update(below35=sum(r<35 for r in rate),maximum_gap_us=max(int(d['gap_max_us']) for d in w),source_hz=sorted(set(d['vi_original_hz'] for d in w)),target_hz=sorted(set(d['target_hz'] for d in w)))
 stages={}
 for d in w:
  for key,value in d.items():
   parts=value.split('/')
   if len(parts)==4 and all(s.isdigit() for s in parts):
    n,total,maximum,ending=map(int,parts)
    # A complete reporting interval can retain a maximum from another event;
    # stage totals are independent; nested sums must never be added together.
    v=stages.setdefault(key,{'n':0,'total_seconds':0,'maximum_us':0})
    v['n']+=n;v['total_seconds']+=total/1e6;v['maximum_us']=max(v['maximum_us'],maximum)
 result['stages']=stages
 if domain=='present-queue':
  keys=['current_decisions','backlog','empty','unavailable','invalid']
  result['counter_interval_seconds']=(int(w[-1]['mono_us'])-int(w[0]['mono_us']))/1e6
  result['counter_changes_between_first_and_last_reports']={k:int(w[-1][k])-int(w[0][k]) for k in keys}
 output[domain]=result
print(json.dumps(output,indent=2))
