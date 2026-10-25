# Porsche PCM4 CarPlay Cluster Projection

> **Experimental prerelease — use at your own risk. This software has not been
> tested on a real car.** It may disrupt PCM or cluster operation and require
> recovery. It is provided without warranty; offline test results do not establish
> vehicle safety, compatibility or reliability.

A development project to bring a separate, iPhone-generated CarPlay navigation
view to the instrument cluster of a Porsche Macan with PCM4/MHI2, while keeping
the main PCM CarPlay display usable.

The project adapts the auxiliary-display approach explored by Audi MHI2 and
related projects to Porsche-specific firmware, graphics routing and HMI logic.
It uses wired CarPlay and the existing navigation display connection. It requests
an additional presentation from the phone rather than mirroring the center screen.

**Status: offline-tested integration candidate. No on-car validation has been
completed.** Native code cross-compiles for QNX ARM; host tests, desktop Java
verification, static firmware import checks and isolated installer/rollback tests
pass. Real QNX/J9 loading, iPhone acceptance, graphics routing and visible cluster
output remain vehicle checks. This repository does not establish compatibility
with every PCM4 unit or firmware release.

## Target and scope

| Item | Development baseline |
| --- | --- |
| Vehicle | 2017 Porsche Macan, PCM4/MHI2 |
| Original owner-reported firmware | `MHI2_US_POG11_P3300`, MU0809, hardware051 |
| Inspected development reference | `MHI2_US_POG11_K5186_1`, MU1476, supplied AIO reference |
| Intended test baseline | MU1476; installed component identities must match the inspected reference |
| Connection | Wired CarPlay |
| Candidate video profile | 448×448, up to30fps; provisional phone safe area348×368 at(60,30) |
| Candidate graphics/HMI route | NvSS output0, logical displayable60, terminal1, context900=[60] |

Output0 follows a conditional offline boot/driver trace. Displayable60 and
context900 are selected development values; active attachment, registration,
geometry and visibility must be checked on the car. Analysis of the AIO reference
is not a recommendation to install that firmware package.

The first goal is auxiliary map video with automatic navigation-page handover
and restoration of the latest stock request. Native turn arrows and speed-limit
fields are preferred follow-on work. They require a separate Porsche guidance
data path and are not implemented by this video candidate. Wireless CarPlay and
Android Auto implementation are outside this project's scope.

## How it works

```text
iPhone navigation app
  -> stock CarPlay session with an advertised auxiliary display
  -> encrypted type111 H.264 stream
  -> PCM session hooks and stock AES derivation/CTR
  -> plaintext Screen messages over localhost
  -> separate NvSS renderer / displayable60
  -> existing cluster display route, subject to vehicle validation

Decoded-frame readiness -> Java handover -> context900=[60]
Disconnect / stale readiness / disable -> restore latest stock HMI request
```

The receiver hook extends `/info` with an auxiliary display descriptor, adds the
listening port to a stream111 SETUP response, and requests navigation through
`showUI` using that display's UUID. Stock functions establish the session and
supply key derivation and decryption. Keys stay inside the hooked process.

`showUI` asks the phone to select auxiliary content. It does not create a PCM
composition context. The Java bridge separately declares900=[60] and selects it
for terminal1 navigation contexts72–77 while readiness is fresh. Unlike stock
context74=[22,33], this projection context does not combine the stock intersection
and map displayables. Other-page requests pass through.

## Start here

See [PRERELEASE.md](PRERELEASE.md) for the test ZIP inventory, release scope and
the distinction between public test assets and the private integration package.

Read the [on-car onboarding and test guide](controlled-test/ONBOARDING_TEST_GUIDE.md)
for prerequisites, exact staged commands, pass/fail gates, evidence to return,
and disable/restore steps. Begin with QNX mocks and stock evidence collection;
installation and live graphics follow the documented target checks.

- [Requirements and research](PCM4_CARPLAY_CLUSTER_RESEARCH.md): owner baseline,
  scope, preferred features and source history.
- [Audi versus Porsche comparison](AUDI_PCM4_METHOD_COMPARISON.md): architectural
  references and why their existing installers are not interchangeable.
- [MU1476 engineering record](PCM4_MU1476_DISPLAY_PORT.md): current implementation,
  firmware findings, verification and remaining vehicle gates.
- [Controlled test kit](controlled-test/README.md): Stage0–2 collection, synthetic
  tests and bounded pattern rendering.
- [Integration procedure](integration/README.md): disabled installation, loading,
  enablement and guarded rollback.
- [Session hooks](IPHONE_SESSION_HOOKS_MU1476.md),
  [Java composition](JAVA_COMPOSITION_MU1476.md) and
  [output/context trace](NVSS_OUTPUT_CONTEXT_TRACE_MU1476.md): supporting analysis.

Some research sections describe earlier states of the investigation. Use the
implementation follow-ups and latest engineering record for current status.

## Source and build layout

| Directory | Contents |
| --- | --- |
| `native/` | Receiver hooks, crypto adapter, ARM guards, renderer and readiness protocol |
| `hmi/` | Porsche Java handover bridge, class patcher and tests |
| `profiles/` | Canonical MU1476 development profile |
| `controlled-test/` | Standalone test kit, evidence collector and onboarding guide |
| `integration/` | Installer, explicit controls, rollback and operating instructions |
| `tests/` | Host fixtures and isolated integration tests |
| `tools/` | Analysis, profile generation, build and packaging helpers |
| `private/` | Ignored local firmware, derived artifacts, evidence and test ZIPs |

Host-side checks and profile generation:

```sh
python3 tools/generate_pcm4_profile.py
sh tools/test_pcm4_projection.sh
sh tools/test_pcm4_runtime.sh
sh tools/test_controlled_kit.sh
```

The runtime tests use localhost sockets and synthetic graphics functions.
Native target builds require a QNX ARM toolchain; ordinary macOS Clang/Homebrew
compilers do not supply the QNX runtime. `tools/build_pcm4_home.sh` uses the
owner's configured SSH/Docker workspace. The Java build helper likewise expects
private, hash-pinned inputs on that workspace. These are environment-specific
helpers, not a clean-checkout toolchain installer; see the engineering record
and `hmi/README.md` for the setup and inputs.

Generated private archives contain the compiled test components; source checkout
alone does not include ignored binaries or firmware inputs. The integration
candidate installs application files, not a new firmware-update bundle. It is
disabled by default and does not automatically remount or restart services.

## References and acknowledgements

These projects informed the investigation. Their supported firmware, output
paths and feature claims are distinct from this Porsche port.

| Reference | Contribution to this investigation |
| --- | --- |
| [aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto) | Primary architectural reference: auxiliary receiver, Tegra graphics, heartbeat-based HMI handover and loader integration |
| [luka-dev/mib2q-carplay-rgi](https://github.com/luka-dev/mib2q-carplay-rgi) | Related auxiliary-stream and route-guidance investigation on Audi MHI2Q |
| [harman-f/mhi2_altscreen_carplay](https://github.com/harman-f/mhi2_altscreen_carplay) | Auxiliary video protocol and an alternative MOST transport implementation |
| [fifthBro/mh2p-cluster](https://github.com/fifthBro/mh2p-cluster) | Related Porsche cluster projection and guidance work on a different platform |
| [chefranov/mhi2-au37x-carplay](https://github.com/chefranov/mhi2-au37x-carplay) | Additional Audi MHI2 implementation reference |
| [harman-f/MHI2_US_POG11_K5186_1-MU1476-AIO](https://github.com/harman-f/MHI2_US_POG11_K5186_1-MU1476-AIO) | Background for the supplied Porsche firmware reference |

The reviewed Audi receiver is pinned in the
[session-hook analysis](IPHONE_SESSION_HOOKS_MU1476.md) to
[commit d5b51417185fa0afefe726fa2df82815693723fd](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto/blob/d5b51417185fa0afefe726fa2df82815693723fd/carplay/src/native/libaltscreen111_mu1438.c).
That implementation captures the session master key through a caller-checked
CBC hook; this port hooks the stock session-security setter directly and binds
the key to the session. Both use the auxiliary connection ID and stock screen
key derivation/CTR functions. The Porsche route, profile and Java patch are
reviewed separately against the supplied firmware.

Firmware, extracted libraries, converted OEM classes, proprietary-derived
integration archives and vehicle evidence belong in ignored private storage,
not Git. Referenced projects retain their own licenses and attribution terms.
This project is independent of Porsche, Audi, Volkswagen, Apple and Harman.

## License

Project code is licensed under the **GNU General Public License, version3 only
(SPDX: GPL-3.0-only)**; see [LICENSE](LICENSE). Copyright2026 the project
contributors. See [NOTICE.md](NOTICE.md) for upstream references and attribution.
Existing third-party notices and licenses remain applicable. This license does
not grant rights to Porsche/Harman/Apple/QNX firmware, libraries or OEM classes.
