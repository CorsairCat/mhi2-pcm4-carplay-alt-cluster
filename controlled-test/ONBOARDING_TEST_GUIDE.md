# PCM4 MU1476 on-car onboarding and test guide

Vehicle: 2017 Porsche Macan, PCM4. This candidate targets the inspected MU1476
reference, not the original MU0809 installation. Firmware updates are a separate
operation. The integration is offline tested; no stage below has passed on this
car yet. Use a parked car, stable power and existing QNX shell/recovery access.

The first visit should establish the baseline and collect evidence. Continue to
real graphics and integration only when each preceding gate has passed. This
procedure does not require driving or triggering warnings artificially.

## What you are testing

```text
iPhone <-- stock wired CarPlay authentication/session --> PCM
       <-- /info advertises auxiliary display UUID, type111, geometry
       --> SETUP asks for stream111 and supplies connection ID
       <-- SETUP response supplies auxiliary listening port
       <-- showUI asks for navigation on that display UUID
       --> encrypted auxiliary H.264
PCM hook --> stock AES derivation/CTR --> plaintext loopback stream
renderer --> NvSS output0 / displayable60 --> cluster output, if route confirmed
         --> readiness marker --> Java context900=[60]
```

`showUI` selects content on the phone; it does not select a PCM HMI context.
The auxiliary descriptor also supplies the initial navigation URL. Our START/
SETUP hooks request showUI once both session start and auxiliary setup exist.
Acceptance and actual phone behavior must be observed; an initialized hook alone
does not prove negotiation.

Separately, Java substitutes context900=[60] for terminal1 navigation requests
72–77 while decoded-frame readiness is fresh. Stock73=[22] is intersection view;
74=[22,33] combines intersection view and stock map. The projection context does
not include stock displayables22/33. Other-page requests pass through, and loss
of readiness restores the latest stock request. This policy still needs target
checks for page and warning priority.

## Bring and prepare on the Mac

Use both private archives from this working folder:

- `pcm4-mu1476-controlled-test.zip`: Stage0–2, original tests/sample/collector.
- The latest `pcm4-mu1476-integration-*.zip`: stages3–4, proprietary-derived
  loader/addon, hook, renderer, installer and rollback. Keep it private.

Unzip on the Mac and copy the complete extracted folders onto writable removable
media. This is not a firmware-update card. Retain an unchanged copy on the Mac.
Verify each extracted folder's SHA256SUMS on the Mac:

```sh
cd /actual/path/to/extracted-folder
shasum -a 256 -c SHA256SUMS
```

Use the actual existing media mount path in the QNX commands below. Replace
`/MEDIA` with that absolute path; never paste it as a literal guessed mount.
The examples assume extracted folders named `controlled` and `integration` and
an existing writable `/MEDIA/pcm4-test-logs` directory. Folder names can differ.
Choose a new evidence/backup directory for every collection/installation.
Record firmware identifier and MU version from PCM, date, iPhone model/iOS,
USB cable/port, selected cluster page, and stock CarPlay/navigation behavior.
Keep library copies, vehicle details and logs private.

## Stage0: native execution without real graphics

From the QNX shell, with the controlled folder selected:

```sh
cd /MEDIA/controlled
sh run-mock-tests.sh > /MEDIA/pcm4-test-logs/stage0.txt 2>&1
echo $?
```

Pass: exit0, all four test executables pass and the final stage PASS appears.
These use synthetic APIs; no real NvSS output is opened. If there is a missing
library/symbol, permission error, assertion or crash, stop and save the log.
Do not treat cross-compilation as a substitute for this QNX execution gate.

## Stage1: stock baseline and read-only evidence

Keep integration uninstalled/disabled. Collect these separately:

| Folder | Cluster page | Phone |
|---|---|---|
| baseline-nav | Stock navigation/map | Disconnected |
| baseline-other | Another normal page | Disconnected |
| baseline-carplay | Navigation/map | Wired CarPlay connected |

Example, repeated using each new folder name:

```sh
cd /MEDIA/controlled
sh collect.sh /MEDIA/pcm4-test-logs/baseline-nav
echo $?
```

The collector queries dmdt gs/gd/gc and copies candidate libraries without
loading them. Check collection.txt for missing files/query failures even if the
collector completes. On the Mac, run for each returned collection:

```sh
python3 verify_collection.py /actual/path/to/baseline-nav
```

Gate: compare library identities with the reference profile, confirm the active
448x448 route and output attachment, examine declared contexts/displayables,
and confirm context900 is not already allocated. A matching hash alone does not
prove physical routing. Stop for review if paths, topology or hashes differ;
do not bypass installer guards or try random output IDs.

## Stage2: bounded pattern on the reviewed output

Use TESTING.md's full driver argument order after Stage1 review. Selected
candidate values are output0, displayable60, priority100, 448x448 full-canvas
destination. The library's actual absolute path must come from the collection.
There is intentionally no unconditional live-render command here: these values
remain conditional on the observed route and identities.

The two-second driver requests show/hide and cleans up its own renderer; it does
not acquire context900 or implement HMI handover. A pattern hidden behind stock
planes is therefore inconclusive. Record selected page, visible pattern, clipping,
orientation and stock behavior, together with complete stdout/stderr and status.
Exit0 proves reported API success, not visible pixels. Stop with Ctrl-C on an
unexpected result; cleanup cannot reliably interrupt a blocked vendor call.

## Stage3: install disabled and test loading

Proceed only with Stage0 passed, Stage1 reviewed, graphics-test findings recorded,
and recovery access retained. The application filesystem must already be writable
through your existing procedure; this guide does not change mounts or privileges.

```sh
cd /MEDIA/integration
sh install.sh --check > /MEDIA/pcm4-test-logs/preflight.txt 2>&1
echo $?
```

Pass: exit0 and payload/original identities accepted. The inspected factory
loader/startup hashes must match, added targets must be absent, and no old
activation flag may exist. If not, stop; do not edit the manifest to make it pass.

Install using a NEW absolute backup directory with an existing parent:

```sh
sh install.sh --install /mnt/app/root/pcm4-backup-NEW > /MEDIA/pcm4-test-logs/install.txt 2>&1
echo $?
```

Pass: exit0, complete backup retained, candidate installed disabled. Copy the
backup to the Mac before enabling. The installer does not restart any process.
Run the loader probe:

```sh
sh /mnt/app/root/pcm4-projection/control.sh probe > /MEDIA/pcm4-test-logs/j9-probe.txt 2>&1
echo $?
```

Pass: exit0 and the patched-class selection/linking PASS message. This checks
the candidate probe path; it does not initialize OEM HMI or prove full startup.
A J9 classpath/JXE/linking failure is a stop-and-review result, not a reason to
launch a converted full stock JAR.

Perform your normal manual MMI restart while the candidate remains disabled.
Verify stock navigation and wired main-screen CarPlay. Preserve process/service
logs using your existing collection method. The disabled hook should report
that projection is disabled if its stderr is available. Missing logs alone do
not prove successful dependency loading; crashes or abnormal stock behavior stop
the test. Do not enable until disabled loading and recovery are established.

## Stage4: enable, negotiate and check handover

Disconnect the iPhone first. Enable for the next process startup:

```sh
sh /mnt/app/root/pcm4-projection/control.sh enable
```

Manually restart MMI, reconnect the shell, and start the renderer in a dedicated
foreground shell, keeping that shell open:

```sh
sh /mnt/app/root/pcm4-projection/control.sh renderer > /MEDIA/pcm4-test-logs/renderer.txt 2>&1
```

A full restart may stop a previously started renderer, so confirm/restart it
before connecting the phone. It is not an unattended startup service. A quiet,
running foreground command is expected; silence does not prove decode success.
Capture the hook initialization/error message through the existing service-log
route. It must pass firmware/entry guards; an initialization message alone does
not prove that iPhone negotiation succeeded.

Use a second shell to collect enabled state before the phone connects:

```sh
cd /MEDIA/controlled
sh collect.sh /MEDIA/pcm4-test-logs/enabled-before-phone
```

Confirm context900 declares membership[60] in dmdt gc. The declaration appears
only when the enabled JVM options and patched HMI initialization actually load.
A Java collision check exists, but actual native registration still needs this
evidence. Stop if declaration/loading is missing or conflicting.

Connect the wired iPhone, open a supported navigation app and set a route while
parked. Select the normal cluster navigation page. Check readiness:

```sh
cat /tmp/pcm4-cluster-ready
```

Expected during accepted video decode:

```text
PCM4READY1
<positive session generation>
<positive decoded-frame count>
```

The numbers should be positive and the frame count should advance when new
frames arrive. File absence before negotiation is normal. A marker proves
accepted decoding by this runtime, not visible scanout. No marker means trace
hook/session/SETUP, transport and decoding before assuming an HMI fault. The
current build does not provide a detailed protocol trace or a negotiation-only
operator mode; do not infer which handshake failed from an absent marker alone.

Collect a NEW enabled-with-phone evidence folder. Record the actual visible
content and whether the main PCM CarPlay remains usable. Do not capture keys or
add logging of decrypted personal video to diagnose negotiation.

| Test | Expected observation | Record |
|---|---|---|
| Navigation page, active stream | Auxiliary navigation visible; context900 selected | Photo/notes, dmdt, marker |
| Another cluster page | Requested stock page wins | Page name, dmdt, visible result |
| Return to navigation | Projection resumes while readiness is fresh | Visible result, dmdt |
| Unplug phone | Marker removed and latest stock map/page returns | Recovery time, dmdt |
| Reconnect phone | New session/setup can produce projection again | Marker, hook errors, visible result |
| Stop renderer with Ctrl-C | Readiness withdrawn; Java restores stock independently | Exit/log, recovery time |
| Restart renderer, reconnect phone | Fresh setup/connection works | Marker and visible result |
| Disable while projecting | Java releases projection | Visible result and context |

After unplug/stop, allow the3s Java marker TTL plus200ms polling, in addition to
any native call delays. A genuine frame stall has a1.5s native timeout; paused
navigation can be static, so do not assume every static picture is a stall.
Observe normal warning/page precedence if naturally present; do not induce
vehicle faults. Native turn arrows and speed limits are not supplied here.

## Disable, restore and finish the visit

```sh
sh /mnt/app/root/pcm4-projection/control.sh disable
```

This requests Java release immediately and removes next-start activation. It
does not hot-remove loaded ARM hooks. Stop the renderer with Ctrl-C and manually
restart MMI. Confirm stock navigation, normal pages and main CarPlay recover.

To remove the candidate, use the original extracted integration folder and the
backup made by that exact installation:

```sh
cd /MEDIA/integration
sh install.sh --restore /mnt/app/root/pcm4-backup-NEW > /MEDIA/pcm4-test-logs/restore.txt 2>&1
echo $?
```

Restore verifies current installed hashes and backup originals, restores factory
files and removes added payloads. Later edits cause refusal; retain logs and
backup for review instead of forcing overwrite. Restart manually and recheck
stock behavior. No automatic rollback can recover a lost shell; keep the
existing independent recovery method available before modifying startup files.

## Return these results

For every attempted stage, record PASS/FAIL/NOT RUN, exact command and exit
status, time, selected page, phone connection state, expected versus actual
behavior and recovery outcome. Include archive name/SHA256, PCM firmware/MU,
iPhone/iOS, Stage0 log, all collection folders, preflight/install/probe/renderer/
restore logs, available hook/service logs, and photos of useful screen results.
Record missing logs as missing. Keep the installation backup separately.
Do not commit libraries, proprietary bundles or vehicle-specific evidence.
