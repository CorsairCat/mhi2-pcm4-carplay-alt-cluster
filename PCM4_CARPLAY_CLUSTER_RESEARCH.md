# Porsche PCM4 CarPlay instrument-cluster display — requirements and research

Created: 2026-10-24. This is the reusable project record; append discoveries, evidence, decisions, and test results here.

Detailed Audi-to-Porsche implementation comparisons are maintained separately in [Audi / PCM4 method comparison](AUDI_PCM4_METHOD_COMPARISON.md), per the owner's request. Requirements and baseline evidence remain in this document.

MU1476 display-port implementation and validation are recorded in the third document, [PCM4 MU1476 display port](PCM4_MU1476_DISPLAY_PORT.md).

Development decision: the owner has selected MU1476 as the eventual target and plans to upgrade before on-car validation. Continue offline against the supplied MU1476 reference; obtaining MU0809 MMX files is no longer a porting prerequisite. This does not change the recorded original vehicle baseline.

## 1. Objective

Enable the existing Porsche PCM4 Apple CarPlay receiver to present a secondary navigation/map display on the instrument cluster through the existing head-unit-to-cluster display connection, adapting the approach used by public MHI2/MHI2Q/MH2P projects where feasible.

## 2. Vehicle baseline supplied by the owner

| Item | Current information | Evidence status |
| --- | --- | --- |
| Brand | Porsche | Owner reported |
| Model / model year | Macan, 2017 | Owner reported |
| Infotainment | PCM4 | Owner reported |
| Firmware identifier | `MHI2_US_POG11_P3300` | Owner clarified the identifier; supersedes the earlier double-underscore transcription |
| Complete displayed software version | `MHI2_US_POG11_P3300 0809` | Owner reported |
| MU software version | `0809` | Owner supplied as the software-version suffix; matched to P3300 in the MIB Helper firmware list |
| Hardware version | `051` | Owner reported |
| Phone driver version | `10.468.116` | Owner reported; a component version, distinct from MU software version |
| Engineering-menu / shell access | Not currently available | Owner reported; access method must be established |
| CarPlay | Built in and available | Owner reported |
| Factory navigation | Present | Owner reported |
| Cluster display connection | LVDS link from PCM to instrument cluster | Owner reported; electrical/interface details not independently verified |
| PCM part number and cluster part number/software | Unknown | To collect |

MU software version is the short software release identifier used alongside the full firmware train in MIB tools and project compatibility tables. For this unit the pair is train `MHI2_US_POG11_P3300` and MU `0809`; the phone driver has its own version. Source: [MIB Helper firmware list](https://mib-helper.com/firmware_list.php), which explicitly maps this train to MU 0809. These identifiers do not by themselves confirm CarPlay AltScreen compatibility.

## 3. Requirements

- R1: Show the phone's CarPlay navigation map on the instrument cluster as an additional display while retaining normal CarPlay operation on the main PCM display.
- R2: Prefer a native CarPlay secondary/auxiliary display stream. Distinguish this from mirroring the entire center screen and from turn-by-turn text/arrows alone.
- R3: Reuse the existing navigation display connection and built-in CarPlay capability where technically possible.
- R4: Establish compatibility with the exact Porsche firmware, receiver stack, graphics hardware, and cluster before selecting or adapting a patch. Audi or PCM5 support alone is not proof of PCM4 support.
- R5: Preserve factory cluster instruments, warnings, original navigation access, and ordinary PCM functions; provide a defined transition back to the stock map when CarPlay ends.
- R6: Make changes recoverable: identify original files, backups, installation checks, and rollback/recovery procedure before vehicle deployment.
- R7: Use wired CarPlay only. Wireless CarPlay, wireless adapters, and their compatibility or implementation are outside this project's scope.
- R8: Record sources, exact supported firmware, limitations, and the difference between upstream claims, source-code evidence, and results measured on this vehicle.
- R9: Keep requirements, intermediate findings, open questions, and subsequent test results in this document.
- R10: Navigation arrows / turn-by-turn maneuver guidance in the instrument cluster are a desirable optional enhancement. Investigate Porsche-specific guidance integration where useful, but do not make it a prerequisite for the core secondary-map display.
- R11: Speed-limit display in the instrument cluster is another preferred optional feature. Use the same head-unit-to-cluster signal/data link as turn-by-turn guidance as the intended design. Verify that the phone/navigation app supplies speed-limit data and that the Porsche guidance path and cluster support conveying and displaying it; this shared path is a requirement to investigate, not a confirmed capability of P3300.

## 4. Initial scope and acceptance criteria

This phase is online research and a reusable requirements/feasibility record. It does not authorize or perform a firmware installation.

Eventual success means a supported navigation app displays its secondary CarPlay map in the intended cluster map area, the main display remains usable, connect/disconnect and navigation transitions work, and reverting to original files restores stock behavior. Geometry, app compatibility, stability, and input behavior must be measured rather than assumed.

All connection and session testing in this project uses wired CarPlay. Navigation arrows are a secondary goal: if implemented, validate maneuver direction, distance and clearing behavior separately from map-video acceptance.

Speed-limit display is also a secondary goal. If implemented, verify the value, km/h versus mph, updates when the limit changes, and clearing when data becomes unavailable or the session ends. Confirm that it uses the same signal/data link as turn-by-turn guidance; distinguish a cluster speed-limit indication driven by data from a sign simply embedded in the phone's map video.

## 5. Research findings

Research checked on 2026-10-24. These are upstream documentation claims, not tests on the owner's vehicle. Default branches and latest-release links can change.

### 5.1 Repository shortlist

| Project / primary source | Documented target and status | Relevance to this PCM4 |
| --- | --- | --- |
| [fifthBro/mh2p-cluster](https://github.com/fifthBro/mh2p-cluster) | Porsche PCM5/MH2P cluster maps and route guidance. The README explicitly lists 718 Cayman/Boxster MHI2/PCM4 as in progress, reporting Android Auto and CarPlay working with a release to follow. | Closest Porsche-specific lead. The PCM4 entry does not establish support for POG11 P3300 or every Porsche model. |
| [fifthBro release v1105](https://github.com/fifthBro/mh2p-cluster/releases/tag/v1105) | The latest-release link resolved to v1105, commit `6e311f8`, dated 24 October. Release requirements still specify Porsche PCM5/MH2P, QNX 6.6/Tegra K1 and MH2P SD ModKit. | Important distinction: PCM4 development mentioned in the README is not a released PCM4 compatibility statement. This package is not established as suitable for the owner's unit. |
| [aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto) | Nvidia Tegra MHI2 / QNX 6.5, `MHI2_ER_AUG22_K3344`, MU1438, LVDS Virtual Cockpit in classic layout. CarPlay map takeover and Apple Maps wheel zoom reported working. One vehicle tested; a fresh installation on another car has not been validated. | Particularly relevant Audi MHI2/LVDS reference. Exact component hashes are enforced; Porsche firmware is outside its validated target. |
| [chefranov/mhi2-au37x-carplay](https://github.com/chefranov/mhi2-au37x-carplay) | Audi MHI2 AU37x and AU276. Tested example: `MHI2_ER_AU37x_P5089`, MU1326, `8V1035036A`, HARMAN iAP2 stack. Offers cluster maps and guidance. Installer rejects other firmware families. | Another useful MHI2 reference, but its Audi HMI replacements and binaries cannot be assumed compatible with POG11. |
| [luka-dev/mib2q-carplay-rgi](https://github.com/luka-dev/mib2q-carplay-rgi) | Audi MHI2Q implementation using stock `libairplay.so`, runtime hooks, external cluster renderers and Java integration. Secondary map video uses CarPlay AltScreen/type 111. | Useful architecture reference; MHI2Q support is distinct from Porsche MHI2. The similarly named [maxim-smirnov/mhi2-carplay-cluster](https://github.com/maxim-smirnov/mhi2-carplay-cluster) is a fork of this project. |
| [kamgurgul/mib2q-carplay](https://github.com/kamgurgul/mib2q-carplay) | Audi MHI2Q; developed on `MHI2Q_ER_AUG22_P5152`, MU1329. Cluster maps, guidance and Android Auto; Java patch built against the unit's HMI. | Additional implementation reference. Its supported-unit reports do not establish Porsche compatibility. |
| [harman-f/mhi2_altscreen_carplay](https://github.com/harman-f/mhi2_altscreen_carplay) | Škoda-first MQB MHI2 research, reference `MHI2_ER_SKG13_P4526`, MU1440. Receives auxiliary type-111 H.264 and sends MPEG-TS through MOST150. Public release and current development have different scopes. | Demonstrates why “MHI2” alone is insufficient: this MOST route differs from the reported Porsche LVDS route. Useful protocol research, not a directly interchangeable output implementation. |
| [harman-f/MHI2_US_POG11_K5186_1-MU1476-AIO](https://github.com/harman-f/MHI2_US_POG11_K5186_1-MU1476-AIO) | Porsche US POG11 custom firmware update, including CarPlay/Android Auto enablement and engineering-menu access. | Background on the same firmware family. Its described features do not establish cluster AltScreen support; a firmware update is not currently a project requirement. |

### 5.2 Preliminary feasibility assessment

**Promising research target; exact compatibility remains unverified.** The Porsche PCM4 development entry is more relevant than relying solely on Audi success. The reviewed sources do not establish a ready-to-install solution for `MHI2_US_POG11_P3300` / MU0809 on the owner's 2017 Macan.

The existing map-capable cluster connection is useful baseline evidence from the owner, but it does not establish that PCM's CarPlay receiver advertises an auxiliary display, receives its stream, or can render it into the correct cluster surface. The generic adaptation model below is an engineering inference from the distinct receiver, renderer, and HMI components described by the Audi projects; it is not a measured description of P3300:

```text
iPhone / navigation app
  -> PCM CarPlay session advertises an auxiliary display
  -> auxiliary navigation video is received
  -> PCM decodes and renders into the appropriate cluster output
  -> existing head-unit-to-cluster connection
  -> cluster map area

Control path: PCM/HMI selects map ownership, geometry, visibility and stock-map handback.
Optional guidance path: maneuver data -> Porsche-specific cluster guidance integration.
Preferred extension: speed-limit data -> same guidance signal/data link -> cluster indication.
```

The shared guidance/speed-limit path above expresses the owner's intended design. Its protocol, available fields and cluster rendering support remain unverified.

The likely porting work must therefore be assessed in three parts: receiver negotiation/stream access, graphics/output routing, and Porsche HMI/control behavior. LVDS availability by itself resolves none of the software compatibility questions. MHI2, MHI2Q and MH2P must remain separate targets during comparison.

### 5.3 Next investigation sequence

1. Confirm the vehicle identity and complete PCM/cluster versions listed in section 6.
2. Re-check fifthBro's PCM4 development and release status against that exact model and POG11 build. Do not infer PCM4 release support from an MH2P package.
3. Obtain a read-only inventory of the owner's firmware: OS/CPU information, process list, CarPlay libraries/receiver, HMI Java archives, graphics/display configuration, stock cluster-map producer, and hashes of files relevant to any proposed patch. Inventory names vary by platform; Audi filenames are leads, not guaranteed Porsche paths.
4. Compare that inventory with the Porsche PCM4 work and Audi Tegra MHI2 reference. Determine which code can be reused and which receiver/HMI/output components require adaptation.
5. Document a version-matched backup and recovery plan before constructing or deploying a vehicle patch.
6. When implementation is justified, validate auxiliary stream receipt, visible cluster rendering, geometry, stock-map handback and main-screen operation as separate milestones. Record the exact build, phone/app versions, logs, and visible outcome for each.
7. Investigate optional turn-by-turn and speed-limit metadata together: identify the source fields, Porsche guidance interface and shared cluster link, then verify each feature independently.

## 6. Open questions and information to collect

1. PCM part number remains to collect. Model/year (2017 Macan), firmware train (`MHI2_US_POG11_P3300`), MU (`0809`) and hardware version (`051`) are now recorded in section 2.
2. Instrument-cluster part number/software, map-area resolution, and factory navigation display behavior.
3. Confirmed transport/interface topology and the process that supplies the stock cluster map.
4. Available read-only diagnostic access and existing backup/recovery facilities.
5. iPhone/iOS and desired navigation applications for wired CarPlay testing.
6. Available Porsche interfaces for optional cluster navigation arrows / turn-by-turn guidance, after the core map-display path is established.
7. Speed-limit metadata availability per navigation app, and whether the same Porsche guidance link can carry it to a supported cluster indication, including units and unavailable-data behavior.

## 7. Research and test log

| Date | Activity | Result / evidence |
| --- | --- | --- |
| 2026-10-24 | Requirements captured before implementation | Owner baseline and desired secondary cluster display recorded; online repository research started. |
| 2026-10-24 | Primary-source repository review | Identified Porsche PCM4 work in progress, Audi Tegra MHI2/LVDS reference, MHI2Q alternatives, and the MQB MOST transport distinction. Links and target firmware recorded above. |
| 2026-10-24 | Release support cross-check | fifthBro latest resolved to v1105 (`6e311f8`); release explicitly targets PCM5/MH2P despite PCM4 development mentioned in README. No P3300 compatibility confirmation established. |
| 2026-10-24 | Owner scope correction | Supersedes the original R7, which left wireless support open for later: wireless work is excluded. Added R10 for optional navigation arrows; secondary map video remains the core goal. |
| 2026-10-24 | Owner preferred-feature addition | Added R11: optional speed-limit display, intended to share the turn-by-turn signal/data link. Recorded the shared path as a design requirement pending verification. |
| 2026-10-24 | Owner baseline clarification | 2017 Macan / PCM4; `MHI2_US_POG11_P3300 0809`, HW `051`, phone driver `10.468.116`. Corrected the original firmware transcription. MIB Helper identifies MU as `0809`; PCM and cluster part numbers and diagnostic access remain unknown. |

## 8. Template for later intermediate findings

For each new finding, append: date; question investigated; repository commit/release or vehicle file hash; source link or local artifact path; observed result; interpretation; unresolved questions; next action. Label entries as owner report, upstream claim, source inspection, or vehicle measurement. Preserve earlier conclusions when superseding them so their evidence remains traceable.

## 9. Proposed start plan — access, evidence, then prototype

Owner has no engineering-menu or shell access. The immediate milestone is a verified backup and a useful firmware inventory, before deploying cluster-projection code. This is a proposed sequence, not a vehicle installation performed by this project.

1. **Record stock behavior.** Capture PCM version screens and cluster map views; note whether factory navigation shows an actual map, arrows, and speed-limit signs. Obtain PCM/cluster part numbers from available identification screens or a diagnostic scan. Record wired CarPlay working on the center display.
2. **Review the access package offline.** Evaluate [M.I.B. / More Incredible Bash](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash) for exact P3300/MU0809 compatibility and inspect the selected release's launcher, backup and recovery behavior. Upstream documents MHI2 support and GEM activation during installation; installation is itself a modification. Keep the current firmware as the initial research baseline. Firmware upgrade necessity has not been established. CarPlay is already active, so activation/FEC patches are not a project task.
3. **Establish access and backups.** Subject to that review, use the version-matched access route, then run the documented backup and inspect the files/logs on a computer. Upstream's [SD preparation/install guide](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/wiki/Prepare-M.I.B-SD-card-%26-install-M.I.B) describes a FAT32 full-size SD card, basic backup during installation and a subsequent backup operation. Use external vehicle power support during programming as upstream requires. Verify backup scope and a usable recovery procedure; the presence of a backup folder does not prove full recovery from every failure. Retain private originals outside Git, since backups may contain vehicle identifiers and credentials.
4. **Inventory and compare offline.** Extract relevant CarPlay receiver/HMI/display components and hashes. Identify the stock map producer and cluster output geometry. Compare with fifthBro's PCM4 work first and the Audi Tegra MHI2/LVDS reference second. If the backup omits needed runtime information, investigate a diagnostic collection script or compatible Ethernet/shell access; do not assume SSH is available. The M.I.B. README documents a legacy USB-Ethernet/Telnet route with adapter and login requirements, which still needs validation for this car.
5. **Build a minimal map prototype.** Prove auxiliary-screen negotiation and stream receipt, then visible cluster output with correct geometry, then connect/disconnect handback. Use bounded temporary changes with original files preserved before any persistent startup integration.
6. **Add preferred metadata features.** Investigate turn-by-turn arrows and speed limits together on the intended shared guidance data link. Verify phone/app metadata, Porsche transport fields and cluster rendering support separately; sharing a link does not imply both features are implemented.

First deliverable: a reviewed access/backup procedure for this exact baseline, followed by an inventory report identifying the receiver, stock map renderer and cluster output. No need to buy an Ethernet adapter until its role and hardware compatibility are confirmed.

## 10. Audi approach — source inspection and implications for PCM4

Checked 2026-10-24. Evidence here is static upstream source/documentation inspection, not a build or vehicle test. The main example is **aberbic's Tegra MHI2 project**, because it publishes the CarPlay hook, Nvidia renderer, HMI integration and build scripts under GPL-3.0. Its documented target remains Audi K3344/MU1438, classic LVDS Virtual Cockpit, tested on one vehicle. This is not a supported Porsche package. OEM firmware inputs and the QNX SDK remain separate dependencies. [README](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/README)

### 10.1 Concrete implementation layers

| Layer | Inspected evidence | What the implementation does |
| --- | --- | --- |
| Load the extension | [patch-loader.py](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/scripts/patch-loader.py), [dio_manager.json delta](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/patches/dio_manager.json) | Hash-checks the exact stock executable, then changes its ELF dependencies/load order so `libaltscreen111.so` loads first. This is a small executable patch, not simply a menu setting. |
| Negotiate and receive auxiliary video | [libaltscreen111_mu1438.c](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/src/native/libaltscreen111_mu1438.c) | Adds `altScreen` / `viewAreas`, secondary display identity and geometry; handles stream 111 alongside stock primary stream 110. Uses stock session crypto functions for auxiliary video, converts H.264 framing, and exposes complete access units locally. The active Audi constructor uses 1440×540; older generic comments/defaults in the file must not be mistaken for the active profile. |
| Decode and present | [cluster_runtime.c](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/src/native/cluster_runtime.c) | Loads `libKD.so` and `libnvss_video.so`, calls `NvSSVideoDecode`, and configures a 1440×540 video surface. Declares display context 900 containing layer 60. Publishes a heartbeat after decoder acceptance; the code alone does not prove visible frames on an unknown cluster. |
| Select cluster content and restore stock state | [ClusterGate.java](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/hmi/src/local/mu1438/ClusterGate.java), [PatchCluster.java](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/hmi/tools/PatchCluster.java) | Patches stock Audi navigation/display-manager classes. While the renderer heartbeat is live, navigation contexts select the custom context; native navigation labels are suppressed and cached. When inactive, prior context/data are restored. Non-navigation contexts pass through. Wheel zoom is relayed to the CarPlay hook. |

```text
Wired iPhone CarPlay session
  ├─ primary video 110 -> existing PCM/MMI main-screen path
  └─ auxiliary map 111 -> hook -> local H.264 -> Nvidia decoder
                                              -> custom cluster display context
                                              -> existing Audi cluster output

HMI bridge -> context ownership, native-label handling, restoration and zoom
```

This is a separate phone-rendered map, not merely a copy of the center screen. Native cluster arrows and speed-limit metadata require additional integration beyond receiving map pixels. The project specifically leaves native CarPlay navigation instructions unfinished and excludes experimental metadata code because of reconnect problems. [README metadata status](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/README)

### 10.2 Which projects are open-source starting points?

- **Tegra MHI2:** aberbic publishes source and native/HMI build scripts under GPL-3.0. [Native build recipe](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/scripts/compile-native.sh), [HMI build recipe](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/scripts/build-hmi.sh). The prebuilt HMI/helper manifest pins source revision `65434aa00a79e583ca1d7d90dad59d4a188465d1`; this is the manifest's revision, not a claim that every default-branch file inspected is pinned to it. [Manifest](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/prebuilt/mu1438-components.json)
- **MHI2Q:** luka-dev supplies a GPL-3.0 project. Its documented architecture keeps stock `libairplay.so`, injects a hook into `dio_manager`, and connects Java HMI/BAP integration and renderers. It provides useful guidance/cluster concepts but targets a different platform. [Repository](https://github.com/luka-dev/mib2q-carplay-rgi), [architecture](https://github.com/luka-dev/mib2q-carplay-rgi/blob/main/docs/architecture.md). Some architecture documentation predates the current repository feature set; verify against current source when porting.
- **Free distribution is not necessarily open source:** chefranov's Audi AU37x project provides working-feature claims and binaries, but its custom license restricts modified redistribution and distribution of reverse-engineered derivatives. Treat it as a reference for behavior, not the same reusable source base as the GPL projects. [License](https://github.com/chefranov/mhi2-au37x-carplay/blob/main/LICENSE)
- **MQB MHI2:** the previously recorded harman-f project is another source-published research lead, but its MOST output path is different from our reported LVDS topology. See section 5.1.

### 10.3 Revised research priority

There is enough public MHI2 source to begin an offline port-feasibility study before engineering access is enabled on the car. Start with the wired CarPlay portion of aberbic's project and compare it with Porsche PCM4 work. For MU0809, obtain original `dio_manager`, CarPlay libraries, `lsd.jxe`/HMI and display/video libraries/configuration through a suitable stock firmware package or verified vehicle backup. Exact file presence and paths remain to be checked.

Compare exported functions and binary layouts, Nvidia video API availability, cluster-output dimensions, and Porsche HMI context/control interfaces. Receiver protocol logic is a candidate for reuse; executable offsets, Audi class names, context 900/layer 60 and 1440×540 geometry are reference-specific values requiring requalification. A full independent open-source replacement for Apple's/OEM's CarPlay receiver has not been established here: these projects extend an existing working receiver.

## 11. MU0809 offline firmware acquisition

### 11.1 Search outcome — 2026-10-24

Offline analysis from a matching firmware package or stock component set is a valid first step and does not require enabling engineering access on the car. **No verified stock P3300/MU0809 archive has been acquired in this session.**

| Source / candidate | Verified finding | Limit |
| --- | --- | --- |
| [MIB Helper exact-train page](https://mib-helper.com/index.php?train=MHI2_US_POG11_P3300) | Identifies P3300 as production firmware, MU0809, and links to MIB Solution for firmware downloads. | An index entry and general download link do not prove that the exact old production archive is hosted there. |
| [MIB Solution](https://mibsolution.one/) | Attempted to open the linked host. | Web fetch returned HTTP 530; local shell could not resolve the hostname. No directory listing or archive contents inspected. Other historical hostnames were also inaccessible through the web tool. This is an access result from this session, not proof the service is unavailable to everyone. |
| [M.I.B. compatibility table](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/wiki/MHI2-M.I.B.-patch-FW-compatibility-table) | Lists `MHI2_US_POG11_P3300_MU0809`. | Patch support is not a complete stock firmware download. |
| [M.I.B. public patches directory](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/tree/main/patches) | Contains support data and a placeholder instructing users where to copy patches. | The inspected directory does not itself supply an exact stock MU0809 application/HMI archive. |
| [Porsche MU1476 AIO project](https://github.com/harman-f/MHI2_US_POG11_K5186_1-MU1476-AIO) | A newer modified K5186/MU1476 package supports upgrading from some older trains. | Being compatible as an upgrade from P3300 does not make its binaries MU0809. Use only as a separately labeled comparison if acquired. |
| [Macan MU0809 owner log](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/discussions/118) | A public report identifies Macan / HW051 / P3300 / MU0809 and logs backup of HMI and `lsd.jxe`. | Useful evidence that a same-train backup is an alternative acquisition route; no downloadable stock component set was verified. The reported part number is another owner's, not this vehicle's. |

### 11.2 What would be sufficient for analysis

Prefer an unmodified exact-train package with manifest/version information, or a stock same-train backup with component provenance. A complete flash image is not essential if the following matching components can be obtained:

- `dio_manager`, its configuration/startup files, and the CarPlay receiver library (candidate name `libairplay.so`).
- `lsd.jxe` or equivalent stock HMI archive, accompanying Java archives and resources.
- Nvidia/display libraries (candidate names `libnvss_video.so`, `libKD.so`) and display/cluster configuration.
- Package manifests such as `metainfo2.txt`, where present, and version records identifying their source build.

These are collection targets based on the Audi reference, not a claim that all names/paths have been confirmed on Porsche MU0809. An activation-only `*_PATCH` or IFS-stage2 image must be inspected for actual contents and cannot be assumed to contain the application partition and HMI. Map database packages are not the requested head-unit software.

### 11.3 Intake and analysis procedure

1. Save the original archive or component set privately under `private/firmware/`; retain its source URL, download date, supplied checksum if any and a locally computed SHA-256. A local hash tracks identity; it does not independently prove authenticity.
2. List archive contents and inspect manifests before extraction. Establish stock versus modified status and exact train/MU; keep P3300/MU0809, K3300/MU0809 and later releases separately until actual component equivalence is shown.
3. Extract data for static inspection only. Record file hashes, ELF architecture/imports/exports, receiver version and HMI classes. Do not execute update/install scripts as part of analysis.
4. Produce a comparison report against the Audi hook/renderer/HMI interfaces. Vehicle access is still needed later to validate runtime topology, actual display geometry and component matches.

Repository handling: `private/` is ignored so downloaded firmware and vehicle-specific backups stay outside the source history. Commit analysis notes, manifests of hashes and original project code instead of raw vendor firmware.

### 11.4 Parallel download search follow-up

On the owner's explicit request, a second agent searched independently. It found the browsable [MIB Vault Porsche firmware directory](https://mibvault.com/files/MHI2%20-%20HARMAN/Firmware/Porsche), subsequently opened by the main agent as well. The listing includes `MHI2_ER_POG11_K4185_MU1119.7z` (4.4 GB), `MHI2_ER_POG11_K5126_MU1394_9P1906961H.7z` (4.5 GB), and `MHI2_US_POG11_K5186_1_AIO_MU1476_20230318.7z` (1.7 GB). These are different versions; listing names and sizes do not verify actual archive contents or authenticity.

The directory exposes a [US subfolder](https://mibvault.com/files/MHI2%20-%20HARMAN/Firmware/Porsche/US), but neither agent could retrieve its contents through the web tool. No exact stock P3300/MU0809 object or direct archive download URL was verified, and no archive was downloaded. This is a live directory lead, not a completed firmware acquisition.

[M.I.B. issue #511](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/issues/511) reports a 2017 Macan K3300/MU0809 backup uploaded to MIB Solution. The issue links a log, not the full backup. K3300 and P3300 component equivalence remains unverified.

### 11.5 Wider search after owner checked MIB Vault

Owner reports MIB Vault does not have P3300. Treat that mirror as exhausted for the exact target unless new evidence appears; section 11.4 remains a record of the earlier lead.

On 2026-10-24, searched exact train and partial identifiers, P3300/MU0809 and K3300/MU0809 variants, firmware/dump/backup terminology, Chinese and Russian download terms, cloud/archive-host references, and indexed automotive forum results. **No additional website with a verified full stock P3300/MU0809 download was found.** No download, purchase, account registration or contact with others was performed.

| Alternative site / source | What was actually verified | Exact-target outcome |
| --- | --- | --- |
| [MIBSolution.it MHI2 archive/service](https://mibsolution.it/en/piattaforma-mhi2-harman.html) | Advertises 62 MHI2 firmware files and subscriber downloads. | No public P3300/MU0809 package was identified; archive size and service coverage do not establish exact-version availability. This is a separate domain/service from the historical MIB Solution links. |
| [VAGupdate firmware catalog](https://vagupdate.com/firmwares) | Offers paid firmware/download services with Porsche PCM options. | No exact P3300/MU0809 archive verified. Do not purchase on the assumption that generic PCM4 coverage includes this old release. |
| [VWODIS listing](https://www.vwodis.com/9687.html) | Chinese listing for `MHI2_US_POG11_K5186_1_MU1476_AIO_20220723`, with gated download UI. | P3300 appears in upgrade compatibility text, not as the supplied archive version. Rejected as the exact baseline. |
| [EuroNavMaps listing](https://euronavmaps.com/en/north-america/322-porsche-pcm-40-mhi2uspog11k5186mu1476-nar-software-update.html) | Product identifies K5186/MU1476 and lists P3300 among supported starting versions. | Newer firmware, not P3300. |
| [Digital Eliteboard Porsche thread](https://www.digital-eliteboard.com/threads/porsche-pcm4-mib2-aio-mhi2_er_pog11_1-mhi2_er_pog24_k5137_1-fw-update-patch-navigation-activation.498079/page-23) | Indexed owner discussion about K3300/MU0809 and newer AIO packages. | No exact downloadable stock archive established from the searched results. |

Recommended acquisition priority remains a stock same-train backup/component set or the owner's own extraction. A later full Porsche package can support architectural comparison offline, but findings must stay labeled by its real version and cannot establish MU0809 hook offsets, HMI compatibility or installation suitability. The inability to locate a public archive does not prove the factory release was never distributed.

### 11.6 P3300 successors and PCM4 upgradeability

Checked 2026-10-24 following the owner's question and explicit observation of MIB Helper's update section. **P3300/MU0809 is not the last release.** The [exact P3300 page](https://mib-helper.com/index.php?train=MHI2_US_POG11_P3300) names `MHI2_US_POG11_P5186` / MU1476 as both its latest official and latest possible update. This is MIB Helper's classification, not a VIN-specific Porsche authorization or a verified downloaded package.

Keep these identities distinct:

| Identifier | Evidence and meaning |
| --- | --- |
| `MHI2_US_POG11_P5186` / MU1476 | [MIB Helper](https://mib-helper.com/index.php?train=MHI2_US_POG11_P5186) labels it production firmware and the latest update for P3300. |
| `MHI2_US_POG11_K5186` / MU1476 | [MIB Helper](https://mib-helper.com/index.php?train=MHI2_US_POG11_K5186) labels it customer-update firmware; the AIO maintainer identifies it as the underlying Porsche firmware. |
| `MHI2_US_POG11_K5186_1` / MU1476 AIO | [Community project](https://github.com/harman-f/MHI2_US_POG11_K5186_1-MU1476-AIO), with additional patches and installation behavior. It is not an unmodified factory package. |

Equal MU numbers do not establish byte-for-byte equivalence, interchangeable hook offsets or identical installation scope. Inspect each acquired package's manifest and actual components.

**PCM4 is updateable on the existing hardware.** Porsche's [2017 WH05 bulletin](https://static.nhtsa.gov/odi/tsbs/2017/MC-10127491-9999.pdf) expressly includes the 2017 Macan PCM4.0 and specifies a region-specific SD update followed by PIWIS coding of PCM, the instrument cluster and Bose amplifier if equipped. Its prescribed programming supply is 13.5–14.5 V. This proves an official update mechanism, but does not by itself identify P5186 as that campaign's target. Porsche's later [2021 WMD1 campaign](https://static.nhtsa.gov/odi/tsbs/2021/MC-10191756-0001.pdf) also includes 2017–2018 Macan and names North American Macan/Cayenne update media `9P1919360AL`. VIN assignment determines campaign applicability; no current entitlement is established here.

For exact old-train evidence, the [AIO maintainer's discussion #2](https://github.com/harman-f/MHI2_US_POG11_K5186_1-MU1476-AIO/discussions/2) responds affirmatively to a 2017 Cayenne owner with `P3300 0809` / HW051. It supports upgrade feasibility for the same train/hardware family, but is not a measured test on this owner's Macan.

Research decision: broaden acquisition to a full US POG11 MU1476 package now, preferably an unmodified K5186 update or verified P5186 package. Analyze its MMX/CarPlay/HMI components offline before deciding whether to move the vehicle to that baseline. Preserve an independent MU0809 backup and coding first. No evidence reviewed here establishes that a stock update alone enables CarPlay cluster AltScreen, turn guidance or speed-limit integration.

### 11.7 P5186 versus K5186 and exact-P5186 download search

Checked 2026-10-24. The prefix meaning has primary-source support: an [Audi MHI2 service bulletin, page 2](https://static.nhtsa.gov/odi/tsbs/2018/MC-10133308-9999.pdf) defines P as production software and K as Kunden/customer update. This describes release/distribution type; K does not inherently mean community-modified. The [Porsche AIO maintainer](https://github.com/harman-f/MHI2_US_POG11_K5186_1-MU1476-AIO) separately describes the modifications added to the K5186-based AIO. Neither prefix nor equal MU1476 establishes a complete functional or binary diff between the Porsche P5186 and K5186 packages; that requires actual manifests and payload hashes.

Searched full and partial P5186 train names, MU1476 archive filenames, download/cloud terms, Russian and Chinese terms, and indexed vendor/archive pages. **No working direct download of a verified full P5186 archive was established.** MIB Helper's exact-train page links to a general archive, not a P5186-specific object. Historical mibsolution.one returned HTTP 530. The live [MIB Vault Porsche directory](https://mibvault.com/files/MHI2%20-%20HARMAN/Firmware/Porsche) was readable, but its US subdirectory failed through the web tool. No firmware was downloaded.

An indexed [Rennlist discussion](https://rennlist.com/forums/991/1461759-991-2-mib2-pcm-4-firmware-update-enable-android-auto-and-update-maps-2.html) mentions a P5186 folder while seeking the latest firmware, then redirects the user to the K5186 AIO package. This is ambiguous evidence, not confirmation of a separate downloadable stock P5186 package. A [P5186 owner report](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/discussions/401) confirms an installed P5186/MU1476 unit but supplies no full archive. Do not substitute a patch folder for full firmware.

Additional correction to interpreting section 11.6: P5186 is **the latest listed by MIB Helper**, not a proven final release of the entire product family. An [owner's issue #788](https://github.com/Mr-MIBonk/M.I.B._More-Incredible-Bash/issues/788), opened 2025-10-06, reports `MHI2_US_POG11_P5251` / MU1503 on a newer PCM4 vehicle. That is an owner claim, not verified package evidence or established compatibility with this 2017 Macan. It prevents treating the helper index as an exhaustive current release history.

Practical acquisition target remains an unmodified K5186/MU1476 customer-update package for offline analysis, alongside continued P5186 search if a verifiable archive appears. There is no demonstrated need for the P prefix to perform architectural analysis; installation suitability remains a separate check.

### 11.8 AIO versus an unmodified update

The [upstream README](https://github.com/harman-f/MHI2_US_POG11_K5186_1-MU1476-AIO) and [packaged README_AIO.txt](https://github.com/harman-f/MHI2_US_POG11_K5186_1-MU1476-AIO/blob/main/README_AIO.txt) describe AIO as the base K5186 firmware combined with automated installation and additional modifications. These include patched IFS-root checks, feature-entitlement changes, developer-menu access, M.I.B. tools, backup/logging, and a default manifest that skips Bose amplifier updates. The unmodified update does not include these community additions. A filename lacking AIO alone cannot authenticate an archive as stock.

Owner observes similar download sizes. This is consistent with both packages carrying the large base firmware payload while patches, scripts and manifests add comparatively little; this explanation is an inference, not a measured comparison of the two archives. Similar rounded/compressed sizes do not establish identical contents or installation behavior. No actual pair of packages has been hashed or compared here.

For this project, an unmodified package is preferable as the reference corpus. AIO remains usable for offline component inspection if its modifications are explicitly tracked. Its published feature list does not establish CarPlay AltScreen support. The AIO installation can start automatically on insertion into a compatible powered PCM; do not use an analysis SD card in the car unintentionally.

### 11.9 Owner-supplied K5186 reference archive: extraction blocked by truncation

On 2026-10-24 the owner supplied `MHI2_US_POG11_K5186_MU1476.7z`. Added the exact archive filename to `.gitignore`; extraction outputs will stay under ignored `private/`. Its name suggests the unmodified reference discussed above, but contents have not yet been verified.

Initial inspection found only **566,165,504 bytes** on disk. The file has a valid 7z signature and start-header CRC. Its start header specifies a next-header offset of 1,820,135,335 bytes and next-header length of 39 bytes; including the 32-byte initial header, the archive therefore requires at least **1,820,135,406 bytes**. The current copy contains about 31.1% of that required extent. `7z l` reports `Unexpected end of archive` and cannot list contents. A newer bundled Keka reader also failed, without producing an inventory.

No extraction, firmware identity confirmation or new component findings can be claimed from this incomplete copy. Complete or re-download the archive, then verify archive integrity and inspect `metainfo2.txt`, component manifests and the MMX payload before static analysis. The source was not modified and no package contents were executed.

## 12. Local MU0809 patch inspection

### 12.1 Input and verification

Owner supplied directory: `MHI2_US_POG11_P3300_MU0809_PATCH/`. The directory is now explicitly ignored by Git, along with `.DS_Store`; derived vendor image contents remain under ignored `private/`.

| Property | Observed value |
| --- | --- |
| Image filename | `MU0809-ifs-root-part2-0x00ba0000-00A2EE00.ifs` |
| Compressed file size | 15,639,052 bytes |
| SHA-256 | `8e003007bcde2f378e969a06d1be8fd3c06563a601fff240000dfbfcd625c406` |
| Format | QNX startup header followed by LZO-compressed ImageFS |
| Decompression | 664 blocks; 43,475,992 output bytes, exactly matching header size |
| Integrity check | ImageFS 32-bit additive checksum balances to zero; this is structural integrity, not proof of stock authenticity |
| Directory | 149 entries: 139 files, 8 symbolic links, 2 directories |
| Executables/libraries | 121 ELF files; all report ARM machine ID 40 |

`Info.txt` describes the image as already patched to remove FEC signature, VCRN and component-protection checks. This is the supplier's description, not a verified diff against an original stock image. `addFecs.txt` and `ExceptionList.txt` are activation/support metadata. No CarPlay AltScreen patch is claimed in the supplied description.

Read-only inspection tool: `tools/inspect_qnx_ifs.py`. It parses startup/directory structures, decompresses with a local LZO library, checks size/checksum/extents and writes an inventory with per-file hashes. Format references are linked in the script. The local run used the existing Keka `liblzo2.2.dylib`; no firmware executable or installation script was run. Example:

```sh
python3 tools/inspect_qnx_ifs.py \
  MHI2_US_POG11_P3300_MU0809_PATCH/MU0809-ifs-root-part2-0x00ba0000-00A2EE00.ifs \
  --lzo-library /Applications/Keka.app/Contents/MacOS/liblzo2.2.dylib \
  --output private/analysis/mu0809-verified
```

### 12.2 Useful findings

This appears to be the **RCC-side system/control image**, based on RCC/MMX service names, proxy libraries and the large `usr/apps/MIBRoot` program. It is not the complete MMX application/HMI component set needed to port the Audi receiver and renderer.

The parsed directory confirms **no files named** `dio_manager`, `libairplay.so`, `lsd.jxe`, `libnvss_video.so` or `libKD.so`. This absence is based on the decompressed file directory, not just a strings search of compressed bytes. It does not rule out other receiver-related functions under different names or in another partition.

Useful contents include:

| Component | Evidence / interpretation |
| --- | --- |
| `usr/apps/MIBRoot` | Contains `QNX_VERSION=650` and `VERSION=U5634` build strings. These identify this component's embedded build metadata, not the complete vehicle OS/firmware identity. |
| `libasirccdisplayproxy.so` | Names the `asi.display.ILvds` interface. MIBRoot strings reference RCC/MMX LVDS status handling. This is evidence of an LVDS control interface, not proof of which physical display is connected. |
| `libdsikomogfxstreamsinkproxy.so` | Exposes the interface name `dsi.komogfxstreamsink.DSIKOMOGfxStreamSink`, a candidate for cluster graphics investigation. |
| `libdsikomonavinfoproxy.so` | Names `dsi.komonavinfo.DSIKOMONavInfo`; MIBRoot includes distance-to-next-maneuver, turn-to-street, ETA and route-guidance selection method strings. |
| `libdsibapproxy.so`, `libdsicarkombiproxy.so`, `libdsiddp20proxy.so` | Present as additional candidates for cluster/control interface analysis. Their presence does not select the active Porsche runtime transport. |

**Relevant to preferred speed-limit support:** MIBRoot contains both DDP2 maneuver handling (`requestSetManeuver`) and traffic-sign handling (`requestSetTrafficSign`, including `frame`, `cellNo`, `symbol`, `restriction`, `update` argument names in diagnostic strings). This is a concrete lead for investigating shared display-data services for maneuvers and signs. A traffic-sign operation is not yet a decoded numerical speed-limit protocol, and this image includes multiple brand/platform services. We still must establish which service the Macan cluster uses, what sign values mean, and whether both features traverse the same physical link on this vehicle.

### 12.3 Consequence for next work

The supplied image is useful for static analysis of cluster-control interfaces. It does not provide the principal CarPlay receiver, Nvidia renderer or Java HMI inputs identified in section 10. Continue seeking stock MMX/application/HMI components while investigating the local KOMO/DDP2 interfaces. Record any reconstructed method mappings as hypotheses until supported by disassembly, HMI call sites or runtime observations. Keep this patched image distinct from a stock baseline.

## 13. Local K5186/MU1476 AIO reference inspection

### 13.1 Acquisition, extraction and integrity

Owner supplied `MHI2_US_POG11_K5186_1_MU1476_AIO_20220723.7z` on 2026-10-24 and explicitly authorized using AIO as the interim reference. The earlier truncated non-AIO archive is no longer present in the working folder. This AIO archive is ignored by Git; source and derived firmware remain local-only under ignored paths.

| Property | Measured result |
| --- | --- |
| Archive bytes | 1,820,817,131 |
| Archive SHA-256 | `d76f90ce2cf6d03abd0443da8ee6961464be0a94531993639682073419bdf7bf` |
| 7z extraction | Successful: `Everything is Ok`; 286 directories, 921 files, 3,799,546,752 expanded file bytes |
| Extracted package | `private/firmware/mu1476-aio/` |
| Active AIO manifest | `metainfo2.txt`: release `MHI2_US_POG11_K5186_1`, MU1476 |
| Bundled reference manifest | `metainfo2_orig.txt`: release `MHI2_US_POG11_K5186`, MU1476; includes P3300 in `SupportedTrains` |
| MMX version marker | `DEV_MMX2_PAG_NAR_G11_090PROD` |

Both MMX `app.img` files and both `mifs-stage2.img` files match **every 524,288-byte SHA-1 chunk** recorded for them in the bundled `metainfo2_orig.txt`: app /50/ 1,432 chunks, app /70/ 1,944, MIFS /50/ 92, MIFS /70/ 84, all with zero mismatches. This demonstrates internal consistency with that supplied manifest, not independent factory authentication: the manifest itself came from the AIO archive and no external trusted reference or signature validation was performed. It also does not establish that AIO installation leaves the installed filesystem unchanged.

The extracted `common/tools/finalScriptSequence.sh` confirms backup, generic AIO changes, feature activation and button-image modification steps. These scripts were read as text only; none were executed. No SD card or car programming was performed.

### 13.2 Filesystem readers and component inventory

Added two host-side Python readers:

- `tools/inspect_qnx6.py`: reads standard little-endian QNX6 application images without mounting, validates both superblock CRCs, walks directories, records an inventory and selectively extracts regular files. Does not materialize symlinks or copy executable permission bits. Format references: [Linux QNX6 structures](https://github.com/torvalds/linux/blob/master/include/linux/qnx6_fs.h), [inode/block handling](https://github.com/torvalds/linux/blob/v6.1/fs/qnx6/inode.c), [directory handling](https://github.com/torvalds/linux/blob/master/fs/qnx6/dir.c).
- `tools/inspect_mifs.py`: decodes the `LZOZ` stage-2 container using the local Keka `liblzo2` LZO1Z decoder, then validates ImageFS size, bounds and additive checksum. This sample uses 2-MiB expanded blocks, a size table at byte 8, data beginning at `0x800`, and compressed blocks aligned to 512 bytes. Format lead: [MHI2 firmware toolkit research](https://github.com/harman-f/MHI2-MHI2Q-MH2P-Firmware-Toolkit/blob/main/docs/MHI2_FLASH_LAYOUT_AND_RECOVERY.md).

These are bounded readers tested on these real inputs, not general QNX repair/rebuild tools. Python compilation checks pass. The application inventories contain 6,022 entries for /50/ and 6,138 for /70/, with 1,218 selected regular files extracted from each. Both images have 1,024-byte filesystem blocks and both superblock CRCs validate. Stage-2 /50/ and /70/ each contain 39 compressed blocks and decode to the **same 80,371,612-byte ImageFS**, SHA-256 `a6b41d85cde915eff35162c9a1e2fb7f80b7c53dcdbe0f067710fe00dc208067`, 311 directory entries, checksum sum zero. Their differing padded/compressed file sizes therefore do not imply different stage-2 contents in this package.

The /50/ and /70/ directory selectors are not the owner's displayed HW051 identifier; actual on-unit selection has not been determined.

| Target | Location inside extracted filesystem | Size | SHA-256 |
| --- | --- | --- | --- |
| CarPlay manager | app: `eso/bin/apps/dio_manager` | 694,715 | `0d81a2180cc684a2ae4e448523815f094e3550f65f3780827f7b84be65e31706` |
| CarPlay/AirPlay receiver | app: `eso/lib/libairplay.so` | 703,856 | `bd0fff7f7345d58493611d637bd6bce6362ca01d3b44c5be1e320c4f5c9c3c9c` |
| NVIDIA video wrapper | app: `armle/lib/libnvss_video.so` | 73,303 | `58d19e127e77b0973149fe83d7c44fd973fd76ab57fb99108c9e76d3f73f5430` |
| Graphics dependency | stage-2: `lib/libKD.so` | 155,648 | `d44df0bb660b2dbcec8f2b52af45c2450fcfb93501d00481e22b675b55cb6cb3` |
| Java HMI | stage-2: `ifs/lsd.jxe` | 61,971,385 | `1eccb000da3ef57e35dec0d59e4b0be5df375b20b41457fcd53f9bca75307a88` |
| Native display manager | stage-2: `eso/bin/apps/displaymanager` | 597,185 | `c1c212e18282a65c6181835c139eb129ab18a629c6809568cf3a7072e997d22e` |

The first three components are byte-identical across app /50/ and /70/. Both stage-2 images are wholly identical after decompression, covering the other three. `dio_manager` and `libairplay.so` are 32-bit little-endian ARM ELF binaries. App image SHA-256 values: /50/ `764b24e99a0431843157f6355a03f59652ae59490c4e8b1c7e04a94c29178465`; /70/ `c6f89212e6473d09d5049b7f27267e0d8f6a75c997fe1a9b73b40b5d904d9446`.

### 13.3 Findings relevant to the cluster CarPlay port

1. **Receiver comparison can now start.** `libairplay.so` contains the version string `210.81`, display-list/UUID keys, and exported receiver lifecycle functions. Symbol-table values include `AirPlayReceiverServerCopyProperty` `0x1d0e4`, `AirPlayReceiverSessionSetup` `0x23fb8`, `AirPlayReceiverSessionStart` `0x21f78`, and `AirPlayReceiverSessionTearDown` `0x23860`. These are ELF symbol values for this exact hashed library, not live addresses, patches or MU0809 offsets. Version strings alone do not prove equivalence to an Audi or MHI2Q library with the same version.
2. **The NVIDIA render dependencies are concrete.** `dio_manager` ELF `DT_NEEDED` entries include `libairplay.so`, `libnvss_video.so`, and `libKD.so`; its RPATH contains a `qnx650_mmx2` build path. The video library exports `NvSSVideoOpen`, `NvSSVideoStreamConfigure`, `NvSSVideoDecode`, and associated attribute/close functions. This supports investigating the Audi Tegra-style receiver/renderer strategy. It does not establish the Macan's output geometry, display index, layer, ownership rules or decoder capacity for a second stream.
3. **The HMI has the Audi comparison classes.** Printable data in `lsd.jxe` includes `de/audi/tghu/fwhmi/DisplayManagerMIB2High`, `de/audi/app/navi/pcore/cluster/CombiBAPListenerPCore`, and `de/audi/tghu/navi/app/cluster/CombiBAPListener`. It also includes Porsche-specific map state `StatePorsche$StateDataMapKombiPorsche` and LVDS map-visibility messages. Shared Audi namespace names do not make the Porsche HMI interchangeable with Audi MU1438. JXE class reconstruction and method/control-flow comparison remain to do.
4. **Both transport modes are represented.** App engineering scripts `showKombiMapMode.sh` and `enableKombiMapMOST.sh` distinguish `enableKombiMapFPK` (described there as LVDS/FPK) and `enableKombiMapMOST` (MOST/Top-Kombi). KOMO graphics, map view and navigation-info proxy libraries are present. This is shared-firmware capability evidence; no active flag, output route or physical transport was read from the owner's car.
5. **Preferred speed-limit feature has further leads.** HMI strings name `trafficregulation/SpeedLimitInfo`, `TrafficSignInformation`, `updateCurrentTrafficSign`, and `updateTrafficSignOnRoute`. These are candidates for tracing the factory navigation/sign path. They do not show CarPlay supplying speed limits, nor prove these data reach the Macan cluster over the maneuver link.
6. **AltScreen remains unproven.** Extraction and exported APIs establish inputs for reverse engineering. No second-display negotiation, H.264 auxiliary reception, cluster rendering, or guidance-data injection has been demonstrated with this firmware.

### 13.4 Reusable local artifacts and next step

Archive listing and extraction log: `private/analysis/mu1476-aio-archive-list.txt`, `mu1476-aio-extraction.log`. Application and MIFS inventories/extractions: `private/analysis/mu1476-aio-app50/`, `mu1476-aio-app70/`, `mu1476-aio-mifs50/`, `mu1476-aio-mifs70/`. Chunk verification reports: `mu1476-aio-app-checksums.json`, `mu1476-aio-mifs-checksums.json`. Native symbol/dynamic reports and targeted HMI strings: `private/analysis/mu1476-aio-symbols/`. `objdump -T` could not produce the executable's dynamic-symbol report; `objdump -p dio_manager` succeeded and supplies its dependencies. Shared-library symbol reports succeeded.

Example repeatable reader invocation (choose a fresh output directory):

```sh
python3 tools/inspect_qnx6.py private/firmware/mu1476-aio/MMX2/app/50/default/app.img --output private/analysis/mu1476-aio-app50
python3 tools/inspect_mifs.py private/firmware/mu1476-aio/MMX2/mifs-stage2/50/default/mifs-stage2.img --lzo-library /Applications/Keka.app/Contents/MacOS/liblzo2.2.dylib --output private/analysis/mu1476-aio-mifs50
```

Next engineering work should compare this hashed receiver's server-property/setup flow with the Audi hook and reconstruct the Porsche HMI display/cluster methods. Establish what needs porting before designing a patch or upgrading the car. MU0809 remains the actual vehicle baseline; this MU1476 corpus does not supply its missing MMX binaries.
