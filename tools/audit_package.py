#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 Yizhan Wu
"""Check the source allowlist, hashes, links and per-file licensing."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = 'reference-manifest.json'
GENERATED = {'build', 'build-release', '.cache', '.git', '__pycache__'}

def source_files():
    def walk(folder):
        for p in sorted(folder.iterdir()):
            rel = p.relative_to(ROOT).as_posix()
            if p.name in GENERATED or rel in {'examples/esp32/build', 'examples/esp32/managed_components'}:
                continue
            if p.is_symlink():
                raise ValueError(f'Symlink is outside the source-package contract: {rel}')
            if p.is_dir():
                yield from walk(p)
            else:
                yield p
    return list(walk(ROOT))

def audit(forbidden=()):
    manifest = json.loads((ROOT / MANIFEST).read_text())
    expected = manifest['files']
    actual = {p.relative_to(ROOT).as_posix(): p for p in source_files() if p.name != MANIFEST}
    if set(actual) != set(expected):
        raise ValueError(f'Allowlist mismatch: unexpected={sorted(set(actual)-set(expected))}; missing={sorted(set(expected)-set(actual))}')
    if manifest['scope'] != ['retrieval', 'coordinates', 'speed-and-drag', '25hz', 'optimization', 'consistency']:
        raise ValueError('Scope declaration changed')
    for rel, digest in expected.items():
        posix = PurePosixPath(rel)
        if posix.is_absolute() or '..' in posix.parts:
            raise ValueError(f'Unsafe path: {rel}')
        raw = actual[rel].read_bytes()
        if hashlib.sha256(raw).hexdigest() != digest:
            raise ValueError(f'Reviewed file changed: {rel}')
        text = raw.decode('utf-8')
        if '\x00' in text:
            raise ValueError(f'Binary data in source payload: {rel}')
        for token in forbidden:
            if token.casefold() in text.casefold():
                raise ValueError(f'Forbidden private token found in {rel}')
        # Match actual workstation paths rather than examples in this script.
        if re.search(r'/(?:Users|home)/[A-Za-z0-9_.-]+/', text):
            raise ValueError(f'Workstation path in {rel}')
        if re.search(r'(?i)(?:gh[pousr]_[A-Za-z0-9]{20,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)', text):
            raise ValueError(f'Potential credential in {rel}')
        if re.search(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}', text):
            raise ValueError(f'Literal device address in {rel}')
        if actual[rel].suffix == '.md':
            for link in re.findall(r'\]\(([^)]+)\)', text):
                if '://' in link or link.startswith('#'):
                    continue
                local = link.split('#', 1)[0]
                if local and not (actual[rel].parent / local).is_file():
                    raise ValueError(f'Broken relative document link in {rel}: {link}')
    licensing = json.loads((ROOT / 'licensing-map.json').read_text())
    copyright_notice = 'Copyright 2026 Yizhan Wu'
    if licensing['package_license'] != 'MPL-2.0' or licensing['copyright_notice'] != copyright_notice:
        raise ValueError('License/copyright decision inconsistent')
    if set(licensing['files']) != set(actual) | {MANIFEST}:
        raise ValueError('File-level licensing map does not cover the source payload')
    mixed = {'include/dragy_ref/ble_session.hpp', 'examples/esp32/main/main.cpp'}
    code_names = {'CMakeLists.txt', '.gitignore', 'Kconfig.projbuild', 'sdkconfig.defaults', 'idf_component.yml'}
    for rel, entry in licensing['files'].items():
        if entry['role'] == 'license-text':
            if rel not in licensing['license_texts']:
                raise ValueError(f'Unrecognized license text: {rel}')
            continue
        expression = 'MPL-2.0 AND Apache-2.0' if rel in mixed else 'MPL-2.0'
        if entry['license_expression'] != expression or entry['copyright_notice'] != copyright_notice:
            raise ValueError(f'Incorrect per-file licensing: {rel}')
        p = ROOT / rel
        text = p.read_text()
        if p.suffix in {'.cpp', '.hpp', '.py', '.md'} or p.name in code_names:
            if f'SPDX-License-Identifier: {expression}' not in text:
                raise ValueError(f'Missing SPDX notice: {rel}')
            if copyright_notice not in text:
                raise ValueError(f'Missing copyright notice: {rel}')
    for rel, entry in licensing['license_texts'].items():
        if hashlib.sha256((ROOT / rel).read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError(f'Canonical license text changed: {rel}')
    if (ROOT / 'LICENSE').read_bytes() != (ROOT / 'LICENSES/MPL-2.0.txt').read_bytes():
        raise ValueError('Root MPL license and canonical copy differ')
    return expected

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--forbid-token', action='append', default=[])
    args = parser.parse_args()
    try:
        files = audit(args.forbid_token)
        print(f'PASS: {len(files)} source files; hashes, scope, links, SPDX/copyright map and canonical licenses checked')
    except (ValueError, OSError, KeyError) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        return 1
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
