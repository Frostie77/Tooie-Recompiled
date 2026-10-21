from pathlib import Path
import sys
import tempfile

r = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(r / 'tools'))
from replay_correction_trace import replay

# Synthetic join protocol fixture, not Odin data or an expected pose. Reuse
# the real captured geometry format while all ownership/clock records here
# explicitly have test-only IDs. This exercises completeness/error reporting.
lines = (r / 'tests/fixtures/odin-step81-geometry.txt').read_text().splitlines()

def record(domain, fields, tid=25058):
    return f'Runtime correction-{domain}: mono_us=7000000 pid=24984 tid={tid} {fields}'

sample, workload, transform = 239465050552, 3825, 110
lines += [record('sample', f'sample={sample} workload={workload}'),
          record('source', 'kind=model id=1 builder=2 epoch=100 source_frame=200'),
          record('source', 'kind=builder id=2 count=1 epoch=100 output=0x3050'),
          record('source', 'kind=bone-data id=3 builder=2 index=0'),
          record('source', 'kind=matrix id=4 model=1 builder=2 bone=0 published=1 selected_list=0x3000 address=0x1000 command=0x2000 epoch=100 source_frame=200'),
          record('link', f'sample={sample} workload={workload} transform={transform} endpoint=current binding=4 matches=1 ancestor=0 address=0x1000 command=0x2000 epoch=100 source_frame=200 decoded_frame=200 input=' + '/'.join(['0']*16))]
for ident, kind, count in [(1, 'root-float', 16), (1, 'model-header', 16), (3, 'bind', 4),
                           (3, 'local', 10), (3, 'result', 16), (4, 'fixed', 16), (4, 'bone-float', 16), (4, 'bone-root-float', 16)]:
    lines.append(record('data', f'id={ident} kind={kind} first_word=0 words=' + '/'.join(['00000000'] * count)))
lines += [record('clock', 'window_start_us=1000000 window_end_us=6000000 elapsed_us=5000000 timer_delta_sum=5 attract_delta_sum=10 invalid=0'),
          record('clock-bucket', 'window_end_us=6000000 records=100'),
          record('clock-bucket', 'window_end_us=6000000 records=999', tid=111)]
text = '\n'.join(lines) + '\n'
with tempfile.TemporaryDirectory() as directory:
    p = Path(directory) / 'log.txt'
    p.write_text(text)
    good = replay(p)
    assert good['expected_pose'] == 'not-established'
    assert good['triangles'][0]['ownership'][0]['current']['status'] == 'copied-bone-inputs'
    assert good['triangles'][0]['ownership'][0]['previous']['status'] == 'missing-link'
    assert good['clock_windows'][0]['replay_records_per_wall_second'] == 20
    assert good['clock_windows'][0]['timer_guest_seconds_per_wall_second'] == 1
    assert good['clock_windows'][0]['attract_guest_seconds_per_wall_second'] == 2
    # A ratio in this synthetic control is not a measured device multiplier.
    p.write_text(text, encoding='utf-16')
    assert replay(p) == good
    p.write_text('\n'.join(line for line in lines if 'kind=local first_word=' not in line))
    assert replay(p)['triangles'][0]['ownership'][0]['current']['status'] == 'direct-source-without-complete-bone-inputs'
    # Nonzero signed 16.16 components in the real word-swapped RAM layout:
    # integer -2 + fraction .5, integer 2 + fraction .25. This catches halfword
    # order/sign errors that an all-zero protocol fixture cannot detect.
    signed = text.replace('id=4 kind=fixed first_word=0 words=00000000/',
                          'id=4 kind=fixed first_word=0 words=FFFE0002/', 1)
    signed = signed.replace('kind=fixed first_word=0 words=FFFE0002/' + '/'.join(['00000000']*7) + '/00000000/',
                            'kind=fixed first_word=0 words=FFFE0002/' + '/'.join(['00000000']*7) + '/80004000/', 1)
    signed = signed.replace('input=0/0/', 'input=-1.5/2.25/', 1)
    p.write_text(signed)
    assert replay(p)['triangles'][0]['ownership'][0]['current']['status'] == 'copied-bone-inputs'
    p.write_text(text.replace('matches=1 ancestor=0', 'matches=1 params=0x100 ancestor=0'))
    inserted = replay(p)['triangles'][0]['ownership'][0]['current']
    assert inserted['status'] == 'direct-source-without-complete-bone-inputs' and inserted['inserted_mvp_unobserved']
    for name, bad in [('process', text + record('source', 'kind=model id=99').replace('pid=24984', 'pid=123')),
                      ('duplicate', text + record('source', 'kind=model id=1 builder=2') + '\n'),
                      ('binding', text.replace('matches=1', 'matches=2')),
                      ('input', text.replace('input=0/', 'input=1/', 1)),
                      ('lifetime', text.replace('epoch=100 source_frame=200 decoded_frame=200 input=', 'epoch=100 source_frame=201 decoded_frame=200 input=')),
                      ('uncaptured-frame', text.replace('decoded_frame=200', 'decoded_frame=0')),
                      ('reused-frame', text.replace('decoded_frame=200', 'decoded_frame=201')),
                      ('missing-marker', text.replace('decoded_frame=200 ', '')),
                      ('word-range', text.replace('words=00000000/', 'words=100000000/', 1)),
                      ('fixed-order', signed.replace('FFFE0002/', '0002FFFE/', 1))]:
        p.write_text(bad)
        try:
            replay(p)
        except ValueError:
            pass
        else:
            raise AssertionError('accepted ' + name)
print('PASS: source/consumer joins, incomplete snapshots remain incomplete, PID/TID clock windows, UTF-16, torn-data/mixed-process/duplicate/binding controls (synthetic protocol data)')
