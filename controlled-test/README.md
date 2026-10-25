# PCM4 MU1476 controlled test kit

**Ready now: read-only vehicle evidence collection. Rendering is not deployment
ready:** there is no QNX target binary yet, and the cluster output device, free
layer, priority and HMI ownership are unresolved. This is not an SD firmware
update or an automatic installer. Existing shell access is required. No vendor
firmware, Audi binary or startup modification is included.

## First vehicle visit

1. Update to the owner's selected MU1476 baseline before renderer testing.
2. Copy this entire folder onto writable removable media. Park the vehicle.
3. From the existing QNX shell, run `sh collect.sh /absolute/path/to/new-results`.
   Choose a new directory on the removable media, outside system filesystems.
4. Repeat into separate folders with the cluster navigation page selected and
   another cluster page selected. Note the selected page, firmware identifier,
   whether CarPlay is connected, and observed stock map behavior for each run.
5. Back on the Mac, run `python3 verify_collection.py /path/to/new-results`.
   Return the collection and notes privately. Copies of libraries stay out of git.

The collector queries `dmdt gs/gd/gc` and copies candidate libraries. It never
loads those libraries, changes contexts or opens a video output. Missing files
and query failures appear in `collection.txt`; a completion message alone is
not proof that every item succeeded. System queries may need existing privileges.

## Controlled video test, after the collection is reviewed

Included `src/` is a snapshot of the original renderer core and dynamic binder,
plus a bounded test driver. No iPhone connection or HMI takeover is attempted.
Build on a host with a licensed QNX SDK compatible with the unit's ARM32
little-endian runtime: source its environment, select its actual qcc variant,
then run `sh build.sh`. The default variant is a candidate, not verified here.
Ordinary Homebrew GCC/Clang is not a replacement for the QNX sysroot and runtime.

The driver takes explicitly reviewed output, layer, priority and dimensions.
It does not choose an HMI context, make a library identity decision, or guarantee
visibility over the stock map. Review host-side library hashes, active topology
and layer ownership **before invoking it**. Its explicit confirmation argument
is an operator assertion, not automatic evidence validation.

`python3 make_sample.py` generates a short original H.264 test pattern with the
existing host ffmpeg and wraps complete access units in `media/pattern.pc4t`.
`python3 make_sample.py --check media/pattern.pc4t` checks the complete container.
The target driver validates the entire file before loading vendor code. Driver
usage lists all arguments when run without parameters. There is deliberately no
live rendering command populated with guessed device/layer numbers.

After rendering, the driver requests hide, closes the renderer and unloads its
library. SIGINT/SIGTERM request cleanup between vendor calls. A blocked driver
call cannot be interrupted reliably by this program. Stock context restoration
is not implemented in this kit, because it does not acquire a context. Check
stock map recovery and main-display CarPlay after each test. Automatic HMI
handover belongs to the separate integration track.

`bin/`, `media/` and `results/` contents are ignored. Keep this folder together
when transferring it. Canonical renderer source lives in `../native/`; update
this snapshot whenever those files change.
