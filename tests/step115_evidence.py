"""Reproduce the inspected handoff analysis and independently check arithmetic.

No device/network access. An optional local original/handoff archive is required
for full provenance checks; the retained fixture always runs in native tests.
"""
from decimal import Decimal, localcontext
from pathlib import Path
import importlib.util
import io
import json
import sys
import zipfile

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('step115',root/'tools/analyze_step115_evidence.py')
reader=importlib.util.module_from_spec(spec);spec.loader.exec_module(reader)
path=Path(sys.argv[1]);result=reader.analyze(path)
with zipfile.ZipFile(path) as outer:
    if reader.NAME in outer.namelist():
        assert json.loads(json.dumps(result))==json.loads(outer.read('analysis.json'))
        with zipfile.ZipFile(io.BytesIO(outer.read(reader.NAME))) as original:
            new_log=original.read(reader.PREFIX+'-NewLog.txt')
        # Rewrite one non-executable payload without its manifest update. A
        # valid original nested archive must not mask a modified handoff.
        stream=io.BytesIO()
        with zipfile.ZipFile(stream,'w') as bad:
            for name in outer.namelist():
                data=outer.read(name)
                bad.writestr(name,data+b'changed' if name=='README-Codex.md' else data)
        changed=Path(sys.argv[2])/'changed-handoff.zip';changed.write_bytes(stream.getvalue())
        try: reader.analyze(changed)
        except ValueError as error: assert 'payload identity mismatch' in str(error)
        else: raise AssertionError('Accepted altered handoff payload')
    else:
        assert reader.digest(path.read_bytes())==reader.EXPECTED_SHA
        new_log=outer.read(reader.PREFIX+'-NewLog.txt')

# All field bytes and record order are retained; only source CRLF becomes LF.
records=b''.join(line for line in new_log.splitlines(keepends=True) if line.startswith((
    b'Runtime correction-clock:',b'Runtime correction-clock-bucket:',b'Runtime correction-clock-event:')))
assert records.replace(b'\r\n',b'\n')==(root/'tests/fixtures/odin-step115-clocks.txt').read_bytes()

with localcontext() as context:
    context.prec=40
    rows={int(c['window_end_us']):c for c in result['clocks']['rows']}
    def ratio(group,key):
        selected=[rows[end] for end in group['window_end_us']]
        elapsed=sum(Decimal(c['elapsed_us']) for c in selected)/1_000_000
        value=sum(Decimal(c[key]) for c in selected)/elapsed
        return elapsed,value
    ordinary=result['clocks']['groups']['ordinary']
    requested=result['clocks']['previously_requested_demo_maps_335_278_314']
    assert ordinary['windows']==28 and requested['windows']==10
    assert ratio(ordinary,'timer_delta_sum')[1].quantize(Decimal('.000001'))==Decimal('.999799')
    elapsed,game=ratio(requested,'timer_delta_sum')
    assert elapsed==Decimal('50.066494') and game.quantize(Decimal('.000001'))==Decimal('1.384159')
    assert ratio(requested,'attract_delta_sum')[1].quantize(Decimal('.000001'))==Decimal('1.467046')

assert result['delta_exact'] and result['capture']['original_pid']==result['capture']['final_pid']==15060
assert result['capture']['timed_pid_samples']==58 and result['capture']['fresh_command_statuses']==115
assert result['capture']['gameplay_windows']==0
assert not result['fresh_fatal_logcat_lines']
gap=result['publication_gap'];queue=result['post_startup_queue_peak']
assert gap['end_us']-gap['start_us']==gap['duration_us']==855894
assert queue['cursor_condition']['max_us']==824899 and queue['cursor_wait']['max_us']==824900
assert queue['cursor_condition']['end_us']-gap['end_us']==16
assert queue['largest_initial_cursor_lock_us']==16
counts=gap['last_result_counts']
assert sum(counts.values())==35725 and counts==dict(current_repeated=0,early_ignored=24124,unchanged=0,published=11601)
details=result['guest_frame_sync']['near_publication_gap_sparse_inputs']
transition=[r for r in details if r['frame']=='2' and r['elapsed_span']=='50']
assert len(transition)==1 and transition[0]['recorded_span']==transition[0]['applied_span']=='15'
wrong=Path(sys.argv[2])/'wrong.zip'
with zipfile.ZipFile(wrong,'w') as z:z.writestr('unrelated.txt','wrong identity')
try: reader.analyze(wrong)
except ValueError as error: assert 'pinned original' in str(error)
else: raise AssertionError('Accepted wrong archive')
print('PASS: complete supplied analysis reproduced; independent Decimal ratios, exact delta/PID scope, publication/wait endpoints and decision totals; wrong identity and modified payload rejected')
