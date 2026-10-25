# Licensing and upstream attribution

Copyright2026 the Porsche PCM4 CarPlay Cluster Projection contributors.
Project code is licensed under GPL-3.0-only; the complete license is in LICENSE.
Existing third-party copyright and licensing notices remain applicable.

## Primary Audi reference

Author/project: aberbic,
[Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto](https://github.com/aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto).
The upstream repository supplies a GPLv3 license. Receiver analysis used commit
`d5b51417185fa0afefe726fa2df82815693723fd`.

The following source files informed this port:

- `carplay/src/native/libaltscreen111_mu1438.c`: auxiliary display advertisement,
  stream111 setup, Screen framing, session crypto and navigation presentation.
- `carplay/src/native/cluster_runtime.c`: NVIDIA graphics integration.
- `hmi/src/local/mu1438/ClusterGate.java`: heartbeat-driven HMI handover.
- `hmi/tools/PatchCluster.java`: targeted Java class patching.
- `carplay/scripts/patch-loader.py`: guarded application-loader integration.

This is a Porsche-specific implementation informed by those sources. Changes
include direct session-security-setter interception with session association,
Porsche firmware identity/entry guards, a separate renderer and atomic readiness
publisher, Porsche geometry/output configuration, and a Porsche Java declaration
and handover patch. These notes describe technical provenance; they are not a
completed line-by-line copyright audit or a claim that upstream influence is
limited to particular lines. The original upstream authors retain their rights.

## Other research references

See the repository README and AUDI_PCM4_METHOD_COMPARISON.md for contributions
from luka-dev/mib2q-carplay-rgi, harman-f/mhi2_altscreen_carplay,
fifthBro/mh2p-cluster, chefranov/mhi2-au37x-carplay, and the Porsche MU1476 AIO
reference. Their own licenses and notices remain applicable to their materials.

## Proprietary inputs and generated artifacts

Porsche/Harman/Apple/QNX firmware, extracted libraries and OEM Java classes are
not licensed by this project. The private integration ZIP contains modified OEM
material and must not be treated as a GPL-licensed firmware distribution or
uploaded as a public prerelease asset. Project source and original compiled test
components are separate from those private inputs. Preserve any applicable
third-party notices when redistributing code.

This project is independent of, and not endorsed by, the vehicle, phone or
platform vendors. Experimental software: use at your own risk; it has not been
tested on a real car and is provided without warranty under the GPL terms.
