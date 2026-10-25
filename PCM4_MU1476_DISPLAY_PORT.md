# PCM4 MU1476 display port — engineering record

Started: 2026-10-24, at the owner's request. This is the third project document, covering implementation of the MU1476 display port and its intermediate results. [Requirements and vehicle baseline](PCM4_CARPLAY_CLUSTER_RESEARCH.md) and [Audi/Porsche comparison and geometry evidence](AUDI_PCM4_METHOD_COMPARISON.md) remain separate.

## 1. Scope and current status

**The offline integration candidate is implemented, host-tested and QNX-cross-compiled; see section20 and `integration/README.md`. A functioning on-car display port is not yet established.** Work targets the supplied `MHI2_US_POG11_K5186_1`, MU1476 reference. The owner's car remains MU0809 unless separately confirmed otherwise. No reference firmware, native library, collector or modification has been run on a vehicle.

The first milestone is to display a controlled video source on the verified Porsche cluster route, then integrate auxiliary wired CarPlay video and Porsche HMI ownership. Wireless is excluded. Native turn guidance and speed limits remain preferred later integration work on the car's guidance path.

### Work delivered in this session

| Artifact | Purpose / status |
| --- | --- |
| `profiles/pcm4-mu1476-display.json` | Selected output0/displayable60/context900 and initial448x448 presentation; generated native header and private package use this profile. Runtime unverified. |
| `native/pcm4_mu1476_display.h` / `.c` | Original renderer core with an injected NVIDIA API table: initialize, open, configure, set geometry, submit a complete access unit, close. No vendor code included. |
| `tools/audit_mu1476_display.py` | Hash-pinned static recheck of native argument, lifecycle, visibility and output-enumeration evidence, import calls, error string and six video exports. Refuses other binaries. |
| `tools/capture_pcm4_display_state.sh` | Future manual QNX collector using only display-manager queries `gs`, `gd`, `gc`. Not run on a unit. |
| `tests/test_pcm4_display.c` | Host mocks for native lifecycle, geometry packing, frame bounds and failure cleanup. |

A QNX Docker toolchain is established on `home` under ~/dev/pcm4-qnx-toolchain.
Native target binaries, a patched HMI addon and a private integration package are
now built. Older sections below record earlier milestones; section20 is the
current implementation status. Nothing has been installed on a car.

## 2. Concrete incompatibility discovered: stream type

The first instruction-level review found a reason the existing Audi renderer cannot simply be rebuilt with a smaller resolution.

In the [current Audi renderer](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/src/native/cluster_runtime.c), the 24-byte open argument places the value **8 at byte offset 16**, in a field its local declaration calls `buffers`.

In our Porsche MU1476 `libnvss_video.so`, that position is checked as a **stream-type selector**. Only zero takes the decoder-open path; nonzero follows an error path associated with the literal `Invalid Stream Type`. Changing only width and height would retain this failure.

Reference SHA-256:

```text
58d19e127e77b0973149fe83d7c44fd973fd76ab57fb99108c9e76d3f73f5430
```

| ELF virtual address | Reviewed instruction / behavior |
| --- | --- |
| `0x26e0` | Save caller's second argument in `r9`. |
| `0x27f8` | `ldr r4, [r9, #0x10]`: read word at offset 16. |
| `0x27c8` and `0x2800` | Set `r2=0`, then compare the word with zero. |
| `0x2810` | Only the equal branch enters `0x2a00`, which opens the decoder. |
| `0x2814`–`0x2854` | Nonzero branch reports the error, closes the allocated object, returns `0x80000005`. |

**Port decision:** write zero at offset 16. Do not copy Audi's field label or its value. The observation applies to this exact Porsche binary; we have not examined Audi's actual NVIDIA library and cannot explain why the Audi source uses that value or establish its behavior there.

The error-message pointer was resolved through the function's GOT-relative literal, rather than associating an arbitrary nearby string: GOT base `0x128d0`, literal at `0x2ca4`, resulting message address `0xf340` = `Invalid Stream Type`. The audit reproduces that resolution.

The earlier 6/6 export-name match remains valid but does not establish this ABI compatibility. This is the first measured argument-level incompatibility found in this display-port phase.

## 3. Output identifiers and geometry

Keep these identifier spaces separate:

| Identifier | Evidence | Port treatment |
| --- | --- | --- |
| Boot mode's leading word | Porsche/PAG mode pair has 1 for 800×480 and 0 for 448×448. | Vendor mode-field meaning not fully decoded. Do not convert directly into another API's ID. |
| Display-manager display ID | Conditional `2_lvds` configuration assigns ID 4 to `2nd_lvds` / `Tegra:TFTLCD0`. | Candidate cluster route; active configuration unverified. |
| NVIDIA output device | `NvSSVideoOpen` accepts values 0 or 1 and matches type-derived returned IDs. Later native tracing links device0 to448x448, device1 to800x480 for the Porsche boot profile. | Conditional offline mapping; confirm active profile. **Do not pass4.** |
| Java HMI terminal | Porsche bytecode maps terminal1 to internal display4. | Selected terminal1; internal display IDs differ from NvSS output IDs. |
| Layer / context | Owner selected Audi-style displayable60 in added context900. | Selected in profile; declaration/integration and runtime validation pending. |

Native output check: `0x2a64` reads the output argument, `0x2a68` compares with 1 and `0x2a6c` accepts unsigned values ≤1. The query helper at `0x646c` enumerates `NvMediaVideoOutputDevicesQuery` results, compares the requested ID with each record and copies the matched mode's dimensions into its result. This supports querying runtime topology before selecting an output, rather than assuming enumeration order or conflating IDs.

The previously established **448×448** second-output raster remains the strongest static geometry candidate. It is not yet a verified CarPlay coded size, safe area or physical panel pixel specification. The 290×330 map-control resource remains an unassociated viewport clue. Profile fields for coded dimensions, frame rate, destination rectangle, safe area and millimetres remain null until traced or measured. Tests use synthetic dimensions and IDs solely to exercise the code.

The core permits source size and destination rectangle to differ, so later evidence can select a map viewport without changing argument packing. The current rectangle interpretation follows the Audi usage and Porsche's eight-word forwarding path; complete vendor rectangle declarations, clipping behavior and scaling remain unverified.

## 4. Reviewed native argument layout

These are provisional local declarations backed by reviewed accesses in this binary, not recovered NVIDIA headers or a full ABI proof.

### Open argument: 24 bytes

| Byte offset | Local field / handling | Evidence |
| --- | --- | --- |
| 0 | 32-bit output device | Read at `0x2a64`; accepted range 0–1. |
| 4 | Unknown/reserved; zero | Not assigned a meaning in this phase. |
| 8 | Priority candidate, byte | Read at `0x2b64`. |
| 9 | Use-layer candidate, byte | Read at `0x2b4c`. |
| 10 | Layer candidate, byte | Read at `0x2b6c`, forwarded toward window-manager connection. |
| 11–15 | Unknown/reserved; zero | No configuration inferred. |
| 16 | Stream type, 32-bit; **zero** | Explicit reject path for nonzero. |
| 20 | Unresolved word; zero | Read at `0x2b5c`, stored internally and forwarded by decode; full semantics pending. |

Priority/use-layer/layer semantics still require tracing the downstream window-manager properties. The offsets are stronger evidence than the provisional names. No layer allocator or context declaration is implemented.

### Configure and decode

Configure reads width and height at offsets 0 and 4 and a float at offset 8. The float participates in a frame-interval calculation. Decode reads an ARM pointer at offset 0, length at 4, a 64-bit timestamp at 8 and a byte flag at 16.

The library allocates **512,000-byte input-pool buffers** in `NvSSVideoOpen` and compares each submitted input length with the acquired buffer's capacity before copying. Our core caps each access unit at 512,000 bytes. This does not prove which H.264 profiles, dimensions or frame rates work, or that concurrent main/auxiliary decoding fits the device's resources.

The target header checks 32-bit pointers and the input offsets at compile time. Host mocks use the host pointer layout and deliberately do not claim to test ARM calling conventions.

### Attribute block

The library's own configure routine allocates and zeroes 64 bytes for attributes. Reviewed get/set paths agree with the following layout used by our core:

- Byte 8 enables forwarding the eight rectangle words at offsets 12 through 40.
- Bytes/words 44 through 56 relate to a separate adjustment path; our core leaves its command flag zero.
- Byte 60 enables a separate display-state command using byte 61. The core preserves the queried value but leaves the command flag zero. It does not force visibility.

The core first zeroes the block, queries existing attributes, replaces geometry and submits only the geometry update. It does not equate successful frame submission with visible pixels. Full display-state and `kdCreateStreamNV` property semantics remain work before live integration.

## 5. Renderer core behavior and limits

The caller must provide a zero-initialized state, resolved display configuration and API table. Opening validates rectangle bounds using subtraction to avoid unsigned overflow, opens video with stream type zero, configures video and applies geometry. Every initialization failure unwinds the completed stages. Close releases video and tolerates a second close. As corrected in section 10.3, NvSS owns KD initialization/termination internally; the core does not acquire a separate graphics session.

Decode accepts a caller-framed complete access unit with a timestamp; it rejects empty, null and oversized inputs. Access-unit framing and H.264 parameter-set handling belong to the future transport layer. The API call's return code is passed to the caller; reconnect, decoder restart and stock-display restoration need the future owner/controller.

This core deliberately has **no runnable target entry point yet**. It does not bind vendor libraries, verify live file hashes, claim a layer, declare/switch a context, publish a heartbeat, modify navigation labels, capture CarPlay or handle HMI ownership. Those are necessary integration stages, not implied by the host test result. The JSON profile currently records evidence and missing parameters; a future launcher must enforce its baseline and readiness checks.

## 6. Runtime evidence collector prepared

The local firmware's `dmdt` help identifies:

| Query | Reported purpose |
| --- | --- |
| `gs` | System information, including displays. |
| `gd` | Known displayables/clients, including sizes and buffers. |
| `gc` | Known contexts. |

`capture_pcm4_display_state.sh` invokes only these three queries and records date, OS and two resolution-override marker states. It rejects non-QNX hosts. It does not run `dc`, `sc`, `sb`, `dm` or screenshot commands. Queries use the standard production IPL configuration path. Actual availability, permissions and query behavior on the car are untested.

When an established shell is available on the intended baseline, manually capture stdout while the factory cluster map is visible, then in another cluster view. Compare display 4's presence and size, contexts, displayables and transitions. These queries may not reveal NVIDIA device IDs or the entire HMI layout; native tracing and HMI reconstruction may still be needed. No shell access or firmware upgrade is required to continue the remaining offline tracing.

## 7. Validation performed

From the project root:

```sh
python3 tools/audit_mu1476_display.py
clang -std=c99 -Wall -Wextra -Werror -DPCM4_HOST_TEST -Inative \
  native/pcm4_mu1476_display.c tests/test_pcm4_display.c \
  -o /tmp/pcm4-display-test
/tmp/pcm4-display-test
sh -n tools/capture_pcm4_display_state.sh
```

Results: **24/24 reviewed instruction checks passed; error-message pointer resolved; all six renderer exports present; host mock tests passed; collector shell syntax passed.** Tests cover successful lifecycle, failure at each of five initialization steps, repeated-open rejection, repeated-close handling, geometry byte packing, oversized/null/empty frames, invalid device ID, invalid frame rate and viewport overflow/out-of-bounds rejection. The same mock tests also passed AddressSanitizer and UndefinedBehaviorSanitizer. Python compilation and profile JSON parsing passed. The collector's non-QNX guard correctly rejected this host. These establish local code behavior, not QNX execution, successful decode or visible cluster output.

Ignored vendor-derived evidence:

- `private/analysis/mu1476-display-port/nvss.disasm.txt`: full local library disassembly.
- `private/analysis/mu1476-display-port/abi-audit.json`: hash, reviewed addresses, instruction words, interpretations and explicit `deployment_ready=false`.
- Geometry evidence remains in `private/analysis/mu1476-geometry/` as described in comparison section 7.

Recreate the disassembly with:

```sh
objdump -d private/analysis/mu1476-aio-app50/files/armle/lib/libnvss_video.so \
  > private/analysis/mu1476-display-port/nvss.disasm.txt
```

## 8. Next implementation steps

1. **Finish downstream native tracing.** Resolve output-device-to-terminal mapping; layer properties; open word at offset 20; rectangle and visibility semantics. Confirm the external `kdInitializeNV` lifecycle against the library's internal window-manager lifecycle before implementing a real binder/launcher.
2. **Reconstruct Porsche HMI map ownership.** Trace `StatePorsche$StateDataMapKombiPorsche`, native-map display creation and context switching. Identify the active viewport and stock-context restoration path. Names in JXE strings alone are insufficient.
3. **Complete the runtime profile from evidence.** Select the output, free layer/context, verified geometry and frame rate. Retain 448×448 as a transport candidate until runtime corroborates it.
4. **Build a controlled-source target test.** Use the correct QNX ARM SDK; bind the hash-verified library, render a known test stream, verify visible geometry and stock handback. Do not start with a modified receiver and HMI simultaneously.
5. **Integrate wired CarPlay auxiliary video and HMI ownership.** Port receiver negotiation/capture separately, feed complete access units, and implement bounded reconnect plus restoration when the renderer/session ends. Then validate native guidance and speed-limit publication as separate preferred outcomes.

The owner has selected **MU1476 as the development and eventual on-car target**, and is willing to upgrade the car before validation. Obtaining MU0809 MMX components is no longer a dependency for this port. The supplied archive remains the offline reference; the upgrade itself has not been performed or verified here.

## 9. Offline continuation: Porsche HMI route recovered

### 9.1 Reproducible reader and results

Added `tools/inspect_pcm4_hmi.py`, an original, read-only reader for the hash-pinned reference JXE. It validates the class directory and follows self-relative pointers to class headers, constant pools and methods. It inspects J9 bytecode directly; it does not reconstruct a JAR, execute Java, load a vendor JVM or modify firmware.

Format cross-checks used the [jxe2jar project's ROM parser](https://github.com/luka-dev/jxe2jar/blob/master/src/jxe.py) and [J9 bytecode definitions](https://github.com/luka-dev/jxe2jar/blob/master/src/bytecode.py). No external converter source was downloaded, installed or vendored. Actual offsets and pointer chains were checked against the local firmware. This limited reader intentionally rejects unsupported native methods and unknown instruction boundaries instead of claiming universal conversion support.

The reference `rom.classes` has a 48-byte image header and a declared extent of 61,062,752 bytes following it: total **61,062,800 bytes**. SHA-256:

```text
54de7f366f29be2bc2624d3b9d35f781c29f1dfa4ca707d1bf506c1aa4177ea7
```

Default run validates names and extents for **30,941 directory entries**, inspects **16 selected classes**, and decodes **353/353 selected method blocks**, with no class or instruction-decode errors. This validates the reader's supported structures and instruction boundaries; it is not full bytecode verification or decompilation. J9 operands are little endian and some instructions differ from standard JVM bytecode, so raw byte searches are not used to identify call sites.

```sh
python3 tools/inspect_pcm4_hmi.py
python3 -m unittest discover -s tests -p 'test_pcm4_hmi_reader.py'
```

Output: ignored `private/analysis/mu1476-display-port/hmi-methods.json`. It includes method descriptors, ROM offsets, per-method bytecode hashes, decoded instructions, resolved member references, branch/switch targets and literal Porsche context declarations. **Nine reader tests passed**, covering truncated/unknown instructions, little-endian operands, J9 interface calls, invalid CP indices, branch boundaries, backward branches, switch cases/count limits and malformed ROM extents. No firmware execution occurred.

### 9.2 Important correction to the Audi HMI port target

The actual Porsche class is:

```text
de/audi/tghu/fwhmi/porsche/DisplayManagerMIB2PorscheHigh
```

It extends **`de/audi/tghu/fwhmi/DisplayManager` directly**, rather than Audi's `DisplayManagerMIB2High`. Its class begins at ROM offset `0x1d4c430`. Therefore patching only Audi's subclass would miss Porsche's override.

`ATIPHMIActivatorEvo.registerDisplayManager()V`, method ROM offset `0x1d5821c`, selects the standard implementation when `isPorscheStd()` is true, otherwise the Bentley implementation for `isBentley()`, otherwise creates and registers **`DisplayManagerMIB2PorscheHigh`**. The Porsche construction is at method PC 89 and registration at PC 100. This is a traced factory path, stronger than the mere presence of an unused class; live selection has not been observed.

### 9.3 Terminal-to-display mapping now traced

Base `DisplayManager.getInternalDisplayID(I)I`, method ROM offset `0x1d3896c`, implements this mapping:

| Java terminal input | Internal display ID returned |
| --- | --- |
| 0 or 3 | 0 |
| 2 or 4 | 1 |
| **1** | **4** |
| Other values | 0 fallback |

Base `switchContext` calls that mapping before `DSIDisplayManagement.switchContext(III)V`. Thus **Java terminal 1 → display-manager ID 4** is now backed by actual MU1476 bytecode, matching the conditional LVDS configuration. This still does not reveal the NVIDIA output-device ID.

Porsche's `getMappedInternalContext(I)I` and `getMappedExternalContext(I)I` both return their argument unchanged. The subclass separately performs KDK-related remapping in `switchContext` before delegating to the base.

### 9.4 Porsche context declarations and hook seam

The reader recognizes **76 literal `DisplayContext(int, int[])` constructors** in Porsche's `defineContexts()V`. Relevant declarations are:

| Context | Declared displayable IDs |
| --- | --- |
| 72 | 33 |
| 73 | 22 |
| 74 | 22, 33 |
| 75 | None |
| 76 | 58 |
| 77 | 22, 58 |

These are static declarations, not a report of which context is active or which surfaces cover the visible Macan panel. The table also includes other contexts; none of these 76 declarations contains displayable 60. That makes Audi's layer 60 a candidate for further investigation, **not proof it is unused system-wide**. Context 900 is likewise not selected or allocated by our code.

Porsche override:

```text
switchContext(IILde/audi/atip/hmi/view/IDisplayListener;)V
method ROM offset: 0x1d4d380
```

Reviewed flow:

1. Consult `visibleKDKs[terminal]`.
2. If it equals 20, apply `addKDKToContext(context)` and replace the context argument.
3. At **method PC 42**, call base `DisplayManager.switchContext` with the resulting context, terminal and listener.

**Candidate hook location:** immediately before the base call, after Porsche's KDK adjustment. This preserves the stock remapping order. A future port must check the requested terminal/context and live renderer state, retain the current stock request, and restore stock routing when ownership ends. No patch has been applied to this method. All offsets are positions in the ROM image/method body, not JVM process addresses or deployable patch offsets.

`addKDKToContext(I)I` also has a branch that adds 79 for Kombi type 4 and a separate system-constant-dependent remap for cases 72–77. Those conditions must be retained; do not impose Audi's context range blindly before Porsche's own adjustment.

### 9.5 Geometry lead refined: map instance 2 is not the cluster configuration

`MapConfigFactory.createMapConfig(ILde/audi/tghu/navi/app/NavigationEnv;)Lde/audi/tghu/navi/app/map/MapConfig;`, method ROM offset `0x22a1d98`, switches on its first argument:

| Factory case | Map instance assigned | Java terminal assigned |
| --- | --- | --- |
| 0 | 0 | 0 |
| 1 | 2 | No terminal setter in this branch |
| 2 | **3** | **1** |
| 3 | **3** | **1** |

The terminal-1 paths assign map instance **3**, not 2. Therefore the earlier `pcmap.conf` dimensions **290×330 for instance 2 should not be treated as the Macan cluster viewport**. That file does not specify instance 3 dimensions. Full call paths and runtime geometry remain to be traced, but the new evidence substantially weakens that earlier viewport lead.

`StatePorsche$StateDataMapKombiPorsche` was also parsed. It is persistent map-settings data, not the compositor ownership class: its constructor delegates to the settings superclass, `init()` calls `initCommon()`, and its default intersection-zoom getter returns zero. Further display work should follow the display manager, map factory and native layout calls rather than treating this state-data class as the takeover seam.

### 9.6 Navigation publication seams verified at method level

The selected `CombiBAPListener` methods resolve these calls to `CombiBAPServiceNavi`:

| Wrapper method | Outgoing method descriptor | Invoke PC |
| --- | --- | --- |
| `updateCurrentPositionInfo()V` | `updateCurrentPositionInfo(Ljava/lang/String;)V` | 21 |
| `updateTurnToInfo()V` | `updateTurnToInfo(Ljava/lang/String;Ljava/lang/String;)V` | 12 |
| `updateMapScale()V` | `updateMapScale(IZIIZ)V` | 37 |

`setCombiService(Lde/audi/atip/interapp/combi/bap/navi/CombiBAPServiceNavi;)V` is present at ROM offset `0x210762c`. These are now resolved method/call-site findings, not string-presence guesses. They support a future cache/gate/restore bridge but do not by themselves implement native maneuver objects, speed-limit publishing or phone metadata acquisition.

### 9.7 Native window-manager trace extended

The static NVIDIA audit now checks **31 instructions** and resolves five call targets through validated ARM PLT/GOT relocation mappings:

| Native address | Resolved import / meaning |
| --- | --- |
| `0xe424` | `kdInitializeNV` in window-manager connection setup |
| `0xe4d0` | `kdCreateStreamNV` |
| `0xe4e8` | `kdSetStreamPropertyivNV`, property **0x1789**, argument points to the layer word carried into the connection |
| `0xe30c` | `kdSetStreamPropertybvNV`, property **0x178f**, true on the layer-validity path |
| `0xe3a8` | `kdTerminateNV` in connection teardown |

The library therefore manages its own KD lifecycle internally. The core's additional external initialization follows the earlier Audi pattern; whether it is necessary and how the exact `libKD.so` handles multiple initializations remains unresolved. Do not remove or retain it in a live launcher on assumption alone. Property numeric IDs and their call paths are established; complete vendor property definitions and visibility-event behavior remain unverified.

Run `python3 tools/audit_mu1476_display.py` to reproduce: **31/31 instruction checks and five resolved import-call checks passed**, with `deployment_ready=false`. The development profile now records the traced Java terminal, Porsche class, method descriptor, candidate hook PC and context/displayable table. It leaves the NVIDIA output, chosen layer/context and viewport unresolved.

### 9.8 Next offline work after this pass

1. Trace map instance 3's dynamic window/viewport setup and the NVIDIA output enumeration to resolve geometry and output selection.
2. Finish `libKD.so` initialization/reference counting and layer visibility semantics; then connect the renderer core to a real API binder.
3. Implement the Porsche HMI bridge against the verified subclass and descriptors, with stock-request caching and restoration. Validate converted bytecode before attempting any target installation.

MU0809 comparison and an upgrade decision are no longer prerequisites. Remaining unknowns concern the MU1476 implementation and eventual hardware validation.

## 10. Offline continuation: 448×448 HMI layout and native binding

### 10.1 Independent HMI confirmation of the screen size

The traced Porsche FPK map path now reaches a concrete layout provider:

```text
MapKombiFPKPorsche.initialize()
  -> new GUIKombiFPKPorsche(...), setGUIInterface(...)
GUIKombiFPKPorsche constructor
  -> new LayoutProviderPAGMainKombi(...)
```

`de/audi/app/navi/porsche/map/gui/layout/LayoutProviderPAGMainKombi` defines:

| Method | Method ROM offset | Returned value |
| --- | --- | --- |
| `getScreenWidth()I` | `0x129b7d8` | **448** |
| `getScreenHeight()I` | `0x129b7f0` | **448** |
| `getVARect()Lorg/dsi/ifc/map/Rect;` | `0x129b7b0` | **Rect(60, 30, 348, 368)** |

The `Rect(IIII)V` constructor, method ROM offset `0x761320`, stores the four arguments into `kordX`, `kordY`, `diffX`, `diffY` in that order. Thus the navigation visible-area rectangle is **x=60, y=30, width=348, height=368**, rather than endpoint coordinates. All four `getVisibleArea` overloads in this provider delegate to `getVARect()`.

This independently supports the boot initializer's **448×448** mode. It is stronger than inferring dimensions from screen diagonal or another Porsche model. It remains a finding about this firmware branch: the complete vehicle-dependent selection of the FPK implementation has not been confirmed on the Macan.

**CarPlay interpretation:** the rectangle is a useful safe-area candidate. It does not automatically mean the H.264 destination should be reduced to that rectangle, or that every pixel outside it is physically masked. Navigation's visible-area abstraction can reserve room for labels/guidance and map content. Keep the full-screen raster, navigation visible area, video destination and CarPlay safe area as separate quantities until their consumers are traced.

The profile now records this as `hmi_geometry_evidence`; `carplay.safe_area` and `renderer.destination_rect` remain unselected. The reader recognizes and checks the exact literal-return and rectangle-construction patterns, and records them under `porsche_layout_geometry` in its report.

### 10.2 Viewport initialization is layout-driven

`CtxKombiInit.enter()V`, method ROM offset `0x2210d88`, obtains width and height from `naviMap.getGuiInterface()`, builds `Rect(0,0,width,height)` and calls `MVRequestControl.viewSetScreenViewportMaximum`. Later initialization uses the GUI's map size for `viewSetScreenViewport`.

The request wrapper resolves to `DSIMapViewerControl.viewSetScreenViewport(Rect)` and its maximum counterpart; the response wrapper stores the returned rectangle and notifies the active map context. These are actual method/call references, not guessed API names.

There is a further distinction: inherited `AbstractLayoutProvider.getMapWidth/getMapHeight` consult model columns 27/26 and fall back to screen width/height when the model does not supply a usable value. Therefore **the maximum screen is 448×448 in this provider, while the active map rectangle may still be model-driven**. The fixed visible area above does not resolve every model/cropping decision.

A separate legacy `LayoutProviderPAGKombiCayenne` contains 240×257 dimensions and `Rect(32,17,186,211)`. No incoming class reference to that provider was found in the scanned Audi map classes. Its existence is not evidence for changing this Macan target to 240×257. Model names and shared firmware branches must not be substituted for traced selection.

Reproduction now defaults to **21 selected classes / 402 decoded method blocks**, with no class or instruction-decode errors. The original 30,941-entry directory validation remains unchanged:

```sh
python3 tools/inspect_pcm4_hmi.py
```

Focused intermediate reports remain ignored as `hmi-viewport.json`, `hmi-layout.json`, `hmi-layout-chain.json` and `hmi-rect.json` under the display-port analysis directory.

### 10.3 KD initialization is a flag, not a nested reference

Reviewed the actual `libKD.so`, SHA-256:

```text
d44df0bb660b2dbcec8f2b52af45c2450fcfb93501d00481e22b675b55cb6cb3
```

At `kdInitializeNV` (`0x7c10`), an already-set global initialized flag takes the immediate zero-return path at `0x7c30`–`0x7c34`. That path does **not** increment a reference count. Initial setup sets the flag at `0x7b18`; termination reaches cleanup and clears the same flag at `0x7a84`. `kdTerminateNV` also checks an ownership field at internal offset `0x240` before cleanup.

Since NvSS's own connection setup and teardown call KD initialization/termination, the renderer core now **removes its extra external KD initialize/terminate pair**. The injected API table contains only the six NvSS functions. This avoids treating a no-op repeated initialization as an independently owned reference. It does not establish safe multi-renderer sharing within one process; the current core is still a single-owner component.

The core now has four fallible opening stages: video open, configure, get attributes, set attributes. Its mock failure tests were updated and passed. Existing stream-type-zero handling, access-unit bounds and geometry checks remain in place.

### 10.4 Dynamic binding implemented and tested with synthetic libraries

Added `native/pcm4_nv_binding.h` / `.c`. The binder accepts an explicit absolute library path, opens it with `RTLD_NOW | RTLD_LOCAL`, and resolves:

```text
NvSSVideoOpen                 NvSSVideoClose
NvSSVideoStreamConfigure      NvSSVideoGetAttribs
NvSSVideoSetAttribs           NvSSVideoDecode
```

It reports loader/symbol errors, clears the entire API table on partial failure, rejects a second bind over a live module, and releases its loader reference on unbind. Callers must close the renderer before unloading the binding. No KD entry points are bound or called separately.

This is a native integration component, **not an installer or launcher**. A future target launcher still needs exact runtime library-identity checks, established output/layer/context values, ownership/restoration, source framing and QNX ARM build validation. The JSON profile is not automatically applied by the binder. Loading a library can execute its constructors; only original synthetic host libraries have been loaded in these tests, never firmware ELF files.

`tests/mock_nvss_library.c` supplies synthetic exports for integration testing; a build variant omits `NvSSVideoClose` to test partial binding failure. `tests/test_pcm4_binding.c` covers relative/missing paths, a missing symbol, successful binding, repeated-bind rejection, open/configure/decode/close through the real core, and unload/repeated unload.

Run the consolidated host test command:

```sh
sh tools/test_pcm4_native.sh
```

Both core and binder suites passed with AddressSanitizer and UndefinedBehaviorSanitizer. Temporary host test libraries/binaries are created outside Git and removed by the runner. No target binary has been built or installed.

### 10.5 Output enumeration traced one layer deeper

Extracted `armle/usr/lib/libnvmedia.so` through the existing QNX6 reader. Both filesystem superblock checksums passed. Extracted size is 62,754 bytes; SHA-256:

```text
2ef0bec48e063e9a71bcae0f724365898059b6ad7eb5ea0c30b11df0970f9d5f
```

`NvMediaVideoOutputDevicesQuery` at `0x7cb4` branches to `0x8794`, which calls `TVMROutputQuery` and translates 16-byte lower-level records into 24-byte output records. It assigns NvMedia device IDs based on the lower-level type: reviewed cases include type `0xb306` → device 1 and type `0xb304` → device 2; the paths for `0xb301` and `0xb302` produce device 0. It also copies a lower-level output index and active-state byte separately from the device ID.

The underlying `TVMROutputQuery` in Stage-1 `libnvtvmr.so` enumerates the driver at runtime, filters records and queries modes. Thus a driver output's ordinal/index, its type-derived NvMedia device ID and the boot mode's first word are separate fields. This pass **does not establish which NvSS device ID selects the Macan's cluster**. The output still needs final downstream mapping or a later runtime query; a numeric guess would not follow from this trace.

Verified extraction of `libnvtvmr.so` uses the Stage-1 reader's new `--select` option. Source SHA-256 is `ba854f58db4190da47f41638df761feb74510d2bd3f77293da90f0df7de5991e`. `tools/make_elf_analysis_copy.py` now makes reproducible disassembly-only section metadata for section-stripped ARM ELFs, verifies that changes to original bytes are limited to ELF section-header metadata, and writes a hash/segment manifest. Its outputs must never be executed or installed. Both KD and TVMR were inspected using such copies.

Reproduction (choose fresh extraction/copy destinations if these already exist):

```sh
python3 tools/inspect_qnx6.py \
  private/firmware/mu1476-aio/MMX2/app/50/default/app.img \
  --output private/analysis/mu1476-nvmedia \
  --select '(^armle/usr/lib/libnvmedia.so$)'
python3 tools/inspect_mmx_boot.py \
  private/firmware/mu1476-aio/MMX2/mifs-stage1/50/default/mifs-stage1.img \
  --output private/analysis/mu1476-tvmr --select '(^lib/libnvtvmr.so$)'
python3 tools/make_elf_analysis_copy.py \
  private/analysis/mu1476-tvmr/files/lib/libnvtvmr.so \
  --output private/analysis/mu1476-display-port/tvmr.analysis-only.elf
python3 tools/audit_mu1476_display.py
```

The expanded audit passes **31 NvSS checks, 8 KD checks, 6 NvMedia checks and the five previously resolved import calls**. Full-library hashes are pinned in the profile. Checks reproduce reviewed instructions, not runtime behavior. Vendor-derived material stays under ignored `private/`.

### 10.6 Remaining integration work

The renderer now has a tested binding component and corrected KD lifecycle. Geometry has a specific 448×448 HMI provider and a 348×368 navigation visible-area candidate. Remaining work is to finish the selected map layout/model path, resolve the NVIDIA cluster output, verify display-state/visibility semantics and implement the Porsche HMI ownership bridge. Receiver capture/negotiation and eventual visible-output validation follow those integration pieces.
## 11. Visibility control and display-driver boundary (2026-10-25)

Work resumed in this chat after its timer completed at **01:55:00.006 EDT**.
No vendor executable was run and no vehicle was accessed.

### 11.1 Public setter polarity established through command dispatch

Completed the previously missing link from `NvSSVideoSetAttribs` to the render
thread. These addresses refer to the hash-pinned NvSS reference in the profile:

1. Public byte 60 enables the visibility command. Byte 61 is inverted at
   `0x17a0`–`0x17b0` into internal control byte 65.
2. Control helper `0x9638` copies that byte into a message payload at `0x98d4`
   and selects **command 8** at `0x98f0`.
3. Setup stores the ninth callback (index 8) at `0x8650`. Its GOT-relative
   literal at `0x8878` resolves to **`0x7280`**, the display-control handler.
   The registration routine copies the callback array at `0xcaf4`.
4. The worker indexes the array by command at `0xcf10`, passes the payload
   after its 12-byte header, and invokes the callback at `0xcf24`.
5. Handler `0x7280` stores the internal enable byte at renderer offset `0xb1`.
   For an initialized renderer it calls helper `0x7000`.
6. Helper `0x7000` takes the zero branch through
   `NvMediaVideoMixerUnbindOutput` (`0x7044`) and
   `NvMediaVideoOutputDestroy` (`0x704c`). Nonzero takes the create/bind path,
   calling `NvMediaVideoOutputCreate` (`0x7148`) and
   `NvMediaVideoMixerBindOutput` (`0x7164`) when no output exists.

| Operation | Public byte 60 | Public setter byte 61 |
|---|---:|---:|
| Request enabled/bound output | 1 | **0** |
| Request disabled/unbound output | 1 | **1** |
| Leave output state alone | 0 | ignored |

**Getter/setter asymmetry:** `NvSSVideoGetAttribs` copies internal enabled state
to byte 61 at `0x1684`–`0x1688`. Therefore copying that queried byte back with the
visibility-update flag set would invert the state. Do not treat this as an
ordinary read/modify/write boolean.

Added `pcm4_display_set_visible(display, visible)` accepting exactly 0 or 1.
It builds a fresh zeroed 64-byte block, sets only byte 60 and the inverted byte
61, and returns the vendor result. It does not query attributes, alter geometry,
close the renderer on failure, or select an HMI context. Opening still requests
geometry only; there is no automatic takeover or new visibility default.

Host tests verify both full command blocks, invalid arguments, closed state and
vendor failure propagation without losing the handle. Both native test suites
pass with AddressSanitizer/UndefinedBehaviorSanitizer. This confirms our packing
and state handling; **visible output on the car remains untested**. Binding an
output alone does not establish HMI ownership or page/alert priority.

### 11.2 Output enumeration now reaches the driver interface

Resolved the TVMR library's actual PLT/GOT relocations with reviewed first thunk
`0x2e3c`; the resolver now accepts an explicit PLT start while retaining thunk/GOT
cross-checks. Relevant calls from `TVMROutputQuery` are:

| Call instruction | Resolved import |
|---|---|
| `0x397d4` | `nvdcOpen` |
| `0x397e8` | `nvdcQueryDisplays` |
| `0x39884` | `nvdcQueryDisplayInfo` |
| `0x39964` | `nvdcQueryHeadStatus` |
| `0x399a4` | `nvdcGetMode` |

The TVMR type-conversion table at `0x57880` yields the following chain through
the already-reviewed NvMedia translation:

| Driver class | TVMR type | NvMedia device ID |
|---:|---:|---:|
| 0 | `0xb303` | fallback path 0 |
| 1 | `0xb302` | 0 |
| 2 | `0xb304` | 2 |
| 3 | `0xb306` | 1 |
| 4 | `0xb301` | 0 |

NvSS accepts only device IDs 0 and 1. The table identifies driver class 3 as the
path to NvSS device 1, **but does not yet identify class 3 as the Macan cluster**.
Connector names must not be assigned to these classes without further evidence.

Extracted Stage-1 `libnvdc.so`, 8,560-byte executable LOAD extent, source SHA-256:

```text
3385097792315eee4dcdc706210704353bd598bf2fa3b515719cc2f0a32bcfe7
```

`nvdcQueryDisplayInfo` at `0x11a0` obtains its record through a `devctl` call at
`0x11fc`, command **`0xc0141502`**, with a 20-byte request/reply buffer. It maps
reply fields to a 16-byte result: class at offset 0, head at offset 4, a
byte-derived flag at offset 8 and a final word at offset 12. TVMR uses the
offset-8 flag to filter records; its precise driver meaning remains unlabelled.
TVMR then queries head status and mode separately. Thus the output identity
depends on driver responses, not simply the boot mode's first word.

Also extracted `libnvdisp.so`, `libnvodm_disp.so` and `devg-nvrm` as driver-side
leads. The latter names `/dev/nvdisp-ctrl`; neither this string nor the ODM
library's `InitDsi2LvdsBridge` symbol resolves the class-to-connector association.
These are next tracing targets, not mapping proof.

### 11.3 Reproduction and remaining work

```sh
python3 tools/inspect_mmx_boot.py \
  private/firmware/mu1476-aio/MMX2/mifs-stage1/50/default/mifs-stage1.img \
  --output private/analysis/mu1476-nvdc --select '(^lib/libnvdc.so$)'
sh tools/test_pcm4_native.sh
python3 tools/audit_mu1476_display.py
```

Choose a fresh extraction directory if the destination exists. The updated audit
passes **49 NvSS instruction/literal checks, 8 KD checks, 6 NvMedia checks,
7 TVMR checks and 6 NVDC checks**, plus 10 NvSS import calls, six TVMR import
thunks, the output-type table and callback-address resolution. Profile hashes
now include TVMR and NVDC. `deployment_ready` remains false.

Next: trace driver class assignment or obtain a later runtime enumeration;
establish the active map model/layout; implement the Porsche HMI ownership bridge
with current-stock-request caching and restoration. The new visibility API is
one component of that handover, not a replacement for the bridge.

## 12. Controlled-test delivery folder

`controlled-test/` contains a self-contained source snapshot, QNX build script,
read-only on-unit collector, host-side identity checker and original sample
generator. `controlled-test/README.md` is the deployment and evidence procedure.
The supplied sample is generated locally under ignored `media/`; no vendor code
or firmware is redistributed. The bounded driver validates the full PC4T input
before binding NvSS, plays at most five seconds of source frames, requests hide
on exit, closes and unloads. API success is not proof of visible output. A vendor
call can block beyond the source duration; signal cleanup runs between calls.
Timestamp units remain unresolved, so test inputs use zero vendor timestamps.

**Delivery status:** collection is runnable with existing QNX shell access.
Rendering remains blocked on compatible QNX ARM32 SDK/build verification,
on-unit library identity, output/layer/priority resolution and ownership review.
The confirmation argument is only an operator assertion. There is no automatic
installer, startup change, chosen HMI context or tested vehicle binary.

The local Homebrew index has no QNX compiler formula. Existing Apple Clang,
Homebrew GCC and ffmpeg are present; no Homebrew packages were changed. Ordinary
ARM code generation does not supply QNX headers, startup objects or libc. The
[official QNX SDK 8 installation requirements](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.qnxsdp.quickstart/topic/install_host.html)
require a licensed account and Linux/Windows host; current SDK availability does
not establish compatibility with this older PCM4 runtime. Select a compatible
SDK only after checking target runtime/ABI requirements.

Validation: `sh tools/test_controlled_kit.sh` compares canonical/snapshot source
and profile, generates 20 complete Annex-B access units, exercises full playback
and cleanup with a synthetic dynamic library under sanitizers, and rejects
trailing data and unsupported output IDs. Existing renderer tests and the
hash-pinned static audit also pass. No vendor library was executed.

## 13. Toolchain investigation — 2026-10-25

Native renderer executables/shared libraries need a QNX-target compiler **and**
compatible headers, CRT/startup objects, linker and runtime libraries. Host mock
tests, evidence collection and Python firmware analysis do not require them.

Measured directly from the supplied MU1476 ELF program headers/dynamic tables:

| File | Class/machine/flags | Interpreter | Relevant dependencies |
|---|---|---|---|
| `dio_manager` | ARM32 little-endian, `0x05000002` | `/usr/lib/ldqnx.so.2` | `libc.so.3`, `libsocket.so.3`, `libm.so.2` |
| `libnvss_video.so` | ARM32 little-endian, `0x05000002` | none | `libc.so.3`, `libm.so.2`, NVIDIA libraries |
| `libKD.so` | ARM32 little-endian, `0x05000002` | none | `libc.so.3`, `libsocket.so.3`, NVIDIA libraries |

These are consistent with the candidate QNX 6.5 ARM EABI toolchain. ELF flags and
library names alone do not prove the entire OS version or floating-point PCS.
Validate a target build's loader, imported symbols and ABI before running it.

The [Audi installation/build instructions](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/INSTALL)
specify a locally built QNX 6.5 ARMv7 container, GCC 4.9, reference revision
`56a66557245af14077678cd28a83ce3a337d9e2d`. Their checked revision requires SDK
inputs. The [current toolchain repository](https://github.com/luka-dev/qnx65-armv7-toolchain)
has since changed: it contains `sdp/`, advertises container-local sysroot/CRT,
and offers GCC 4.9 and 8.5 builds without a separate external SDP installation.
Its compiler hosts are Linux amd64 and output ARMv7 EABI5/base PCS softfp with
`ldqnx.so.2`. Inspect/pin the revision actually used; current `main` must not be
silently substituted for the older Audi recipe. No toolchain was downloaded,
built or executed here yet. This corrects the earlier implication that a
separately installed official SDK is the only build route.

This Mac is Intel `x86_64`, macOS 15.7.9. Neither Docker nor Podman is installed.
[Colima](https://colima.run/docs/getting-started/) plus Docker CLI is a candidate
Linux-container host. Buildx is useful for the Audi build recipe. Keep the
toolchain in its own image and use a named Colima profile rather than changing
system compilers, shell startup files or existing Docker contexts.

Homebrew's local index has no QNX compiler formula. Its Docker/Buildx formulae
have no Intel bottle here and require source-build dependencies, including a
newer Go than the installed version. Blindly installing them could therefore
affect existing build dependencies. Prefer verified upstream Intel release
binaries in a separate tools directory, or review the full dependency plan
before a Homebrew installation. Homebrew package names alone are insufficient
to promise an installation will preserve existing packages.

The attempted Homebrew **dry run** stopped at unwritable Homebrew/cache paths
under this session's filesystem restrictions. No package was installed,
upgraded, unlinked or removed. Do not follow its blanket `chown` suggestion:
this result does not establish an ownership problem outside the sandbox.

Next: install/verify the container host without upgrading existing packages;
pin and inspect the toolchain checkout; build its C-only image; compile a tiny
ABI probe and the controlled renderer; inspect target imports and run mock
tests under QNX/QEMU before the vehicle. No QNX 8 SDK or bare-metal Homebrew
`arm-none-eabi-gcc` is selected as a replacement for the QNX 6.5 target runtime.
[QNX ARMv7 options](https://www.qnx.com/developers/docs/6.5.0SP1/neutrino/technotes/QNX_for_ARMv7_Cortex_Processors.html)
describe `gcc_ntoarmv7le`; [QNX dlopen documentation](https://www.qnx.com/developers/docs/6.5.0SP1.update/com.qnx.doc.neutrino_lib_ref/d/dlopen.html)
places dynamic loading in libc, so an assumed Linux `-ldl` is unnecessary.

## 14. Debian toolchain host established — 2026-10-25

The owner's SSH alias `home` now hosts the toolchain under the requested
`~/dev/pcm4-qnx-toolchain` directory. Docker is accessed through `sudo -n`,
without changes to group membership, daemon configuration, host packages or
existing containers. The host is Debian amd64 with eight CPUs and 23 GiB RAM;
Docker's storage volume had over 100 GiB free. Home Assistant and OnlyOffice
containers remained running during the build.

Provenance:

- Toolchain source revision: `baa45224f18ae6b13c2e9c77c6663bb8181eb6a1`.
- Docker image: `pcm4-qnx65-armv7:8.5-baa45224f18a`.
- Image ID: `sha256:6e05615103ef9062931c7503485d0bc335cfa5a744b7cfeb9cb39327074ae675`.
- GCC 8.5.0, GNU assembler 2.38, C/C++ `base-env` stage. Go/Rust not built.
- Pristine upstream checkout retained as `source/`; build context exported with
  `git archive`, preserving SDK symlinks. Local changes limit GCC to four jobs,
  assembler to two jobs, and enable five download retries. The first attempt
  hit transient Debian snapshot connection resets; the retried build succeeded.
  Upstream snapshot pinning, package verification and GNU archive hashes remain.

`tools/setup_qnx_debian.sh` reproduces setup on that host. Logs/status are
`setup.log`, `setup-first-attempt.log`, `setup.status`; the latter is `complete`.
No separate QNX SDK installation or host apt installation was needed.

`bash tools/build_pcm4_home.sh` transfers only original PCM4 source and mock
tests, runs `tools/build_pcm4_qnx.sh` in the image, and retrieves outputs. The
compile container runs as the login UID with a read-only root, temporary `/tmp`,
no network and no extra capabilities. Firmware/vendor libraries stay on the
Mac. The build uses C99, `-O0 -marm -mfloat-abi=softfp -static-libgcc`; native
QNX/ARM/layout compile-time checks stay enabled, without `PCM4_HOST_TEST`.

Successfully cross-compiled the controlled renderer, display/projection/binding
mock tests and synthetic complete/missing-symbol libraries. The local driver is
`controlled-test/bin/pcm4-render-test` (16 KiB, ignored by git), SHA-256:

```text
1f2cd12f089b7d77687e1b372adafdb788cf98f2ce84e058963ef1f87ddc99fd
```

`tools/check_qnx_target.py` checked this executable against extracted MU1476
Stage-1 `proc/boot/libc.so.3`, SHA-256
`455c05c655883792b745332d5a723965cc27954dfa5c4a2ea817492d97eaad5c`.
Results: ELF32 little-endian ARM EABI5 executable; interpreter
`/usr/lib/ldqnx.so.2`; sole direct dependency `libc.so.3`; **all 29 strong import
names present**. Two weak frame-registration imports are reported separately.
Reports, binary checksums and image provenance are under ignored
`private/remote-qnx/` locally and `pcm4-project/qnx-out/` remotely.

Four upstream static self-checks passed: VFP multiply-accumulate encodings,
absence of recursive float math stubs in static/shared libstdc++, and a C++
header/math/mutex/chrono compile probe. The script's extra per-variant loops
were skipped because its generic image tags were not created; the explicit
image above was tested. Log: `selftest.log` on the remote host.

**Limits:** no target binary or mock test has executed under QNX/QEMU or in the
vehicle. Import-name presence is not symbol-version, calling-convention or
runtime proof. Output/layer/priority and Porsche HMI ownership are still
unresolved; the collector remains the next on-car action. Do not interpret the
presence of a compiled binary as approval to guess those parameters.

## 15. Staged package and auxiliary/HMI integration status

`controlled-test/TESTING.md` gives the ordered vehicle procedure: Stage 0 runs
only original/synthetic QNX mock tests; Stage 1 collects topology/library
evidence; Stage 2 is controlled real rendering after output/layer/geometry
review; Stages 3/4 describe the pending receiver and live HMI tests.
`tools/package_controlled_test.py` packages an explicit allowlist of 20 files
plus a SHA256 manifest. Current archive is ignored
`private/releases/pcm4-mu1476-controlled-test.zip`; binaries, synthetic libraries
and the original sample are included. Results/vendor libraries are excluded.
Archive CRC and per-file content checks passed. No on-unit stage has run yet.

Original offline integration components now exist:

- `native/pcm4_aux_stream.c/.h`: parses bounded avcC SPS/PPS configuration and
  converts a complete, already-decrypted AVCC access unit into Annex-B. Waits
  for IDR, prepends parameter sets at IDR, rejects stale session generations,
  oversize/truncated NALs and unsupported configuration extensions. It does not
  negotiate AirPlay, decrypt iPhone traffic or reconstruct TCP messages.
- `native/pcm4_handover.c/.h`: policy callbacks acquire projection, show/hide
  output and restore the latest stock context. Only terminal 1/nav contexts
  72–77 can request takeover, with connected state and a recent accepted frame.
  Other-page requests, disconnect and frame timeout release ownership. Failed
  cleanup is retried; faults prevent immediate reacquisition. This is an
  in-process policy core, not a live Java bridge or independent watchdog.
- `native/pcm4_projection.c/.h`: connects conversion to the renderer and
  handover policy through injected HMI callbacks. Malformed current-session
  input ends the session and releases output; stale-session input cannot end
  a newer session. Vendor timestamps remain a caller-adaptation requirement.

`sh tools/test_pcm4_projection.sh` passed host sanitizer tests for framing,
IDR/session isolation, ownership/restoration, clock regression, failure/retry
behavior and integrated mock decode/show/restore. These tests also successfully
cross-compiled for QNX, but have not executed on QNX. The compiled projection
mock is included in Stage 0; no receiver preload or HMI patch is packaged.

Next implementation boundaries: Porsche-specific receiver advertisement,
SETUP/start/teardown hook and stock-AES adapter; bounded screen-message framing;
negotiated geometry and actual timestamp semantics; then the post-KDK Porsche
Java seam preserving stock listener/request state and higher-priority pages.
A watchdog on the HMI side must restore stock even if the renderer process
dies. Those target adapters remain unfinished; native policy tests cannot
establish their correct behavior or make the live bridge operational.

## 16. Screen receiver and Porsche HMI addon implementation — 2026-10-25

This section supersedes section 15's unfinished framing/Java implementation
items. It does not establish a live iPhone-to-cluster path.

### Incremental native receiver

`native/pcm4_screen_receiver.c/.h` now reconstructs stream-111 Screen messages
across arbitrary fragmented or coalesced input. It bounds each body at 512000
bytes, reads the 128-byte header and little-endian body length, passes plaintext
avcC configuration into the projection core, and decrypts video bodies through
an injected callback before AVCC conversion and NvSS decoding. Video before
configuration still traverses the decrypt callback, preserving continuous
cipher state; it cannot make the display ready. Invalid current-session input
or a truncated final message ends projection; obsolete generations cannot
terminate a newer session. Known control messages do not confer readiness.
Vendor timestamp units remain unresolved, so the adapter currently submits
zero, matching the controlled renderer rather than guessing a conversion.

Host ASan/UBSan tests passed for single-byte fragmentation, coalesced messages,
all 127 partial-header endings, pre-configuration crypto, malformed/oversize
input, decrypt failure, session isolation, decode and context restoration.
The crypto fixture is synthetic, not a test of Apple's actual encryption.
The updated test also cross-compiled using the established QNX Docker toolchain
on `home`; the Stage 0 projection mock was refreshed. No QNX execution occurred.
There is still no live socket listener, stock AES binding/key interception,
receiver advertisement or SETUP/teardown hook.

### Actual Porsche Java patch built offline

`hmi/src/local/pcm4/ProjectionBridge.java` implements selection at the actual
Porsche post-KDK display-manager seam. `hmi/tools/PatchPorsche.java` checks the
exact converted vendor class, inheritance, synchronized method and unique
superclass call before emitting an addon. It saves the raw request on entry,
preserves the listener, and restores through Porsche's normal method using the
raw value so KDK mapping happens once. Terminal 1 navigation contexts 72–77 are
the only takeover candidates; other pages retain control. A worker checks a
bounded, fresh readiness marker independently of the renderer and serializes
replay against stock requests. Missing configuration disables selection.
A stock request for the candidate projection context stops ownership.

The marker protocol is `PCM4READY1`, positive session generation and positive
decoded-frame count, newline terminated. Freshness is three seconds. Its native
publisher is not connected yet. The existing native handover callbacks and this
Java worker are separately tested components: a single runtime coordinator must
connect them without two competing context owners.

The user-supplied JXE was converted privately on the authorized Debian host:

- JXE SHA-256: `1eccb000da3ef57e35dec0d59e4b0be5df375b20b41457fcd53f9bca75307a88`.
- jxe2jar revision: `9eeb45bbf14bf8afe3452c7be96a4d1f0206a286`;
  30941 classes converted.
- Input Porsche class SHA-256:
  `551aab856826a16448c57a65705f5581f7e07aa3bab7823774351cbc56fbe0d3`.
- Patched Porsche class SHA-256:
  `75c081aa0bc67bbb0cacbc5dd8efb8ffae8d2c70e90ab9a4c59ea718e9ba58f1`.

`tools/build_hmi_debian.sh` builds Java 1.4-target bridge classes inside pinned
Java 8 Docker, checks input/dependency hashes, and creates a fresh private addon.
Tests passed for default-disabled behavior, actual converted interfaces,
non-idempotent raw/KDK remapping, listener preservation, automatic takeover,
other-page precedence, stale/malformed/disabled markers and context collision.
The patched Porsche class links under JVM verification without initializing OEM
code. This proves neither J9 runtime compatibility nor installation viability.
Latest remote build: `hmi-tools/builds/bridge.xfIPF0hr` under the existing workspace.
Local addon and verification reports are under ignored
`private/analysis/mu1476-hmi-bridge/`; vendor-derived classes are excluded from
Git and the controlled-test ZIP.

The extracted stock `eso/hmi/lsd/lsd.sh` offers loader leads: it appends JARs from
`$BASE_DIR/lsd/jars`; development mode can use `hmi.zip` and launch the main class
instead of the JXE. These are firmware observations, not proof that an addon
will override the resident JXE class. Class precedence, context registration,
startup and rollback remain to be validated before an installer is produced.

### Independent firmware trace: display IDs are not hardware planes

The requested agent recorded addresses, hashes and evidence in section 8 of
`AUDI_PCM4_METHOD_COMPARISON.md`. NvSS's “layer” parameter selects a logical
DSI displayable; physical hardware-plane properties are a separate interface.
Factory enums include 33 `KOMBI_MAP_VIEW`, 58 `GOOGLE_EARTH_KOMBI_MAP_VIEW`,
59 `EXTERNAL_SMARTPHONE`, and **60 `HUD_MAP_VIEW`**. Therefore Audi's hardcoded
60 is not evidence that this identifier is globally free on Porsche. Porsche
stock smartphone context 78 references displayables 16/59. The scanned static
context tables did not allocate context 900, but dynamic allocation remains
possible; 900 is a synthetic test value, not an approved vehicle setting.

Output device 1 remains plausible but unproven. A traced driver reply maps
internal type 1 to public type 1 and type 2 to public type 3. **Subsequent
correction:** both the client and driver use devctl `0xc0140502`; the prior
mismatch claim was an ARM immediate-decoding error, corrected in comparison
section8.4. Enumeration-to-connector tracing continues.

The intended path is a separate decoder surface/displayable in a projection
context, replacing the active stock navigation composition temporarily. It does
not redirect the stock decoder into a new buffer or overwrite the stock map
framebuffer. A stock surface can obscure the new one if its composition remains
active above it; logical ID 60 alone cannot establish plane ordering. Correct
context registration and on-car visibility tests are still required.

Next: confirm collected device/layer/context identities; connect the native
readiness publisher to the Java owner; implement and review the Porsche-specific
receiver/AES lifecycle; validate J9 addon loading and controlled installation.
Actual negotiated geometry, warning priority, decode latency, timeout restoration
and output visibility still require vehicle evidence. Native arrows and speed
limits remain follow-on work on the separate guidance path.

## 17. Routing requirements and graphics model — 2026-10-25

For the next display milestone, distinguish logical contexts/displayables from
physical outputs/planes. The stock context map has been recovered; finding more
context names alone will not establish the NvSS-to-LVDS route.

The current public Audi MU1438 runtime uses fixed output 1, priority 100,
displayable 60, context 900 and 1440×540 at 30 fps. It declares the context using
`dmdt dc 900 60`; renderer readiness is communicated through a RAM-file heartbeat.
The Java bridge handles terminal 1/navigation contexts 72–77 and changes context
while that heartbeat is fresh. This is a fixed target profile rather than
startup topology discovery. Sources:

- https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/carplay/src/native/cluster_runtime.c
- https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/main/hmi/src/local/mu1438/ClusterGate.java

Porsche requirements and outstanding evidence:

1. Resolve the active output route end-to-end: NvSS output -> NvMedia/TVMR/NvDC
   identity -> selected display driver/head -> TFTLCD0/second LVDS. Continue
   static tracing of driver selection/loading, including the EFS variant.
   The earlier devctl mismatch was a decoding error and is now corrected;
   it is no longer a blocker.
2. Establish a usable logical displayable and priority. ID 60 is factory-named
   HUD_MAP_VIEW, and absence from Porsche's literal context lists does not prove
   that it has no dynamic owner. Trace native registration/validation and
   priority handling; preserve factory registrations.
3. Verify the Porsche `dmdt dc` declaration path and native context semantics,
   including where contexts live and whether any cleanup/reset is required.
   Context 900 is a candidate only. No declaration command has run on the car.
4. Select source/destination rectangles from recovered 448×448 transport and
   Porsche layout evidence, then confirm actual clipping. Panel geometry and
   an iPhone safe area are separate quantities.
5. Connect renderer readiness and Java ownership, preserving raw KDK requests,
   listener state and non-navigation pages. This can be tested first with the
   synthetic video source without waiting for iPhone session interception.

Logical graphics model (an abstraction, not a proven call-by-call native stack):
producers render/decode into pixel surfaces; streams are associated with logical
displayables; DisplayManager contexts select displayables for a terminal/output;
KD/NVIDIA composition routes eligible surfaces to available hardware planes;
the display controller scans out to a physical connector and panel. Control
(context changes) and pixel data (surface buffers) are separate paths. Some
surfaces may be scanned out as hardware overlays without copying into a single
combined CPU framebuffer. Exact composition/plane assignment remains unproved.
Java terminal 1 -> internal display ID 4 is proven from bytecode; the physical
TFTLCD0 association is conditional configuration evidence; NvSS output 1 is not
yet linked conclusively to that route.

A read-only runtime collection can replace broad probing by identifying the
actual topology, contexts, streams and component hashes. A subsequent short
synthetic-video test is still needed to establish visible scanout, clipping,
priority and restoration. Firmware analysis can reduce these tests but cannot
prove the owner's current coding, loaded variants or physical panel behavior.

## 18. Selected projection identities — 2026-10-25

Owner decision: follow the Audi separate-surface method using logical
**displayable60 in projection context900**. Porsche's native factory name table
already defines60 as HUD_MAP_VIEW. Context900 is our added composition; it is
not claimed to be a stock Porsche declaration. This decision supersedes earlier
statements that no displayable/context has been selected. Runtime ownership and
visibility have not yet been tested.

Both `profiles/pcm4-mu1476-display.json` and the controlled-test reference profile
now select renderer.layer=60 and ownership.context=900 with displayables[60].
The HMI launch setting is explicitly `-Dpcm4.cluster.context=900`; the bridge
continues to default to disabled. Context declaration and readiness coordination
still need implementation before this becomes a live handover.

The completed native investigation in `NVSS_OUTPUT_CONTEXT_TRACE_MU1476.md`
traces the Porsche448x448 LCD mode to **NvSS output0**, unlike Audi output1.
It follows boot mode records into driver objects and enumeration, then through
NvDC/TVMR/NvMedia. This is an offline-proven mapping conditional on the Porsche
mode profile being active. The conditional route records output0; the executable
renderer output remains unspecified pending the active topology check.
No vehicle action or automatic rendering has been enabled by this selection.

## 19. Consolidated development configuration — 2026-10-25

After owner review, the canonical profile and controlled-test reference both
select **NvSS output0, displayable60, context900 with[60], Java terminal1**.
This supersedes section18's unspecified executable-profile output. The profile
retains vehicle_runtime_verified=false and the conditional boot-profile evidence.
Java launch properties select context900 and the existing readiness/disable
paths; they are recorded configuration, not automatically applied by Java.
Priority, coded dimensions/fps, destination rectangle and iPhone descriptor
geometry remain unspecified rather than inventing values not yet chosen.

The staged documentation and local ZIP were synchronized. Still missing:
acknowledged context declaration; native readiness publisher and one Java owner;
Porsche auxiliary-advertisement/SETUP/socket lifecycle and stock-AES binding;
validated J9 loading/startup/rollback; selected geometry/priority; then target
execution and actual visibility, priority and restoration tests. Crypto caller
and derivation seams and conditional output enumeration are now traced offline,
so they are no longer discovery blockers. This update installs nothing on a car.

## 20. Offline integration candidate completed — 2026-10-25

This section supersedes earlier missing-implementation lists. Sources are
original project code; derived vendor outputs remain ignored under private/.
The selected route is output0/displayable60/context900=[60], terminal1.
Initial presentation is448x448 at30fps, priority100, full-canvas destination;
phone safe area348x368@(60,30) follows the recovered Porsche layout. These are
explicit development values and remain physically unvalidated. Physical
millimetres are omitted unless supplied through the hook's paired environment
settings; no measurement was invented.

### Implemented pipeline

- `pcm4_airplay_hook.c`: retains actual stock session association at the reviewed
  security setter, captures the successful16-byte master key, advertises a cloned
  auxiliary descriptor while preserving stock arrays/features, delegates stock
  SETUP then adds the111 response/listening port. Uses exact symbol-offset and
  prologue guards. It supports SETUP-before-START and SETUP-after-START, requests
  showUI for an established auxiliary session, rejects missing/mismatched keys
  and duplicate111 descriptors, filters private-only teardown from stock and
  filters111 out of mixed teardown. Its conservative main110 teardown policy
  also releases auxiliary output. Keys are wiped and never logged.
- `pcm4_crypto.c/.h`: uses reviewed stock derivation and continuous CTR API with
  aligned512-byte opaque storage, in-place updates and cleanup.
- `pcm4_arm_hook.c/.h`: startup-only guarded ARM trampolines, RW-to-RX text
  transitions and partial-install rollback. No live injection/thread suspension
  or automatic MAP_FIXED fallback. Denied text permissions disable projection;
  failure to restore executable permission exits126 to avoid executing RW-only
  stock text. Target cache/permission/constructor behavior remains untested.
- Hook's bounded single-session TCP worker decrypts Screen messages and sends
  them only to127.0.0.1:19820. No firmware decoder is opened by the hook.
- `pcm4_projection_runtime.c`: separate NvSS owner, exclusive loopback receiver,
  per-connection generation, configuration/IDR gating, target NvSS identity check
  before loading, decoder lifecycle, timeout and signal cleanup. Timestamp0 is
  retained until target timing semantics can be measured. No continuous decoder
  recovery is attempted after a failed stream; it waits for a new connection.
- `pcm4_ready.c/.h`: atomic frame-driven marker publisher. External-owner mode
  in projection.c bypasses native context takeover entirely. Java is the sole
  context owner; accepted frames publish readiness, errors/timeout withdraw it.
- Java patch extends the normal factory declaration with900=[60], disables on
  collision, supplies public getDisplayables lookup for the added context, and
  preserves raw KDK request/listener state for restoration. A private listener
  absorbs projection acknowledgements: the base method otherwise applies%79 to
  immediate listener contexts. A worker controls nav72..77 only and restores
  after stale markers independently of the renderer.
- `LoadProbe.java` checks the marker field and links the patched class without
  initializing OEM HMI. The patched converter class is normalized from49 to48
  after rejecting Java5 features; addon classes are all Java1.4-compatible by
  file version. This remains a desktop/J9-loader candidate, not target proof.

### Configuration and packaging

`tools/generate_pcm4_profile.py` generates native/pcm4_profile.h from the canonical
JSON; the controlled-test profile stays identical. The private package builder
hash-guards dio_manager, changes only its unused Jenkins-build RPATH into an
absolute hook dependency, preserves original dependency order and emits a
conditional VMOPTIONS addition to the stock lsd.sh. The stock jars-before-JXE
mechanism supplies the addon; no full converted30941-class runtime is substituted.

`integration/install.sh` validates source payloads and original target hashes,
snapshots all originals before publication, installs disabled, and rolls back
partial publication. Restore refuses later edits, clears activation, requests
Java release, restores originals and removes added files. It does not remount,
restart, kill services or contact the car. `integration/control.sh` exposes
explicit probe, renderer, enable and disable operations. The renderer is started
manually for testing rather than installed as an unattended service.

`private/releases/pcm4-mu1476-integration-r4.zip` is the current private stage3/4
candidate; the original Stage0–2 ZIP is refreshed separately. The private r1/r2 builds are superseded. No proprietary payload is added to Git.

### Verification and limits

Host ASan/UBSan tests passed framing/session isolation, external-owner readiness,
marker format/removal, crypto-adapter state continuity, actual CF retaining
fixtures, descriptor preservation, missing/mismatched keys, start/setup ordering,
auxiliary setup/teardown, and zero leaked fixture objects. Actual localhost
runtime tests with a synthetic NvSS shared library passed receive/decode,
atomic markers, reconnect, frame timeout and SIGTERM cleanup. SHA256 matches
Python hashlib across padding/block boundaries.

Pinned Java8 Docker tests passed declaration extension/collision, private lookup
and listeners, raw KDK restoration, page precedence, malformed/stale/disable
markers, class linking and the loader probe. The addon remains disabled without
configuration. All native targets cross-compiled with the QNX ARM toolchain;
strong imports were checked against supplied vehicle libc/socket exports and
ARM EABI5 softfp metadata. No target executable or vendor ARM code ran here.

An isolated-filesystem test passed installer preflight, original-hash rejection,
rollback after deliberately failed partial publication, refusal to overwrite a
later edit, and complete restoration. Archive CRC and per-file contents passed.
These checks do not prove real QNX memory protection, constructor ordering,
J9 initialization, iPhone negotiation/crypto or physical scanout.

The remaining gates require target evidence: verify active library/profile and
attachment; run QNX mocks; test disabled dependency loading and J9 probe; inspect
actual context900 declaration; then enable wired projection and test geometry,
main-screen behavior, page/warning priority and recovery. Native turn arrows and
speed limits remain the separately scoped guidance follow-on, not supplied by
this video/handover candidate.

### Final offline build record — 2026-10-25

The final private integration archive is `pcm4-mu1476-integration-r2.zip`.
Its SHA256 is
`0046cd2e61d973045a662519baa771525b578f161c2935917ce393d082c04e56`.
The refreshed Stage0–2 archive contains21 components plus SHA256SUMS, including
four QNX mock executables; its SHA256 is
`270816f197abe593c3828f14ca14d278d62fa5c442e65e3017f5177b394e201b`.
Both archives are under ignored `private/releases/`.

The final hook is `28ca9a3c3d43ff9d6f16c5a4e51a7c824a68355b33184fce4f50400a61717f16`;
the Java addon is `7a36427f2c76b1af4fcb560affd0da6f2c32078613d8c1a8f1438a81fc54d85a`.
The four integration static-import reports are in `private/remote-qnx/` as
`runtime-imports.json`, `hook-imports.json`, `verify-imports.json`, and
`hook-mock-imports.json`. Each passed, and each recorded binary hash matches the
final build. `loader-delta-check.json` under the private HMI analysis directory
confirms that the loader delta changes only the RPATH string/tag and preserves
all original dependency names and order.

The final installer regression also executes only our generated VMOPTIONS block
with activation absent/present, rejects a stale activation flag before creating
a backup, and verifies that rollback removes temporary publication files.
Original vendor startup scripts and ARM executables are never executed by these
host checks. Python/shell syntax and `git diff --check` passed. The private
archives were checked against current binary bytes and the canonical profile.
No files were installed on a vehicle, and no commit or push was performed in
this implementation pass.

### On-car onboarding guide and documentation refresh

`controlled-test/ONBOARDING_TEST_GUIDE.md` now provides the complete vehicle
visit: baseline, QNX mocks, read-only route/identity evidence, bounded pattern,
disabled installation/loading, wired iPhone integration, page handover/recovery,
disable/restore and results to return. It distinguishes phone-side showUI from
PCM context900=[60] and makes actual vehicle pass/fail gates explicit.

The same guide is included in both private archives. Integration r3 supersedes
r2 with documentation only; installed payload files and binaries are identical.
The controlled kit now contains22 components plus SHA256SUMS. Historical r2
hashes above describe that earlier build; the refreshed archives are:

- `pcm4-mu1476-integration-r3.zip`: `624b6a27812e2b6c8e5a8cab1f24c90e3ca6ab9a4c292109d0e518197f9dd60a`
- `pcm4-mu1476-controlled-test.zip`: `5ccc1a1bb0aafa8960d102e7cb7312e63a743d92b9452756933d7c3b74ac468b`

Both archives passed CRC, every SHA256SUMS entry and exact guide-content checks.
The installer/rollback regression passed against r3. No car was contacted.

### GPLv3 and prerelease preparation

Project code is now declared GPL-3.0-only, with the full LICENSE and upstream
NOTICE.md. Both test ZIPs carry the license, attribution and explicit use-at-own-
risk / untested-on-a-real-car warning. Integration r4 supersedes r3 with no
installed-payload changes; the controlled kit now contains24 files plus its
checksum manifest. PRERELEASE.md lists public test/source assets separately from
the OEM-derived private integration ZIP. Packaging checks verify license bytes,
warnings, all manifest hashes, CRC and r3/r4 installed-payload equality. The
installer/rollback regression passes against r4. No on-car testing was performed.

## Local unattended continuation (superseded)

`sh tools/codex_pcm4.sh ...` starts a CLI session with
`model_auto_compact_token_limit=330000`. It does not change the already-running
chat or the global configuration. Model context limits can constrain the effective
threshold.

`python3 tools/schedule_pcm4_offline.py --dry-run` previews the next 03:00 in
America/New_York. Without `--dry-run`, it starts a detached, one-shot timer that
invokes `codex exec` with that same threshold, workspace-write sandbox and no
interactive approvals. The prompt continues offline output/visibility analysis
and HMI ownership work, preserving existing changes. It does not flash, contact a
car, commit or push. It starts a new run using these documents as its handoff.

The detached timer was launched for **2026-10-25 03:00 EDT**, PID **80405**, then
cancelled by owner request in favor of a current-session wait. Its state file was
marked cancelled; the sandbox prevented signalling the detached PID, so process
termination could not be confirmed. The current-session 03:00 timer was later
successfully cancelled with Ctrl-C and replaced with a 01:55 timer, which
completed at 01:55:00.006 EDT on October 25. No detached continuation is desired.
Historical state, timer
output, execution events and the final response are under
`private/scheduled-runs/20261025T030000-0400/`. Process inspection from the current
sandbox returned permission denied, so continued liveness and eventual execution
have not been verified. This is not a system-installed scheduler: reboot or
process cleanup can remove the timer; sleep postpones execution until wake.
Authentication, network access and CLI startup permissions must still succeed at
run time. The state file records completion or failure if the timer reaches the
execution stage. From an ordinary local terminal, `kill 80405` cancels the timer
while that PID still identifies this process. Do not schedule the same start time
twice; the scheduler refuses an existing run directory.
