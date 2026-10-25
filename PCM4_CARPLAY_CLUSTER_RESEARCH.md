# Porsche PCM4 CarPlay instrument-cluster display — requirements and research

Created: 2026-10-24. This is the reusable project record; append discoveries, evidence, decisions, and test results here.

## 1. Objective

Enable the existing Porsche PCM4 Apple CarPlay receiver to present a secondary navigation/map display on the instrument cluster through the existing head-unit-to-cluster display connection, adapting the approach used by public MHI2/MHI2Q/MH2P projects where feasible.

## 2. Vehicle baseline supplied by the owner

| Item | Current information | Evidence status |
| --- | --- | --- |
| Brand | Porsche | Owner reported |
| Infotainment | PCM4 | Owner reported |
| Firmware identifier | `MHI2__US_POG11_P3300` | Owner supplied verbatim; preserve the double underscore until confirmed from a photo or diagnostic readout |
| CarPlay | Built in and available | Owner reported |
| Factory navigation | Present | Owner reported |
| Cluster display connection | LVDS link from PCM to instrument cluster | Owner reported; electrical/interface details not independently verified |
| Vehicle model/year, cluster part number/software, MU version | Unknown | To collect |

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

**Promising research target; exact compatibility remains unverified.** The Porsche PCM4 development entry is more relevant than relying solely on Audi success. The reviewed sources do not establish a ready-to-install solution for `MHI2__US_POG11_P3300`.

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

1. Exact Porsche model, model year, PCM part number, complete software train, and MU version.
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

## 8. Template for later intermediate findings

For each new finding, append: date; question investigated; repository commit/release or vehicle file hash; source link or local artifact path; observed result; interpretation; unresolved questions; next action. Label entries as owner report, upstream claim, source inspection, or vehicle measurement. Preserve earlier conclusions when superseding them so their evidence remains traceable.
