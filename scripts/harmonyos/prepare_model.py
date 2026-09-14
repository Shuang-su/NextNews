#!/usr/bin/env python3
"""Create a bounded SH0 3DGS PLY copy; never overwrite source or destination.

For compressed inputs, first use the pinned splat-transform command documented
in docs/harmonyos/README.md. Sampling is deterministic and recorded in a manifest.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct

FIELDS = ['x', 'y', 'z', 'f_dc_0', 'f_dc_1', 'f_dc_2', 'opacity',
          'scale_0', 'scale_1', 'scale_2', 'rot_0', 'rot_1', 'rot_2', 'rot_3']
TYPES = {'float': 'f', 'float32': 'f', 'double': 'd', 'float64': 'd',
         'uchar': 'B', 'uint8': 'B', 'char': 'b', 'int8': 'b',
         'short': 'h', 'int16': 'h', 'ushort': 'H', 'uint16': 'H',
         'int': 'i', 'int32': 'i', 'uint': 'I', 'uint32': 'I'}


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def prepare(source, destination, limit=100000, label='', license_note='Private local validation only'):
    source, destination = Path(source).resolve(), Path(destination).resolve()
    if source == destination or destination.exists() or destination.with_suffix('.manifest.json').exists():
        raise ValueError('Refusing to overwrite an existing model')
    if not 1 <= limit <= 4000000:
        raise ValueError('Limit must be between 1 and 4000000')
    with source.open('rb') as stream:
        if stream.readline().strip() != b'ply':
            raise ValueError('Not PLY; decompress with splat-transform first')
        props, fmt, count, done = [], [], None, False
        current = ''
        format_seen = False
        for _ in range(512):
            line = stream.readline(8192).decode('ascii').strip()
            words = line.split()
            if line == 'end_header':
                done = True
                break
            if not words:
                raise ValueError('Incomplete PLY header')
            if words[0] == 'format':
                if format_seen or words[1:] != ['binary_little_endian', '1.0']:
                    raise ValueError('Expected one binary little-endian PLY format declaration')
                format_seen = True
            if words[0] == 'element':
                current = words[1]
                if current == 'vertex':
                    if count is not None:
                        raise ValueError('Duplicate vertex element')
                    count = int(words[2])
                elif int(words[2]) != 0:
                    raise ValueError('Expected Gaussian vertices only')
            if words[0] == 'property':
                if current != 'vertex' or words[1] not in TYPES or len(words) != 3:
                    raise ValueError('Unsupported PLY property')
                if words[2] in props:
                    raise ValueError('Duplicate property')
                props.append(words[2])
                fmt.append(TYPES[words[1]])
        if not done or not format_seen or count is None or count < 1:
            raise ValueError('Invalid PLY header')
        row = struct.Struct('<' + ''.join(fmt))
        if source.stat().st_size - stream.tell() != count * row.size:
            raise ValueError('Payload length does not match header')
        indices = [props.index(f) for f in FIELDS]
        output_count = min(count, limit)
        # Deterministic, spread-out selection, retaining source order.
        selected = [(i * count) // output_count for i in range(output_count)]
        data_start = stream.tell()
        packed = bytearray()
        opacity_limits = 0
        for index in selected:
            stream.seek(data_start + index * row.size)
            values = row.unpack(stream.read(row.size))
            v = [values[i] for i in indices]
            # Exact alpha 0/1 has logit -/+infinity in otherwise valid 3DGS exports.
            # Convert only these mathematically defined endpoints, never NaN.
            if math.isinf(v[6]):
                v[6] = math.copysign(20.0, v[6])
                opacity_limits += 1
            if not all(math.isfinite(x) for x in v):
                raise ValueError(f'Non-finite Gaussian at vertex {index}')
            if any(abs(x) > 1e6 for x in v[:3]) or any(x < -30 or x > 14 for x in v[7:10]):
                raise ValueError('Position or log-scale outside renderer limits')
            norm = math.sqrt(sum(x*x for x in v[10:14]))
            if norm < 1e-6:
                raise ValueError('Zero quaternion')
            v[10:14] = [x/norm for x in v[10:14]]
            packed.extend(struct.pack('<14f', *v))
    header = 'ply\nformat binary_little_endian 1.0\ncomment NextNews SH0 deterministic sample\n'
    header += f'element vertex {output_count}\n'
    header += ''.join(f'property float {name}\n' for name in FIELDS) + 'end_header\n'
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open('xb') as stream:
        stream.write(header.encode('ascii'))
        stream.write(packed)
    manifest = {'label': label, 'source': str(source), 'source_sha256': sha256(source),
                'output': destination.name, 'output_sha256': sha256(destination),
                'input_count': count, 'output_count': output_count, 'sh_degree': 0,
                'sampling': 'even source indices floor(i * input_count / output_count)',
                'high_order_sh_removed': any(x.startswith('f_rest_') for x in props),
                'opacity_infinity_to_logit_20': opacity_limits,
                'quaternion': 'wxyz normalized', 'coordinate_transform': 'none', 'license': license_note}
    destination.with_suffix('.manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--limit', type=int, default=100000)
    parser.add_argument('--label', default='')
    parser.add_argument('--license', default='Private local validation only')
    args = parser.parse_args()
    print(json.dumps(prepare(args.source, args.destination, args.limit, args.label, args.license), ensure_ascii=False, indent=2))
