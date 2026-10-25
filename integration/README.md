# PCM4 MU1476 integration candidate — stages 3/4

This is a compiled, offline-tested vehicle-test candidate, **not a vehicle-validated
release**. The private generated archive contains a patched proprietary loader
and one patched OEM Java class. Keep the archive private and out of Git.
No firmware-update bundle is required. These are application-level files.
No gauges or cluster ECU firmware are modified.

The private archive includes `ONBOARDING_TEST_GUIDE.md` with the complete staged
vehicle procedure, pass/fail gates and results checklist. Its repository source
is `controlled-test/ONBOARDING_TEST_GUIDE.md`.

Selected development profile: NvSS0, displayable60, context900=[60], Java terminal1,
448x448 H.264 at30fps, priority100, full-canvas destination. The phone's provisional
safe rectangle is348x368 at(60,30), from the Porsche layout. These presentation
choices are starting values, not a claim of validated clipping. Physical display
millimetres are omitted rather than invented; phone acceptance must be checked.

## Contents and ownership

- `dio_manager`: exact-hash original with ONLY its unused Jenkins build RPATH
  converted into an absolute dependency on our hook. Original NEEDED order stays
  intact; vendor instruction bytes are unchanged.
- `libpcm4-carplay.so`: disabled by default. Once enabled at startup, checks the
  AirPlay file identity and five ARM entry guards, prepares trampolines and
  installs security/info/setup/start/teardown hooks. Unsupported prologues or
  denied writable text leave projection disabled. Partial installations restore
  earlier entries. No anonymous fixed-page replacement is attempted. Failure to
  restore executable permissions exits the process with126 rather than returning
  into non-executable stock code. This behavior requires a target smoke test.
- Hook security uses the reviewed stock setter with a retained session identity;
  stock derivation and continuous CTR handle stream111. Keys are wiped on session
  replacement/termination and are neither logged nor sent to the renderer.
- The auxiliary listener is reachable by the wired iPhone, with its ephemeral
  port returned in SETUP. It accepts one connection. The stock main stream stays
  stock-owned. Plaintext Screen messages go only to127.0.0.1:19820.
- `pcm4-projection-runtime`: separate NvSS process, exclusive loopback listener,
  hash-checks the NvSS file before loading, decodes complete access units and
  publishes atomic `PCM4READY1` markers after accepted decoding. No native
  context ownership runs in this mode. Frame timeout1.5s, disconnect and shutdown
  withdraw readiness; a failed stream waits for a new SETUP/connection.
- `pcm4-cluster-addon.jar`: extends factory declarations with900=[60] when
  configured and without an ID collision. The Java worker alone owns handover.
  It preserves raw Porsche/KDK requests and listeners, uses a private listener
  for projection acknowledgements, and restores the latest stock request.
  A stale marker releases ownership within the existing3s Java TTL plus polling.
- `lsd.sh`: exact original script plus a conditional VMOPTIONS block. The normal
  jars directory is discovered before the factory JXE; we retain that mechanism
  rather than replacing all30941 classes. Options are set only while enabled.
- `pcm4-verify`: original streaming SHA256 utility. `manifest` records original
  and new fingerprints, targets and modes. `install.sh` supports check, install
  with complete original snapshots, and guarded rollback. It mounts nothing,
  stops no service and restarts nothing.

## Build/reproduce on the development computer

```sh
python3 tools/generate_pcm4_profile.py
sh tools/test_pcm4_projection.sh
sh tools/test_pcm4_runtime.sh
bash tools/build_pcm4_home.sh
```

The runtime test uses localhost and synthetic vendor functions. The HMI build is
`tools/build_hmi_debian.sh` on the authorized Debian host after source sync.
Retrieve its addon into ignored `private/analysis/mu1476-hmi-bridge/`, then:

```sh
python3 tools/package_pcm4_integration.py --output private/releases/NEW-build-name
```

The package builder checks all addon class versions<=48, verifies the original
loader hash, creates a fresh output only, and verifies archive CRC/content.

## Future parked-unit sequence

Use the existing Stage0/1 kit first: QNX mocks and read-only evidence collection.
Confirm MU1476, installed identities, the448x448/output0 profile and a recovery
shell. The integration installer expects the inspected original dio_manager and
lsd.sh and no pre-existing project targets. A different file stops preflight.

Copy the private candidate folder to removable media. From its QNX shell:

```sh
sh install.sh --check
```

After the Stage0 native execution checks, install into an already writable app
filesystem with a NEW absolute backup directory whose parent exists:

```sh
sh install.sh --install /mnt/app/root/pcm4-backup-NEW
```

This installs disabled. Retain the backup on the computer. Installation does not
claim that the files have loaded. Run the original-code class-loading probe:

```sh
sh /mnt/app/root/pcm4-projection/control.sh probe
```

The probe links the patched Porsche class and checks its marker field, without
initializing OEM HMI services or switching contexts. Failure leaves the candidate
disabled. Passing proves this probe's J9 loader path, not the full HMI startup.
The hook also remains inactive until explicitly enabled. A normal restart with
the candidate disabled tests dependency loading without applying ARM patches.

For the controlled integration test, disconnect the phone, enable and manually
restart MMI. Reconnect the shell after restart, then start the renderer in its
own foreground shell and capture stdout/stderr before connecting the phone:

```sh
sh /mnt/app/root/pcm4-projection/control.sh enable
# Manually restart MMI, then reconnect the shell before the following command.
sh /mnt/app/root/pcm4-projection/control.sh renderer
```

The renderer is deliberately not installed as an unattended startup service yet.
After restart, confirm actual HMI class loading, context900's declared membership
with `dmdt gc`, stock main CarPlay, and the hook initialization message. Connect
the wired iPhone; check negotiation, visible output, navigation-page handover,
other-page precedence, disconnect, no-frame timeout and receiver failure. Confirm
stock map returns. Native arrows/speed limits are not supplied by this package.

## Disable and restore

```sh
sh /mnt/app/root/pcm4-projection/control.sh disable
```

This creates the Java disable marker immediately and removes future-startup
activation. Loaded ARM hooks are not hot-unloaded; manually restart the MMI.
Stop the standalone renderer with Ctrl-C. To restore prior files, from the
original candidate folder:

```sh
sh install.sh --restore /mnt/app/root/pcm4-backup-NEW
```

Rollback first checks the installed hashes and saved originals. Unexpected later
edits stop it. It clears activation, requests Java release and restores both
factory files while removing added payloads. It retains the backup and may leave
empty project directories. Restart manually afterward.

## Offline proof versus target proof

Host tests cover retained CF dictionary ownership and unchanged stock responses,
missing/mismatched keys, descriptor handling, session setup/teardown, framing,
crypto-state adapter, real localhost transport into synthetic NvSS, actual atomic
markers, timeout/reconnect/termination, Java takeover/restoration and collisions.
All target components cross-compile for QNX ARM. SHA256 is checked against an
independent host implementation across block/padding boundaries. Installer and
rollback are tested against an isolated host filesystem.

No offline test can establish QNX loader constructor order, real text-page
permissions/cache behavior, iPhone acceptance, active vehicle coding, actual
NvSS decode/scanout or warning priority. These are the remaining target gates.
The currently chosen main-stream teardown policy also stops the auxiliary stream;
it favors releasing projection over retaining a possibly obsolete session.
