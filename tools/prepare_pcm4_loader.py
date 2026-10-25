#!/usr/bin/env python3
"""Private offline loader delta. Exact input guard, no vendor instructions edited."""
import argparse
import hashlib
from pathlib import Path
import struct
from compare_audi_interfaces import file_offset
EXPECTED='0d81a2180cc684a2ae4e448523815f094e3550f65f3780827f7b84be65e31706'
DEPENDENCY=b'/mnt/app/root/pcm4-projection/libpcm4-carplay.so'

def patch(source):
    if hashlib.sha256(source).hexdigest()!=EXPECTED:
        raise ValueError('Unsupported original dio_manager hash')
    b=bytearray(source)
    start=struct.unpack_from('<I',b,28)[0]
    size,count=struct.unpack_from('<HH',b,42)
    entries=[]
    for i in range(count):
        kind,offset,_,_,length=struct.unpack_from('<5I',b,start+i*size)
        if kind==2:
            for at in range(offset,offset+length,8):
                tag,value=struct.unpack_from('<II',b,at)
                entries.append((at,tag,value))
                if not tag:break
    strings=file_offset(b,next(v for _,t,v in entries if t==5))
    paths=[e for e in entries if e[1]==15]
    if len(paths)!=1:raise ValueError('Expected one RPATH')
    slot,_,index=paths[0];pos=strings+index
    end=b.index(0,pos);old=bytes(b[pos:end])
    if not old.startswith(b'/home/jenkins/') or len(DEPENDENCY)>len(old):
        raise ValueError('Expected unused Jenkins build RPATH space')
    # Replace the build-machine-only RPATH with an absolute dependency. Retain
    # all original NEEDED entries and their order; no search-path guess needed
    # for our library. Stock runtime still supplies its normal LD_LIBRARY_PATH.
    b[pos:end]=DEPENDENCY+bytes(len(old)-len(DEPENDENCY))
    struct.pack_into('<II',b,slot,1,index)
    assert len(b)==len(source)
    return bytes(b)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args()
    if a.output.exists():raise SystemExit('Refusing existing output')
    result=patch(a.source.read_bytes());a.output.write_bytes(result)
    print(hashlib.sha256(result).hexdigest(),a.output)
if __name__=='__main__':main()
