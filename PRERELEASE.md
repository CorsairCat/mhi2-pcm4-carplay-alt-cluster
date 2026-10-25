# Prerelease preparation: v0.1.0-alpha.1

**Use at your own risk. This software has not been tested on a real car.**
This is an experimental MU1476 test candidate, provided without warranty.
Host tests and static checks do not establish vehicle compatibility, safe
operation, iPhone acceptance or a functioning instrument-cluster display.

This document prepares a prerelease inventory; it does not mean a GitHub release
or tag has been published. Existing r1/r2/r3 integration ZIPs are superseded.

## ZIP inventory

The local files are under ignored `private/releases/`:

| ZIP | Purpose | Contents | Release handling |
| --- | --- | --- | --- |
| `pcm4-mu1476-controlled-test.zip` | Stage0–2 test kit | Four QNX mock tests, synthetic libraries, bounded renderer, original sample video, collector, profile, guide, GPL license and attribution | Candidate public test asset; no extracted vehicle libraries or OEM HMI patch included |
| `pcm4-mu1476-project-source.zip` | Source companion | Tracked project source, build/packaging helpers, tests, documentation, GPL license and attribution from the release-preparation commit | Publish alongside project binaries; private firmware and SDK inputs are excluded |
| `pcm4-mu1476-integration-r4.zip` | Stage3–4 private integration test | Session/AES hooks, renderer, verifier, patched loader, patched OEM Java addon, startup changes, installer, rollback, guide and licensing notices | Private owner test only; contains OEM-derived material and is not a public release asset |

The controlled test ZIP alone does not implement phone negotiation or automatic
HMI handover. Those components are in the separate private integration candidate.
The private candidate is installed disabled and requires the preceding target
checks. These ZIPs are application-level test packages, not SD firmware updates.

`SHA256SUMS-prerelease.txt` records the two candidate public ZIPs.
`SHA256SUMS-private-integration.txt` records the private integration ZIP.
Use the actual files and manifests from the same packaging run; a rebuilt ZIP
can have a different hash even when its compiled payload is unchanged.

## Changes in this preparation

- Declare project code GPL-3.0-only and include the complete license.
- Add explicit attribution for the reviewed Audi implementation and related work.
- Include licensing notices and the untested-on-car warning in both test kits.
- Include the complete onboarding guide, with pass/fail gates and recovery.
- Refresh packaging and verify archive contents and checksums.

The r4 integration refresh changes documentation/licensing only relative to r3;
installed executable, hook, startup and Java payload bytes remain unchanged.
No new vehicle validation is claimed.

## Target and known limits

Development target: 2017 Macan PCM4, inspected POG11 MU1476 reference, wired
CarPlay. The original MU0809 firmware is not the supported test input.
Candidate route: NvSS0, displayable60, context900=[60], 448×448 at up to30fps.
Actual installed identities, active output attachment, J9 loading, declaration,
phone negotiation, scanout, page/warning precedence and recovery need on-car
validation. Native turn arrows and speed-limit fields are not implemented.

The build helpers require the documented QNX/Java environments. A clean source
archive contains no proprietary SDK, firmware, converted stock JAR or vehicle
backup; some analysis/integration builds require those owner-supplied inputs.

## Test order and reporting

Follow `controlled-test/ONBOARDING_TEST_GUIDE.md` in the repository, or
`ONBOARDING_TEST_GUIDE.md` inside either test archive:

1. Record stock behavior and run QNX mocks.
2. Collect/review active topology and library identities.
3. Review the bounded pattern result on the confirmed route.
4. Check/install disabled, preserve the backup, and test loading.
5. Enable wired projection and check page handover and recovery.
6. Disable/restore and verify normal stock behavior.

Return each stage as PASS/FAIL/NOT RUN with commands, exit status, firmware/MU,
iPhone/iOS, selected pages, archive hashes, logs and recovery observations.
Keep vehicle library copies, backups and proprietary integration material private.
