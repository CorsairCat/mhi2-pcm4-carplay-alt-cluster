"""Actual localhost runtime, synthetic NvSS: readiness/timeout/reconnect/crash cleanup."""
from pathlib import Path
import signal
import socket
import subprocess
import sys
import time

marker = Path('/tmp/pcm4-cluster-ready')
if marker.exists():
    raise SystemExit('Refusing to replace an existing readiness marker')
config = bytes([1,66,0,30,255,225,0,2,0x67,0,1,0,2,0x68,0])
idr = bytes([0,0,0,2,0x65,0xaa])
def message(opcode, body):
    return len(body).to_bytes(4,'little') + bytes([opcode]) + bytes(123) + body

def await_marker(present):
    until=time.monotonic()+4
    while time.monotonic()<until:
        if marker.exists()==present:
            return
        time.sleep(.02)
    raise AssertionError(f'marker present should be {present}')

p=subprocess.Popen([sys.argv[1], '--enable', str(Path(sys.argv[2]).resolve())])
try:
    for attempt in range(100):
        try:
            client=socket.create_connection(('127.0.0.1',19820),timeout=.2)
            break
        except ConnectionRefusedError:
            if p.poll() is not None:
                raise AssertionError('runtime exited before accept')
            time.sleep(.02)
    else:
        raise AssertionError('listener unavailable')
    with client:
        client.sendall(message(1,config)+message(0,idr))
        await_marker(True)
        assert marker.read_text()=='PCM4READY1\n1\n1\n'
    await_marker(False)
    with socket.create_connection(('127.0.0.1',19820)) as client:
        client.sendall(message(1,config)+message(0,idr))
        await_marker(True)
        assert marker.read_text()=='PCM4READY1\n2\n1\n'
        await_marker(False)  # No fresh frames: native 1.5-second timeout.
    with socket.create_connection(('127.0.0.1',19820)) as client:
        client.sendall(message(1,config)+message(0,idr))
        await_marker(True)
        p.send_signal(signal.SIGTERM)
        assert p.wait(timeout=5)==0
        await_marker(False)
    print('PASS: localhost Screen receive/decode, real atomic marker, reconnect, stale-frame timeout, SIGTERM cleanup')
finally:
    if p.poll() is None:
        p.terminate()
        p.wait(timeout=5)
