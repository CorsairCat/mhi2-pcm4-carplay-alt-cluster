# MU1476 native output and projection-context trace

Offline investigation, 2026-10-25. Target: Porsche PCM4/Macan, MU1476 reference AIO firmware. This report supplements `PCM4_MU1476_DISPLAY_PORT.md` and `AUDI_PCM4_METHOD_COMPARISON.md`. No firmware executable was run, patched or installed. Addresses below are original ELF virtual addresses; display_init record locations explicitly labelled file offsets are exceptions.

## 1. Result and confidence

**New: the firmware now supplies a conditional, end-to-end mapping to NvSS output device 0 for the Porsche 448×448 LCD path.** This follows the boot mode records through the driver initializer and query response, rather than equating an enumeration index, Java display ID, or mode-field value with the NvSS ID.

| Porsche mode record | Driver internal type | Driver query class | TVMR output type | NvMedia/NvSS device |
|---|---:|---:|---:|---:|
| word 0 = 0; 448×448 | 1, labelled LCD by driver | 1 | `0xb302` | **0** |
| word 0 = 1; 800×480 | 2, labelled HDMI by driver | 3 | `0xb306` | **1** |

This is **proven for the inspected code and selected Porsche mode pair**, conditional on that profile being active and attached in the car. Driver labels describe its software categories, not external consumer connectors: calling the 800×480 path HDMI does not establish that the car exposes an HDMI socket. Runtime head assignment, attachment/active state, the current driver build, and visible physical routing still need a read-only check before enabling presentation. The result does not make Java display ID 4 equal NvSS device 0; they belong to different namespaces.

**Correction:** the previously reported `libnvdc`/`libnvdisp` command discrepancy does not exist. Both use `0xc0140502`. The earlier `0xc0141502` value arose from decoding an ARM MOVW instruction incorrectly.

**Context/displayable status:** `dmdt dc` and the Java DSI declaration path are present. They carry integer IDs and allocated lists. Reviewed native paths do not establish a maximum logical displayable of 60, but do not yet prove that a newly chosen ID such as 61 is accepted throughout configuration, live stream registration, context membership, and composition. Neither context 900 nor displayable 60 has been established collision-free or suitable for production use. Java composition details are tracked separately in `JAVA_COMPOSITION_MU1476.md`.

## 2. Firmware identities

SHA-256 values identify original firmware bytes, not the analysis-only ELF copies.

| Component and local extraction | SHA-256 |
|---|---|
| `mu1476-aio-routing/files/armle/lib/libnvss_video.so` | `58d19e127e77b0973149fe83d7c44fd973fd76ab57fb99108c9e76d3f73f5430` |
| `mu1476-nvmedia/files/armle/usr/lib/libnvmedia.so` | `2ef0bec48e063e9a71bcae0f724365898059b6ad7eb5ea0c30b11df0970f9d5f` |
| `mu1476-tvmr/files/lib/libnvtvmr.so` | `ba854f58db4190da47f41638df761feb74510d2bd3f77293da90f0df7de5991e` |
| `mu1476-nvdc/files/lib/libnvdc.so` | `3385097792315eee4dcdc706210704353bd598bf2fa3b515719cc2f0a32bcfe7` |
| `mu1476-display-driver/files/lib/libnvdisp.so` | `767ffced9ed2e9c3a320809ef419f545a5d0a80fe15923291c306927591f0ca1` |
| `mu1476-geometry/eifs-50/files/lib/libnvdisp.so` | `dc29996cda7d558235daa213093e3128397e84f68fc1d05527d4b9b6b9470312` |
| `mu1476-display-driver/files/lib/libnvodm_disp.so` | `cc1530ae3d677980f4c10f5c7051a7b510f948baa3c7d121c87a77f003f3b533` |
| `mu1476-geometry/verified-stage1-50/files/sbin/display_init` | `368fff98a3ac2d18d0cc38ee779792385dc04560b62a08233e53c5a3cf0d088e` |
| `mu1476-aio-mifs50/files/eso/bin/apps/displaymanager` | `c1c212e18282a65c6181835c139eb129ab18a629c6809568cf3a7072e997d22e` |
| `mu1476-aio-routing/files/eso/bin/apps/dmdt` | `df0429315432828d416a70025d3ad0ab0caced1893c14d242c65358b22906e6e` |
| `mu1476-aio-routing/files/eso/lib/libdmdt_core.so` | `6f4ec550f4dc9aeee4a69099ec3c2b40984218953796ee61a6204d573d498e13` |

Paths in this table are relative to ignored `private/analysis/`. The two libnvdisp files have different whole-file hashes, but the specific initializer and query instructions listed below are identical at the same addresses in both. This is not a claim that the complete libraries are identical or interchangeable.

## 3. Output enumeration: all namespaces kept separate

The existing NvSS/NvMedia trace shows NvSS enumerating outputs and selecting by the **type-derived NvMedia device ID**, rather than assuming that returned element 0 or 1 is the desired screen. NvSS only accepts the device-ID cases 0 and 1 on the inspected path.

`NvMediaVideoOutputDevicesQuery` at `0x7cb4` branches to `0x8794` and translates lower-level 16-byte TVMR records into 24-byte records. A TVMR output index and its active-state byte are separate from the NvMedia device-ID field. The reviewed conversions include `0xb302` → 0 and `0xb306` → 1.

`TVMROutputQuery` calls these actual imports, resolved from ARM PLT/GOT relocations, not thunk order guessed from symbol order:

| Call site | Import |
|---:|---|
| `0x397d4` | `nvdcOpen` |
| `0x397e8` | `nvdcQueryDisplays` |
| `0x39884` | `nvdcQueryDisplayInfo` |
| `0x39964` | `nvdcQueryHeadStatus` |
| `0x399a4` | `nvdcGetMode` |

Its table at `0x57880` maps driver classes 1 and 3 to `0xb302` and `0xb306` respectively. Attachment and head-status checks can filter outputs; a known class does not guarantee a usable live output.

### 3.1 Actual client command and driver response

`nvdcQueryDisplayInfo` starts at `0x11a0`. It prepares a 20-byte buffer and calls devctl at `0x11fc`. Original instruction words are:

```text
0x11dc: e3001502 = movw r1, #0x0502
0x11e4: e34c1014 = movt r1, #0xc014
                     command = 0xc0140502
```

The `1` immediately before `502` in the first word is the destination-register encoding, not an additional immediate nibble. Re-disassembly with `xcrun llvm-objdump --triple=armv7-none-eabi` confirms this. There is no evidence here for another ioctl ABI or mismatched driver variant.

Driver handler `0x18ee0` dispatches this command to `0x19028`. It checks the requested display index against the enumerated count, obtains a display-object pointer from the driver list (`0x19044`–`0x19048`), then reads its internal type at `0x1904c`:

```text
internal type 1 -> branch 0x191f8, retain class 1
internal type 2 -> branch 0x191f4, set class 3
other          -> invalid argument path
```

`0x191fc` writes the class into the response. The attachment bit comes from the display object's attached-head pointer at offset `0x80`; the head index comes from that head object's offset `0x320`, or -1 when detached (`0x191f8`–`0x19228`). `libnvdc` converts the wire record into class at result+0, head at +4, attachment at +8, and another word at +12. Thus the formerly unnamed flag is an attachment indication in this driver implementation. It is not the device-ID field.

### 3.2 Boot records really do reach the driver's internal display type

The inspected Porsche/PAG selection in display_init is already identified by the `0x1012cc`–`0x1012e4` branch to selector 2. Its mode-pair setup copies two 76-byte records via `0x101088`–`0x1010ac`. Original file offsets and first fields are:

```text
file 0x180c: [1, 800, 480, ...]  (19 words / 76 bytes)
file 0x1858: [0, 448, 448, ...]  (19 words / 76 bytes)
```

The sender helper `0x100b60` builds IOVs containing the count and 76-byte mode records. At `0x100c3c`/`0x100c40` it constructs **`0x80040503`**, and sends it through devctlv at `0x100c50`.

The same command is built in the driver at `0x18f74`/`0x18f78`, dispatching to `0x19078`. This handler validates count 1–5 and uses `resmgr_msgread` at `0x19118` to read count×76 bytes from request offset20 into driver state+`0x288`. At `0x19190`, it invokes initializer `0x16368` with the DDK instance, that record pointer, and the count.

In the initializer, `0x16490`/`0x16498` preserve each record's first word in an internal mode record. The second pass retrieves the same field at `0x165e8`–`0x165f0` and performs the explicit conversion:

```text
0x165f4 cmp r3,#0; 0x165f8 moveq r1,#1
0x16600 cmp r3,#1; 0x16604 moveq r1,#2
0x16644 str r1,[display_object,#0]  (base+0x820+152*slot)
```

The display objects are selected from available slots; their slot numbers must not be substituted for type or NvMedia device ID. `NvDdkDispListDisplays` at `0x14214` filters configured objects and returns their exact pointers as base+`0x820`+152×slot (`0x14290`–`0x1429c`); this is the same object layout written by the initializer. The initializer therefore connects the boot mode's leading word to the exact field queried by `0x1904c`, closing the previously missing link.

The driver text-dump path independently labels objects with type1 as `LCD` and its other path as `HDMI`: `0x1bef0`/`0x1bef4` branch to `0x1c188`; the literal-backed strings resolve to `LCD` at `0x1e778` and `HDMI` at `0x1d820`. This reinforces the code-level route, while the previously traced stock display-manager `TFTLCD0`/cluster configuration supplies a separate HMI configuration association.

### 3.3 Remaining output check

The useful offline conclusion is now **device0 is the first candidate backed by a complete conversion chain for this profile**, rather than a blind two-output experiment. A read-only runtime capture should still report device ID, returned ordinal, native type/class, attached flag, head and mode together, plus current boot-profile selection and library hashes. No output should be changed merely to discover which screen it drives. The exact runtime head number is assigned by attachment state and is not established just by the static class conversion.

## 4. Native context declaration and logical displayable handling

### 4.1 What `dmdt dc` demonstrably does

The dmdt command path calls imported `_ZN14CDMDTInterface14declareContextEiPii` at `0x10180c` through thunk `0x100fe0`. Its parsed command structure supplies context ID, list pointer and count at offsets8/12/16. It frees the temporary list after the call. This is a request path, not proof that a context became usable on a particular screen.

The implementation in `libdmdt_core.so` at `0x314c` constructs an IDL integer array, copies the count entries and submits request selector **123** through its remote proxy (`0x31a8`–`0x31d4`). The inspected method has no caller-visible branch proving a successful native composition change. `getContexts` exists at `0x4194` and should be used for before/after enumeration rather than treating a CLI exit status as acknowledgement.

The native Java/DSI entry `CDSIDisplaymanagement::declareContexts` at `0x14be4c` walks declaration structures and queues `CDeclareContextEvent` objects through `0x138b04`. Context ID/list/count are copied by `0x137c44` into event fields +`0x14`, +`0x18`, +`0x1c`. That helper allocates count×4 bytes. No upper-bound check against logical ID60 is present in this copying function. Event posting at `0x138be4` is asynchronous, so it is not itself a rendering acknowledgement.

The event's destructor at `0x137cb0` frees its owned list (+`0x18`, call `0x137cf4`). This establishes request-list lifetime only. It does **not** establish context deregistration, replacement semantics, reboot persistence or collision handling in the downstream context registry. Those remain unverified in this pass.

### 4.2 Displayable 60 is not a proved free graphics plane

A logical displayable ID is stream metadata used by displaymanager to decide membership in the selected composition. It is not a Tegra hardware plane number or a guaranteed z-order. In the native NvSS open structure these are distinct controls: output device, priority byte, layer-valid byte and logical displayable byte. Logical IDs passed through this NvSS ABI are byte-sized; the wider display-manager/Java integer API does not prove every integer can be supplied identically through NvSS.

The KD property ID `0x1789` carries the live stream's logical displayable identifier. Native handler `0x14582c` reads it through the KD query at `0x1459ec` and stores it in the stream object at `0x145a08`. The common path at `0x1458e0` calls `0x10a55c`, iterates display wrappers, and calls `0x108f10`. That function dispatches membership handling to the active context's virtual method at offset`0x2c`, then marks composition dirty when appropriate. No `ID<=60` test was found in this reviewed notification path. This is not a complete proof of stream insertion or ID acceptance.

Separately, configuration helper `0x146c60` maintains a dynamically allocated array of 12-byte displayable records (count+`0x40`, pointer+`0x44`, capacity+`0x48`). It compares IDs and rejects duplicates, logging `displayable with id: %d already configured` around `0x146d70`; its success path appends a record and grows capacity. No maximum-ID check was found inside that helper. Its identified direct caller at `0x1210f8` belongs to configuration parsing. It must **not** be cited as live KD stream registration or as proof of duplicate live-stream semantics.

Consequently, 61 or another unused byte-sized ID is technically plausible but not established. Existing contexts containing IDs above60 already disprove using an enum-name table ending at60 as a universal maximum, but neither that observation nor an unused static name proves native resource availability. Before choosing an ID, enumerate configured displayables, live streams, all declared contexts and their memberships, and inspect the complete insert/update path. Also verify the selected NvSS priority byte separately from logical displayable numbering.

### 4.3 What has not yet been established

- The final context-registry handler after queued declaration, its duplicate-context policy, whether a declaration replaces or merges, and its removal/reset API.
- The complete live-stream insertion path and collision rules for two live producers with the same logical ID.
- A validated private context ID and logical displayable ID, and a priority that is suitable alongside the stock map's actual hardware-plane configuration.
- Context persistence across owner shutdown, service restart, Java reinitialization and full reboot. Process-local event memory and destructor behavior are not persistence proof.
- Automatic native-to-Java readiness/handover. Context declaration alone does not notify Java that an auxiliary stream is decoding and ready.

## 5. Recommended next actions

1. Record the new **conditional device0 mapping** in the renderer profile as evidence, keeping deployment disabled until read-only runtime validation confirms the active profile, mode and attachment. Do not copy Audi's hardcoded output1 into Porsche.
2. Continue native dispatch tracing from the declaration event/request123 into the registry. Establish lookup, insert/update, duplicate handling and teardown before choosing context900 or any substitute. Couple that with the separate Java composition report.
3. Enumerate the installed displayables/contexts and current stream associations. Select a private, representable logical ID only after collision checks; prove membership and visibility with the controlled test. Do not increment priority as a substitute for understanding map-plane ownership.
4. Keep renderer-ready, context-ready and HMI ownership as separate states. Cache the current stock request, request the auxiliary-only composition only after decoding/presentation readiness, and restore the cached request on disconnect/error. The output mapping alone does not implement this policy.

## 6. Reproduction notes and ignored artifacts

`private/analysis/mu1476-output-context/` contains original-byte hash/instruction/record evidence in `route-proof.json`, ARMv7 disassembly of the NvDC and EFS driver copies, dmdt/core disassembly, PLT resolution records and exploratory string-reference lists. Exploratory xrefs are search aids, not independent proof; control flow was reviewed at the addresses cited above.

The repository's `tools/make_elf_analysis_copy.py` adds analysis section metadata to section-stripped QNX ELF files without changing original code bytes. All such copies are analysis-only and must never be executed or deployed. Disassemble with an ARMv7 triple so MOVW/MOVT immediates decode correctly. Existing `tools/compare_audi_interfaces.py:file_offset` translates original virtual addresses to file offsets. `route-proof.json` records original instruction words in both driver variants and the original 19-word Porsche mode records for independent checking.

## Implementation follow-up — 2026-10-25

The analysis above records the evidence and open questions at discovery time.
The offline integration candidate now implements guarded session hooks, a
separate renderer and readiness publisher, context900=[60] declaration, Java
handover, and private packaging/rollback. See
[the current implementation and validation record](PCM4_MU1476_DISPLAY_PORT.md)
and [the test procedure](integration/README.md). These implementations do not
convert conditional firmware findings into vehicle runtime proof.
