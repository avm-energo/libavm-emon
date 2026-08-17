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

- Step 0 (doc analysis) — done.
- Step 1 (planning files + protocol reconstruction) — done, this file.
- Step 2 (Python skeleton) — done, see `src/python/emon/`.
- **Next action:** run `src/python/scripts/00_find_crc.py` against a real
  captured frame to pin down the CRC16 variant, then work through
  `plan.md` step 4 in order (connect → device info → limits → analog
  inputs), updating this file and `pseudocode.md` after each one.

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

## Open questions for hardware validation (step 4)

1. Which CRC16 variant? (`scripts/00_find_crc.py` brute-forces common
   variants against one captured request/response pair.)
2. What baud rate / parity / stop bits? Default guess in `transport.py`
   is 9600 8N1, unverified.
3. Does the real device's Mode 15 response for the system area match the
   assumed layout (device type ASCII, serial number, device-type ID) at
   the offsets used in `device.py`?
4. SW version/date field — capture a real response and figure out the
   actual encoding (see note above).
5. Does field 5's RMS decoding need the "clamp/external-amplifier"
   exception (16/16 split, ÷65536) instead of the default (12/20 split,
   ÷1048576) for this particular unit's configuration?
