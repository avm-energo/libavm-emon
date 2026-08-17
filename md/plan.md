# EMon — plan

Status legend: `[x]` done, `[~]` in progress, `[ ]` not started.

Process contract for every step below: when a step is finished (or its
scope changes), update `mindflow.md` with what was concluded/decided,
update `pseudocode.md` with anything reusable for implementation, and
amend this plan if reality diverges from it. `mindflow.md`'s "Current
status" section is the single resume point — check it first.

## 0. Vendor doc analysis
- [x] Convert and read both `.doc` files.
- [x] Understand framing, command set, numeric formats well enough to
      judge implementation complexity.

## 1. Planning scaffolding
- [x] `md/mindflow.md`, `md/plan.md`, `md/pseudocode.md` created.
- [x] Frame format, numeric formats and Modes 3/4/5 payload layout fully
      reconstructed from the docs (see `mindflow.md` step 1).

## 2. Python skeleton
- [x] `src/python/emon/` package: `constants.py`, `crc.py`, `frame.py`,
      `codecs.py`, `transport.py`, `device.py`.
- [x] `src/python/scripts/`: one small manual-test script per function,
      plus a CRC-variant finder.
- [ ] Fill in `requirements.txt` install + smoke-test that the package at
      least imports cleanly (no hardware needed for this part).

## 3. Real-hardware validation (needs the USB-UART bridge + device)
Work through in order — later steps depend on earlier ones (transport
before framing, framing before any command).

- [ ] 3.1 Determine serial parameters (baud/parity/stop bits) empirically.
      `scripts/01_test_connect.py` — try candidate bauds until the device
      responds instead of timing out.
- [ ] 3.2 Determine the CRC16 variant. `scripts/00_find_crc.py` — capture
      one real request/response pair (e.g. Mode 3 with no payload, cheap
      to send) and brute-force which candidate variant reproduces the
      trailing 2 bytes.
- [ ] 3.3 Validate connect/disconnect end-to-end once 3.1/3.2 are locked in.
- [ ] 3.4 Validate `get_device_info` (Mode 15 read of the system area) —
      confirm device type string, serial number, device-type ID land at
      the expected offsets; figure out the SW version field for real.
- [ ] 3.5 Validate `set_current_limits` (Mode 2, ЭМ‑3.1КМ 3-word variant)
      — confirm the request is accepted (NORMA) and the echoed values in
      the response match what was requested.
- [ ] 3.6 Validate `get_analog_inputs` (Mode 3) — confirm payload length
      matches the computed 4536 bytes, confirm RMS/angle/frequency decode
      to sane values (compare against a known load or a multimeter if
      available).
- [ ] 3.7 Decide whether Mode 3's latency (~3s worst case) is acceptable,
      or whether it's worth switching to a merged Mode 4 + Mode 5 call
      (see `mindflow.md` "Mode coverage gap").

At each of 3.1–3.7: fix the skeleton code as needed, then update
`mindflow.md` (what was actually true on the wire) and `pseudocode.md`
(if the reconstructed protocol details changed), and adjust this plan if
new steps turn out to be needed (e.g. a device quirk requiring a retry
strategy).

## 4. Consolidate Python implementation
- [ ] Fold in whatever fixes came out of step 3, clean up the skeleton
      into the final implementation.
- [ ] Re-run all four functions against the real device once more,
      end-to-end, to confirm nothing regressed.

## 5. C++ port (only after step 4 is fully green)
- [ ] CMake project + Qt (`QSerialPort`) library mirroring the validated
      Python logic: same frame/CRC/codec structure, same four functions.
- [ ] Validate the C++ library against the real device.

## Known unimplemented / out of scope for now
- Mode 19 (set reduction coefficients) — doc section is missing entirely,
  can't be implemented from what we have.
- ЭМ‑3.3Т1/ЭМ‑3.1К sibling-device Mode 2 variant — only the ЭМ‑3.1КМ
  variant is implemented.
- Harmonics/inter-harmonics arrays, sequence-component powers, and every
  other field in the Mode 3 payload beyond RMS/angle/frequency — not
  requested yet; the offset table in `pseudocode.md` covers the full
  payload if/when they're needed later.
