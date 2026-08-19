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
real hardware, including with a real load.** The protocol
reverse-engineering phase (steps 0-3) is done. Step 4.3's real-load test
found and fixed two real field-offset bugs (RMS, frequency) inherited
from the doc-derived tables in Step 1 — a reminder that "matches the
docs' worked example" and "matches real hardware" are different bars, and
only the latter is fully trustworthy. Remaining work is narrow, targeted
follow-ups (see "Next action").

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
