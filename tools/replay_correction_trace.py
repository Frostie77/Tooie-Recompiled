#!/usr/bin/env python3
"""Join Step 82 copied source inputs to sampled RT64 geometry and guest clocks.

Reports evidence completeness; it does not infer bone identity, choose an
intermediate pose or declare a device speed multiplier.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import sys

from replay_geometry_trace import consumer_input


def read(path):
    raw = Path(path).read_bytes()
    return raw.decode('utf-16' if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig')


def replay(path):
    sources, data, links, samples, clocks, buckets, pids = {}, {}, {}, set(), [], [], set()
    for number, line in enumerate(read(path).splitlines(), 1):
        if 'Runtime correction-' not in line:
            continue
        try:
            domain, text = line.split('Runtime correction-', 1)[1].split(': ', 1)
            f = dict(word.split('=', 1) for word in text.split() if '=' in word)
            pids.add(int(f['pid']))
            if domain == 'source':
                key = int(f['id'])
                if key in sources:
                    raise ValueError('duplicate source ID')
                sources[key] = f
            elif domain == 'data':
                key = (int(f['id']), f['kind'])
                values = data.setdefault(key, {})
                for offset, value in enumerate(f['words'].split('/'), int(f['first_word'])):
                    if not re.fullmatch(r'[0-9A-Fa-f]{8}', value) or not 0 <= offset < 16:
                        raise ValueError('invalid raw data word/offset')
                    if offset in values:
                        raise ValueError('duplicate data word')
                    values[offset] = int(value, 16)
            elif domain == 'link':
                key = (int(f['sample']), int(f['workload']), int(f['transform']), f['endpoint'])
                if key in links:
                    raise ValueError('duplicate consumer link')
                links[key] = f
            elif domain == 'sample':
                key = (int(f['sample']), int(f['workload']))
                if key in samples:
                    raise ValueError('duplicate sample')
                samples.add(key)
            elif domain == 'clock':
                clocks.append(f)
            elif domain == 'clock-bucket':
                buckets.append(f)
        except (KeyError, ValueError) as error:
            raise ValueError(f'line {number}: {error}') from error
    if len(pids) != 1:
        raise ValueError('expected correction evidence from exactly one process')

    def words(ident, kind, count):
        values = data.get((ident, kind), {})
        return [values[i] for i in range(count)] if set(values) == set(range(count)) else None

    builders = {}
    for ident, f in sources.items():
        if f['kind'] != 'builder':
            continue
        count = int(f['count'])
        if not 0 < count <= 128:
            raise ValueError('invalid builder count')
        bones = {}
        for bone_id, bone in sources.items():
            if bone['kind'] != 'bone-data' or int(bone['builder']) != ident:
                continue
            index = int(bone['index'])
            if index in bones or not 0 <= index < count:
                raise ValueError('invalid/duplicate builder bone')
            bind = words(bone_id, 'bind', 4)
            local = words(bone_id, 'local', 10)
            result = words(bone_id, 'result', 16)
            bones[index] = dict(bind_words=bind, local_words=local, result_words=result)
            if bind:
                parent = bind[3] & 65535
                bones[index].update(parent=parent - 65536 if parent >= 32768 else parent,
                                    channel=bind[3] >> 16,
                                    authored_xyz_bits=[f'{v:08X}' for v in bind[:3]])
        complete = len(bones) == count and all(all(v[k] is not None for k in ('bind_words', 'local_words', 'result_words')) for v in bones.values())
        builders[ident] = dict(source=f, complete=complete, bones=bones)

    def ownership(key):
        link = links.get(key)
        if not link:
            return dict(status='missing-link')
        ident = int(link['binding'])
        if not ident:
            return dict(status='ancestor-only' if int(link['ancestor']) else 'unresolved', link=link)
        binding = sources.get(ident)
        if not binding or binding['kind'] != 'matrix':
            return dict(status='missing-source', link=link)
        if int(link['matches']) != 1 or binding['published'] != '1' or any(
                int(binding[k], 0) != int(link[k], 0) for k in ('address', 'command')):
            raise ValueError('consumer/source binding mismatch')
        for k in ('epoch', 'source_frame'):
            if binding.get(k) != link.get(k) or k not in binding:
                raise ValueError('consumer/source lifetime mismatch')
        if int(link.get('decoded_frame', '0')) != int(binding['source_frame']) or not int(binding['source_frame']):
            raise ValueError('consumer did not decode the matching source-frame marker')
        model_id = int(binding['model'])
        model = sources.get(model_id)
        fixed = words(ident, 'fixed', 16)
        if not model or model['kind'] != 'model' or fixed is None or 'input' not in link:
            return dict(status='incomplete-source', link=link, binding=binding)
        if any(model.get(k) != binding[k] for k in ('epoch', 'source_frame')):
            raise ValueError('model/source lifetime mismatch')
        # Same layout and conversion as FixedMatrix::toFloat (including the
        # host halfword xor). Verify the RSP input, not an assumed world matrix.
        converted = []
        for i in range(4):
            for j in range(4):
                shift = 16 if j % 2 == 0 else 0
                integer = (fixed[i*2+j//2] >> shift) & 65535
                integer = integer - 65536 if integer >= 32768 else integer
                fraction = (fixed[8+i*2+j//2] >> shift) & 65535
                converted.append(struct.unpack('f', struct.pack('f', integer + fraction/65536))[0])
        inputs = [struct.unpack('f', struct.pack('f', float(x)))[0] for x in link['input'].split('/')]
        if inputs != converted:
            raise ValueError('fixed-matrix/RSP-input mismatch')
        builder_id = int(binding['builder'])
        builder = builders.get(builder_id)
        bone_index = int(binding['bone'])
        inserted_mvp = bool(int(link.get('params', '0'), 0) & 0x100)
        complete = bool(not inserted_mvp and words(ident, 'bone-root-float', 16) and words(model_id, 'model-header', 16) and
                        builder and builder['complete'] and
                        int(builder['source'].get('output', '0'), 0) == int(binding.get('selected_list', '0'), 0) + 0x50 and
                        builder['source'].get('epoch') == binding['epoch'] and bone_index in builder['bones'] and
                        words(ident, 'bone-float', 16) == builder['bones'][bone_index]['result_words'])
        return dict(status='copied-bone-inputs' if complete else 'direct-source-without-complete-bone-inputs',
                    link=link, binding=binding, model=model, builder_id=builder_id,
                    inserted_mvp_unobserved=inserted_mvp,
                    root_float_words=words(ident, 'bone-root-float', 16) or words(model_id, 'root-float', 16),
                    model_header_words=words(model_id, 'model-header', 16))

    triangles = []
    has_geometry = any('Runtime geometry:' in line and 'found=1 ' in line for line in read(path).splitlines())
    if samples and has_geometry:
        for g in consumer_input(path)['triangles']:
            key = (g['sample'], g['workload'])
            if key not in samples:
                continue
            if int(g['header']['pid']) not in pids:
                raise ValueError('geometry belongs to another process')
            g['ownership'] = [dict(current=ownership(key + (int(c['meta']['transform']), 'current')),
                                   previous=ownership(key + (int(c['meta']['transform']), 'previous')))
                              for c in g['corners']]
            triangles.append(g)
    windows = []
    for clock in clocks:
        end = int(clock['window_end_us'])
        elapsed = int(clock['elapsed_us']) / 1e6
        matched = [b for b in buckets if int(b['window_end_us']) == end and b['tid'] == clock['tid']]
        windows.append(dict(clock=clock, buckets=matched, startup_or_short=elapsed < 4,
                            replay_records_per_wall_second=sum(int(b['records']) for b in matched) / elapsed if elapsed else None,
                            timer_guest_seconds_per_wall_second=float(clock['timer_delta_sum']) / elapsed if elapsed else None,
                            attract_guest_seconds_per_wall_second=float(clock['attract_delta_sum']) / elapsed if elapsed else None))
    if not triangles and not windows:
        raise ValueError('no joined geometry samples or guest-clock windows')
    nodes = {ident: dict(source=f, prefix_words=words(ident, 'node', 8))
             for ident, f in sources.items() if f['kind'] == 'node'}
    return dict(schema=1, pid=next(iter(pids)), expected_pose='not-established',
                builders=builders, source_nodes=nodes, triangles=triangles, clock_windows=windows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log')
    args = parser.parse_args()
    try:
        print(json.dumps(replay(args.log), indent=2, allow_nan=False))
    except (OSError, UnicodeError, KeyError, ValueError, OverflowError) as error:
        print(f'Correction replay rejected: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
