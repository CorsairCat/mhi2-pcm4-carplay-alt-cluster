# Audi method versus Porsche PCM4: port feasibility

Date: 2026-10-24. Detailed companion to [requirements and research](PCM4_CARPLAY_CLUSTER_RESEARCH.md). Static analysis only; no firmware or vehicle changes.

Implementation follow-up: [MU1476 display port engineering record](PCM4_MU1476_DISPLAY_PORT.md). Its instruction-level review found that the Porsche NVIDIA library rejects the value the current Audi renderer supplies at open-argument offset 16. The export-name checks below do not establish argument compatibility.

Later offline findings are in that record's section 9: Porsche uses a separate `DisplayManagerMIB2PorscheHigh` subclass, terminal 1 maps to display ID 4, and terminal-1 map configurations use map instance 3. The 290×330 instance-2 resource below is therefore a weaker viewport lead, not selected cluster geometry. The owner has chosen MU1476 as the development target.

Section 10 of the port record independently confirms a **448×448 HMI screen** in `LayoutProviderPAGMainKombi`, with a navigation visible-area rectangle **(60,30,348,368)**. That rectangle is a safe-area candidate, not yet a selected CarPlay profile or video destination.

## Conclusion

**Reuse the Audi MHI2 architecture as the leading port candidate. Do not apply its existing installer or replacement HMI to this Porsche.** The PCM4 MU1476 reference passes the interface checks below, including five matching receiver entry guards. That is stronger evidence than a shared MHI2 name, but it does not establish a functioning port.

The main unresolved work is Porsche display routing and HMI ownership. Receiver integration still needs instruction-level and ABI review. The owner's installed baseline remains MU0809: the measurements here apply to the supplied MU1476 AIO reference, not its missing MU0809 MMX components. No upgrade is required to continue offline work, and this report does not recommend installing AIO.

## 1. Which existing approach fits?

| Project / method | Relevant mechanism | Use for this project |
| --- | --- | --- |
| [Audi MHI2 Tegra MU1438](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto) | Existing receiver plus auxiliary video, NVIDIA renderer and HMI ownership bridge; reference is LVDS Virtual Cockpit. | Closest architectural match given the reported LVDS link and local NVIDIA dependencies. |
| [Audi MHI2Q RGI](https://github.com/luka-dev/mib2q-carplay-rgi) | Stock AirPlay 210.81 in `dio_manager`, preload hooks, auxiliary video and external renderers, Java integration for BAP/RGI. | Useful negotiation and guidance reference; Qualcomm platform means its renderer and installation are separate porting work. |
| [Škoda MHI2 MU1440](https://github.com/harman-f/mhi2_altscreen_carplay) | Auxiliary H.264 remuxed into MPEG-TS and transmitted through `/dev/mlb/isoTX2` over MOST150. | Alternative transport reference if the actual vehicle route contradicts the LVDS assumption; not the first renderer choice. |
| [Porsche MH2P / PCM5](https://github.com/fifthBro/mh2p-cluster) | Second video and RGI-to-cluster integration; README lists PCM4/718 work in progress. | Porsche integration reference, not a released compatibility claim for a 2017 Macan PCM4. |

The Audi MHI2 README reports a working map on one MU1438 car and leaves native navigation instructions pending. Its installer validates exact firmware components. Broader feature claims from other projects must not be attributed to that tested MHI2 configuration. [Audi status](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/README).

## 2. What we would replicate

The receiver advertises an additional display, accepts auxiliary stream 111, and provides H.264 to a local consumer while retaining the stock main stream. [Receiver implementation](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/src/native/libaltscreen111_mu1438.c).

The Audi renderer feeds NVIDIA video APIs, uses output 1 / layer 60 and declares context 900; its HMI bridge selects that context while a renderer heartbeat is live. Those values belong to the Audi reference. [Renderer](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/src/native/cluster_runtime.c), [HMI bridge](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/hmi/src/local/mu1438/ClusterGate.java).

This is a phone-generated second presentation, not a copy of the center display. It still requires a working head-unit-to-cluster video route. Existing native navigation on the cluster makes that route worth investigating; it does not identify the correct plane, format or control path.

## 3. Measured local compatibility

Reproduce from the already extracted reference:

```sh
python3 tools/compare_audi_interfaces.py
```

Machine-readable output: `private/analysis/audi-pcm4-interface-comparison.json` (ignored). The script reads ELF program/dynamic tables, including libraries without useful section headers. It does not execute firmware. Input hashes match the component identities in research section 13.

| Check | Local MU1476 result | What it establishes |
| --- | --- | --- |
| Receiver API/callback names checked against Audi initialization | **41/41 defined dynamic exports present** | No missing name among this checked set; not a complete ABI or behavior check. |
| Five lifecycle/control hook entry guards | **5/5 exact first-eight-byte matches** | Audi's two-instruction guards match this receiver; subsequent instructions and trampoline behavior are not validated. |
| Optional mode-state parser | Present | Candidate for lifecycle observation. |
| NVIDIA video API names used by renderer | **6/6 present** | Video open/close/configure/attributes/decode interfaces exist. |
| Graphics initialization/termination API names | **2/2 present** | `kdInitializeNV` and `kdTerminateNV` are defined in `libKD.so`. |
| HMI class and method names | Candidate classes and seam names present in raw JXE printable data | Supports further reconstruction, not verification of Java descriptors or call sites. |

The two-instruction expectations come from the [Audi receiver source](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/src/native/libaltscreen111_mu1438.c). Local symbol values below are not live addresses or MU0809 patch offsets.

| Function | PCM4 MU1476 symbol value | Opening ARM words; Audi guard matches |
| --- | --- | --- |
| `AirPlayReceiverSessionSetup` | `0x23fb8` | `e92d4ff0 ed2d8b02` |
| `AirPlayReceiverSessionStart` | `0x21f78` | `e92d4ff0 ed2d8b02` |
| `AirPlayReceiverSessionTearDown` | `0x23860` | `e92d4ff0 ed2d8b02` |
| `AirPlayReceiverSessionPlatformControl` | `0x1be90` | `e92d4ff0 ed2d8b04` |
| `AirPlayReceiverSessionControl` | `0x25be0` | `e92d4ff0 e1a06002` |

Audit correction: `objdump -T` returned an empty table for the extracted `libKD.so`, initially suggesting two exports were absent. Direct dynamic-table parsing found both. They are **not** a missing-interface blocker. The checked-in comparison tool uses the corrected method.

## 4. Why a direct Audi installation does not work

### Different exact binaries

The [Audi profile](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/profiles/mu1438.json) pins these component hashes. Each differs from our measured Porsche reference:

| Component | Audi MU1438 SHA-256 | Porsche MU1476 SHA-256 |
| --- | --- | --- |
| `dio_manager` | `bd42c9ee43021eca4db1dbd42550d86f7c7df60d350063564e985667a63bfe70` | `0d81a2180cc684a2ae4e448523815f094e3550f65f3780827f7b84be65e31706` |
| `libairplay.so` | `34503a9f18799420005d7bd40c3cbf84634c8c9030bb0dcba79d519f0227e949` | `bd0fff7f7345d58493611d637bd6bce6362ca01d3b44c5be1e320c4f5c9c3c9c` |
| `lsd.jxe` | `e43d80e7eeb803d6a7db29908562b9545e7b17138d4660ce3a6efafa974b99ce` | `1eccb000da3ef57e35dec0d59e4b0be5df375b20b41457fcd53f9bca75307a88` |

The [Audi loader delta](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/patches/dio_manager.json) converts an RPATH entry into a hook dependency and changes dependency ordering. Its [patcher](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/scripts/patch-loader.py) rejects a different source hash. A Porsche loader patch would need its own verified layout and output hash; changing the accepted hash alone is not a port.

### Porsche HMI and display routing

The [Audi Java patch](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/hmi/tools/PatchCluster.java) edits the target vehicle's converted stock classes and checks exact method descriptors and call counts. It targets `CombiBAPListener` and `DisplayManagerMIB2High`. Our JXE contains both names, plus `setCombiService`, `setMapScale`, `switchContext`, `updateCurrentPositionInfo`, `updateTurnToInfo` and `updateMapScale` strings. We have not reconstructed their bytecode, established matching descriptors or shown these are the active Macan path.

Porsche-specific `StatePorsche$StateDataMapKombiPorsche` is also present. Trace that state and the factory map's display-context requests before choosing an ownership seam. Copying Audi classes would bypass that work.

The Audi profile uses a 1440×540 video presentation on a 1440×542 panel, with Audi-specific safe areas. Those dimensions are not evidence for the Macan panel. Determine actual pixel geometry, clipping, physical size, output, layer and terminal/context mappings. Our reference has both MOST and LVDS mode scripts; shared package capabilities do not select the owner's active topology.

### Runtime behavior remains untested

Matching symbols and guards do not establish calling conventions, structure layouts, security-call behavior, decoder capacity, successful phone negotiation, or visible output. No Audi OEM binary corpus was downloaded for a full function-body diff. This comparison uses public source/profile data and the supplied Porsche reference.

## 5. Turn arrows and speed limits

Keep two outcomes distinct:

- **Information drawn inside auxiliary video:** app-dependent phone presentation. It can appear without publishing numerical speed limits or maneuver objects to the car. A map screenshot containing a sign would not satisfy native cluster integration by itself.
- **Native cluster arrows and speed-limit fields:** requires phone metadata acquisition, value translation, publication through Porsche's active cluster service, and correct clearing/handback. The Audi MHI2 reference does not supply a demonstrated complete solution for these preferred features.

The MHI2Q project is a useful RGI reference, but its advertised feature set is not proof of a portable speed-limit publisher. The local MU0809 RCC patch has DDP2 maneuver and traffic-sign operation strings; MU1476 HMI has speed-limit/sign classes. Neither establishes an active shared physical guidance link on this Macan, or that CarPlay supplies every needed field. Preserve the requirement to investigate speed limits alongside turn guidance through the same signal path; record it as unverified.

## 6. Recommended next engineering sequence

1. **Reconstruct relevant Porsche JXE classes.** Start with both Audi patch targets and Porsche's Kombi map state. Compare descriptors, superclass dispatch and actual service calls. Deliver a method/call-site map, not a replacement Audi HMI.
2. **Review the receiver hook boundary.** Disassemble complete affected functions, PLT/security call sites and symbol binding. Confirm the checked exports' ABI and examine loader ordering. Deliver a Porsche-specific hook design tied to hashes.
3. **Map the factory display route.** Inspect remaining configuration/boot partitions and native display-manager calls. Later read the vehicle's active configuration, coding, display identity and map mode. Deliver measured geometry and output/context mappings; do not infer them from Audi.
4. **Choose the actual deployment baseline.** Obtain the vehicle's MU0809 MMX/HMI set, or separately establish and back up a suitable newer baseline. Repeat interface checks on the files that will actually run.
5. **Validate in stages on the established baseline.** First original CarPlay under observation, then auxiliary negotiation/capture, then decoding to the verified cluster route, then HMI takeover and stock handback. Require visible output and reconnect/recovery evidence at each relevant stage.
6. **Add preferred metadata features after map projection.** Reuse RGI research where appropriate, but trace the Porsche maneuver/sign publisher and validate both features on the intended guidance link.

The immediate offline task is **Porsche HMI reconstruction plus receiver disassembly**. The present evidence justifies that porting work; it does not justify an installation-ready claim. Wireless and Android Auto implementation remain outside this project's scope.

## 7. Macan display geometry investigation

### 7.1 Current result

**448×448 is the strongest firmware-backed candidate for the Porsche G11 cluster video output.** This is a static finding in the supplied MU1476 firmware, not a measurement of the owner's MU0809 car or proof of the panel's visible pixel count. The boot initializer selects that second mode alongside an 800×480 main mode when invoked for `PORSCHE` / `PAG`.

| Quantity | Result | Confidence / scope |
| --- | --- | --- |
| Original cluster display diagonal | 4.8 inches | Porsche's period Macan specifications; nominal diagonal only. |
| Main PCM output mode | 800×480 | Explicit first record in the selected boot mode pair. |
| Second / cluster output candidate | **448×448** | Explicit second record in the Porsche/PAG boot mode pair, identical initializer in /50/ and /70/. Strong static evidence; owner runtime not read. |
| Display-manager second-LVDS route | `Tegra:TFTLCD0`, display ID 4 | Present in a conditional `2_lvds` configuration block; active branch on the owner's unit unverified. |
| Usable cluster map viewport and offset | **Unresolved** | Must trace HMI/compositor layout; do not assume all 448×448 pixels are available to CarPlay. |
| Physical pixel array, bezel mask and millimetres | **Unresolved** | Output timing can differ from visible panel area. No exact panel identity or datasheet acquired. |
| CarPlay stream size, safe area and frame rate | **Not selected** | Must follow actual output and layout evidence; neither 448×448 nor Audi geometry alone supplies a complete CarPlay profile. |

The [Porsche Macan price list, edition 08/2016](https://files.porsche.com/filestore/download/uk/none/po-416-catalogue-price/default/3a1696d0-2c79-11e6-9225-0019999cd470/Macan-Price-list.pdf) specifies the 4.8-inch cluster colour display. It does not state its pixel count. Marketing diagonal is insufficient to calculate CarPlay physical width/height: the visible shape and nominal diagonal convention are not established here.

### 7.2 How the 448×448 result was obtained

Decoded MMX `mifs-stage1` /50/ and /70/, plus the /50/ emergency image, using their zlib boot wrappers. The wrapper format lead is documented in the [firmware toolkit's flash-layout research](https://github.com/harman-f/MHI2-MHI2Q-MH2P-Firmware-Toolkit/blob/main/docs/MHI2_FLASH_LAYOUT_AND_RECOVERY.md). The new local reader `tools/inspect_mmx_boot.py` bounds the compressed extent, follows the nested startup header to ImageFS and validates its additive checksum before extracting selected regular files. No firmware contents were executed.

| Input | Expanded bytes | ImageFS bytes | Entries | ImageFS checksum |
| --- | --- | --- | --- | --- |
| Stage-1 /50/ | 6,865,836 | 6,759,068 | 114 | 0 |
| Stage-1 /70/ | 6,865,836 | 6,759,068 | 114 | 0 |
| Emergency /50/ | 12,742,784 | 12,636,016 | 169 | 0 |

All three have ImageFS at expanded offset `0x1a110`, derived from the nested startup header. Earlier printable occurrences of `imagefs` are incidental strings, not filesystem headers.

`usr/sbin/display-starter.sh` passes its HMI type and OEM arguments to `display_init`. The system EFS contains `HMI_TYPE=PORSCHE` and `OEM=PAG`. These connect the package's intended identity to the code branch, but do not prove the owner's live invocation.

Both Stage-1 variants contain the **same 12,611-byte** `sbin/display_init`, SHA-256:

```text
368fff98a3ac2d18d0cc38ee779792385dc04560b62a08233e53c5a3cf0d088e
```

Disassembly establishes this path in that exact ELF:

1. At ELF virtual `0x101234`–`0x101248`, compare the HMI argument with `PORSCHE`; the matching path goes to `0x1012cc`.
2. At `0x1012cc`–`0x1012e4`, compare the OEM argument with `PAG`; success sets selector register `r8=2`.
3. At `0x101088`–`0x1010ac`, copy two adjacent 76-byte mode records from file offsets `0x180c` and `0x1858` into a pair beginning at `sp+628` (`r9`).
4. The selector-2 branch reaches `0x1014d4`: pass that pair and count 2 to `0x100b60`.
5. That helper opens `/dev/nvdisp-ctrl` and submits the records with `devctlv`. The two records contain the following first three 32-bit little-endian words:

| Record file offset | Output/mode field (semantics not independently decoded) | Horizontal value | Vertical value |
| --- | --- | --- | --- |
| `0x180c` | 1 | 800 | 480 |
| `0x1858` | 0 | **448** | **448** |

The paired width/height interpretation is strongly supported by the other table entries' familiar 1440×540, 1440×542 and 1024×480 modes, the mode-selection flow and the matching main-screen configuration. The complete vendor structure definition was not recovered. Do not interpret all remaining timing fields or derive refresh rate without that definition.

The original binary has no useful code section for `objdump -d`. To inspect it, an **analysis-only copy** was given a synthetic executable section describing the existing first LOAD segment. Only that copy's section metadata changed; no vendor instruction bytes were changed or run. The disassembly includes headers/data as well as instructions; the control-flow conclusions above use the actual code region.

### 7.3 Display-manager route and smaller dimensions

A contiguous text carve from `MMX2/efs-sys/50/default/efs-system.img`, bytes `[0x8cae8, 0x8e51b)`, recovers the relevant display-manager configuration text. This is a **heuristic carve**, not a fully parsed EFS file or proof that a runtime mode is enabled. Its `2_lvds` block maps:

| Internal terminal | Driver terminal | Display ID |
| --- | --- | --- |
| `main` | `Tegra:HDMI0` | 0 |
| `2nd_lvds` | `Tegra:TFTLCD0` | 4 |

This block specifies fullscreen displays, not a fixed second-display resolution. Its 800×480 `force_res` dimensions belong to the main display, not the cluster. Likewise, `max_width=1600` / `max_height=960` are described as oversampling limits, not panel dimensions.

The application navigation resources provide further geometry candidates:

- `navigation/resources/app/po/nar/pcconfig/pcmap.conf`: map-control instance 0 is 800×480; instance 2 is **290×330**. No verified HMI association yet connects instance 2 to the owner's active cluster viewport.
- `komoviewstyle.conf`: MOST maneuver view 800×252; FPK maneuver view 210×153; RGI/RGI2 views 362×373 and 362×374. These describe maneuver/view styles and must not be promoted to panel resolutions.
- Stage-1 `cap_app.json` describes a 1024×480 LVDS **capture input** scaled to an 800×480 target. That input configuration does not identify the outbound cluster mode.

The compressed Kombi map-style XML resources were inspected; their `resolution` attributes include map-scale thresholds and are not display pixel declarations. No authoritative Macan viewport rectangle was recovered from them.

### 7.4 Cross-checks and remaining proof

The [Nickl vehicle-interface manufacturer's list](https://www.nickl.com/en/Products/CarImaging/VehicleList/) gives an 800×480 MIB main display for Macan. It also lists 240×257 and 440×440 cluster interfaces for particular **other Porsche model/year rows**. Those cannot establish this Macan's panel resolution or override the measured 448×448 boot mode. No exact Macan cluster pixel specification was verified online.

Next, reconstruct Porsche's Kombi HMI/layout methods and trace creation/cropping of display ID 4. When access to the car is available, read its actual second-output mode and native map rectangle before selecting CarPlay coded size, safe area or physical dimensions. This will distinguish the transport raster, panel pixels and usable map area. It also checks whether MU0809 uses the same mode as this later reference.

### 7.5 Local evidence and reproduction

All derived vendor material remains ignored under `private/analysis/mu1476-geometry/`. Authoritative boot inventories/extractions for this investigation are `verified-stage1-50/`, `verified-stage1-70/` and `verified-eifs-50/`. `geometry-evidence.json` records the mode words, source hash and limits; `display_init.disasm.txt` contains the analysis-copy disassembly; `displaymanager-carved.txt` is the explicitly labelled EFS carve. Earlier exploratory directories are not needed to reproduce the result.

Example (choose a fresh output directory):

```sh
python3 tools/inspect_mmx_boot.py \
  private/firmware/mu1476-aio/MMX2/mifs-stage1/50/default/mifs-stage1.img \
  --output private/analysis/mu1476-geometry/verified-stage1-50
```

The reader passed on all three real input images with zero ImageFS checksum sums. Structural checks do not authenticate the AIO package independently.
