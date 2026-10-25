#!/usr/bin/env python3
"""Build an ignored private integration candidate; never install or contact a car."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile
from prepare_pcm4_loader import patch
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();out=args.output.resolve()
    private=(ROOT/'private').resolve()
    if private not in out.parents:raise SystemExit('Vendor-derived bundle must stay under ignored private/')
    if out.exists():raise SystemExit('Refusing existing output; choose a new build directory')
    addon=ROOT/'private/analysis/mu1476-hmi-bridge/pcm4-cluster-addon.jar'
    with zipfile.ZipFile(addon) as z:
        required=['de/audi/tghu/fwhmi/porsche/DisplayManagerMIB2PorscheHigh.class','local/pcm4/LoadProbe.class']
        for name in required:
            if name not in z.namelist():raise SystemExit('Updated addon missing '+name)
        for name in z.namelist():
            if name.endswith('.class') and int.from_bytes(z.read(name)[6:8],'big')>48:
                raise SystemExit('Addon contains class newer than Java1.4: '+name)
    original=ROOT/'private/analysis/mu1476-aio-app50/files/eso'
    dio=original/'bin/apps/dio_manager';lsd=original/'hmi/lsd/lsd.sh'
    changed=patch(dio.read_bytes())
    startup=lsd.read_text();needle='DEV_ACTIVATED=$BASE_DIR/lsd/development_activated\n'
    assert startup.count(needle)==1
    properties=json.loads((ROOT/'profiles/pcm4-mu1476-display.json').read_text())['ownership']['java_system_properties']
    options=' '.join('-D'+k+'='+v for k,v in properties.items())
    injection='''
# PCM4 projection: disabled unless explicitly enabled through control.sh.
if [ -f /mnt/app/root/pcm4-projection/enabled ]; then
    VMOPTIONS="$VMOPTIONS @PCM4_PROPERTIES@"
fi
'''.replace('@PCM4_PROPERTIES@',options)
    assert 'VMOPTIONS="$VMOPTIONS '+options+'"' in injection
    startup=startup.replace(needle,needle+injection)
    out.mkdir(parents=True)
    (out/'dio_manager').write_bytes(changed);(out/'lsd.sh').write_text(startup)
    rows=[('dio_manager','/mnt/app/eso/bin/apps/dio_manager','755',digest(dio)),
          ('lsd.sh','/mnt/app/eso/hmi/lsd/lsd.sh','755',digest(lsd))]
    target='/mnt/app/root/pcm4-projection/'
    sources={
      'libpcm4-carplay.so':ROOT/'private/remote-qnx/libpcm4-carplay.so',
      'pcm4-projection-runtime':ROOT/'private/remote-qnx/pcm4-projection-runtime',
      'pcm4-verify':ROOT/'private/remote-qnx/pcm4-verify',
      'control.sh':ROOT/'integration/control.sh',
      'pcm4-cluster-addon.jar':addon,
    }
    for name,p in sources.items():
        shutil.copy2(p,out/name)
        destination='/mnt/app/eso/hmi/lsd/jars/'+name if name.endswith('.jar') else target+name
        rows.append((name,destination,'644' if name.endswith('.jar') else '755','NEW'))
    (out/'manifest').write_text(''.join('|'.join((name,destination,mode,digest(out/name),old))+'\n' for name,destination,mode,old in rows))
    shutil.copy2(ROOT/'integration/install.sh',out/'install.sh')
    shutil.copy2(ROOT/'integration/README.md',out/'README.md')
    shutil.copy2(ROOT/'controlled-test/ONBOARDING_TEST_GUIDE.md',out/'ONBOARDING_TEST_GUIDE.md')
    shutil.copy2(ROOT/'profiles/pcm4-mu1476-display.json',out/'profile.json')
    (out/'SHA256SUMS').write_text(''.join(digest(p)+'  '+p.name+'\n' for p in sorted(out.iterdir()) if p.is_file()))
    archive=out.with_suffix('.zip')
    if archive.exists():raise SystemExit('Archive already exists')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(out.iterdir()):z.write(p,out.name+'/'+p.name)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        for p in out.iterdir():assert z.read(out.name+'/'+p.name)==p.read_bytes()
    print('PRIVATE candidate:',out);print('Verified archive:',archive)
if __name__=='__main__':main()
