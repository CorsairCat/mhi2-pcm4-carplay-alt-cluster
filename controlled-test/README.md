# PCM4 MU1476 controlled test kit

Start with [ONBOARDING_TEST_GUIDE.md](ONBOARDING_TEST_GUIDE.md) for the complete
first-visit workflow, pass/fail gates, integration tests and recovery.
[TESTING.md](TESTING.md) details the standalone kit stages.

**Ready now: read-only vehicle evidence collection. Rendering is not deployment
ready:** a cross-compiled QNX target binary is now present locally under `bin/`,
but has not executed under QNX or on the vehicle. The development profile selects
NvSS output **0**, displayable **60**, and context **900 containing [60]**.
Output0 follows the offline Porsche448x448 boot-profile trace; active topology,
priority, geometry and integrated HMI ownership still need validation. This is not an SD firmware
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
The current build uses the isolated QNX 6.5/GCC 8.5 container on `ssh home`, under
`~/dev/pcm4-qnx-toolchain`. From the project root on the Mac,
`bash tools/build_pcm4_home.sh` rebuilds and retrieves the driver and reports.
The target is ARM32 little-endian EABI5, base PCS softfp, dynamically linked to
`libc.so.3`. Static checks against the supplied MU1476 libc pass; runtime and
symbol-version compatibility have not been proved. The binary is ignored by
git: another checkout must rebuild or receive this locally built folder.

Alternatively, with a compatible official SDK, source its environment, select
its actual qcc variant, then run `sh build.sh`. The default qcc variant remains
untested. Ordinary Homebrew GCC/Clang does not replace the QNX sysroot/runtime.

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
