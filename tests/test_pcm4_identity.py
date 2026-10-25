import ctypes
import hashlib
from pathlib import Path
import sys
import tempfile

library=ctypes.CDLL(sys.argv[1])
check=library.pcm4_identity_file
check.argtypes=[ctypes.c_char_p,ctypes.c_char_p]
check.restype=ctypes.c_int
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory)/'input'
    for size in [0,1,3,55,56,63,64,65,127,128,4095,4096,4097,65536]:
        data=bytes((i*17+5)%256 for i in range(size))
        p.write_bytes(data)
        assert check(bytes(p),hashlib.sha256(data).hexdigest().encode())==0
        assert check(bytes(p),b'0'*64)==-1
print('PASS: streaming file SHA256 against hashlib across padding/block boundaries')
