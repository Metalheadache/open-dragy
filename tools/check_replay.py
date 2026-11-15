#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 Yizhan Wu
"""End-to-end delivery-loss and malformed-input checks against the built tool."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('build', type=Path)
args = parser.parse_args()
build = args.build.resolve()
with tempfile.TemporaryDirectory(prefix='dragy-reference-replay-') as folder:
    root = Path(folder)
    clean = root / 'clean.csv'
    subprocess.run([str(build/'reference_generate'), str(clean)], check=True)
    lines = clean.read_text().splitlines()
    def run(name, rows, flags, code):
        path = root / (name + '.csv')
        path.write_text('\n'.join(rows) + '\n')
        result = subprocess.run([str(build/'reference_replay'), str(path), *flags], text=True, capture_output=True)
        assert result.returncode == code, (name, result.returncode, result.stdout, result.stderr)
        return result
    run('clean', lines, ['--arm', '--expect-complete'], 0)
    run('clean-qualified25', lines, ['--arm', '--expect-complete', '--require-25hz'], 0)
    # Delete an entire established 40 ms epoch during the active launch.
    gap = [line for line in lines if not line.startswith('13000000,')]
    # 3000 ms includes zero jitter on this synthetic profile.
    assert len(gap) == len(lines)-3
    result = run('gap', gap, ['--arm', '--expect-complete'], 1)
    assert 'GPS sample gap' in result.stdout
    run('gap-qualified25', gap, ['--arm', '--expect-complete', '--require-25hz'], 0)
    dense = [line for line in gap if not line.startswith('13120000,')]
    assert len(dense) == len(lines)-6
    run('dense-gap-qualified25', dense, ['--arm', '--expect-complete', '--require-25hz'], 1)
    corrupt = lines.copy()
    row = 1 + (3000//40)*3 + 1
    stamp, data = corrupt[row].split(',')
    corrupt[row] = stamp + ',' + ('01' if data[:2] != '01' else '00') + data[2:]
    result = run('corrupt', corrupt, ['--arm', '--expect-complete'], 1)
    assert 'checksum_errors=1' in result.stderr
    run('unarmed', lines, ['--expect-complete'], 1)
    run('backwards', ['arrival_us,hex', '2,00', '1,00'], [], 2)
    run('oversize', ['arrival_us,hex', '1,'+'00'*513], [], 2)
    run('badhex', ['arrival_us,hex', '1,zz'], [], 2)
print('PASS replay: clean completion, missing epoch, checksum corruption, unarmed operation, backwards time, oversize and bad hex')
