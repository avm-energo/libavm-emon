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
- [x] `requirements.txt` install + smoke-test that the package at least
      imports cleanly (no hardware needed for this part).

## 3. Real-hardware validation (needs the USB-UART bridge + device)
Work through in order — later steps depend on earlier ones (transport
before framing, framing before any command). **All done** — see
`mindflow.md` steps 3.1 through 4.6 for what was actually found on the
wire (checkboxes below were left stale during the work; trued up now).

- [x] 3.1 Determine serial parameters (baud/parity/stop bits) empirically.
      Resolved: 115200 8N1 (`mindflow.md` "Step 3.1").
- [x] 3.2 Determine the CRC16 variant. Resolved: CRC-16/XMODEM, transmitted
      big-endian — the one field in the protocol that isn't little-endian
      (`mindflow.md` "Step 3.2/3.3").
- [x] 3.3 Validate connect/disconnect end-to-end. Also caught and fixed a
      frame-byte-order bug in the process: SYNC comes before status/mode
      in both directions, not after as the docs' prose (but not their
      worked examples) implied (`mindflow.md` "Step 3.2/3.3").
- [x] 3.4 Validate `get_device_info`. Device-type string needs a
      word-byte-swap (`codecs.swap_word_bytes`) the docs don't mention;
      serial number/device-type ID confirmed. SW version field is still
      unresolved — raw words captured (`0x0702`, `0x0153`) but no encoding
      tried decodes them cleanly (`mindflow.md` "Step 3.4", "Step 4.1").
- [x] 3.5 Validate `set_current_limits`. Confirmed via two independent
      paths agreeing (the command's own echo, and Mode 3's footer fields
      45/46 read back separately) (`mindflow.md` "Step 4.2").
- [x] 3.6 Validate `get_analog_inputs`. Found and fixed three real
      off-by-one-word field-offset bugs (RMS, frequency, angle) and one
      decode-format bug (48-bit power fields' word order), all confirmed
      against front-panel ground truth on real loads. Fields 6-10's exact
      internal boundaries are still unresolved — no independent ground
      truth available for them (`mindflow.md` "Step 4.3" through "4.5").
- [x] 3.7 Mode 3's latency: measured directly, not just estimated from
      the docs — a steady ~2.5s normally, but with an occasional (~1-in-15
      to 25) multi-second device-side stall. Decision: keep Mode 3 (no
      Mode 4+5 split attempted), mitigate the stall with an automatic
      retry in the transport layer rather than switching modes
      (`mindflow.md` "Step 4.6").

## 4. Consolidate Python implementation
- [x] Fixes from step 3 are folded directly into `emon/` as they were
      found (not batched into a separate cleanup pass) — see the git log
      for the individual commits.
- [x] Re-ran all four functions against the real device repeatedly
      through the process; last full pass in `mindflow.md` "Step 4.6"
      confirmed no regressions.

## 5. Repo restructuring for the C++ port
- [x] Move Python sources from `src/python/` to `py/`, freeing `src/` for
      the new C++ project (commit `ab2ec6a`).

## 6. Verify the protocol over USB before porting anything
**Done.** USB carries the byte-identical EMon protocol (same frame
format, same CRC-16/XMODEM), just at different serial parameters:
**3,000,000 baud, 8 data bits, no parity, 2 stop bits (8N2)** instead of
RS-232's 115200 8N1. See `mindflow.md` "Step 5" for the full
investigation (it took several wrong turns before landing here - a
misidentified unrelated USB device, a red herring pointing at SCSI
pass-through, before the real cause turned out to be a baud rate and
stop-bit count nobody had tried together).

- [x] 6.1 Connect the device via its USB port; identify which new COM
      port Windows enumerates for it. Done: FTDI-chip-based, enumerates
      as a standard COM port (`USB Serial Port`, VID:PID 0403:6001).
- [x] 6.2 Re-run the existing Python scripts against that port. The
      confirmed RS-232 parameters (115200 8N1) produced total silence -
      an exhaustive sweep (13 bauds x 4 parity/stop-bit combos, DTR/RTS
      toggling) found nothing either, because 3,000,000 baud was outside
      every range tried.
- [x] 6.3 Compare results against the RS-232 baseline. Root cause found
      via a genuine USB-level capture (USBPcap + Wireshark, not a
      software COM-port tap) of the vendor's dedicated USB program: it
      configures the FTDI chip via direct vendor control transfers
      (`SetBaudRate`, `SetData`) to 3,000,000 baud / 8N2, then exchanges
      the exact same frame bytes `build_request()` already produces.
      Verified by replicating those exact parameters ourselves: full,
      valid, CRC-checked responses. Latency is comparable to RS-232 (a
      steady ~2s baseline, with the same kind of occasional multi-second
      stall already characterized in Step 4.6 - a plain retry recovers).
- [x] 6.4 Recorded in `mindflow.md` "Step 5". **Happy path, mostly**: the
      protocol layer (CRC/frame/codecs/field offsets) needs zero changes
      for USB - only the transport's serial parameters differ
      (baud/stop-bits), which `QSerialPort` supports natively. No
      SCSI-pass-through or raw USB API code needed after all; an earlier
      dead end in this investigation (see `mindflow.md`) suggested
      otherwise, but that was based on a misidentified, unrelated USB
      device.

## 7. C++ port
- [ ] 7.1 Qt CMake project named `libavm-emon` at the repo root: shared
      library target, public headers in `include/`, implementation in
      `src/`.
- [ ] 7.2 Implement `GetDeviceInfo`, `SetLimits`, and `GetAnalogInputs`
      only (not the full Python surface) — same frame/CRC/codec/field
      logic already validated in Python, ported to C++/`QSerialPort`.
      Transport needs to support both confirmed serial parameter sets
      (RS-232: 115200 8N1; USB: 3,000,000 baud 8N2 — see step 6), plus
      the automatic-retry-on-timeout mitigation from Step 4.6 (occasional
      multi-second device-side stall, same on both interfaces).
- [ ] 7.3 Verify the CMake project configures and compiles successfully
      (Qt 6.10.1 MSVC2022 64-bit kit, already located locally at
      `D:\qt\6.10.1\msvc2022_64`).
- [ ] 7.4 Validate the compiled C++ library against the real device —
      both over RS-232 and over USB, since they need different transport
      parameters.

## Known unimplemented / out of scope for now
- Mode 19 (set reduction coefficients) — doc section is missing entirely,
  can't be implemented from what we have.
- ЭМ‑3.3Т1/ЭМ‑3.1К sibling-device Mode 2 variant — only the ЭМ‑3.1КМ
  variant is implemented.
- Harmonics/inter-harmonics arrays, sequence-component powers, and every
  other field in the Mode 3 payload beyond RMS/angle/frequency — not
  requested yet; the offset table in `pseudocode.md` covers the full
  payload if/when they're needed later.
