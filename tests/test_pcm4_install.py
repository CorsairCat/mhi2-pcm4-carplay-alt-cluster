"""Exercise actual installer against an isolated filesystem, never /mnt/app."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
source=Path(sys.argv[1]).resolve()
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory(prefix='pcm4-install-') as temporary:
    work=Path(temporary);bundle=work/'bundle';shutil.copytree(source,bundle)
    # Execute only our added startup block, never the OEM startup script.
    startup=(bundle/'lsd.sh').read_text()
    begin=startup.index('# PCM4 projection:')
    end=startup.index('\nfi\n',begin)+4
    enabled=work/'enabled'
    block=startup[begin:end].replace('/mnt/app/root/pcm4-projection/enabled',str(enabled))
    properties=json.loads((bundle/'profile.json').read_text())['ownership']['java_system_properties']
    expected='original '+ ' '.join('-D'+k+'='+v for k,v in properties.items())
    def startup_options():
        return subprocess.check_output(['sh','-c','VMOPTIONS=original\n'+block+'\nprintf "%s" "$VMOPTIONS"'],text=True)
    assert startup_options()=='original'
    enabled.touch();assert startup_options()==expected
    enabled.unlink()
    subprocess.run(['clang','-std=c99','-Wall','-Wextra','-Werror','-Inative',
                    'native/pcm4_identity.c','native/pcm4_verify.c','-o',str(bundle/'pcm4-verify')],cwd=ROOT,check=True)
    text=(bundle/'manifest').read_text();rows=[]
    for line in text.splitlines():
        name,target,mode,expected,original=line.split('|')
        if name=='pcm4-verify':expected=digest(bundle/name)
        rows.append((name,target,mode,expected,original))
    (bundle/'manifest').write_text(''.join('|'.join(r)+'\n' for r in rows))
    filesystem=work/'unit';(filesystem/'tmp').mkdir(parents=True)
    originals={}
    vendor=ROOT/'private/analysis/mu1476-aio-app50/files/eso'
    for name,target,mode,expected,old in rows:
        destination=filesystem/target.lstrip('/')
        destination.parent.mkdir(parents=True,exist_ok=True)
        if old!='NEW':
            p=vendor/('bin/apps/dio_manager' if name=='dio_manager' else 'hmi/lsd/lsd.sh')
            shutil.copy2(p,destination);assert digest(destination)==old
            originals[destination]=destination.read_bytes()
    env=dict(os.environ,PCM4_INSTALL_ROOT=str(filesystem),PCM4_INSTALL_TEST='1')
    def run(*args,okay=True,environment=env):
        p=subprocess.run(['sh',str(bundle/'install.sh'),*map(str,args)],env=environment,capture_output=True,text=True)
        if okay and p.returncode:raise AssertionError(p.stdout+p.stderr)
        if not okay:assert p.returncode
        return p
    run('--check')
    assert all(p.read_bytes()==data for p,data in originals.items())
    activation=filesystem/'mnt/app/root/pcm4-projection/enabled';activation.touch()
    run('--install',work/'active-backup',okay=False)
    assert not (work/'active-backup').exists();activation.unlink()
    # Original identity mismatch stops before any publication.
    first=next(iter(originals));first.write_bytes(b'unexpected original')
    run('--install',work/'bad-original-backup',okay=False)
    assert not (work/'bad-original-backup').exists();first.write_bytes(originals[first])
    # A controlled cp failure after earlier publications must roll everything back.
    commands=work/'commands';commands.mkdir();cp=commands/'cp'
    cp.write_text('#!/bin/sh\ncase "$1" in */control.sh) exit 7 ;; esac\nexec /bin/cp "$@"\n');cp.chmod(0o755)
    fail_env=dict(env,PATH=str(commands)+os.pathsep+env['PATH'])
    run('--install',work/'failed-backup',okay=False,environment=fail_env)
    assert all(p.read_bytes()==data for p,data in originals.items())
    assert all(not (filesystem/target.lstrip('/')).exists() for _,target,_,_,old in rows if old=='NEW')
    assert not list(filesystem.rglob('*.pcm4-new-*'))
    backup=work/'backup';run('--install',backup)
    for _,target,_,expected,_ in rows:assert digest(filesystem/target.lstrip('/'))==expected
    altered=filesystem/'mnt/app/root/pcm4-projection/pcm4-projection-runtime'
    actual=altered.read_bytes();altered.write_bytes(b'later edit')
    run('--restore',backup,okay=False)
    altered.write_bytes(actual)
    activation=filesystem/'mnt/app/root/pcm4-projection/enabled';activation.touch()
    run('--restore',backup)
    assert not activation.exists()
    assert all(p.read_bytes()==data for p,data in originals.items())
    assert all(not (filesystem/target.lstrip('/')).exists() for _,target,_,_,old in rows if old=='NEW')
print('PASS: installer preflight, identity rejection, partial-publication rollback, later-edit refusal, full restore')
