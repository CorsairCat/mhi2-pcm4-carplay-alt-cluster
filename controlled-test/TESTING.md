# Staged test procedure

Current delivery: compiled ARM/QNX renderer, three compiled mock tests, two
synthetic shared libraries, original two-second sample and collection scripts.
No phone receiver hook or Java HMI patch is installed by this package. Use
existing shell access. Unzip on the Mac and copy the extracted folder to the
removable media; this is not a firmware-update SD card.

## Stage 0 — QNX execution with synthetic APIs

From a QNX shell, change into the copied folder and run:

```sh
sh run-mock-tests.sh > /absolute/removable-media/stage0.txt 2>&1
echo $?
```

Expected: status `0`, three test PASS messages, then the stage PASS message.
This checks native C execution, mocked renderer lifecycle, auxiliary conversion,
handover policy and loading our synthetic libraries. It never loads the vehicle
NvSS library and cannot prove real H.264 decode or physical display behavior.
If execution fails (permission denied, missing interpreter/symbol/library,
assertion or crash), save the full log and stop before a real output test.
These tests have been cross-compiled; this stage has not been run on QNX yet.

## Stage 1 — collect the active display configuration

Park the car. Run separate collections, each with a NEW absolute output path
on writable removable media:

```sh
sh collect.sh /absolute/removable-media/evidence-nav
```

Repeat with different directory names for: stock navigation page selected,
another cluster page selected, and stock navigation page with wired CarPlay
connected. Write down which condition each folder represents, the firmware
identifier, and stock behavior. The collector writes query failures and missing
candidate libraries in `collection.txt`; check them even when it finishes.

On the Mac, run:

```sh
python3 verify_collection.py /path/to/evidence-nav > /path/to/identity-nav.json
```

Return the collection directories and notes privately for review. The SHA-256
check validates reference identity only; it does not select a renderer output,
free layer or HMI context. Do not put vehicle library copies in git.

## Stage 2 — controlled real video output: pending configuration

Do this only after Stage 0 passes and Stage 1 establishes actual NvSS output
device, free layer, priority, geometry and compatible runtime-library paths.
There is no ready-to-copy execution command with guessed values. The driver's
full argument order is:

```text
bin/pcm4-render-test --mapping-and-identity-reviewed ABSOLUTE_NVSS_LIBRARY SAMPLE_PC4T OUTPUT_DEVICE LAYER PRIORITY OUTPUT_W OUTPUT_H DEST_X DEST_Y DEST_W DEST_H
```

Use `media/pattern.pc4t` as the sample after reviewing those parameters. The
confirmation flag asserts that review; it performs no library hash verification.
The driver neither acquires nor restores an HMI context. It requests visibility,
plays the two-second source, hides, closes and unloads. Driver blocking can
extend elapsed time; SIGINT/SIGTERM cleanup occurs between vendor calls.

Capture stderr/stdout and record visible output, clipping, orientation, and
whether the stock map resumes. Stop with Ctrl-C if behavior is unexpected; do
not repeat with random output/layer values. A zero exit code means the API calls
reported success, not that the cluster showed the pattern. Check stock cluster
page selection, warning display and main-screen CarPlay after the test.

## Stages 3 and 4 — auxiliary stream and automatic HMI handover

These stages are **not included as runnable vehicle modifications**. They need
the Porsche receiver hook/negotiation/decryption adapter and Java HMI bridge.
The offline native conversion/handover core is included in Stage 0 mocks only.

Planned sequence: first negotiate/capture the extra iPhone stream while stock
CarPlay keeps working; then decode it through the verified output; finally
connect the HMI bridge. Test navigation-page entry/exit, non-navigation pages,
disconnect/reconnect, missing frames and renderer failure, requiring stock-map
restoration in each release case. The HMI-side watchdog must work independently
of the renderer. Native arrows and speed limits remain a later guidance path.
