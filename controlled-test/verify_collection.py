#!/usr/bin/env python3
"""Host-only SHA256/ELF identity check of collected libraries. No firmware execution."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

EXPECTED = {
    'libnvss_video.so': 'nvss_sha256', 'libKD.so': 'kd_sha256',
    'libnvmedia.so': 'nvmedia_sha256', 'libnvtvmr.so': 'tvmr_sha256',
    'libnvdc.so': 'nvdc_sha256',
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('collection', type=Path)
    args = parser.parse_args()
    profile = json.loads(Path(__file__).with_name('reference-profile.json').read_text())
    report = {'render_authorized': False, 'libraries': [],
              'note': 'Identity checks do not resolve output, layer, ownership or SDK compatibility.'}
    okay = True
    for name, key in EXPECTED.items():
        files = sorted((args.collection / 'libraries').glob('*__' + name))
        if not files:
            report['libraries'].append({'library': name, 'missing': True})
            okay = False
        for path in files:
            data = path.read_bytes()
            digest = hashlib.sha256(data).hexdigest()
            arm32le = (len(data) >= 52 and data[:6] == b'\x7fELF\x01\x01'
                       and struct.unpack_from('<H', data, 18)[0] == 40)
            matches = digest == profile[key] and arm32le
            okay &= matches
            report['libraries'].append({'path': str(path), 'sha256': digest,
                                        'arm32le': arm32le, 'reference_matches': matches})
    report['all_reference_libraries_match'] = bool(okay)
    print(json.dumps(report, indent=2))
    return 0 if okay else 1

if __name__ == '__main__':
    raise SystemExit(main())
