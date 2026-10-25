# Porsche MU1476 HMI bridge — offline build

`ProjectionBridge.java` is original Java 1.4-compatible code for the actual
converted Porsche display-manager interface. `PatchPorsche.java` generates an
addon containing this bridge and ONE patched Porsche class. No JXE is rewritten,
no factory map buffer is overwritten. A private application-level installation
candidate now exists; see `integration/README.md`. Nothing is installed on a car.
The addon is **disabled by default**.

The design selects a separately declared projection context only for terminal 1
navigation requests (post-remap contexts 72–77). It preserves both the raw
entry request and listener; restoration re-enters Porsche's normal method with
the raw value so KDK mapping happens exactly once. Other-page requests pass
through. The worker uses the display-manager monitor to avoid racing a newer
stock request. Reentrant worker requests do not replace the cached stock one.

The patcher accepts only converted class SHA-256
`551aab856826a16448c57a65705f5581f7e07aa3bab7823774351cbc56fbe0d3`,
checks direct inheritance, the synchronized signature, exactly one superclass
call and its argument setup. It saves the raw context at entry and injects
selection before the post-KDK superclass call. Original branches are retained;
it also extends `defineContexts` with900=[60], adds private-context displayable
lookup, and uses a private listener for projection acknowledgements. Factory
listeners are retained for restoration because the base class reduces callback
context values modulo79. ASM recomputes stack maxima and analyzes modified
methods. The converter's v49 synthetic-accessor class is normalized to v48 only
after rejecting Java5 features; bridge/probe classes also target Java1.4.
The converted JAR is private, derived from the supplied hash-pinned JXE using
jxe2jar revision `9eeb45bbf14bf8afe3452c7be96a4d1f0206a286`.

Build on the authorized Debian workspace after provisioning the private inputs:

```sh
ssh home 'bash ~/dev/pcm4-qnx-toolchain/pcm4-project/tools/build_hmi_debian.sh'
```

`tools/build_hmi_debian.sh` uses the pinned Java 8 container, private converted
stock JAR/OSGi interface dependency, and hash-checked ASM libraries. Source is
under remote `pcm4-project/hmi/`. Each build gets a new `hmi-tools/builds/bridge.*`
directory, recorded by `hmi-tools/latest-build.txt`. No existing addon is replaced.
The generated addon contains a vendor-derived class; keep it under ignored
private storage, not git or the Stage 0–2 test archive.

Selected future launch properties are recorded in
`profiles/pcm4-mu1476-display.json` under `ownership.java_system_properties`.
The JSON is not read by Java directly. The package builder emits a conditional
VMOPTIONS block in the staged stock startup script; it supplies the values as
JVM `-D` options only after explicit enablement. Current runtime configuration interface:

- `pcm4.cluster.context`: explicitly reviewed context in range 256–65535;
  missing/invalid configuration disables the bridge. This range is a project
  constraint, not evidence that every ID is supported/free. The owner selected
  **context900 containing displayable60** for this port. With the property
  supplied, the patched defineContexts appends the declaration before the stock
  DSI call. A conflicting declaration leaves stock intact and disables takeover.
  The bridge remains disabled when the property is absent.
- `pcm4.cluster.marker`: absolute renderer heartbeat path, default
  `/tmp/pcm4-cluster-ready`. The required ASCII content is `PCM4READY1`, positive
  decimal session generation and decoded-frame count, each followed by newline.
  A marker older than three seconds, malformed content, a clock reversal, or
  the disable file causes release. The separate native runtime now publishes
  this protocol atomically after accepted decoding.
- `pcm4.cluster.disable`: override marker, default `/tmp/pcm4-cluster-disable`.

The native renderer must publish readiness only after accepted decoding and must
withdraw it on session failure/termination. The Java worker polls independently
of the renderer, so loss of heartbeat can restore stock even after renderer
death. It cannot recover an HMI JVM/thread that has itself failed. A stock use of
the configured candidate context stops ownership rather than overriding it.

The private bundle includes startup/rollback and a LoadProbe that checks patched
class selection without initializing OEM HMI. Desktop JVM tests pass; actual J9
loading, native declaration acknowledgement, warnings/page priority and map
restoration remain vehicle checks. Stock
guidance labels/arrows/speed limits are not modified by this bridge.
