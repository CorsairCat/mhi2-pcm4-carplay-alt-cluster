# PCM4 MU1476 iPhone auxiliary-session hook investigation

Date: 2026-10-25. Scope: supplied Porsche MU1476 reference; wired CarPlay only.
This is an offline investigation, not an installed receiver or a vehicle-tested
hook. No vendor instructions were patched or executed.

## Result

The most important previously missing crypto seam is now supported by concrete
Porsche instructions: the stock session-security setter calls CBC initialization
with the master key, and the stock screen setup derives a distinct key/IV from
that same master key plus a 64-bit connection identifier. Its screen receiver
then uses the exported CTR implementation in place. This supports reusing the
unit's crypto functions, without implementing a new key-derivation algorithm.
Actual symbol interposition, loader ordering and lifecycle behavior remain
unverified on QNX.

## Inputs and reproducibility

Porsche `private/analysis/mu1476-aio-app50/files/eso/lib/libairplay.so`:
SHA-256 `bd0fff7f7345d58493611d637bd6bce6362ca01d3b44c5be1e320c4f5c9c3c9c`.
Addresses in this document are ELF virtual addresses for this exact file. They
are neither live process addresses nor deployable patch offsets.

An analysis-only copy was made with `tools/make_elf_analysis_copy.py`, exposing
unchanged executable bytes through synthetic section metadata, then disassembled
with `xcrun llvm-objdump --triple=armv7-none-eabi -d`. The verified ARM PLT starts
at `0x15938`; `tools/audit_mu1476_display.py`'s `plt_imports` resolved 962 thunks
by checking their GOT addresses against dynamic relocations. This avoids naming
a call solely from a guessed PLT stride.

Private artifacts: `private/analysis/mu1476-session-hooks/` contains the analysis
copy, disassembly, PLT map, instruction-word evidence JSON and upstream snapshot.
These remain ignored and are excluded from test ZIPs and Git.

## What the Audi reference does

The [receiver source at commit d5b51417185fa0afefe726fa2df82815693723fd](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/d5b51417185fa0afefe726fa2df82815693723fd/carplay/src/native/libaltscreen111_mu1438.c)
advertises auxiliary capability through server information, clones the main
display descriptor, and supplies secondary-display geometry, identity and URL
metadata. It preserves stock session setup before adding a stream-111 response
and its listening port. It captures the master key from CBC initialization only
when called by the stock security setter, derives auxiliary key/IV using the
requested connection ID, and decrypts video with stock CTR functions. Hooks cover
setup/start/teardown and control. Teardown distinguishes the private auxiliary
stream, the main screen, and the whole session. These are upstream design
choices, not demonstrated Porsche behavior.

The retrieved file is SHA-256
`28a8dcbdf1943ea7c11cdfa5c160bb17718e8f16a5c70dbed6490055e2d97fd7`;
the commit-pinned and initially retrieved main copies matched byte-for-byte.
Its comments and identifiers mix older MU1438 names with later target work.
Port decisions must use reviewed instructions and profile identities rather
than infer compatibility from the filename.

## Proven Porsche master-key seam

`AirPlayReceiverSessionSetSecurityInfo` is exported at `0x25ab4`, size `0x6c`.

| Address | Observed operation | Consequence |
| --- | --- | --- |
| `0x25ab8` | Compute session + `0xbc` | CBC context is embedded in session. |
| `0x25ac4`, `0x25ac8` | Preserve input r2/r1 | Key/IV input pointers survive cleanup. |
| `0x25ad0–0x25ae4` | r3=0; r0=CBC context; r1=first pointer; r2=second pointer | Matches decrypt-side `(context,key,IV,encrypt)` call shape. |
| `0x25ae8` | Call `0x17108`, verified `AES_CBCFrame_Init` | Exact candidate interception call; ARM return address `0x25aec`. |
| `0x25aec–0x25af0` | Test returned status and skip success path on nonzero | Capture must require successful initialization. |
| `0x25af4–0x25b04` | Copy 16 bytes from first input to session + `0x1c8` | Stock retains master-key material. |
| `0x25b08–0x25b14` | Copy 16 bytes from second input to session + `0x1d8` | Stock retains the corresponding IV. |

The copies call verified `memcpy` PLT `0x15ea8`. A receiver hook can use an exact
reviewed return-address check for this call rather than a broad security-function
address range. It must delegate to the original function and avoid logging key
material. The copy observations establish data flow, not permission to read a
live session by fixed offsets. Capturing function arguments avoids that extra
session-layout dependency.

The PLT has a dynamic relocation for `AES_CBCFrame_Init`; this provides a
candidate preemption point. It does not prove that the QNX loader resolves that
entry to a preload, particularly with the process's actual dependency order.

## Proven per-screen key and cipher path

The stock screen-setup helper at `0x23c9c` contains:

- `0x23d78`: call verified `CFDictionaryGetInt64` (`0x16358`); its returned
  low/high words are moved into r2/r3 at `0x23d7c/0x23d80`.
- `0x23d84–0x23d90`: reject a zero identifier in this path.
- `0x23d94`: r0 = session + `0x1c8`, the same key storage written above.
- `0x23d98`: r1 = 16.
- `0x23d9c`: place two output pointers on stack.
- `0x23da0`: call `AirPlay_DeriveAESKeySHA512ForScreen` through verified
  PLT `0x15b54`.
- `0x23da4–0x23db0`: pass those output pointers to the stock screen's
  `AirPlayReceiverSessionScreen_SetSecurityInfo` via PLT `0x1673c`.
- `0x23db4–0x23dd8`: wipe both temporary 16-byte outputs.

The derive export starts at `0x2a2fc`. Its entry saves r2/r3 as a 64-bit value,
retains r0/r1 as key pointer/length, and loads the two output pointers from the
incoming stack. This independently supports the Audi-compatible prototype:

```c
void derive_screen(const void *master, size_t master_length,
                   uint64_t connection_id, unsigned char key[16],
                   unsigned char iv[16]);
```

The getter key resolves to the factory CF string object at `0x935e0`, whose
inline text at `0x935e8` is **`streamConnectionID`**. The address calculation is
`r5 = (0x23cc4 + 8) + [0x23f90] = 0xaa6c0`, then
`r1 = r5 + [0x23f9c] + 408 = 0x935e0` (32-bit arithmetic). This closes the
request-key identification independently of the upstream prototype. The target
compiler must use the correct ARM calling convention for the aligned 64-bit
argument; do not substitute two ad hoc pointer-sized arguments.

`AirPlayReceiverSessionScreen_SetSecurityInfo` at `0x276d8` calls verified
`AES_CTR_Init` at `0x27700` with context = screen object + `0xc8`. Existing valid
CTR state is finalized before replacement. The exported initializer `0x2e804`
uses r0=context, r1=key, r2=IV, invokes `aes_init` and `aes_encrypt_key128`, copies
16 IV bytes to context + `0xf4`, and resets fields at +`0x114` and +`0x118`.
`AES_CTR_Final` at `0x2e4b8` explicitly clears **284 bytes**. Thus Audi's aligned
512-byte opaque storage is large enough for the reviewed implementation;
284 is an observed implementation extent, not a general QNX SDK ABI promise.

At `0x284a0–0x284b0`, stock processing calls verified `AES_CTR_Update` with
r0=context, r1=payload, r2=payload length, r3=the same payload pointer. This
establishes that in-place operation is used by the firmware. The export at
`0x2e500` reads and updates buffered cipher position at context +`0x114`, which
supports maintaining one continuous CTR state across video messages. Resetting
CTR for every frame would not match this stateful interface.

## Lifecycle and setup dispatch

The existing five eight-byte entry guards still match Audi's expected guards:

| Export | ELF address | First words |
| --- | --- | --- |
| `AirPlayReceiverSessionSetup` | `0x23fb8` | `e92d4ff0 ed2d8b02` |
| `AirPlayReceiverSessionStart` | `0x21f78` | `e92d4ff0 ed2d8b02` |
| `AirPlayReceiverSessionTearDown` | `0x23860` | `e92d4ff0 ed2d8b02` |
| `AirPlayReceiverSessionPlatformControl` | `0x1be90` | `e92d4ff0 ed2d8b04` |
| `AirPlayReceiverSessionControl` | `0x25be0` | `e92d4ff0 e1a06002` |

These initial instructions are register/stack operations without PC-relative
loads. That makes the first eight bytes a plausible relocation unit, but does
not establish safe runtime installation, page protection changes, instruction
cache synchronization or concurrent-thread safety.

The reviewed SETUP dispatch at `0x24268–0x24290` handles 100/101 separately,
compares screen type against **110**, and only then invokes screen helper
`0x23c9c`. A non-110 value branches back to the dispatch loop at `0x24210`.
There is no dedicated 111 branch in this reviewed dispatcher. This supports a
separate private auxiliary receiver rather than assuming stock creates another
screen automatically. The corresponding teardown dispatcher compares 110 at
`0x23960` and 100 at `0x23968`; private-111 cleanup must be coordinated separately.
Full mixed-stream/default-dispatch behavior needs broader review before adopting
upstream's exact filter/delegation policy.

## Remaining information and implementation seams

| Area | Established | Still needed |
| --- | --- | --- |
| Crypto interception | Exact CBC caller, key length and stock derive/CTR data flow | Process-level symbol resolution; session association; no stale key crossing sessions. |
| Advertisement | `AirPlayCopyServerInfo` exists at `0x1ef54`; needed CF exports present | Complete Porsche dictionary call/ownership review; appropriate UUID, geometry, safe area, physical size and display capability fields. |
| SETUP | Stock main-screen dispatcher and setup entry found | Mutable response ownership, aux response/error path, listener port/address binding on wired CarPlay link. |
| Lifecycle | Setup/start/teardown/control symbols and plausible entry windows | Complete hook fail-closed behavior, concurrent setup/disconnect ordering, retain/release rules and mixed-stream teardown policy. |
| Deployment | Target compiler exists; no vendor API name blocker found | Porsche `dio_manager` dependency ordering and private hash-guarded loader plan; actual QNX hook/crypto smoke tests. |
| Output | Incremental Screen parser and decoder integration already exist | Connect the socket worker and real CTR callback; reject negotiated sizes beyond supported limits; single owner for Java readiness/context selection. |

A useful implementation order is: (1) hash-guarded binding/crypto adapter and
synthetic known-input tests; (2) passive observation hook with no advertisement
changes, proving call ordering without persisting keys; (3) reviewed auxiliary
advertisement/SETUP/socket lifecycle; (4) integration with existing framing and
readiness. Runtime observations should record event order and identifiers, never
session keys or decrypted private content.

No vehicle test has been performed. The findings remove uncertainty about
several local ABI/data-flow details; they cannot prove the iPhone accepts our
secondary-display descriptor or that the hook loads in the actual process.
