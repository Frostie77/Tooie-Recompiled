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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', help='Native debug log or Logcat containing Runtime geometry records')
    args = parser.parse_args()
    try:
        print(json.dumps(replay(args.log), indent=2, allow_nan=False))
    except (OSError, UnicodeError, ValueError) as error:
        print(f'Geometry replay rejected: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
