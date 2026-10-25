# PCM4 MU1476 display port — engineering record

Started: 2026-10-24, at the owner's request. This is the third project document, covering implementation of the MU1476 display port and its intermediate results. [Requirements and vehicle baseline](PCM4_CARPLAY_CLUSTER_RESEARCH.md) and [Audi/Porsche comparison and geometry evidence](AUDI_PCM4_METHOD_COMPARISON.md) remain separate.

## 1. Scope and current status

**The first offline native renderer core is implemented and host-tested. A functioning on-car display port is not yet established.** Work targets the supplied `MHI2_US_POG11_K5186_1`, MU1476 reference. The owner's car remains MU0809 unless separately confirmed otherwise. No reference firmware, native library, collector or modification has been run on a vehicle.

The first milestone is to display a controlled video source on the verified Porsche cluster route, then integrate auxiliary wired CarPlay video and Porsche HMI ownership. Wireless is excluded. Native turn guidance and speed limits remain preferred later integration work on the car's guidance path.

### Work delivered in this session

| Artifact | Purpose / status |
| --- | --- |
| `profiles/pcm4-mu1476-display.json` | Reference identity, 448×448 raster candidate and conditional route; unresolved runtime fields explicitly null. Development profile, not consumed by an installer. |
| `native/pcm4_mu1476_display.h` / `.c` | Original renderer core with an injected NVIDIA API table: initialize, open, configure, set geometry, submit a complete access unit, close. No vendor code included. |
| `tools/audit_mu1476_display.py` | Hash-pinned static recheck of 24 reviewed ARM instructions, the referenced stream-type error string and six video exports. Refuses other binaries. |
| `tools/capture_pcm4_display_state.sh` | Future manual QNX collector using only display-manager queries `gs`, `gd`, `gc`. Not run on a unit. |
| `tests/test_pcm4_display.c` | Host mocks for native lifecycle, geometry packing, frame bounds and failure cleanup. |

No QNX SDK/compiler is installed in this workspace. There is no target binary, automatic startup change, patched HMI, receiver hook or installation package from this phase.

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
| NVIDIA output device | `NvSSVideoOpen` accepts values 0 or 1, then queries output devices and matches their returned IDs. | Unknown which maps to the cluster. **Do not pass 4.** |
| Java HMI terminal | Audi bridge uses terminal 1 for its cluster. | Porsche mapping unverified; profile null. |
| Layer / context | Audi uses layer 60, context 900. | No Porsche selection made; profile null. |

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

The caller must provide a zero-initialized state, resolved display configuration and API table. Opening validates rectangle bounds using subtraction to avoid unsigned overflow, then initializes graphics, opens video with stream type zero, configures video and applies geometry. Every initialization failure unwinds the completed stages. Close releases video and terminates the core's graphics session, and tolerates a second close.

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
