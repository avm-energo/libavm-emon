# EMon — mindflow

Running log of conclusions, decisions and open questions, one section per
work step, in chronological order. **Read "Current status" first** — that's
the resume point for picking the work back up.

Device under study: ЭМ‑3.1КМ (a.k.a. "EMon" in this repo), a Russian
electricity/power-quality meter. Source docs: `emon/РЕЖИМЫ обмена
ЭМ-31KМ.doc` ("exchange modes") and `emon/Формат архива ЭМ-31KМ.doc`
("archive format" — despite the title, its body is ~95% the same exchange
protocol, plus one extra Mode 2 variant for sibling models ЭМ‑3.3Т1/ЭМ‑3.1К).

## Current status

**Both implementations are done and validated against real hardware.**
Python (`py/emon/`) implements all 4 milestone functions
(connect/get_device_info/set_current_limits/get_analog_inputs); the C++
port (`libavm-emon`, root `src/`/`include/`) implements the 3 targeted
functions (GetDeviceInfo/SetLimits/GetAnalogInputs). Both work over
RS-232 and USB. The protocol itself is fully reverse-engineered and
confirmed correct field-by-field against real hardware — "matches the
vendor docs' worked example" and "matches real hardware" turned out to
be different bars throughout this project, and only the latter proved
trustworthy (see Steps 4.3-4.5, 6.1 for the specific bugs the docs got
wrong that hardware testing caught).

### Next action (resume point — read this first)

**2026-08-26 update:** investigated a reported frequency discrepancy
(USB read ~49.3Hz vs. front-panel ~50.0Hz for the same live signal) - see
"Step 7". Root-caused as a real precision limitation of Mode 3's one-shot
frequency field, not a decode bug; the field-33 offset was independently
re-confirmed via a live 50→60→50Hz sweep and a non-nominal 52.347Hz
front-panel cross-check, then quantified with a proper 40-sample
statistical read (mean 49.5683Hz, median 49.5015Hz, stdev 0.4392Hz at a
steady 50.000Hz reference). **Done, including the doc caveat** on
`AnalogInputs::frequency`/`AnalogInputs.frequency` in both `Types.h` and
`py/emon/device.py`. The numbered list below (from the 2026-08-19 pause)
is still exactly as it was, unrelated to this investigation. One new
minor open item surfaced along the way: rapid
back-to-back `getAnalogInputs()`/Mode-3 polling with zero spacing
triggered a worse-than-usual multi-attempt stall once (recovered on its
own); mitigated ad hoc with a small inter-read delay during this
session's stats run, but not yet turned into real guidance anywhere in
the library itself - see the "possible follow-up" note at the end of
"Step 7" if picking this up later.

**Also this session (Step 8): the SW-version field is resolved** - real
DAA-digit decode confirmed exactly against the device's own display
("702.153.519", combining `sw_version="702.153"` with the already-known
serial number), plus a bonus confirmed SW-version-date decode
("2014-06-24"). Item #5 in the list below is now struck through/marked
resolved rather than removed, to keep the list's numbering stable for
anything referencing it. `DeviceInfo` gained real `sw_version`/
`sw_version_date` fields (both languages) in place of "raw, unresolved."
One loose end from this: the device's display shows a *second*, shorter
version number ("2.01") that doesn't correspond to anything in the
512-byte system area - likely a separate bootloader/hardware version not
exposed by this protocol at all; not resolved, not blocking anything.

Session paused here (2026-08-19) with the RS-232 cable unavailable.
Nothing below blocks using the library for its targeted functions — all
of it is either an accuracy/completeness gap outside that narrow scope,
or a verification step that just hasn't happened yet. Roughly in
priority order:

1. **Re-verify RS-232 with the C++ library specifically.** Step 6.1's
   RS-232 pass happened *before* the DTR/RTS and configuration-order
   fixes were fully settled; only USB got a full post-fix re-test. Get a
   cable, run `tests/manual/main.cpp` or the `examples/` against RS-232
   once more to close this out. Should just work (the fixes are
   link-agnostic), but hasn't been directly reconfirmed.
2. **`.gitlab-ci.yml` is untested.** Structure mirrors `libavm-gen`'s,
   but whether the referenced Docker images (`debian-qt6`, `fedora-qt6`,
   `fedora-mingw-qt6`) actually have `Qt6SerialPort` installed is
   unconfirmed — their own docs only mention "Qt6 base+svg". First thing
   to check if the pipeline fails.
3. **Packaging scaffolding is untested.** `package/`'s debian/rpm files
   exist and mirror `libavm-gen`'s structure, but nobody's actually run
   `dpkg-buildpackage` or `rpmbuild` against them to confirm they
   produce a working package.
4. **`Maintainer`/`License: GPLv3` in the packaging files** were copied
   from `libavm-gen` as a same-organization assumption, never explicitly
   confirmed as correct for this specific project - worth a quick check.
5. ~~**SW-version field encoding unresolved**~~ **RESOLVED 2026-08-26** -
   DAA-digit decode of each word (`0x0702`->"702", `0x0153`->"153"),
   confirmed exactly against the device's own display ("702.153.519",
   the ".519" being the already-known serial number). See "Step 8".
6. **Mode 3 payload fields 6-10** (RMS-at-1st-harmonic, RMS line value,
   mean/DC component, mean-rectified value) - offsets genuinely unknown,
   no independent ground truth available to pin them down (front panel
   doesn't expose these quantities). Would need a reference power-quality
   instrument or the vendor PC software's own detailed readout. See
   "Step 4.5".
7. **Field 12 (angle u^u)** - lower priority than 6-10, likely
   unresolvable with this test rig regardless, since it needs two
   energized voltage phases and testing so far has only driven one. See
   "Step 4.5".
8. **Root cause of the intermittent device/link stall** (RS-232's rare
   version, Step 4.6; USB's much-more-frequent large-transfer version,
   Step 6.1) was never found, only mitigated with retries. Would need
   deeper access to the device firmware or the Windows serial driver
   stack to chase further - flagged as likely out of reach from this
   side, not an active todo.
9. **Environment housekeeping**: two `USBPcapCMD` processes (leftover
   from the Step 5 USB packet-capture investigation) were still holding
   a lock on the USB device as of this session's end, and killing them
   needed elevated rights this session didn't have. A reboot or an
   elevated `Stop-Process` clears it.

**Hardware state note**: the physical meter is currently configured for
**1-phase-2-wire** (`wiring_scheme = 0b100`), not the 3-phase-4-wire
(`0b001`) baseline referenced throughout most of this document (Steps
4.2 onward) - changed deliberately during Step 6.2's validation and left
that way at the user's request. Expect `wiring_scheme` and any
wiring-dependent readings to reflect this, not the older baseline, until
someone changes it back.

- Step 0 (doc analysis) — done.
- Step 1 (planning files + protocol reconstruction) — done, this file.
- Step 2 (Python skeleton) — done, see `src/python/emon/`, including the
  `requirements.txt` install + import smoke-test (`pyserial` installs
  cleanly, `emon` and all its submodules import with no errors, all 5
  `scripts/*.py` parse `--help` without crashing) — no hardware needed.
- Step 3.1 (baud rate) — **resolved: 115200 8N1.** The earlier "zero bytes
  back at every baud" block (see historical note below) turned out to be a
  physical-layer issue, fixed on the hardware side outside this repo.
- Step 3.2/3.3 (CRC16 variant + frame byte order) — **resolved by passive
  capture**, see "Step 3.2/3.3" below. CRC-16/XMODEM, big-endian on the
  wire; frame is `[SYNC][status/mode][len_lo][len_hi][payload][crc_hi
  crc_lo]` in both directions (SYNC always first — the docs' abstract
  status/sync prose had the order backwards, but their own worked byte
  dumps were consistent with SYNC-first all along).
- Step 3.4 (device-info text field encoding) — **resolved**, see "Step
  3.4" below: ASCII fields are word-byte-swapped on the wire
  (`codecs.swap_word_bytes`), confirmed against a real Mode-15-shaped
  payload that decodes to the device string `"EM3.1KM - 02"`.
- Step 3.5/3.6 (two more passive captures, aimed at catching a request
  frame, including across live reconnects and a genuinely shared COM
  port) — **done; request frames are not observable via any software
  serial-port-sharing tap on this machine, confirmed architecturally, not
  just empirically** — see "Step 3.5"/"Step 3.6" below. Bonus
  confirmations along the way: Mode 2/limits response shape (10-byte
  payload), and a second independent device-info decode.
- Step 4.1 (live round-trip against real hardware, bare COM1) — **done,
  full pipeline confirmed working end to end** against the actual device,
  not just captured bytes — see "Step 4.1" below.
- Step 4.2 (`set_current_limits`, with independent cross-check via Mode
  3's footer fields instead of just trusting Mode 2's own echo) — **done**,
  see "Step 4.2" below. Device left in its original state after testing.
- Step 4.3 (real load on phase A, `get_analog_inputs()` field-offset
  validation) — **done; found and fixed two real off-by-one-word bugs in
  the doc-derived field table** (RMS and frequency), confirmed against
  the meter's own front-panel display as ground truth — see "Step 4.3"
  below.
- Step 4.4 (inductive load on phase A, angle field validation) — **done;
  found and fixed a third off-by-one-word bug** (angle), plus discovered
  the field's unit is degrees, not radians — see "Step 4.4" below. All 3
  of the offset bugs findable this way (RMS, frequency, angle) are now
  fixed and hardware-confirmed.
- Step 4.5 (reverify fields 6-10 and 13-19 against hardware) — **done for
  13-19: found and fixed a real decode-format bug** in the 48-bit power
  fields (magnitude words transmitted most-significant-first, not
  least-significant-first like every other multi-word field) — once
  fixed, fields 13-19 all check out **at their originally documented
  offsets**, no position shift needed. Fields 6-10 remain **genuinely
  unresolved** - no independent ground truth available for them (front
  panel doesn't expose those quantities), but now narrowed to a concrete,
  quantified 2-word shortfall. See "Step 4.5" below.
- Step 4.6 (investigate the transient short-read timeout) — **done; root
  cause found (real device-side stall, not a client bug), mitigated with
  an automatic retry** — see "Step 4.6" below.
- **Next action:** (1) resolve the SW-version field encoding (real raw
  words known: `0x0702`, `0x0153` — doesn't decode cleanly under the DAA
  hypothesis, still open); (2) fields 6-10's internal boundaries are still
  unknown (2-word shortfall confirmed to exist somewhere in that 5-field
  region, but not which field(s)) - would need either a reference
  instrument that reports RMS-at-1st-harmonic/mean-rectified-value
  directly, or the vendor PC software's own detailed readout, as ground
  truth; field 12 (angle u^u) is separately unpinned but may be
  unresolvable with this test rig regardless, since it needs two active
  voltage phases and this rig only energizes phase A. See "Open
  questions" below for the full list with detail.
- Step 5 (verify the protocol over USB, before starting the C++ port) —
  **done; same protocol, different serial parameters**: USB uses
  3,000,000 baud / 8N2 (vs. RS-232's 115200 / 8N1), everything else
  (frame format, CRC, field offsets) is identical — see "Step 5" below
  for the investigation, including two dead ends (a program that needs
  exclusive USB access, and a capture that turned out to be tapping an
  unrelated device) before landing on the real answer.
- Step 6 (C++ port: `libavm-emon` Qt/CMake shared library) — **done and
  validated against real hardware**. `GetDeviceInfo`/`SetLimits`/
  `GetAnalogInputs` implemented against `QSerialPort`, supporting both
  confirmed link types (RS-232 and USB) via a `LinkType` enum rather than
  exposing raw baud/stop-bits. Packaging/build conventions (deb/rpm
  `package/` directory, `BUILD_SHARED_LIBS` switch, `emon_export.h`, the
  rest of the CMake structure) ported from the sibling `libavm-gen`
  project for consistency with other AVM-Energo C++ libraries. See "Step
  6" for the build/packaging work and "Step 6.1" for hardware validation
  - found and fixed three real bugs (QSerialPort configuration ordering,
  missing DTR/RTS assertion, and characterizing a genuine USB-link
  large-transfer stall also present in the reference Python
  implementation) before all three functions were confirmed working
  correctly over both RS-232 and USB. "Step 6.2" adds `examples/`,
  `README.md`, `.gitlab-ci.yml`, and a nice unplanned validation: the
  library correctly detected a deliberate live wiring-scheme change on
  the device through two independent read paths.

Repo layout note: Python sources moved from `src/python/` to `py/` in
this same push, to free up `src/` for the new C++ port (see `plan.md`
section 5).

## Step 0 — Reading the vendor docs

Both `.doc` files needed conversion (`soffice --headless --convert-to txt`
gave a clean result; `catdoc`'s output had garbled binary noise around
embedded tables). Both documents describe the same custom binary
request/response protocol, not a standard one (not Modbus, not DLMS/COSEM).

Conclusions:

1. **No physical-layer spec.** Nothing on baud rate / parity / stop bits
   beyond an opaque "current speed index" field inside the device's system
   memory area — implies a lookup table that isn't documented anywhere in
   these two files. Must be determined empirically against real hardware.
2. **CRC16 algorithm unspecified.** Every command just says "CRC16
   checksum over all preceding bytes" with no polynomial/init/reflection
   given. Must be reverse-engineered from a real captured frame.
3. **Mode 19** ("Установка коэффициентов приведения" / set reduction
   coefficients) is listed in both documents' table of contents but its
   body section is **absent** from both — not a conversion artifact, the
   text simply jumps from Mode 18 straight to the frequency-constant
   formula and Mode 8. Not implementable from these docs; out of scope for
   the current 4-function milestone anyway.
4. Two closely related but distinct devices are documented: ЭМ‑3.1КМ
   (our device, per both filenames) and ЭМ‑3.3Т1/ЭМ‑3.1К/ЭМ‑3.2 (mentioned
   in `Формат архива` only, with its own shorter Mode 2 variant and a
   different system-memory layout past offset 26). Implementation targets
   the **ЭМ‑3.1КМ** variant throughout; the sibling variants are not
   implemented.

## Step 1 — Protocol reconstruction (frame format)

The docs never state frame byte order explicitly as a rule, but every
command section includes a worked example with the literal byte sequence
transmitted, e.g. for Mode 3: *"Побайтно - это 6 байт: 02h, 03h, 00, 00,
..."*. Cross-checking several of these examples against their "0302h"-style
shorthand notation resolves the layout completely:

- **All multi-byte fields are little-endian** (low byte transmitted
  first). The doc's own byte-order note for "СЛОВО" (`2б. 1б.` column
  header) is consistent with this once you realize the printed hex
  shorthand writes the high byte first but the wire order is low-byte-first.
- **Frame layout** (both directions), reconstructed from every worked
  example in both docs:

  ```
  request:  [0x02 SYNC][mode][len_lo][len_hi][payload...][crc_lo][crc_hi]
  response: [status]   [0x02][len_lo][len_hi][payload...][crc_lo][crc_hi]
  ```

  `status` is `0x00` (NORMA/OK) or `0xFF` (NOT NORMA/error). `len` = byte
  count of `payload` only, not counting the CRC. CRC16 is computed over
  every byte before it (sync/status + mode/sync + len + payload).
- Mode byte == decimal mode number in hex (Mode 18 → `0x12`, Mode 24 →
  `0x18`, etc.) — confirmed directly, several sections spell this out.
- **Doc typo found:** the Mode 9 (Read RAM) section header in both docs
  writes the shorthand `"0B02h - 09h - ..."`, which is internally
  inconsistent (`0x0B` vs "mode 9") — almost certainly copy-pasted from the
  neighbouring Mode 11 section (`0x0B`). The explicit byte dump right below
  it (`"02, 09h, 06, 00, ..."`) is authoritative and confirms the request
  header really is `0x02 0x09`, matching the little-endian rule above. Code
  uses `0x09`, not `0x0B`, for Mode 9.

## Step 1 — Numeric formats

- Unsigned fixed point: concatenate N little-endian words into one
  unsigned int, then divide by `2**frac_bits`.
- Signed fields: MSB of the full concatenated value is the sign bit; if
  set, invert via `X = 0 - X` on the *whole* value before dividing (per
  the docs' own instructions) — this is standard two's complement, not
  sign-magnitude; see the correction note below (this bullet originally
  said "sign-magnitude, not two's complement", which had it backwards).
- **Resolved inconsistency:** the two docs disagree on the angle field's
  integer/fraction bit split (15/16 in `РЕЖИМЫ обмена`, 16/16 in `Формат
  архива`). Checking the bit budget: `sign(1) + int(15) + frac(16) = 32`
  is self-consistent for a 32-bit (2-word) field; `sign(1) + int(16) +
  frac(16) = 33` does not fit in 32 bits at all. So `Формат архива`'s
  16/16 claim is the typo; `frac_bits=16, total_bits=32` is correct
  regardless of which int-bit count you believe, since decoding only
  needs `frac_bits` and `total_bits` (the int-bit count is cosmetic).
  → No real ambiguity here after all; `codecs.decode_signed_fixed`
  implements it with `frac_bits=16, total_bits=32`.
- **Signed field decoding correction** (found while unit-testing the
  skeleton, not from the docs): the general "Знаковые величины" section
  describes signed fields abstractly as sign+magnitude ("determine the
  sign bit, take the modulus, treat the rest as unsigned"), which is what
  the first version of `codecs.py` implemented — masking off just the
  sign bit. But the *field-specific* instructions (e.g. for field 9, the
  angle fields) are more precise: *"if the sign bit is set, negate the
  entire 32-bit number (X = 0 − X), then treat the result as unsigned."*
  Negating the **whole** width (sign bit included) rather than clearing
  the sign bit and negating only the low bits is exactly how you decode
  **two's-complement**, not sign-magnitude — the two only agree for most
  values, but diverge at the boundary case where the sign bit is set and
  every other bit is 0 (e.g. `0x80000000`), which a plain "clear the sign
  bit" mask decodes as 0 while true two's-complement decodes as the most
  negative representable value. Fixed in `codecs.py`
  (`decode_signed_fixed`, renamed from `decode_sfixed_sign_magnitude`) to
  do a proper full-width two's-complement negation; verified against that
  boundary case in a unit test. Lesson: when a general/abstract section
  and a field-specific worked instruction disagree on the mechanics, trust
  the field-specific one — it's the one someone actually implemented
  against real firmware.
- 48-bit power fields: word 0 is a sign flag (`0x0000` positive / `0xFFFF`
  negative — not a sign bit), words 1–3 are a 48-bit unsigned magnitude,
  28 integer bits / 20 fraction bits.
- There are **three different date/time-ish encodings** in the docs, not
  one: (a) an 8-byte full timestamp (sec/hour/min/month/day/year) used for
  registration windows, (b) a 2-word DAA-BCD `ГГГГ,ММДД` encoding used for
  the SW-version date example, (c) a 2-word `ЧЧ:ММ DAA` encoding for
  peak-load time-of-day fields. None of these are needed for the current
  4-function milestone (`get_device_info` only needs the plain ASCII/int
  fields), so only (b)'s DAA-digit trick is implemented in `codecs.py`
  (`decode_daa_date`), and it is **not currently wired into
  `get_device_info`** — see the SW-version note below.
- **SW version field, originally unresolved here** - the two docs
  disagree on its width (4 bytes vs 8 bytes) and the one worked example
  given (`0x0704,0x0101 => "707.101"`) doesn't reduce cleanly under DAA,
  plain hex, or plain decimal reading of either word - it looks like the
  example itself is wrong or refers to a format not otherwise described.
  **Resolved against real hardware in Step 8**: simple per-word DAA-digit
  decode (the same trick already used for dates), no special-casing
  needed - the doc's own worked example was just wrong/misleading, not a
  sign of real format complexity.

## Step 1 — Response payload layout for Modes 3/4/5

Modes 3 ("all quantities"), 4 ("currents/voltages/powers only") and 5
("harmonics only") all return a payload of the **same fixed size**
`2 * ((242*6) + 64 + 2 + 150 + 600)` bytes = 4536 bytes; the mode only
controls which fields are actually computed, not the payload layout. The
field list (fields 1–48) sums to exactly this word count, which let me
reconstruct absolute byte offsets for every field:

- Fields 1–19 repeat once per channel block, 242 words/block, 6 blocks in
  a fixed order: **Ia, Ua, Ib, Ub, Ic, Uc**. Field 5 ("действующее
  фазное" / RMS phase value) sits at word offset 204 within a block —
  present and meaningful in *all* blocks, so this gives Ia/Ua/Ib/Ub/Ic/Uc
  RMS directly. Field 11 ("угол u^i") sits at word offset 216 — occupies
  space in every block for uniformity, but is only meaningful in the I
  blocks (Ia/Ib/Ic), which is exactly what's needed for
  phi(Ia/Ua), phi(Ib/Ub), phi(Ic/Uc).
- Fields 20–48 follow after all 6 channel blocks ("the footer"), starting
  at byte offset `242*6*2 = 2904`. Field 33 ("частота") sits at word
  offset 40 within the footer.
- Full derivation and the offset table are in `pseudocode.md` under
  "Modes 3/4/5 payload layout" — reuse that table, don't recompute it.

**Mode coverage gap (important design decision):** Mode 4's documented
field list (`5, 7, 9-10, 13-24, 28, 45-46`) does **not** include field 11
(angle) or field 33 (frequency). Mode 5's field list (`1-4, 6, 8, 11-12,
25-27, 29-48`) does **not** include field 5 (RMS). So neither the fast
Mode 4 nor Mode 5 alone can produce the requested `get_analog_inputs()`
result (RMS + phi + frequency together); only Mode 3 (full calc,
documented worst case ~3s) computes all three at once. **Decision:**
`get_analog_inputs()` uses Mode 3 for correctness first; revisit later
(Mode 4 + Mode 5 combined, two faster round trips merged) only if latency
turns out to matter in practice. Flag this to the user if 3s per poll is
too slow for their use case.

## Step 1 — get_device_info: which read command?

The system-area table in both docs is a 512-byte structure ending in its
own 2-byte CRC at offset 510 — i.e. it is exactly one of the "blocks
closed by CRC" that Mode 15 (Read FRAM) is described as reading (its
request docs an address space "0...32767" words with the parenthetical
"начальные адреса блоков, закрытых CRC"). Mode 9 (Read RAM) is a generic
page+offset+count accessor with no stated tie to this persistent
structure. **Decision:** `get_device_info()` uses Mode 15, start word 0,
count 256 words (512 bytes) — not Mode 9.

## Step 1 — set_current_limits: which Mode 2 variant?

Mode 2 has two incompatible request shapes across the docs: a 2-word
(4-byte) version for ЭМ‑3.3Т1/ЭМ‑3.1К and a 3-word (6-byte) version
explicitly labelled "для ЭМ 3.1КМ" (current-limit **type** + index +
voltage-limit index). Since our device is ЭМ‑3.1КМ by name in both
filenames, **decision:** implement only the 3-word/ЭМ‑3.1КМ variant.

## Step 2 — Python skeleton

Package laid out as `src/python/emon/{constants,crc,frame,codecs,
transport,device}.py` plus `src/python/scripts/` with one small
standalone script per function for manual testing against real hardware,
matching the user's requested workflow. No packaging (`pyproject.toml`)
added yet — scripts do a `sys.path` insert to import the local `emon`
package directly; revisit if/when this needs to be pip-installable.

Everything that depends on an undetermined protocol detail (CRC variant,
baud rate, SW version field) is isolated behind a constructor parameter or
left unimplemented with a comment, rather than guessed silently, so wrong
guesses fail loudly instead of producing silently-corrupt data.

## Step 3.1 — Baud rate determination (2026-08-18, historical — resolved 2026-08-19 at 115200, see below)

Hardware: PL2303-based USB-serial adapter ("I-756x", VID:PID=067B:2303),
enumerated as COM12 on Windows after a driver reinstall. Meter is
ЭМ‑3.1КМ, powered on with display active, connected via DB9 RS-232 cable.

Tried, all producing **zero bytes back** (`scripts/01_test_connect.py
--probe`, Mode 3 empty-payload request, default MODBUS CRC guess):
- All of 300/600/1200/2400/4800/9600/19200/38400/57600/115200 baud, 8N1.
- Both cable orientations: as-received, and with pins 2/3 (RX/TX)
  manually crossed at the connector.
- With the meter's ground (pin 5) confirmed wired.
- With the meter's on-screen "remote comms" menu setting toggled on
  (previously unconfirmed whether it was enabled).
- With DTR and RTS explicitly forced high in software (in case the
  meter's isolated interface draws its opto-isolator power from those
  lines) — no change.

Isolated the USB adapter itself as **known-good**: shorting pins 2/3 at
the adapter's DB9 connector (meter disconnected) and sending the same
probe produced an exact byte-for-byte echo at 9600 8N1. So the adapter,
driver, and pyserial transport are all confirmed working; the silence is
downstream of the adapter (cable/meter side).

Why this isn't just "wrong baud": every documented command response
format includes a fixed 6-byte NOT-NORMA (`ff 02 00 00 <crc>`) reply
shape, which strongly implies the device parses sync+mode+length before
checksum-validating the payload — i.e. a *wrong CRC guess* at the
*right* baud should still produce 6 bytes back, not silence. Getting
literally nothing at every baud, even after the loopback-confirmed-good
adapter, points at the physical layer (wrong pin mapping on the meter's
connector, wrong connector/port entirely, or a level/interface mismatch)
rather than at baud or CRC.

Neither vendor doc contains any physical connector pinout, voltage
level, or interface-hardware description (confirmed by re-grepping both
converted docs) — this really is undocumented outside this file, matches
the step-0 conclusion, and can't be resolved from these docs at all.

**Next action:** needs eyes/hands on the physical meter — check for a
pinout label near the port or a separate (non-protocol) operations
manual, confirm this is actually the RS-232 "remote" port and not some
other connector (e.g. an optical/local-readout port), and ideally verify
with a multimeter that the meter's TX pin idles at a standard RS-232
mark voltage. Once the physical connection is confirmed right, resume
the baud sweep from `scripts/01_test_connect.py --probe`.

## Step 3.2/3.3 — Live capture, CRC16 variant, and frame byte order (2026-08-19)

Setup: device → real COM1 (115200, confirmed working now that the Step
3.1 physical issue is fixed) → HHD Software "Virtual Serial Port Tools"
split into two aliases, COM2 (used by the vendor PC program) and COM3
(free for us). `scripts/99_sniff_mirror.py COM3 --seconds 90 --baud
115200` captured 90s of real traffic passively while the vendor program
ran: 56 raw reads, 123670 bytes total, reassembled (by <0.5s gap) into 28
distinct frames.

**Everything captured was response-shaped traffic** (frames start `0x02`
and have a plausible length field) — no separate small request frames were
visible in the capture, even though the vendor program must be sending
them. Two working theories, unresolved: (a) HHD's split only mirrors one
direction to each alias port (i.e. COM3 sees device→PC only), or (b) the
request is tiny enough (6 bytes for an empty-payload request) that it
landed inside the same `ser.read(4096)` call as the response that
immediately follows and is indistinguishable from response bytes in the
dump. Needs a follow-up capture (e.g. watch for the pattern to reappear
with a request visibly separated by a timing gap, or check HHD's split
settings for a "monitor direction" option) before assuming (b).

**Frame shapes seen**, both matching Step 1's byte-count derivation
exactly:
- Two 512-byte-payload frames (518 bytes total) near the start of the
  capture, ~130ms apart. First one's payload is all `0xFF` (512 bytes);
  second one's payload starts with a decodable device-info string (see
  Step 3.4) — device-info is read once at connect, not on every poll.
- 26 repeats of a 4536-byte-payload frame (4542 bytes total, arriving as
  one 4096-byte read + one 446-byte read back to back), ~2.9s apart —
  matches Step 1's derived Mode 3/4/5 payload size exactly and looks like
  the vendor program's live-monitoring poll loop (~2.9s per poll, close to
  the docs' "up to ~3s" for Mode 3 full calc — another point in favor of
  this being Mode 3 specifically, not 4 or 5).

**CRC16 variant, confirmed**: brute-forced ~30 named CRC16 parameter sets
against all 28 frames simultaneously (`crc_bruteforce.py`, ad hoc, not
checked in — logic folded into `emon/crc.py` below). Exactly one
combination reproduces the trailing 2 bytes on **every** frame (2 frame
shapes, 28 independent payloads, so this isn't a coincidental match):

- **CRC-16/XMODEM**: poly=`0x1021`, init=`0x0000`, no input/output
  reflection, xorout=`0x0000`.
- Computed over the frame from its first byte (`SYNC`) through the end of
  `payload` — i.e. `[SYNC][status][len_lo][len_hi][payload...]`.
- **The CRC itself is transmitted big-endian** (high byte first) — the
  *only* multi-byte field in the whole protocol that isn't little-endian.
  Easy to miss/misdiagnose as "wrong CRC variant" if you assume
  little-endian like everything else.

`emon/crc.py` (`DEFAULT_VARIANT`) and `emon/frame.py`
(`build_request`/`parse_response`, CRC pack/unpack now use `"big"`) are
updated accordingly and verified: all 28 real captured frames now parse
and CRC-check cleanly through the actual library code, not just the ad hoc
brute-force script.

**Frame byte order correction**: the docs' prose lists fields as
"status, sync" for responses, which `frame.py` had literally implemented
(`status, sync = raw[0], raw[1]`). Real frames start `02 00 ...` —
`SYNC` (`0x02`) first, `status` (`0x00`=OK) second, matching the
*request* side's byte order and matching the docs' own worked byte-dump
examples (which is what Step 1 originally went off of) rather than their
abstract field-order prose. Fixed in `frame.py`. Lesson reinforced from
Step 1: when a doc's abstract description and its worked example
disagree, trust the worked example — now proven correct against real
hardware too.

## Step 3.4 — Device-info payload: text field byte order (2026-08-19)

The second 512-byte-payload frame from the capture (see Step 3.2/3.3)
matches `get_device_info()`'s Mode 15/system-area hypothesis: its
first 16 bytes, read as raw ASCII, are garbled (`"ME.3K1 M -20\0\0\0\0"`)
but are exactly right if you first swap the byte order **within each
16-bit word** (`codecs.swap_word_bytes`, new): `"EM3.1KM - 02\0\0"` — a
believable rendering of "ЭМ‑3.1КМ" (Latin transliteration) plus a
sub-version/index suffix. This is a different quirk from `decode_ascii`'s
existing `>0xA0 → +0x20` rule (not exercised here, since none of these
bytes exceed `0xA0`) — both may be needed together for fields with
non-ASCII (Cyrillic) content, untested so far. `device.py`'s
`get_device_info` now applies `swap_word_bytes` before `decode_ascii` for
the device-type field.

Other fields read off this same real payload (offsets per `device.py`,
**not yet cross-checked against the device's actual display/label**, so
treat as raw observations, not confirmed semantics):
- `serial_number` (bytes 16-18, LE): `0x0207` = 519.
- `sw_version_raw_words` (bytes 18-20, 20-22, LE): `0x0702`, `0x0153`.
  Still doesn't decode cleanly under the DAA-BCD hypothesis from Step 1
  (`0702`/`0153` as raw digit strings don't obviously match a plausible
  version number) — remains **unresolved**, now with real data to test
  future hypotheses against (see `devinfo_decode.txt` dump approach in
  this session, not checked in — rerun via `99_sniff_mirror.py` +
  `parse_response` if needed again).
- `device_type_id` (bytes 26-28, LE): `0x0003` = 3.

## Step 3.5 — Second capture across a live reconnect (2026-08-19)

Ran a second `99_sniff_mirror.py COM3 --seconds 60 --baud 115200`
capture, this time asking for the vendor PC program to be disconnected
and reconnected *while already listening*, specifically to catch
whatever request(s) kick off a fresh session (device-info read, start of
polling, etc.) — on the theory that the first capture (Step 3.2/3.3)
might simply have started mid-session, after the one-time "start
streaming" command had already been sent.

Result: **still zero request-shaped frames**, across 150s combined and a
live reconnect event. Every single frame captured in both sessions starts
`02 00 ...` — `SYNC` then a byte that's always exactly `0x00`. If any of
these were actually request frames, that second byte would be the mode
number (`0x03`, `0x0F`, `0x02`, ...), which varies by command — seeing it
pinned at `0x00` (`status=OK`) on literally every frame is strong evidence
every captured frame is a **response**, never a request. Combined with
Step 3.2/3.3's identical finding, this upgrades that section's "(a) vs
(b)" open question to a near-certain conclusion: **the HHD split only
forwards the device→PC direction to the COM3 alias; PC→device bytes
written on COM2 never reach COM3 at all.** This is a property of the
splitter, not something fixable by capture timing or duration — further
attempts with this exact tap won't succeed either.

Bonus finding from the reconnect sequence itself: right before the
device-info re-read (the two 518-byte frames, now at t≈27.97s/28.09s in
this session), a **new, previously unseen 16-byte frame** appeared at
t=22.345s: `02 00 0a 00 00 00 03 00 00 00 00 00 01 00 10 c4` — a
10-byte-payload response. CRC checks out. Decoded per `device.py`'s
`AppliedLimits` layout (5 little-endian words):
`current_limit_type=0, current_limit_index=3, voltage_limit_index=0,
access_level=0, wiring_scheme=1` (1 = 3-phase-4-wire per the docs) — a
completely plausible real setting for a bench unit, and an exact size
match for Mode 2's documented response shape. Third independent
payload-size confirmation (after Mode 3's 4536B and Mode 15's 512B) that
`device.py`'s mode/payload assumptions line up with real hardware, even
without ever having directly observed the matching request's mode byte.

## Step 3.6 — Third capture via genuinely shared COM2, request frames ruled impossible via software tap (2026-08-19)

User reconfigured HHD Virtual Serial Port Tools so COM2 itself is a
"Shared Serial Port" (not an alias/split pair — COM3 and COM10002 from
the earlier topology are gone; both the vendor PC program and this repo's
scripts now open the *same* COM2 concurrently). Confirmed COM2 opens fine
standalone (`pyserial`, 115200 8N1) alongside the running vendor program.
Ran `99_sniff_mirror.py COM2 --seconds 60 --baud 115200`, again asking for
a live reconnect during the window.

Result: **identical to Step 3.5 — zero request-shaped frames**, despite
the device-info re-read (two 512-byte-payload frames) confirming the
reconnect handshake genuinely happened inside the capture window this
time (first bytes at t=17.8s, not t=0, so we caught the real handshake,
unlike Step 3.2/3.3's capture which may have started mid-session).

**Conclusion, now well-supported by three consecutive captures across two
different HHD sharing modes: capturing the PC→device direction is not
achievable through any software-level COM-port-sharing tap on this
machine, as a matter of physical-layer architecture, not a
configuration/timing problem.** RS-232 TX and RX are separate wires with
no inherent loopback; a port-sharing driver's job is to (a) merge N
writers' bytes onto the single physical TX line and (b) fan the physical
RX line's bytes out to all listeners — there's no reason for it to also
echo a byte *written* by one listener back as if it arrived on RX for the
others, because that's not what happens on the real wire. This applies to
any such tool, not just HHD's specific implementation — trying yet another
HHD mode is not expected to change this result. The only way to see the
PC→device bytes directly would be a hardware tap (logic analyzer or
breakout board on the physical TX line between the adapter and the
meter), not attempted.

**Practical takeaway:** stop trying to capture a request frame via
software port-sharing — it can't work here. Confidence in `build_request()`
producing byte-correct requests is high without it: the CRC16/XMODEM
algorithm and frame layout are symmetric and already verified against 45+
real response frames across 3 captures, and 3 independent response
payload sizes (Mode 3: 4536B, Mode 15: 512B, Mode 2: 10B) exactly match
`device.py`'s mode/payload assumptions. Proceed to Step 4 (running
`scripts/01_test_connect.py` etc. directly against hardware) rather than
pursuing more passive capture. **Caution:** don't run the test scripts
against COM2 while the vendor program is also actively polling — two
writers sharing one physical async serial line have no collision
arbitration, so simultaneous writes could interleave and corrupt frames
on the wire (garbling either side's request, not just ours). Close/pause
the vendor program before running our own scripts.

## Step 4.1 — Live round-trip against real hardware (2026-08-19)

With the vendor PC program closed and HHD port-sharing torn down (bare
COM1, no virtual-port layer at all), ran the read-only test scripts
directly against the device for the first time — previously everything
had only been checked against passively-captured bytes, never sent live:

- `scripts/01_test_connect.py COM1 --probe --timeout 6`: sent a live Mode
  3 request (`02 03 00 00 b4 38`, built by `build_request()` using the
  now-default XMODEM CRC) and got back a well-formed 4096-byte-first-read
  NORMA response with the exact expected header (`02 00 b8 11...`,
  4536-byte payload) — **this is the strongest possible confirmation of
  the CRC/frame work**: it validates `build_request()`'s CRC on the
  *request* side, which passive capture alone could never do (see Step
  3.6). Needed `--timeout 6`; the default 2s timeout wasn't enough for
  Mode 3's up-to-~3s worst case and produced a false "received nothing"
  on the first attempt.
- `scripts/02_test_get_info.py COM1`: `EMonDevice.get_device_info()`
  returned `device_type='EM3.1KM - 02'`, `serial_number=519`,
  `device_type_id=3`, `sw_version_raw=(0x0702, 0x0153)` — **exactly**
  matching the values decoded from the Step 3.2/3.3 passive capture,
  confirming the whole pipeline (transport → frame → CRC →
  word-swap-ASCII decode) end to end through the real library code, not
  just an ad hoc analysis script.
- `scripts/04_test_get_analog_inputs.py COM1`: `EMonDevice.get_analog_inputs()`
  returned all zeros (Ia/Ib/Ic/Ua/Ub/Uc/frequency/all three phi angles) —
  again matching the Step 3.2/3.3 capture exactly. Plausible if nothing is
  wired to the meter's current/voltage inputs right now, but this is not
  a positive confirmation that the field-offset table in `device.py` is
  correct — a zero-payload match is uninformative either way. Needs a
  known load connected to actually exercise those offsets.
- `scripts/03_test_set_limits.py` (Mode 2, **writes** device
  configuration) deliberately not run — see "Current status" next-action
  note.

## Step 4.2 — set_current_limits, with independent verification (2026-08-19)

`pseudocode.md`'s Mode 3/4/5 footer field table has field 45 ("applied
current-limit code", footer word offset 64) and field 46 ("applied
voltage-limit index", footer word offset 65) — the *same* settings Mode 2
(`set_current_limits`) reports, but readable via a completely different
command (`get_analog_inputs`/Mode 3). This gives a real independent check
instead of just trusting Mode 2's own echo of its own input.

Baseline (read via the Mode 3 footer, before touching anything):
current-limit code = 3, voltage-limit index = 0 — matching the values
already seen echoed in the Mode 2 response captured passively during the
Step 3.5 reconnect. Two independent read paths agreeing before any write,
good sign.

Test: `set_current_limits(current_limit_type=0, current_limit_index=0,
voltage_limit_index=0)`, then restore to the baseline
`(0, 3, 0)`. Both the Mode 2 echo and the independent Mode 3 footer
readback agreed at every step:

| step | Mode 2 echo (type, current_idx, voltage_idx) | Mode 3 footer fields 45/46 |
|---|---|---|
| baseline | (not queried this way) | (3, 0) |
| after set(0,0,0) | (0, 0, 0) | (0, 0) |
| after restore(0,3,0) | (0, 3, 0) | (3, 0) |

`access_level` (0) and `wiring_scheme` (`0b001`, 3-phase-4-wire)
unchanged throughout, as expected since only the limit indices were
touched. Device left in its original state. This confirms Mode 2
(`SET_LIMITS`) works correctly and completes hardware validation for all
4 milestone functions (connect, device info, set limits, analog inputs) —
see "Current status" next-action note for what's still open.

## Step 4.3 — Real load on phase A: field-offset validation, two bugs found and fixed (2026-08-19)

With a known 10V/0.1A AC (50Hz nominal) reference connected to the
meter's phase A inputs, re-ran `get_analog_inputs()` and got obviously
wrong values: `Ia≈2444A`, `Ua≈1568V`, `frequency≈65526Hz` (or ≈-5 to -10Hz
read as signed), `phi≈28466 rad`. Channel isolation was still correct
(B/C stayed clean zero — only A had any nonzero content), which ruled out
a gross channel-block-mapping error and pointed at something finer:
either an intra-block field-offset bug or a units/calibration mismatch.

Got the device's own front-panel ground truth: **Ia=0.998A, Ua=10.0247V,
Freq=50.0Hz**. Neither matched our decoded values by any clean scale
factor (Ia off by ~2449x, Ua by ~160x - inconsistent ratios rule out "one
global wrong divisor"), which pointed at wrong bytes, not a wrong
formula. Scanned the raw payload word-by-word around the documented
offsets for values matching known-good numbers:

- **Field 5 (RMS) is at word offset 205, not 204 as `pseudocode.md`/Step 1
  had it.** `Ua` at 205 (frac_bits=20) decodes to **10.0247** - exact
  match to the front panel to all 4 decimal digits. `Ia` at the same
  offset decodes to **0.0998** - matches the ~0.1A actually injected, not
  the front panel's 0.998A (see below for why that's expected, not a
  second bug). This is an unambiguous, high-precision confirmation, not a
  coincidence. Root cause not pinned down further than "some field in the
  1-4 range is undercounted by one word in the doc-derived table" - fields
  6-19 have not been individually reverified and may need the same shift.
- **Field 33 (frequency) is at footer word offset 39, not 40.** Scanned
  the whole footer region 35-45 for anything frequency-shaped: offset 39
  (unsigned, frac_bits=16) gives **49.44 Hz** - every neighboring offset
  instead reads as an obvious flag/status word (values right at or near
  `0xFFFF`/`0x0000` boundaries, or suspiciously round large integers like
  29020). ~1% off the nominal 50Hz, plausible for real bench-source
  tolerance rather than a red flag. Note the **shift direction is
  opposite** field 5's (-1 word here vs. +1 there) - almost certainly an
  independent transcription error in the footer's field list, not the
  same root cause propagating through.
- **Angle (field 11) remains unresolved.** The test load was
  near-resistive (a `cos(phi)≈0.9998`-looking value was spotted elsewhere
  in the payload during the scan), so the true phase angle should be near
  zero regardless of whether the offset is right - which means this test
  can't discriminate "offset correct, genuinely ~0" from "offset wrong,
  garbage that happens to look small". Word offset 216 (documented) reads
  as a large nonsense value (~28000+ rad); offset 218 (field 12, angle
  u^u, N/A/reserved in I blocks per the doc) reads exactly 0.0000, which
  is *consistent* with the original unshifted table rather than
  supporting a shift - genuinely ambiguous either way with this test
  setup. Needs a load with a known non-trivial phase angle
  (inductive/capacitive) to actually resolve.
- **Important semantic finding, not a bug**: Mode 3's RMS fields return
  the device's **raw pre-calibration** measurement, not the calibrated
  value its own front panel displays. The front panel's 0.998A for a real
  0.1A input applies the device's internally-configured current-ratio
  setting (see `set_current_limits`'s `current_limit_type`/`current_limit_index`,
  currently `(0, 3)` on this unit) on top of the raw ADC reading; the
  protocol field itself does not. `get_analog_inputs()`'s docstring now
  says this explicitly. This also retroactively explains Step 4.1's "all
  zeros with no load connected" result as entirely expected (raw
  measurement really is ~0 with nothing connected) rather than merely
  "uninformative either way" as originally logged there.

Fixed in `constants.py` (`FIELD5_RMS_WORD_OFFSET` 204→205,
`FIELD33_FREQUENCY_FOOTER_WORD_OFFSET` 40→39); re-ran
`get_analog_inputs()` after the fix and got `Ia=0.0998, Ua=10.0243,
frequency=49.4340` - consistent with the scan, confirming the fix flows
through the real code path, not just the ad hoc diagnostic scripts used
to find it.

## Step 4.4 — Inductive load on phase A: angle field validation (2026-08-19)

Step 4.3 left the angle field (11) unresolved because the resistive test
load's near-zero expected phase angle couldn't distinguish a correct
offset from a wrong one that happened to look small. Connected an
inductive load to phase A instead and got a real, non-trivial front-panel
reading to test against: **Ia=0.499563A, Ua=13.0325V,
phi(Ua-Ia)=-45.63deg**.

First pass (still using the old offset 216) reproduced the same kind of
garbage seen in Step 4.3 (`phi≈24374`, nowhere near -45.63 in any unit).
Scanned the whole Ia channel block for a value matching -45.63 (both as
degrees directly and as radians, converted): **word offset 215 (signed,
frac_bits=16) → -45.96 deg**, matching the front panel to within ~0.7% -
good enough to be confident given real measurement noise, and dramatically
better than every other offset/format combination tried (nothing else
came within an order of magnitude). `FIELD11_ANGLE_UI_WORD_OFFSET` fixed
204+11→**215** in `constants.py` (was 216 - a **-1** word shift, same
direction as frequency's fix, opposite RMS's +1 - reinforcing that these
are 3 independent transcription errors in the original doc-derived
tables, not one systematic bug).

**Also discovered the field's physical unit is degrees, not radians** -
the raw decode naturally comes out matching the front panel's degree
reading directly (no `math.radians`/`math.degrees` conversion needed).
`device.py`'s `AnalogInputs.phi_*` fields and `04_test_get_analog_inputs.py`'s
print statements previously assumed/labelled radians; both corrected.

Bonus: `Ib`/`Ic` (phases with no signal connected) now read as small
nonzero noise (~1 deg) at the corrected offset, rather than the suspicious
exact `0.0000` the old offset gave — consistent with genuinely hitting
live measurement circuitry picking up ADC noise floor, rather than empty
reserved memory. Further circumstantial support for the fix, on top of
the direct front-panel match.

This test also doubled as a second independent RMS/frequency data point
at different magnitudes than Step 4.3 (13.03V/0.4996A/49.37Hz here vs.
10.02V/0.0998A/49.43Hz there) - both self-consistent with their
respective front-panel readings, reinforcing Step 4.3's fixes weren't a
single-point coincidence.

One unrelated observation during this test: `_read_exact` hit a
short-read `TimeoutError` twice (got 4535 or 4537 bytes of a declared
4538-byte frame, even with `--timeout 8`) - a transient serial reliability
quirk, not a decode issue; a plain retry succeeded both times. Not
investigated further; flagged in "Current status" next-action.

## Step 4.5 — Reverifying fields 6-10 and 13-19 against hardware (2026-08-19)

Continuing with the same inductive-load setup from Step 4.4
(Ia≈0.4996A, Ua≈13.033V, phi≈-45.96deg, all now hardware-confirmed),
worked through the remaining per-channel-block fields.

**Field 18 (cos(phi))**: found at its **documented, unchanged** offset
240 - decodes to 0.6992 vs. an independently-computed cos(-45.96deg) =
0.6951 (0.6% off). This was the first sign that not *everything* needed a
shift - only specific fields had individual transcription bugs.

**Fields 13-17 (per-block power) and 20-27 (footer summed power) - real
bug found, but a decode-format bug, not an offset bug.** Computed
expected P/S/Q from the confirmed RMS+angle values (P=U*I*cos(phi),
S=U*I, Q=U*I*sin(phi)) and searched the documented offsets - initially
found nothing remotely close (values in the hundreds of millions or
unrelated small numbers). Brute-forced word-order permutations of
`decode_power48`'s 3 magnitude words against the footer region and found
a clean match: the magnitude must be assembled **most-significant-word-
first** (`words[3],[2],[1]` concatenated in that order), the opposite of
`words_to_uint`'s least-significant-first convention used by every other
multi-word field in this protocol - an easy thing to get wrong when
implementing from the docs' abstract description, since nothing else in
the protocol works this way. Fixed in `codecs.decode_power48`. With the
fix, **every field 13-17 and 20-27 matches cleanly at its documented
offset, no position shift needed at all**:

| field | word offset | decoded | expected | match |
|---|---|---|---|---|
| 13/20 active power | 220 / footer 0 | 4.5524 | 4.5258 | 0.6% |
| 14/21 apparent power | 224 / footer 4 | 6.5105 | 6.5105 | exact |
| 15/22 reactive (method A) | 228 / footer 8 | ~0.0003 | - | see below |
| 16/23 reactive (method B) | 232 / footer 12 | -4.6540 | -4.6802 | 0.6% |
| 17/24 reactive (method C) | 236 / footer 16 | -4.6542 | -4.6802 | 0.6% |
| 25-27 sequence powers | footer 20/24/28 | 1.5173 (all 3 identical) | - | plausible for single-phase-only test |

Field 15/22's near-zero reading isn't a red flag - the docs describe
three *different* reactive-power computation methods, and it's
believable one gives a materially different result than the simple
U*I*sin(phi) estimate for a real (harmonically imperfect) waveform, while
the other two agree closely with each other and with the estimate.

**Fields 6-10 - still unresolved, no independent ground truth
available.** Neither the front panel nor any reference instrument here
reports "RMS at 1st harmonic," "RMS line value," "mean/DC component," or
"mean-rectified value" directly, so there's nothing to pattern-match
against the way P/S/Q or the front-panel Ia/Ua/phi readings served for
other fields. Scanning this region turned up several candidate offsets
whose *values* looked plausible (close to the RMS magnitude, as expected
for "RMS at 1st harmonic" on a near-sinusoidal signal) but multiple
adjacent offsets all looked equally plausible, with nothing to
discriminate between them.

What the confirmed neighbors *do* pin down precisely: field 5 real-ends
at word 207 (205 + width 2), field 11 real-starts at word 215 - leaving
exactly **8 words for fields 6,7,8,9,10, a 2-word shortfall against
their documented 10 words (5 fields x 2 words each)**. At least one field
in this range is narrower than documented, or one doesn't exist as a
real distinct field in this firmware - which one, unresolved.

Separately (lower confidence): field 11 real-ends at word 217 (if its
width is genuinely 2, unconfirmed) and field 13 confirmed real-starts at
220, leaving 3 words for field 12 (documented width 2) - a 1-word
*surplus*, the opposite direction from fields 6-10's shortfall. Field 12
("angle u^u", angle between two voltage phases) is likely unresolvable
with this test rig regardless of scanning effort, since it needs two
energized voltage phases and this setup only ever drives phase A.

**Unrelated observation**: the transient short-read `TimeoutError` first
noted in Step 4.4 recurred several more times during this round of
testing - now often enough (roughly 1 in 3 requests when polling in
quick succession) to suspect it's a real, reproducible quirk rather than
a fluke, though still not investigated. A plain retry has resolved it
every time so far.

## Step 4.6 — Investigating the transient short-read timeout (2026-08-19)

The short-read `TimeoutError` first seen in Step 4.4 kept recurring
through Step 4.5's back-to-back diagnostic queries, often enough to
suspect a real, reproducible cause rather than a fluke. Two things worth
knowing before the root-cause finding below:

- Windows pyserial (`serialwin32.py`) issues one `ReadFile` per `read(n)`
  call with a **flat total timeout regardless of `n`** (`ReadTotalTimeoutConstant`,
  no multiplier) - so a request for 4538 bytes gets exactly the configured
  `timeout` seconds to arrive in full, not extra time scaled to the byte
  count. This isn't itself the bug, but it does mean there's no built-in
  slack beyond whatever `timeout` you pass.
- `transport.request()` already calls `reset_input_buffer()` fresh before
  every write, so stale bytes from a previous exchange can't bleed into
  the next one - ruled out as a contributing factor.

Measured actual latency directly: 15 back-to-back Mode 3 requests (no
artificial delay between them, `timeout=15s`) via a throwaway timing
probe. Result: **14 of 15 took a remarkably steady 2.476-2.478s** (about
1ms of jitter across the whole batch - this is a real internal
computation constant, not something that "usually finishes around 3s"
the way the docs' vague "up to ~3s" phrasing suggested). The 15th request
took **17.086s and still came up 3 bytes short even with a 15-second
timeout** - not "response time nudged past a slightly-too-tight budget",
a genuine multi-second device-side stall far outside its own normal
behavior. A second batch of 25 requests immediately after (same
back-to-back pattern) completed all 25 cleanly at the steady ~2.477s
figure, with no stall - consistent with an intermittent, roughly
1-in-15-to-25 event, not something that reproduces on demand.

**Conclusion: this is a real, occasional device-side hiccup, not a bug in
this library's transport code.** Every manual retry throughout this
entire session (this stall and the two similar ones noted in Steps 4.4
and 4.5) recovered cleanly on the very next attempt - simple re-issue is
a reliable, well-evidenced mitigation. Added `retries: int = 1` to
`SerialTransport.request()` (default: one automatic retry on
`TimeoutError`, `retries=0` to restore old fail-fast behaviour);
`EMonDevice._call()` picks this up automatically since it doesn't pass
`retries` explicitly. Verified: a fresh 25-request back-to-back batch
through the retry-enabled `request()` produced zero visible failures.

Root cause of the underlying device-side stall itself (why Mode 3
occasionally takes 15s+ instead of its steady ~2.5s) is **not**
determined - out of scope to chase further without vendor firmware
insight; the retry is a practical mitigation, not a fix for whatever
causes it.

## Step 5 — USB carries the same protocol, at 3,000,000 baud / 8N2 (2026-08-19)

Before starting the C++ port, checked whether the EMon protocol is the
same over the device's USB port as over RS-232 — the device has both,
and USB is strongly preferred for the eventual C++ consumer since so few
PCs still have a real serial port. This took several wrong turns before
landing on the actual answer.

**USB enumerates as a plain FTDI virtual COM port** (`USB Serial Port`,
VID:PID `0403:6001`) — no vendor driver needed, shows up like any other
COM port. First attempt: probe it with the confirmed RS-232 parameters
(115200 8N1) — total silence, the same "zero bytes at every baud" symptom
that plagued the original RS-232 bring-up in Step 3.1. Unlike that case,
this one needed an **exhaustive** sweep before it gave up any information:
13 bauds (1200 through 921600) x 4 parity/stop-bit combinations x 4
DTR/RTS states — 200+ combinations, all silent. That's stronger evidence
than the RS-232 case ever produced (a wrong CRC guess should still elicit
a 6-byte NOT-NORMA reply at the *right* baud), which pointed at something
more fundamental than a simple parameter mismatch.

**Dead end: the vendor's dedicated USB program needs genuinely exclusive
device access.** It refused to connect at all if any other USB device was
present, and separately refused to connect while *any* other process (even
a passive one) had an open handle on the port - true even through HHD's
"split" mode (two independent virtual ports), which had been transparent
to every RS-232 tool all session. That ruled out the passive
COM-port-tap approach that worked so well for RS-232 (Steps 3.2-3.6) and
motivated switching to genuine USB packet capture (USBPcap + Wireshark),
which taps at the host-controller level below any COM-port software layer
and can't interfere with anything.

**Dead end: a misidentified device.** The first USBPcap capture found
what looked like the real protocol - byte-for-byte identical to a known
RS-232 capture, including the CRC - tunneled inside what appeared to be
raw commands on a USB Mass Storage interface (SCSI Bulk-Only Transport
endpoints, alongside Windows' own automatic "Test Unit Ready" disk
polling). This led down a real rabbit hole (confirmed the exact endpoint
mapping via the USB configuration descriptor, worked out that reaching it
from C++ would need raw `DeviceIoControl`/`IOCTL_SCSI_PASS_THROUGH_DIRECT`
rather than `QSerialPort`) before the user caught the actual problem:
that USB device (VID:PID `0xc251:0x5750`) was an unrelated DIY project of
theirs that happened to be connected to the same PC at the same time, not
the meter. Confirmed by disconnecting it and re-enumerating - it vanished,
leaving only the real meter's FTDI device. **The byte-identical protocol
match was a complete coincidence of timing** (or possibly a completely
separate reason to revisit if it ever matters again) - worth remembering
as a lesson: a strong content-level match doesn't prove you've identified
the right *device*, only the right *bytes*, when multiple USB devices are
active during a capture. Always verify via VID:PID/bus/address which
physical device a capture frame actually belongs to, especially when
multiple devices are present.

**Real answer, found the same way, on the correct device this time**:
re-ran the USBPcap capture with the DIY device physically disconnected
and the COM-port split removed (to let the vendor program get its needed
exclusive access). The vendor program talks to the FTDI chip via direct
USB vendor control transfers (FTDI's low-level protocol: `Reset`,
`SetBaudRate`, `SetLatTimer`, `SetData`, `GetModemStat`) rather than
through the standard Windows serial API - explaining why it needs
exclusive access and why nothing we tried via `pyserial`/`QSerialPort`-style
APIs worked initially. The critical two parameters, read directly out of
the `SetBaudRate` and `SetData` control transfers:

- **Baud rate**: raw divisor value `0x0000` in FTDI's encoding, which per
  FTDI's documented special-case table means **3,000,000 baud** (not
  2,000,000, which is divisor `1`) - a rate never tried in any sweep
  before this, since prior sweeps went up to 921,600.
- **Stop bits**: **2**, not 1 - so 8N2, not 8N1. RS-232 uses 8N1 at
  115200; USB uses 8N2 at 3,000,000. Different framing *and* different
  baud, which is why the earlier broad sweep (which did include 8N2, but
  only up to 921600) never landed on the right combination - the two
  differences needed to be found together, not independently.

The actual application-layer bytes captured on the wire (`02 0f 04 00 00
00 00 01 13 cf` for a Mode 15 request, full matching 518-byte device-info
response) are **byte-for-byte identical** to `build_request()`'s own
output and to previously-captured RS-232 frames - confirmed by
replicating the exact parameters directly:

```
baud=3000000, bytesize=8, parity=NONE, stopbits=2 (8N2)
```

produces a clean, CRC-valid, full Mode 3 response (4542 bytes) when
sent the same `build_request(Mode.CALC_ALL)` bytes used throughout this
whole session. Latency is comparable to the RS-232 baseline (~2s in the
one measurement taken) and shows the same kind of occasional
multi-second stall characterized in Step 4.6 (first attempt: 12s and
short; a plain retry succeeded cleanly).

**Practical implications:**
- The protocol layer (CRC-16/XMODEM, frame format, all field offsets and
  codecs) needs **zero changes** for USB - it's the exact same protocol.
  Only the transport's serial parameters differ.
- `QSerialPort` handles this natively - custom baud rates and 2-stop-bit
  framing are both standard features, no raw Windows USB/SCSI API needed.
  The earlier SCSI-pass-through conclusion doesn't apply; that whole
  branch of the investigation was chasing the wrong device.
- The C++ transport layer should support both parameter sets (RS-232:
  115200 8N1; USB: 3,000,000 8N2) and the Step 4.6 retry mitigation,
  since the occasional stall shows up on both interfaces.
- Not yet done: running the *actual* `EMonDevice`/Python library (not
  just raw probe bytes) against USB, since `transport.py`'s
  `SerialTransport` currently hardcodes `STOPBITS_ONE`. Would need a
  `stopbits` constructor parameter before `get_device_info()` /
  `get_analog_inputs()` could be run against COM11 directly. Low
  priority given the C++ port is next and will need this parameterization
  anyway.

## Step 6 — C++ port: `libavm-emon` Qt/CMake shared library (2026-08-19)

Repo restructured for this: Python sources moved `src/python/` → `py/`
(commit `ab2ec6a`) to free up `src/` for C++, and `src/`/`include/` now
hold the new C++ project instead.

**Scope, deliberately narrow**: only `GetDeviceInfo`, `SetLimits`, and
`GetAnalogInputs` are implemented (not the full Python surface -
`set_current_limits` verification-via-Mode-3-footer, the CRC-finder
script, the sniffer, etc. stay Python-only for now). Same
frame/CRC/codec/field-offset logic as the Python implementation, ported
directly - every correction found in Steps 4.3-4.5 (RMS/frequency/angle
offsets, the power-field word order, degrees-not-radians) carried over
with a comment pointing back to the relevant step.

**Structure**:
```
CMakeLists.txt                    project(libavm-emon), target "libavm-emon"
include/libavm-emon/
  emon_export.h                   EMON_EXPORTS/EMON_IMPORTS/HAVE_STATIC_BUILD macro
  Exceptions.h                    ProtocolError, DeviceError
  Types.h                         LinkType, DeviceInfo, AppliedLimits, AnalogInputs
  Device.h                        public Device class
src/
  Constants.h                     mode numbers, confirmed field offsets
  Crc.h/.cpp                      CRC-16/XMODEM
  Codecs.h/.cpp                   fixed-point/ASCII/power-field decoders
  Frame.h/.cpp                    request/response framing
  Transport.h/.cpp                QSerialPort transport, both link types, retry mitigation
  Device.cpp                      GetDeviceInfo / SetLimits / GetAnalogInputs
package/, cmake/, VERSION.in      packaging - see below
```

**API design choice**: `Device`'s constructor takes a `LinkType` enum
(`Rs232` or `Usb`) rather than raw baud-rate/stop-bits parameters. It
maps directly to the two parameter sets actually confirmed against real
hardware (115200 8N1 for RS-232 per Step 3.1, 3,000,000 8N2 for USB per
Step 5) - there's deliberately no way to pass an untested combination.
`Transport::request()` carries over the Step 4.6 retry-on-timeout
mitigation (default `retries=1`), since the occasional multi-second
device-side stall was observed on both link types.

**Build verified**: Qt 6.10.1 MSVC2022 64-bit (local install at
`D:\qt\6.10.1\msvc2022_64`, no system-wide Qt or CMake was present on
this machine otherwise - used VS2022 Community's bundled CMake/MSVC
toolchain). Configured via the Visual Studio 17 2022 generator, built via
MSBuild. **Zero compiler warnings or errors**, including no MSVC C4275
(dll-interface base class) warning on the `Exception` hierarchy's
`std::runtime_error` base, which was a specific worry given it's exported
across the DLL boundary.

**Packaging conventions ported from the sibling `libavm-gen` project**
(same organization, AVM-Energo, different repo - left entirely untouched
per instruction), for consistency with their other C++ libraries:

- `BUILD_SHARED_LIBS` switch: shared build defines `EMON_EXPORTS`
  (build)/`EMON_IMPORTS` (consumers); static build defines
  `HAVE_STATIC_BUILD` instead. Verified both configurations actually
  build correctly (shared -> `.dll`+`.lib`+`.exp`; static -> `.lib` only,
  no `.dll`). **Note**: this switches the *default* from the original
  ask's "shared library" to CMake's native `BUILD_SHARED_LIBS` default
  (OFF, i.e. static) unless explicitly passed `-DBUILD_SHARED_LIBS=ON` -
  a real behavior change from the first cut of this project, flagged to
  the user, not yet resolved either way (their call whether to add
  `option(BUILD_SHARED_LIBS ... ON)` to flip the default back).
- `include/avm_emon/` renamed to `include/libavm-emon/` to match
  `libavm-gen`'s convention of install-path folder name = package name.
  The C++ `namespace avm_emon` itself is untouched - matches how the
  reference project keeps its own namespaces (`Files`, `Colors`,
  `Error`, ...) independent of the `libavm-gen` folder name; namespace
  identifiers can't contain hyphens anyway, so this was never going to be
  a 1:1 rename.
  `Global.h` renamed to `emon_export.h`, `#if defined(...)` control
  macros renamed to `EMON_EXPORTS`/`EMON_IMPORTS` (matching `libavm-gen`'s
  `GEN_EXPORTS`/`GEN_IMPORTS` short-name pattern); kept the actual export
  macro's own name as `AVM_EMON_EXPORT` since it was already used across
  every public header before this pass.
- `package/src/{debian,debian-dev}/{control.in,changelog,substvars}` and
  `package/src/rpm/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SRPMS}/.gitkeep`
  + `SPECS/libavm-emon.spec` - structurally identical to `libavm-gen`,
  content adapted (package name/description; `Maintainer: AVM-Energo
  <support@avmenergo.ru>` and `License: GPLv3` copied as-is since they're
  evidently this org's standing convention, not verified against this
  specific project - flagged to the user as an assumption).
- `cmake/arch/{amd64,arm64,armhf}.cmake` (Linux clang cross-compile
  toolchains) copied verbatim - nothing in them is project-specific.
- `cmake/libavm-emonConfig.cmake.in`, `VERSION.in` -> `package/src/VERSION`
  at configure time, `avm-energo::libavm-emon` alias target,
  `VERSION`/`SOVERSION`/`PREFIX ""` target properties, `install()` +
  `Config.cmake`/`ConfigVersion.cmake` generation - all structurally
  identical to `libavm-gen`.

`.gitignore` extended with C++/Qt/MSVC build-artifact patterns merged in
from `libavm-gen`'s.

**Validated against real hardware since** - see "Step 6.1" below: found
and fixed three real bugs the compile-only pass couldn't catch (DTR/RTS,
QSerialPort configuration ordering, and a large-USB-transfer read
pattern), then confirmed all three functions work correctly over both
link types.

## Step 6.1 — C++ library validated against real hardware (2026-08-19)

Added `tests/manual/main.cpp` + `tests/CMakeLists.txt` - a small
dev-only executable (not installed/packaged, only built when
`libavm-emon` is the top-level project) mirroring `py/scripts/02/03/04`'s
CLI conventions, to actually exercise the compiled library against the
device: `manual_test <port> <rs232|usb> <info|analog|limits ...>`.

First run crashed immediately with `STATUS_DLL_NOT_FOUND` - a red
herring chased partway down an ICU-dependency investigation before the
user pointed out the real cause: `libavm-emon.dll` (in `build/Release/`)
and `manual_test.exe` (in `build/tests/Release/`) are in different
directories, and only the Qt bin directory had been added to `PATH`, not
the library's own output directory. Both need to be on `PATH` (or the
DLL copied next to the exe) for the loader to find it. Lesson: `dumpbin
/dependents` is useful for diagnosing *which* DLL is missing, but a
"can't find X.dll" symptom is often simpler than it looks - check
whether X is just not colocated with the exe or on `PATH` before
assuming a deeper problem.

With that resolved, `manual_test COM1 rs232 info` opened the port fine
but got zero bytes back - identical symptom to the original RS-232
bring-up struggle (Step 3.1), and confirmed via the already-working
Python library that the device *was* responding normally on that same
port at that moment, so the cause had to be in the C++ code, not
hardware. Two real bugs found and fixed, both worth remembering for any
future QSerialPort-based work:

1. **Configuration order**: parameters (`setBaudRate`, `setStopBits`,
   etc.) were being set *before* `open()`. Qt's documentation says this
   should work, but in practice the Windows backend doesn't reliably
   apply pre-open settings. Fixed by opening first, then configuring
   (matching Qt's own examples) - `Transport::Transport()` now does this
   unconditionally.
2. **DTR/RTS**: `pyserial` (used throughout this project's Python
   implementation) apparently asserts DTR/RTS by default; `QSerialPort`
   does not. Without explicitly asserting both, the device produced
   total silence - no error, just zero bytes back, indistinguishable
   from a wrong baud/framing guess purely from the symptom. This was the
   fix that actually got a response - confirmed via embedding diagnostics
   directly in exception messages (`qDebug()` wasn't reaching stderr in
   this environment, routed to `OutputDebugString` instead since no
   console was attached the way Qt expected - a good reminder not to
   assume `qDebug()` output is visible without checking). Whether the
   device genuinely needs these lines electrically (matching a
   historical Step 3.1 theory about opto-isolator power) or an
   intermediate adapter does, asserting both unconditionally is now what
   `Transport`'s constructor does.

With those two fixes, `GetDeviceInfo`, `SetLimits`, and
`GetAnalogInputs` all worked correctly over RS-232, producing output
byte-identical to the reference Python implementation across repeated
runs (including the same near-zero readings for `GetAnalogInputs` with
no load connected, matching Step 4.1's finding exactly).

**USB uncovered a third, more interesting issue.** `GetDeviceInfo` (518
bytes) and `SetLimits` (16 bytes) worked reliably every time over USB.
`GetAnalogInputs` (4536-byte payload, 4542-4538 bytes on the wire
depending on how you count) failed intermittently - not the rare,
already-characterized Step 4.6 device stall, but a much more frequent
short-read, missing anywhere from a handful to a few hundred bytes, no
two failures losing quite the same amount. Chased this through several
hypotheses before finding the real one:

- Tried polling in short time slices instead of one long
  `waitForReadyRead()` call - no improvement.
- Tried an event-loop-driven read (`readyRead` signal + `QEventLoop`,
  the pattern Qt's own examples use for bulk reads) instead of the
  blocking wait/read pattern - no improvement either, though kept as
  good practice regardless.
- **The decisive test**: reproduced the exact same intermittent
  short-read using raw `pyserial` at the same USB parameters (3,000,000
  baud, 8N2), completely independent of Qt/C++ - 3 of 5 attempts timed
  out short (missing tens to hundreds of bytes), 2 of 5 succeeded
  cleanly in ~2s. **This proves the issue is a genuine hardware/driver
  characteristic of this specific USB link at 3,000,000 baud for large
  transfers, not a bug in this library's transport code** - it affects
  Python identically. Also established: once a transfer starts stalling,
  waiting longer within that same attempt doesn't reliably help (pyserial
  attempts still came up short even at their own 15-17s timeout) -
  abandoning a stalled attempt and retrying fresh works better than one
  very long wait.

**Practical mitigation, not a root-cause fix** (the root cause is outside
this library's control): `Transport::request()`'s default `retries`
raised from 1 to 4, documented clearly as coping with a real, confirmed
link characteristic rather than masking a bug. With this, `GetAnalogInputs`
over USB succeeds reliably in practice (fast, ~2s, when the first good
attempt lands; a few retries, each cheap once they fail fast rather than
hanging, when it doesn't) - not a 100% first-try guarantee, but consistent
with what the reference Python implementation achieves too, since it has
the identical underlying limitation. A held-open connection's retries did
*not* show an obvious "once bad, always bad for this session" pattern
worth chasing further - fresh Python opens showed the same ~40-60% raw
success rate per attempt as retries within one open C++ connection.

**Root cause of the underlying stall itself remains unknown** - likely
either the device's own transmission occasionally pausing mid-payload
(a large-payload-specific variant of Step 4.6's stall) or an OS/driver
buffering limitation specific to sustaining 3,000,000 baud for a
multi-KB burst. Out of scope to chase further without deeper access to
either the device firmware or the Windows serial driver stack - the
retry mitigation is the practical stopping point here, same as Step 4.6.

RS-232 hardware validation had to stop mid-session (the test cable
became unavailable), but the three functions were already confirmed
working correctly over RS-232, repeatedly, before that happened - see
above.

## Step 6.2 — Examples + docs, and a live device-state confirmation (2026-08-19)

Added `examples/` (three focused, runnable programs - `get_device_info`,
`get_analog_inputs`, `set_limits` - distinct from `tests/manual/`'s
diagnostic swiss-army-knife tool), a `README.md` (structure modeled on
the `coma` project's, scaled to this library's size), and `.gitlab-ci.yml`
(structure mirrored from `libavm-gen`'s - same job names/images/MinGW
cross-compile approach, flagged as untested whether those CI images
actually have `Qt6SerialPort` available, since `libavm-gen`'s own comment
only lists "Qt6 base+svg"). All three examples build cleanly and were
smoke-tested against real hardware over USB.

While smoke-testing, `set_limits`'s echoed `wiring_scheme` came back as
`0b100` (1-phase-2-wire) instead of the `0b001` (3-phase-4-wire) seen
consistently everywhere else in this whole document - unprompted by
anything this session had done, since `wiring_scheme` is read back from
the device's own state, not one of the values a `SetLimits` call
actually sends. Flagged this to the user rather than assuming a bug; it
turned out to be a deliberate test - they'd changed the meter's physical
wiring configuration specifically to check whether the library would
pick it up correctly. It did, through two independent paths that agreed
with each other: `SetLimits`'s echoed `wiring_scheme`, and separately
`GetAnalogInputs`'s Ib/Ic phase angles reading exact `0.0000` deg
(consistent with those channels being genuinely unmeasured in
single-phase mode, rather than the small ADC noise floor seen throughout
prior sessions when phases were wired but simply unloaded - e.g. Step
4.4). Reconfirmed stable across a second independent pair of calls. Left
in the 1-phase-2-wire configuration at the user's request - not restored
to the earlier 3-phase-4-wire baseline.

## Step 7 — Frequency (field 33) reads ~1-2% low vs. the front panel; root-caused as device-side measurement noise, not a decode bug (2026-08-26)

Resumed after a gap (the `retomemon`/`libavm-retom` integration projects
happened in between - out of scope for this file, see their own repos).
User reported: driving the EMon's inputs from a RETOM-51 reference source
set to exactly 50.000Hz, `get_analog_inputs()`'s frequency read ~49.285Hz
over USB, while switching the meter to its own front-panel display mode
(no USB) showed a precise 49.999-50.001Hz for the *same physical setup*.
Asked to re-investigate field 33's offset/formula given this gap.

**Session-start snag, unrelated to the actual investigation:** the
device's own USB port is behind a physical mode switch - display and USB
are mutually exclusive, and the switch had been left in display mode.
Symptom was identical total silence to the old RS-232-era debugging
(`expected 4 header bytes, got 0`); resolved once the user flipped it to
USB mode. Also hit the familiar "two FTDI adapters, ambiguous port"
trap again (unrelated leftover adapter plus the device's own FTDI) -
resolved the same way as before, by unplugging one and observing which
`/dev/ttyUSB*` node disappeared. **Pattern worth remembering for next
time:** always re-derive port identity by serial number or plug/unplug,
never trust `ttyUSB` numbering carrying over between sessions.

**Investigation.** Rather than rely on the user manually setting RETOM
and reading the front panel by eye (slow, and the display/USB interlock
above means the panel can't be watched *while* USB is live anyway), drove
RETOM directly from this session via `linretom.LinRetom51` (the
`retomemon` process from an earlier session was still holding
`/dev/ttyUSB2` and had to be stopped first - `fuser -v` found it).
`RetomConfig`/`LinRetom51` only auto-discover the port by VID:PID
(`0403:6001`), which is **the same VID:PID as the EMon's own FTDI chip** -
genuinely ambiguous auto-detection between the two devices. Worked around
by constructing `FtdiTransport(RETOM_VID, RETOM_PID, port='/dev/ttyUSB2')`
directly and assigning it over `LinRetom51`'s private `_t` before
`connect()`, rather than patching linretom's public API (out of scope for
this repo, and that project is under active independent development).

Applied a balanced 10V/0.1A 3-phase signal at exactly 50.000Hz (via
`apply()`, phases at 0°/-120°/+120°) and immediately pulled a live Mode 3
payload from the EMon over USB. RMS fields matched the applied signal
precisely (`Ua≈10.025, Ia≈0.0998`, consistent with Step 4.3's earlier
validation and with the current-ratio note in `Types.h`) - confirms the
signal genuinely reached the meter and the overall pipeline (RETOM →
meter → our decode) is sound; the investigation is specifically about
field 33, not a wiring/pipeline problem this time.

Frequency at the existing offset (footer word 39, `frac_bits=16`) read
**49.154Hz** for this single sample - in the same ~1-2%-low ballpark as
the user's 49.285Hz and Step 4.3/4.4's historical 49.43/49.37Hz readings.
Two follow-up tests to distinguish "wrong offset/formula" from "device-
side measurement noise":

1. **Six consecutive reads with the signal held perfectly steady**
   (50.000Hz the whole time, `keep_alive()`'d between reads to avoid
   RETOM's ~10-13s auto-reset): `49.4646, 49.2630, 49.6831, 49.2116,
   49.8544, 49.6976`. Real jitter (~0.6Hz spread across reads of the
   *same* nominal signal), not a fixed repeatable number - rules out "a
   deterministic formula bug would give the same wrong answer every
   time" in favor of "this field has genuine per-call measurement noise."
2. **Frequency sweep 50→60→50Hz**, 3 reads at each: `[49.068, 49.647,
   49.245]` → `[59.526, 59.526, 59.551]` → `[49.174, 49.624, 49.808]`.
   The offset **tracks the real frequency change precisely** (jumps from
   ~49 to ~59.5 and back), which conclusively rules out "wrong
   field/offset" - a wrong byte position would not track a real signal
   change like this. This is the offset's most direct validation yet,
   independent of Step 4.3's original single-point match.

**Bias analysis, from the sweep:** at 60Hz the three reads clustered
tightly (59.526/59.526/59.551, ~0.03Hz spread) with a bias of about
**-0.47Hz** below true; at 50Hz the bias averaged **-0.5 to -0.9Hz**
across the various samples. The bias is **roughly constant in absolute
Hz, not proportional to the true frequency** (a wrong `frac_bits` or
divisor would produce a *percentage* error that scales with frequency -
e.g. ~1.2% of 60Hz would be ~0.72Hz, not the ~0.47Hz actually seen).
Raw `uint32` values behind the 6 steady-state reads (`3241712` to
`3267258`, vs. `3276800` for an exact 50.000Hz) confirm this isn't
formula/offset noise - it's the raw device-reported count itself that
varies call to call, always landing somewhat below the true value, by an
amount roughly consistent with the field having something like a ~0.5-1Hz
quantization step and a systematic round-down (floor, not round-to-
nearest) bias of about half that step.

**Conclusion: not a decode bug.** The offset (word 39) is confirmed
correct via direct frequency-tracking, not just a plausible-looking
single-point match. The remaining ~1-2% low bias plus real inter-call
jitter is best explained by Mode 3's frequency field being a **one-shot,
relatively coarse measurement** - the vendor docs describe every Mode
3/4/5 field as a "разовый расчет" (single/one-time calculation), computed
fresh on each call, as opposed to the front panel's live display, which
almost certainly free-runs a continuous, long-window-averaged (or
PLL-tracked) measurement in the background and is therefore both far
more stable and far more accurate. There is no evidence of a second,
more-precise frequency field elsewhere in the payload - the whole-payload
scans in this session and Step 4.3 found nothing else in the plausible
range that isn't already accounted for by other documented fields.

**Decisive follow-up test, prompted by a sharp question:** the user
pointed out that the front panel showing an implausibly-exact "50.000Hz"
every single time was itself suspicious - could the display be snapping
to/faking a nominal grid frequency instead of genuinely measuring the
live input? Distinguishing that from "it's just a much more precise
measurement" needed a non-nominal test frequency, since 50.000Hz can't
tell the two apart. Held RETOM at an odd, clearly non-nominal **52.347Hz**
for several minutes (via the same `LinRetom51`/`keep_alive()` approach)
while the user switched the meter to its front-panel display mode and
read it directly: **the display showed 52.3459Hz - a match to within
0.002%.** This rules out "fixed/nominal display" conclusively: the panel
is genuinely tracking the live input, and doing so far more precisely
than Mode 3's one-shot field. Combined with the earlier 50→60→50Hz sweep
(which validated the *offset* by showing Mode 3's field tracks real
frequency changes, just noisily), the full picture is now settled on both
sides: word 39 is the right field with real ~1-2% one-shot noise, and the
front panel is a genuine, high-precision, presumably longer-window/
PLL-style measurement of the same underlying signal - not a coincidence,
not a decode bug, and not a fake/nominal display on either side.

**Statistical characterization.** Added the accuracy caveat to
`AnalogInputs::frequency`/`AnalogInputs.frequency` in `Types.h` and
`py/emon/device.py` (mirroring the existing RMS pre-calibration note in
both places), then took 40 consecutive one-shot readings with RETOM held
steady at exactly 50.000Hz to quantify the noise properly (small samples
so far - 6 in this session, a handful across historical sessions - had
been suggestive but not a real distribution). First attempt at zero
inter-read spacing hit a **worse-than-usual stall**: 4 consecutive
complete timeouts (32s of total silence, not the usual short-read), on a
usage pattern (dozens of back-to-back Mode 3 calls with no gap at all)
that hadn't been stress-tested before this session - the device recovered
on its own once probed again a bit later with a single simple request.
Re-ran with a small 0.2s gap between reads and a full port-reopen
fallback if retries were exhausted (to survive a repeat without losing
the whole run) - **40/40 succeeded cleanly this time, zero retries, zero
reopens needed**, at ≈0.42 reads/sec (≈2.4s/read, most of which is Mode
3's own real computation/response time, not the added gap):

```
mean   = 49.5683 Hz
median = 49.5015 Hz
stdev  = 0.4392 Hz
min    = 49.0207 Hz
max    = 50.9115 Hz
```

Refines the earlier 6-sample read: with a proper sample size the
distribution looks like genuine, roughly-symmetric one-shot measurement
noise (σ≈0.44Hz) - a few readings landed *above* 50 (max 50.91), not the
strictly one-sided "always low" pattern the smaller sample suggested -
riding on top of a real ~0.43-0.5Hz (~0.9%) low shift in the center
(mean/median both sit there, not at 50.0). Consistent with, and a more
rigorous version of, the "one-shot coarse measurement" conclusion above;
doesn't change it, just replaces "±1-2%, based on a handful of points"
with an actual measured σ. **Possible follow-up, not done:** the
back-to-back-call stall risk observed here is a real, previously-
undocumented interaction between rapid Mode-3 polling and device
stability, distinct from the already-known Step 4.6/6.1 stall - worth a
line in `mindflow.md`/`Device.h`/`device.py` recommending callers pace
repeated `getAnalogInputs()`/`get_analog_inputs()` calls rather than
hammering them back-to-back, if this library ever grows a polling-loop
example.

**Second, larger sample (100 reads, same steady 50.000Hz reference,**
same 0.2s inter-read pacing): **100/100 succeeded cleanly, zero retries,
zero reopens** this time (the earlier stall didn't recur).

```
mean   = 49.6394 Hz
median = 49.6730 Hz
stdev  = 0.4167 Hz
min    = 49.0079 Hz
max    = 50.9819 Hz
range  = 1.9740 Hz
```

0.1Hz-bin histogram (per-bin counts, `[lo,hi)`):
`[49.0,49.1)=8 [49.1,49.2)=6 [49.2,49.3)=11 [49.3,49.4)=9 [49.4,49.5)=4
[49.5,49.6)=8 [49.6,49.7)=8 [49.7,49.8)=17 [49.8,49.9)=11 [49.9,50.0)=6
[50.0,50.1)=3 [50.1,50.2)=0 [50.2,50.3)=2 [50.3,50.4)=2 [50.4,50.5)=1
[50.5,50.6)=0 [50.6,50.7)=0 [50.7,50.8)=2 [50.8,50.9)=0 [50.9,51.0)=2`.

Consistent with the 40-sample run (mean/median/stdev all within ~0.1Hz of
each other across the two independent samples) - not a fluke. Shape is a
rough, somewhat left-of-50-skewed hump concentrated in 49.0-50.0Hz (86 of
100 readings), with a thin, scattered tail out to nearly 51Hz rather than
a clean symmetric bell curve or a hard floor - consistent with "real
one-shot measurement noise with a systematic low center," not with a
simple fixed-offset bug (which wouldn't produce this much spread) or with
truncation-only noise (which would show a hard wall at 50.0 with nothing
above it - it doesn't; 14 of 100 readings exceed 50.0).

**Third sample, another 100 reads, same setup** (steady 50.000Hz, same
0.2s pacing): **100/100 succeeded cleanly again**, zero retries/reopens.

```
mean   = 49.5818 Hz
median = 49.5309 Hz
stdev  = 0.4557 Hz
min    = 49.0028 Hz
max    = 50.9991 Hz
```
91/100 below 50.0Hz (vs. 86/100 in the previous 100-sample run - some
run-to-run variation in the tail proportion, same overall shape).

**Notable pattern across all three independent samples so far (n=40,
100, 100):** mean/median cluster tightly (49.50-49.67Hz, i.e. within
~0.17Hz of each other across all three runs) and stdev is consistently
0.42-0.46Hz - reproducible, not a fluke of any one run. More strikingly,
**the min and max are suspiciously stable too**: min landed at 49.0207,
49.0079, 49.0028Hz across the three runs (all within 0.02Hz of each
other), and max at 50.9115, 50.9819, 50.9991Hz (within 0.09Hz of each
other) - despite each run being 40-100 independent one-shot
measurements. Unbounded/Gaussian-like noise would be expected to
occasionally produce a wider outlier as sample size grows (more samples
= more chances to see the tail); instead the extremes stay pinned to
almost the same values every time. This looks less like open-ended
random noise and more like **the measurement or its reporting has a
real, roughly-±2%-wide bound** (something clamping/saturating raw values
to within about 49.0-51.0Hz of a nominal-50Hz lock), though the exact
mechanism (ADC/counter range limit, an internal “close enough to
nominal” snap-and-clamp, or something else in the one-shot calculation)
hasn't been identified and would need lower-level access (e.g. varying
the *true* input frequency further from 50Hz, or firmware-level
information) to pin down further. Not pursued further this session -
flagging as a genuinely interesting open detail for whoever picks this
up next, not a blocker for anything.

**Mode 5 cross-check.** User noted the vendor docs list field 33
(frequency) at the same footer position for Mode 5 too (docs: Mode 5
averages over a shorter ~0.32s window, vs. Mode 3's fuller/slower
computation) - worth checking whether Mode 5 gives a cleaner reading, or
whether Mode 3's frequency value is really just reusing the same
underlying routine as Mode 5 (consistent with Mode 3 being documented as
effectively the union of Mode 4's and Mode 5's field coverage). Ran 20
reads of each, back-to-back in the same session, same steady 50.000Hz
reference, same word-39 offset:

```
                Mode 5 (CALC_HARMONICS)   Mode 3 (CALC_ALL)
mean            49.7445 Hz                49.4956 Hz
median          49.4948 Hz                49.4720 Hz
stdev           0.5570 Hz                 0.3865 Hz
min / max       49.0467 / 50.7819 Hz      49.0500 / 50.6192 Hz
reads/sec       1.149                     0.418
```

Mode 5 is ~2.75x faster per read (matches the docs' shorter averaging
window claim) but **statistically the same character of noise** - same
low-shifted center (~49.5Hz vs the true 50.0), same rough range
(~49.0-50.8Hz), same order of magnitude of jitter (Mode 5 slightly
noisier, consistent with less averaging). No evidence Mode 5 is either
more accurate or fundamentally different - supports "Mode 3's frequency
field reuses the same underlying quick-calc routine as Mode 5" rather
than "Mode 3 has its own separate, buggier frequency calculation."
Practical takeaway: switching to Mode 5 buys polling speed (useful if a
caller wants faster, if noisier, frequency-only updates without the rest
of Mode 3's payload) but not accuracy - the ±1-2%/σ≈0.4-0.5Hz
characterization applies to both modes, not just Mode 3.

**Practical implications for `get_analog_inputs()` callers:** field 33
should be treated as accurate to roughly **±1-2%** (not the sub-0.01%
implied by comparing it to a front-panel reading), consistent with the
existing "raw pre-calibration" caveat already on `AnalogInputs`. If a
caller needs better frequency precision than that, averaging multiple
consecutive Mode 3 calls narrows the *jitter* but did not fully remove
the *bias* in this session's samples (a 6-sample average was still
~49.53Hz, ~0.9% low) - so averaging alone is not a full fix; there is no
known way to get front-panel-grade frequency precision through this
protocol field. Worth documenting on `AnalogInputs::frequency` /
`AnalogInputs.frequency` directly (not yet done as of this entry) so
future callers don't rediscover this by surprise.

No code changes made this session (RETOM was disconnected from EMon and
returned to a stopped state via `safety_stop()`/`disconnect()`
afterward) - this was purely an investigation to confirm/refute a
suspected offset bug, and it came back "the offset is right, the
underlying field is just imprecise by design." Next step, if the user
wants it: add the accuracy caveat to `Types.h`/`Types.py`'s
`AnalogInputs.frequency` docstring, mirroring how the RMS pre-calibration
caveat is already documented there.

## Open questions for hardware validation (step 4)

1. ~~Which CRC16 variant?~~ **Resolved** — CRC-16/XMODEM, big-endian on
   the wire. See "Step 3.2/3.3".
2. ~~What baud rate / parity / stop bits?~~ **Resolved** — 115200 8N1. See
   "Step 3.1".
3. Does the real device's Mode 15 response for the system area match the
   assumed layout at the offsets used in `device.py`? **Partially
   resolved** — device-type string confirmed (needs `swap_word_bytes`,
   see "Step 3.4"); serial_number/device_type_id offsets produced
   plausible-looking values but aren't cross-checked against the device's
   own display; still need to confirm the mode byte actually used is 15
   (not directly observed — see "Step 3.2/3.3" next-action note).
4. SW version/date field — now have real raw words (`0x0702`, `0x0153`)
   but still doesn't decode cleanly under any hypothesis tried so far. See
   "Step 3.4".
5. ~~Does field 5's RMS decoding need the "clamp/external-amplifier"
   exception (16/16 split, ÷65536) instead of the default (12/20 split,
   ÷1048576)?~~ **Resolved** — the default 12/20 split (÷1048576) is
   correct; the real bug was the field's *word offset* (204 vs the
   correct 205), not the frac_bits split. See "Step 4.3".
5b. ~~Is field 11 (angle)'s word offset also off?~~ **Resolved** — yes,
   -1 word (216→215), plus the unit is degrees not radians. Confirmed
   with an inductive load (-45.63deg front panel vs. -45.96deg decoded).
   See "Step 4.4".
5c. ~~Are fields 13-19's offsets also shifted?~~ **Resolved, no** — all
   check out at their documented offsets. The real bug there was a
   decode-format error in `decode_power48` (magnitude word order), fixed.
   See "Step 4.5".
5d. Fields 6-10's offsets - **still genuinely unresolved**, no
   independent ground truth available to test against (front panel
   doesn't expose these quantities). Confirmed there's a 2-word shortfall
   somewhere in this 5-field region (field 5 ends at word 207, field 11
   starts at word 215, only 8 words available for 10 documented words) -
   which specific field(s) account for it is unknown. See "Step 4.5".
5e. Field 12's offset - lower-priority unresolved; likely unresolvable
   with the current test rig regardless (needs two energized voltage
   phases to measure a U-to-U angle; this rig only drives phase A). See
   "Step 4.5".
6. What mode number does the vendor program actually send for the request
   side? Not directly observed across two 2026-08-19 captures (150s
   total, including a live reconnect) — see "Step 3.2/3.3" and "Step
   3.5". Near-certain root cause: the HHD split only mirrors device→PC
   traffic to the COM3 alias, so this tap structurally cannot see it.
   Would need a different tap (HHD "both directions" setting if one
   exists, or a physical RS-232 tap) to resolve directly; otherwise rely
   on the (now 3-for-3) payload-size matches as indirect confirmation.

## Step 8 — SW version field resolved against real hardware (2026-08-26)

Continuing the same session as Step 7. User asked to read device info
"as stated in vendor docs" - ran `get_device_info()`/`getDeviceInfo()`
against the live device (still on `/dev/ttyUSB0`, USB/3,000,000-baud/8N2)
and got the same values seen in every prior session (Steps 3.4/4.1):
`device_type="EM3.1KM - 02"`, `serial_number=519`, `device_type_id=3`,
`sw_version_raw=(0x0702, 0x0153)` - good consistency confirmation, but
nothing new until two things happened:

1. **Noticed a previously-unremarked oddity**: the docs' lookup table for
   `device_type_id` says `3 = ЭМ3.3Т, 2 = ЭМ3.2`, but this unit's own name
   string says `EM3.1KM`, not `EM3.3T`. Flagged to the user as a genuine
   discrepancy between what the doc's small ID table claims and what this
   device actually reports - not resolved, just surfaced (plausibly the
   ЭМ‑3.1КМ/ЭМ‑3.3Т variants share a type ID for some internal reason;
   the docs don't say enough to know).
2. **User provided ground truth from the device's own display**: it
   shows two version-looking numbers, `2.01` and `702.153.519`, and
   wasn't sure why there were two. This unlocked the SW-version field.

**The decode**: DAA-digit-decoding each raw word directly (`f"{word:04x}"`,
the exact same trick already used and confirmed for date fields elsewhere
in this protocol - see Step 1) gives `0x0702` -> `"0702"` -> **702**, and
`0x0153` -> `"0153"` -> **153**. Combined with the already-known
`serial_number=519`, that's **"702.153.519" - an exact match** to the
longer of the two numbers on the display. The doc's own worked examples
for this field (`0x0704,0x0101 => "707.101"`, etc.) were simply
misleading/wrong - no special multi-format handling was ever needed, just
the same plain DAA-digit decode used everywhere else in this protocol.
Re-reading the doc's own words-per-version-era table with fresh eyes
after having the real answer: it's plausible those "704.101"-style
examples were never meant to describe *this* device's exact field at all,
just illustrate the general DAA convention loosely - not worth more
excavation now that real hardware has settled it unambiguously.

**Bonus, unprompted confirmation**: the field immediately following the
version words (already hypothesized as a "SW version date," 2-word DAA,
never wired into `get_device_info` before this session - see Step 1)
decodes via the *same already-existing* `decode_daa_date`/equivalent to
**2014-06-24** - a genuinely plausible real calendar date, not garbage.
Strong independent evidence the whole offset/DAA-convention picture in
this area of the FRAM system area is right, not a lucky one-field
coincidence.

**The shorter "2.01" display value remains unexplained** - nothing in
the 512-byte system-area dump (re-fetched fresh this session, raw hex
dumped in full for offsets 0-64) corresponds to "2" and "01" anywhere
near the version/date/type fields. `device_type_id` itself is `3`, not
`2`, ruling out the obvious confusion. Most likely explanation: a
separate bootloader/hardware-revision version that this protocol simply
doesn't expose at all (common in embedded devices - bootloader version is
often burned into a region the application-level read commands can't
reach). Not pursued further - no lead to chase without either the
vendor's own tooling or firmware-level access neither of which is
available here.

**Code changes**: added `DeviceInfo.sw_version`/`.swVersion` (decoded
string, e.g. `"702.153"`) and `.sw_version_date`/`.swVersionYear`+
`Month`+`Day` (decoded date) to both `py/emon/device.py` and
`include/libavm-emon/Types.h`, alongside the existing raw words (kept for
transparency, same pattern as every other decoded field in this
project). Added `decodeDaaDigitString`/`decodeDaaDate` to the C++
`Codecs.h`/`.cpp` (Python already had `decode_daa_digit_string`/
`decode_daa_date` from Step 1, just never wired up before now). Updated
`scripts/02_test_get_info.py`, `examples/get_device_info/main.cpp`, and
`tests/manual/main.cpp` to print the decoded values instead of "raw,
unresolved." Verified end to end against the real device on both
languages - `py/scripts` ad hoc check and both C++ tools all print
`sw_version=702.153`, `sw_version_date=2014-06-24`, matching each other
and the display. C++ side rebuilt clean (`ninja`) after every edit; a
handful of clangd single-file diagnostics appeared during editing
("Device.h file not found", undeclared `avm_emon`/`Qt::endl`/
`QStringLiteral", etc.) but are a broken isolated-index artifact, not
real - confirmed by every actual `ninja` build succeeding throughout.

## Step 9 — Current/voltage limits table decoded; raw-vs-calibrated ratio fully explained (2026-08-26)

Continuing the same session. Asked to develop the offset-44 "supported
limits" table and figure out its practical meaning, after Step 8's
static decode of `Types.h`'s "Коэффициент приведения I/U" (432/436) came
back as **literally zero on this unit** - confirmed by direct read, so
those fields are ruled out as the source of Step 4.3's observed ~10x
raw-to-display current scaling; they're simply unpopulated on this
firmware/config.

**User provided real ground truth from the device's own menu**: voltage
limits `240,120,60,30,10,5,2,1` V, current limits
`100,50,25,10,5,2.5,1,0.5,0.25,0.1,0.05` A (7 whole-amp values + 4
milliamp values). This is exactly the kind of independent reference this
table needed - decoding it blind (like the frequency field originally)
wasn't converging.

**Decode, fully confirmed against both lists:**
- Table starts at offset 44 as documented, but is **variable-length,
  terminated by a `type=0` entry** - it genuinely does spill into the
  doc's mislabeled "98: РЕЗЕРВ (150 bytes)" region, exactly as the doc's
  own "Сколько влезет!" ("however much fits!") comment hinted. Read
  offsets 44 through 248 in one pass to capture the whole thing.
- **Voltage (`type=1` in this table's own numbering, 8 entries):** raw
  int-words `15360,7680,3840,1920,640,320,128,64`, each divided by
  **64**, give exactly `240,120,60,30,10,5,2,1` - **8/8 exact match**.
- **Current transformers (`type=2`, 11 entries):** raw int-words divided
  by **512** (a different divisor than voltage's 64) give
  `100,50,25,10,5,2.5,1,0.5,0.25,0.0996,0.0488`. The last two needed the
  fractional word too - full formula
  `value = int_word/512 + frac_word/(512*65536)` - which turns
  `0.0996+0.00039=0.1` and `0.0488+0.00117=0.05` **exact**. **11/11 exact
  match**, including both fractional-correction cases landing precisely
  on the target.
- General formula, by extension: `value = int_word/D + frac_word/(D*65536)`,
  `D=64` for voltage, `D=512` for current-transformer entries. Two more
  categories exist in the table (`type=3` "clamps", 5 entries; `type=4`
  "УПТТ", 8 entries, the last matching the CT category's tail exactly) -
  not independently confirmed against ground truth (the user's two lists
  covered voltage and CT current only), left as unconfirmed-but-plausible
  since the same formula family is almost certainly at play.
- This resolves **item #6** from "Current status" (fields 6-10's
  independent-ground-truth problem was about *other* Mode 3 fields, not
  this table, but the same "no way to verify without real reference data"
  pattern applied here until now).

**Connecting this to the actual practical question (why does raw ≠
front-panel-calibrated?):** the known active setting from earlier
sessions is `current_limit_index=3`, which is exactly the **10A** entry
in the CT category (0-indexed: 100,50,25,**10**,5,2.5,1,0.5,0.25,0.1,0.05).
Step 4.3's real measurement showed the front panel reading was almost
exactly **10x** the raw reading (0.998V vs 0.0998A raw) - not a
coincidence. Hypothesis: `calibrated_I = raw_I * (selected_CT_rating_A / 1A)`,
"1A" being the assumed standard CT secondary convention; voltage gets no
such multiplication (Step 4.3 also showed `Ua` raw matched the panel
exactly, no scaling), consistent with voltage limits being an ADC-range/
precision selector rather than a transformer-ratio concept.

**Tested it directly**: held RETOM at a steady ~0.5A on phase A, cycled
`set_current_limits(type=0, index, 0)` through CT-category indices 0
(100A), 3 (10A), 6 (1A), 7 (0.5A), re-reading raw `Ia` after each:

```
index=0 (100A rating): raw Ia = 0.00000 A
index=3 (10A rating):  raw Ia = 0.49978 A
index=6 (1A rating):   raw Ia = 0.49981 A
index=7 (0.5A rating): raw Ia = 0.49979 A
```

**Raw stayed essentially constant (~0.4998A) across the 10A/1A/0.5A
selections** - direct proof that `current_limit_index` does NOT change
the raw ADC scaling at all; it's a pure configuration value (telling the
device what CT ratio to *assume* is physically installed), not a
measurement-range switch. The 100A-range zero isn't a contradiction -
0.5A is 0.5% of a 100A range, almost certainly below that range's
resolution floor, and RETOM's own `max_i=2.0A` config means a proper
high-range test (e.g. 50A) isn't reachable on this bench at all - flagged
as an inherent test-rig limitation, not a finding.

**Conclusion, now fully closing the "raw pre-calibration" story that's
been on `AnalogInputs` since Step 4.3:**
- `Ia/Ib/Ic/Ua/Ub/Uc` are always the true secondary-side/ADC measurement,
  independent of any limit configuration - confirmed directly, not just
  inferred.
- `calibrated_I = raw_I * (rating_from_table[active_current_limit_index] / 1A)`.
- `calibrated_U = raw_U` (no scaling).
- The active `current_limit_index`/`voltage_limit_index` are already
  exposed two ways: `SetLimits`'s own echo (`AppliedLimits`), and Mode
  3's footer fields 45/46 (already decoded nowhere in `device.py`/
  `Device.cpp` yet, despite being in `pseudocode.md`'s table - a small
  gap worth closing if calibrated output is implemented).

**Not yet implemented in the library** - this session was analysis/
verification only, no code changes. If picked up later: would need (a) a
`decodeCurrentLimitsTable()`/`decode_current_limits_table()` helper
(offset 44, variable-length until `type=0`), (b) reading fields 45/46
from the Mode 3 footer (offsets already known, just never wired to
anything), and (c) a `calibrated`-flavored accessor or documented
formula so callers don't have to do the multiplication by hand. Worth
discussing scope with the user before implementing (e.g. does the C++
API need a whole new type, or is a documented formula enough?).

## Step 10 — Calibration implemented and verified in both languages (2026-08-26)

User asked to implement Step 9's findings. Same session, continuing
directly.

**Python (`py/emon/`):**
- `constants.py`: `FIELD45_CURRENT_LIMIT_FOOTER_WORD_OFFSET`/
  `FIELD46_VOLTAGE_LIMIT_FOOTER_WORD_OFFSET` (64/65), `LimitTableType`,
  `LIMIT_TABLE_DIVISOR`, `LIMITS_TABLE_BYTE_OFFSET` (44).
- `codecs.py`: `decode_limits_table(raw, divisor_by_type)` - generic
  variable-length decoder, stops at a type=0 entry.
- `device.py`: new `SupportedLimits` dataclass (4 lists, `current_list_for()`
  helper), new `CalibratedAnalogInputs` dataclass, new module-level
  `calibrated_current()`/`calibrated_voltage()`/`calibrate()` functions,
  `EMonDevice.get_supported_limits()`, and `AnalogInputs` gained
  `applied_current_limit_type`/`_index`/`applied_voltage_limit_index`
  (decoded in `get_analog_inputs()` from footer words 64/65).
- `scripts/04_test_get_analog_inputs.py` updated to print both raw and
  calibrated values plus the active range.

**C++ (`include/libavm-emon/`, `src/`):** mirrored exactly -
`Constants.h` (same offsets/enum), `Codecs.h/.cpp`
(`decodeLimitsTable`), `Types.h` (`SupportedLimits` with
`currentListFor()`, `CalibratedAnalogInputs`, `AnalogInputs` gained the
three applied-limit fields), new `Calibration.h`/`Calibration.cpp`
(free functions `calibratedCurrent`/`calibratedVoltage`/`calibrate` -
a new file pair rather than cramming into `Types.h`, matching the
project's one-feature-per-file convention), `Device::getSupportedLimits()`,
`Device::getAnalogInputs()` updated for fields 45/46. `examples/get_analog_inputs`
and `tests/manual_test` (new `supported` subcommand, `analog` now prints
calibrated values + active range too). No `CMakeLists.txt` change needed -
sources are globbed.

**Verification, both languages, against real hardware:**
- `get_supported_limits()`/`getSupportedLimits()` reproduce the exact
  same 4 lists as the ad hoc Step 9 analysis (voltage/CT confirmed 19/19
  against user-provided ground truth; clamp/UPTT decoded but unconfirmed,
  as before).
- Baseline (no live signal): `applied_current_limit_type=0`,
  `applied_current_limit_index=3`, `applied_voltage_limit_index=0` -
  matches every prior session.
- **Live test with RETOM held at 10V/0.1A/50Hz**, run through the actual
  compiled C++ library (`tests/manual_test ... analog`), not just the ad
  hoc scripts: `raw Ia=0.0998A -> calibrated Ia=0.9980A`,
  `Ua=10.0249V` unchanged raw vs. calibrated - matches Step 4.3's
  original front-panel observation (~0.998A) almost exactly. Python
  path verified identically beforehand (`cal.Ia=0.99815`). Needed a
  synchronization marker file to get the timing right - `LinRetom51.connect()`'s
  enable/calibration replay took longer than an 8s guess allowed for on
  one attempt (zeros came back because the manual_test ran before
  `apply()` had actually landed), not a defect in the library under test.

This closes out the "raw pre-calibration" caveat as an actual library
feature, not just documentation - `AnalogInputs`'s docstring/comment
should probably now point callers at `calibrate()` explicitly, which it
doesn't yet (both languages still just describe the raw-value caveat,
not the fix). Minor follow-up if picked up later.
