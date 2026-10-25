#!/usr/bin/env python3
"""Package an explicit allowlist of original files; exclude collected/vendor data."""
import hashlib
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
KIT = ROOT / 'controlled-test'
FILES = '''README.md TESTING.md ONBOARDING_TEST_GUIDE.md collect.sh run-mock-tests.sh verify_collection.py
make_sample.py build.sh reference-profile.json
src/pcm4_mu1476_display.c src/pcm4_mu1476_display.h
src/pcm4_nv_binding.c src/pcm4_nv_binding.h src/render_test.c
bin/pcm4-render-test bin/qnx-mocks/display-mock-test
bin/qnx-mocks/projection-mock-test bin/qnx-mocks/binding-mock-test bin/qnx-mocks/hook-mock-test
bin/qnx-mocks/mock.so bin/qnx-mocks/missing.so media/pattern.pc4t'''.split()

def main():
    missing = [name for name in FILES if not (KIT / name).is_file()]
    if missing:
        raise SystemExit('Missing kit components: ' + ', '.join(missing))
    content = {name: (KIT / name).read_bytes() for name in FILES}
    manifest = ''.join(hashlib.sha256(data).hexdigest() + '  ' + name + '\n'
                       for name, data in content.items())
    output = ROOT / 'private/releases/pcm4-mu1476-controlled-test.zip'
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name in FILES:
            archive.write(KIT / name, 'pcm4-mu1476-controlled-test/' + name)
        archive.writestr('pcm4-mu1476-controlled-test/SHA256SUMS', manifest)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise SystemExit('Archive CRC verification failed')
        for name, data in content.items():
            if archive.read('pcm4-mu1476-controlled-test/' + name) != data:
                raise SystemExit('Archive content mismatch: ' + name)
    print(output)
    print(f'{len(FILES)} components + SHA256SUMS, {output.stat().st_size} bytes; verified')

if __name__ == '__main__':
    main()
