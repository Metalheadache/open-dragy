#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright 2026 Yizhan Wu
"""Refresh snapshot hashes after reviewing changes to the listed source files."""
import hashlib
import json
from audit_package import ROOT, MANIFEST, source_files, audit

mapping = json.loads((ROOT / 'licensing-map.json').read_text())
actual = {p.relative_to(ROOT).as_posix(): p for p in source_files() if p.name != MANIFEST}
if set(actual) | {MANIFEST} != set(mapping['files']):
    raise SystemExit('Review added/removed files and update licensing-map.json before refreshing the manifest')
manifest = json.loads((ROOT / MANIFEST).read_text())
manifest['files'] = {rel: hashlib.sha256(p.read_bytes()).hexdigest() for rel, p in sorted(actual.items())}
(ROOT / MANIFEST).write_text(json.dumps(manifest, indent=2) + '\n')
audit()
print(f'Updated and audited {len(actual)} source hashes')
