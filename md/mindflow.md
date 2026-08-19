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

**All 4 milestone functions (connect, get_device_info, set_current_limits,
get_analog_inputs) are implemented and confirmed working live against
real hardware.** The protocol reverse-engineering phase (steps 0-3) is
done; remaining work is narrow, targeted follow-ups (see "Next action").

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
- **Next action:** two narrow open items remain, neither blocking use of
  the 4 milestone functions: (1) get a real load onto the meter's
  current/voltage inputs to sanity-check `get_analog_inputs()`'s field
  offsets against known values (everything read as exactly 0 so far,
  consistent with "no load connected" but not a positive confirmation the
  offsets are right); (2) resolve the SW-version field encoding (real raw
  words known: `0x0702`, `0x0153` — doesn't decode cleanly under the DAA
  hypothesis, still open). See "Open questions" below for the full list
  with detail.

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
- **SW version field is unresolved.** The two docs disagree on its width
  (4 bytes vs 8 bytes) and the one worked example given
  (`0x0704,0x0101 => "707.101"`) doesn't reduce cleanly under DAA, plain
  hex, or plain decimal reading of either word — it looks like the example
  itself is wrong or refers to a format not otherwise described. Left
  unimplemented (`DeviceInfo.sw_version_raw_words` exposes the two raw
  words) until it can be checked against a real device.

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
5. Does field 5's RMS decoding need the "clamp/external-amplifier"
   exception (16/16 split, ÷65536) instead of the default (12/20 split,
   ÷1048576) for this particular unit's configuration? Still open — the
   4536-byte poll frames were captured but not yet decoded field-by-field
   against `get_analog_inputs()`.
6. What mode number does the vendor program actually send for the request
   side? Not directly observed across two 2026-08-19 captures (150s
   total, including a live reconnect) — see "Step 3.2/3.3" and "Step
   3.5". Near-certain root cause: the HHD split only mirrors device→PC
   traffic to the COM3 alias, so this tap structurally cannot see it.
   Would need a different tap (HHD "both directions" setting if one
   exists, or a physical RS-232 tap) to resolve directly; otherwise rely
   on the (now 3-for-3) payload-size matches as indirect confirmation.
