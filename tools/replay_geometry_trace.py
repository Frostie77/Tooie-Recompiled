#!/usr/bin/env python3
"""Reconstruct sampled world triangles from opt-in Runtime geometry records.

Uses Python arithmetic on recorded float32 inputs, not GPU execution. Categories
and endpoint-envelope departures are observations, not error classifications.
"""
import argparse
import json
import math
from pathlib import Path
import sys


def vector(value, count):
    result = [float(x) for x in value.split('/')]
    if len(result) != count or not all(math.isfinite(x) for x in result):
        raise ValueError('invalid/non-finite vector')
    return result


def world(vertex, matrix, weight):
    point = [a - b * (1 - weight) for a, b in zip(vertex['pos'], vertex['vel'])] + [1]
    return [sum(point[i] * matrix[i][j] for i in range(4)) for j in range(4)]


def edges(points):
    return [math.dist(points[k][:3], points[(k + 1) % 3][:3]) for k in range(3)]


def replay(path):
    data = Path(path).read_bytes()
    text = data.decode('utf-16' if data[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig')
    headers, triangles = {}, {}
    for number, line in enumerate(text.splitlines(), 1):
        if 'Runtime geometry: ' not in line:
            continue
        try:
            fields = dict(word.split('=', 1) for word in line.split('Runtime geometry: ', 1)[1].split() if '=' in word)
            key = (int(fields['sample']), int(fields['workload']))
            if 'weight' in fields:
                weights = [float(fields['weight']), float(fields['previous_weight'])]
                if not all(math.isfinite(x) for x in weights) or not 0 < weights[0] < 1:
                    raise ValueError('invalid sample weights')
                headers[key] = weights
                continue
            key += (fields['class'],)
            triangle = triangles.setdefault(key, {'vertices': {}})
            if 'found' in fields:
                triangle['found'] = fields['found'] == '1'
                triangle['call'] = int(fields['call'])
                triangle['face'] = int(fields['face'])
            elif 'corner' in fields:
                corner = int(fields['corner'])
                if corner not in range(3):
                    raise ValueError('invalid corner')
                vertex = triangle['vertices'].setdefault(corner, {})
                if 'matrix' in fields:
                    rows = [vector(row, 4) for row in fields['rows'].split(';')]
                    if len(rows) != 4:
                        raise ValueError('invalid matrix')
                    vertex[fields['matrix']] = rows
                else:
                    vertex.update({k: vector(fields[k], 3) for k in ('pos', 'vel', 'prev_source')})
                    vertex['pair'] = [int(fields['transform']), int(fields['prev_transform'])]
                    vertex['previous_in_range'] = fields['prev_vertex_in_range'] == '1'
        except (KeyError, ValueError) as error:
            raise ValueError(f'line {number}: {error}') from error
    results = []
    for key, triangle in triangles.items():
        if triangle.get('found') is False:
            continue
        try:
            weight, previous_weight = headers[key[:2]]
            vertices = [triangle['vertices'][i] for i in range(3)]
            raw_previous = [world(v, v['previous'], 0) for v in vertices]
            raw_current = [world(v, v['current'], 1) for v in vertices]
            consumed = [world(v, v['consumed'], weight) for v in vertices]
            consumed_previous = [world(v, v['consumed_previous'], previous_weight) for v in vertices]
            a, b, c = edges(raw_previous), edges(raw_current), edges(consumed)
            departure = max(max(0, min(x, y) - z, z - max(x, y)) / max(x, y, 1e-6) for x, y, z in zip(a, b, c))
            results.append(dict(sample=key[0], workload=key[1], category=key[2], identity='unknown',
                                call=triangle['call'], face=triangle['face'], weight=weight,
                                previous_weight=previous_weight, pairs=[v['pair'] for v in vertices],
                                raw_previous_edges=a, raw_current_edges=b, consumed_edges=c,
                                consumed_previous_edges=edges(consumed_previous),
                                consumed_positions=consumed, endpoint_envelope_departure=departure,
                                previous_source_delta=[math.dist(v['prev_source'], [p - vel for p, vel in zip(v['pos'], v['vel'])]) if v['previous_in_range'] else None for v in vertices]))
        except KeyError as error:
            raise ValueError(f'incomplete sample {key}: missing {error}') from error
    if not results:
        raise ValueError('no complete geometry samples (trace disabled, absent or torn)')
    return results


def consumer_input(path):
    """Export complete recorded inputs without inferring missing match/history.

    Kept separate from the historical edge-report schema. The native consumer
    replay uses the recorded mapping and effective flags, not a newly guessed
    AUTO match or velocity history. Mixed processes and duplicate/torn records
    must not silently become one apparently complete sample.
    """
    data = Path(path).read_bytes()
    text = data.decode('utf-16' if data[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig')
    samples, owners = {}, set()
    matrices = {'previous', 'current', 'consumed', 'consumed_previous'}
    for number, line in enumerate(text.splitlines(), 1):
        if 'Runtime geometry: ' not in line:
            continue
        try:
            fields = dict(word.split('=', 1) for word in line.split('Runtime geometry: ', 1)[1].split() if '=' in word)
            owner = (int(fields['pid']), int(fields['tid']))
            owners.add(owner)
            key = (int(fields['sample']), int(fields['workload']))
            sample = samples.setdefault(key, {'owner': owner, 'triangles': {}})
            if sample['owner'] != owner:
                raise ValueError('sample has conflicting PID/TID')
            if 'weight' in fields:
                if 'header' in sample:
                    raise ValueError('duplicate sample header')
                weights = [float(fields['weight']), float(fields['previous_weight'])]
                if not all(math.isfinite(x) and 0 <= x <= 1 for x in weights) or not 0 < weights[0] < 1:
                    raise ValueError('invalid sample weights')
                sample['header'] = fields
                continue
            triangle = sample['triangles'].setdefault(fields['class'], {'corners': {}})
            if 'found' in fields:
                if 'draw' in triangle or fields['found'] not in ('0', '1'):
                    raise ValueError('invalid/duplicate draw record')
                triangle['draw'] = fields
                continue
            corner = int(fields['corner'])
            if corner not in range(3):
                raise ValueError('invalid corner')
            vertex = triangle['corners'].setdefault(corner, {})
            kind = fields.get('matrix', 'meta')
            if kind in vertex or kind not in matrices | {'meta'}:
                raise ValueError('duplicate/unknown corner record')
            if kind == 'meta':
                flags = int(fields['lerp_flags'], 0)
                if flags & ~0x3F or fields['mapped'] not in ('0', '1'):
                    raise ValueError('unsupported interpolation flags or mapping state')
                for mode in ('position_mode', 'rotation_mode', 'vertex_mode'):
                    if int(fields[mode]) not in (0, 1, 2):
                        raise ValueError('invalid component mode')
                vertex['meta'] = fields
                for name in ('pos', 'vel', 'prev_source'):
                    vertex[name] = vector(fields[name], 3)
            else:
                rows = [vector(row, 4) for row in fields['rows'].split(';')]
                if len(rows) != 4:
                    raise ValueError('invalid matrix')
                vertex[kind] = rows
        except (KeyError, ValueError) as error:
            raise ValueError(f'line {number}: {error}') from error
    if len({pid for pid, _ in owners}) != 1:
        raise ValueError('expected geometry from exactly one process; filter the log explicitly')
    result = []
    for key, sample in samples.items():
        if 'header' not in sample:
            raise ValueError(f'incomplete sample {key}: missing header')
        for category, triangle in sample['triangles'].items():
            if 'draw' not in triangle:
                raise ValueError(f'incomplete sample {key}: missing draw')
            if triangle['draw']['found'] == '0':
                if triangle['corners']:
                    raise ValueError(f'unselected triangle {key} contains corners')
                continue
            if set(triangle['corners']) != {0, 1, 2} or any(
                    not (matrices | {'meta', 'pos', 'vel', 'prev_source'}) <= set(vertex)
                    for vertex in triangle['corners'].values()):
                raise ValueError(f'incomplete sample {key}: missing corner inputs')
            result.append(dict(sample=key[0], workload=key[1], category=category,
                               header=sample['header'], draw=triangle['draw'],
                               corners=[triangle['corners'][i] for i in range(3)]))
    if not result:
        raise ValueError('no complete consumer inputs')
    return dict(schema=1, identity='unknown', history='not-recorded',
                effective_flags='recorded-not-recomputed', triangles=result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', help='Native debug log or Logcat containing Runtime geometry records')
    parser.add_argument('--consumer-input', action='store_true',
                        help='Export matrices, modes, flags and provenance for the production C++ consumer replay')
    args = parser.parse_args()
    try:
        print(json.dumps(consumer_input(args.log) if args.consumer_input else replay(args.log), indent=2, allow_nan=False))
    except (OSError, UnicodeError, ValueError) as error:
        print(f'Geometry replay rejected: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
