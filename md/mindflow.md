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
- Step 3.1 (find baud rate) — in progress, but the real blocker turned
  out to be wiring topology, not baud. Full diagnosis in "Step 3" below;
  short version: multimeter measurements at the device's own DB9
  connector show it transmits on pin 2 and receives on pin 3 (a DCE-style
  pinout, opposite of the PC/DTE convention pin2=RX/pin3=TX we'd been
  assuming) — so the correct cable is straight/same-numbered-pin
  (adapter pin2↔device pin2, pin3↔pin3, pin5↔pin5 GND), not a null-modem
  cross.
- Exhausted RS-232 (both DTE-straight and DCE-pin-matched wiring) and the
  device's own USB/FTDI port — 270+ baud×CRC×parity/stopbits combos and
  several raw-burst visual checks, all silent. Full trail in "Step 3"
  below.
- **Next action — switched strategy:** rather than more blind sweeps,
  user is moving to a Windows PC to run the vendor's original software
  against the real device and capture ground-truth traffic (real baud
  rate from its settings dialog, plus a raw hex capture of a real
  request/response pair). See the "Capture checklist for the Windows
  session" at the end of "Step 3" below for exactly what to bring back
  and how to use it once captured (feeds directly into
  `crc.guess_crc16()` and `SerialTransport(..., baudrate=...)`). Once
  that data is in hand, resume at `plan.md` step 3.2 (CRC) → 3.3+
  (connect → device info → limits → analog inputs).

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

## Step 3 — wiring sanity check

Before spending time guessing baud rates against the device, the user
flagged uncertainty about whether the USB-UART bridge's TX/RX wires are
crossed correctly. Software alone can't distinguish "wrong baud" from
"TX/RX swapped" from "wrong logic levels/no GND" — all three just look
like silence when probing the device. So this needs to be split into two
independent checks:

1. **Loopback (PC/adapter side only, no device):** jumper the adapter's
   TX pin straight to its own RX pin, nothing else connected, and run
   `scripts/01_test_connect.py <port> --loopback`. It writes a known byte
   pattern and checks it reads back unchanged. This only tests the
   adapter + driver + port name/permissions — the device and its wiring
   aren't part of the loop at all, so a pass here rules out everything on
   the PC side of the cable.
2. **Probe (device side, no jumper):** once loopback passes, reconnect to
   the device as wired and run `--probe` at a few candidate bauds. Only
   *after* loopback has passed does persistent silence here actually
   implicate the device-side wiring (TX/RX polarity, logic levels, GND)
   rather than the PC side.

Added `--loopback` to `scripts/01_test_connect.py` for step 1; step 2
(trying the actual device wiring, and swapping TX/RX if needed) is
manual — no way to automate "physically swap two wires" from here.

**This session actually has real access to the hardware** (`/dev/ttyUSB*`
is a real device node, this user is in the `dialout` group, `pyserial`
is already installed) — so from here on these scripts are run directly
rather than handed to the user to run and report back.

**Port identity trap:** there are **two separate USB-UART adapters**
plugged in simultaneously (both Prolific PL2303, different USB hub ports
and serial numbers). First loopback attempt against the port the user
*assumed* was the jumpered one (`/dev/ttyUSB1`) failed with 0 bytes back
— turned out that port is the *second, unrelated* adapter, not the one
with the jumper. Confirmed by having the user unplug the jumpered
adapter and observing which `/dev/ttyUSB*` node disappeared: it was
`/dev/ttyUSB0`. **The device under test is on `/dev/ttyUSB0`, not
`/dev/ttyUSB1`.** Re-ran loopback against `/dev/ttyUSB0` → **LOOPBACK
OK**. Lesson for later steps: if only one adapter is ever plugged in this
won't recur, but if in doubt again, re-derive the port from a plug/unplug
event rather than trusting which port was assumed earlier — `ttyUSB`
numbering isn't stable/meaningful across sessions.

**Probing the device itself:** with loopback confirmed OK on
`/dev/ttyUSB0`, probed the real device at 9600/19200/38400/57600/115200
— total silence at all five. Then had the user swap TX/RX at the device
connector (the crossing they originally suspected) and re-probed the
same five bauds — **still total silence at all five, both wiring
orientations.** Since the PC/adapter side is proven good and both TX/RX
orientations have now been tried, a simple TX/RX cross is no longer a
likely sole explanation. Remaining suspects, in rough likelihood order:
GND not connected between adapter and device (never explicitly
confirmed — only asked about, not verified), device not actually
powered/ready, wrong physical pins entirely, or a logic-level mismatch
(the PL2303 adapters in use are commonly sold as USB→RS-232 DB9 dongles,
which output RS-232 voltage levels via an internal level shifter — if
the device's port expects raw TTL UART levels instead, or vice versa,
nothing will be received either way regardless of baud or TX/RX
orientation). Need to ask the user for the physical setup details before
guessing further bauds blindly.

User confirmed: device is powered on, GND is wired (3-wire TX/RX/GND
link, not a full 9-wire DB9 cable), connector is DB9 RS-232 on the
device's side too. Ruled out power/GND/connector-type as explanations.

Made `SerialTransport` accept `bytesize`/`parity`/`stopbits` (previously
hardcoded to 8N1) since the docs don't specify framing any more than they
specify baud — same "unverified default, easy to override" treatment as
baud/CRC. Then ran two full sweeps against the current (crossed) wiring,
no further physical changes needed from the user:

- 9 bauds × 3 parities × 2 stop-bit settings (54 combos, default CRC
  guess) — **zero hits**.
- 9 bauds × 6 CRC variants (54 combos, default 8N1 framing) — **zero
  hits**.

**Important methodological realization:** every one of these probes
sends a *fully framed* request (sync+mode+len+payload+CRC). If this
device silently drops frames that fail their own CRC check — very
plausible, since a corrupted CRC also means the device can't trust the
length field enough to know where the frame ends, so discarding
silently rather than replying is a reasonable firmware choice — then
**"wrong CRC guess" alone produces the exact same total-silence symptom
as "wrong baud" or "wrong wiring"**, regardless of how many CRC variants
get tried, because none of the 6 candidates in `emon.crc.VARIANTS` may
be the real one. This means the string of silent results across both
wiring orientations, 9 bauds, and 3 parity/2-stopbit settings is **not**
strong evidence against any specific one of those factors individually —
it's only evidence that at least one necessary condition (wiring, baud,
framing, or CRC) is still wrong, and a raw byte-framed request can't tell
us which. A response-independent check (does the device show *any*
physical reaction — LED, display, beep — to receiving raw bytes,
regardless of protocol validity) is needed to make further progress
without guessing blindly across an ever-growing combo space.

Also worth checking: some instruments of this era only listen on their
serial port after being switched into an explicit "PC exchange"/service
mode via the front panel — asked the user; **no such mode exists** on
this device (per the user).

Sent a raw, protocol-agnostic burst (repeating 0x55 bytes, a clean
square-wave-ish bit pattern, for ~5s at 9600 baud) and asked the user to
watch the device for *any* reaction at all — display flicker, LED, beep,
error message — regardless of whether the protocol was understood.
**Zero reaction.**

Then repeated the *entire* diagnostic set in the other wiring
orientation (user swapped back to original/uncrossed): baud×parity×
stopbits (54 combos), baud×CRC-variant (54 combos), and the raw-burst
visual check — **zero hits, zero reaction, same as the crossed
orientation.** Both wiring polarities have now had full, equal, and
exhaustive treatment.

**Conclusion so far:** re-checked both vendor docs for any mention of the
physical connector/pinout/interface standard (`разъем`, `DB9`, `контакт`,
`RS-232`, `RS-485`, etc.) — **neither document says anything about the
physical layer at all**, confirming the earlier note in Step 0. Combined
with 216 total software-parameter combos tried across two wiring
orientations and zero observable reaction (protocol-level or raw
electrical) from the device in any of them, the problem is very unlikely
to be a baud/parity/CRC/TX-RX-polarity question anymore — those are
exhausted. Suspects left, none of which are checkable without hands-on
access to the actual hardware:

1. A wire/connection that looks connected but isn't (open circuit, bad
   crimp/solder joint, oxidized pin) — needs a multimeter continuity
   check, which only the user can do.
2. Wrong physical pins entirely — the docs don't give a pinout for this
   device's port, so if the 3 wires were run to the wrong DB9 pins
   (there are 9, easy to miscount on a hand-wired connector), TX/RX/GND
   might not be where we think they are regardless of "crossing."
3. Voltage-level mismatch — the PL2303 adapters are commonly sold as
   USB→RS-232 DB9 dongles (real ±RS-232 voltage swing via an internal
   level shifter); if the device's DB9 port is actually TTL-level despite
   the DB9 shell, RS-232 voltages could either fail to register or, worse,
   have already damaged a TTL-level receiver pin on first contact.
4. Wrong physical port on the device — instruments like this sometimes
   have more than one rear connector (e.g. one for a printer, one for PC
   exchange); worth double-checking against the device's own label/manual
   if one exists, since neither of our two docs describes the enclosure.

Recommending the user do a few physical checks before any more blind
software sweeping, since the parameter space on this side is now fully
covered.

**Physical check results:** user has a multimeter and already checked
continuity — confirmed good on DB9 pins 5 (GND), 2 and 3 (TX/RX), the
standard RS-232 DTE assignment. Wiring is individual bare wires to
specific pins (not a molded DB9-to-DB9 cable), and the pins chosen are
exactly the ones that should be used. This rules out suspects #1
(bad/open connection) and #2 (wrong pins) from the list above — the
remaining live suspect is **#3, a voltage-level mismatch**: the PL2303
adapter is very likely genuine RS-232 (±5–12V swing, confirmed
functional end-to-end by the loopback test, which was jumpered at the
DB9 pins so it exercised the adapter's actual line driver/receiver, not
just TTL logic before it). If the device's DB9 port is actually
TTL/CMOS-level (0–5V) internally despite the DB9 shell — not uncommon on
older industrial/instrument gear that reuses a DB9 shell for a
non-standard or TTL interface — an RS-232 voltage swing could simply
fail to register as valid logic transitions (silence, matching what
we've observed), or in the worst case could have stressed/damaged a
TTL-level receiver pin not rated for it. Next: ask the user to measure
the adapter's idle TX-pin DC voltage relative to GND — a genuine RS-232
transmitter idles at a negative voltage (~-5 to -12V, "mark"/logic 1);
if it instead reads within 0–5V, that would point at a TTL/RS-232
mismatch as the real culprit.

**Measured:** adapter TX (pin 3) idle = **-9.89V** — a clean, textbook
RS-232 mark/idle level. This rules out "adapter isn't real RS-232"
entirely; the adapter's transmitter is functioning correctly and our
commands genuinely are going out as proper RS-232 voltage swings.

Adapter RX (pin 2) idle = **-1.967V** — this is the more interesting
number. It's neither a clean RS-232 mark (~-5 to -9V, like the TX
reading) nor a TTL idle-high (~+3.3 to +5V); a weak, small-negative
reading like this is the characteristic "open/undriven input" bias that
RS-232 receiver ICs (MAX232-style) commonly present on their own when
*nothing* is actively driving the line — most such receivers have an
internal pull-down toward a small negative reference specifically so an
unconnected input has a defined idle state. Reads more like "this pin
isn't actually being driven by anything" than "a real signal at the
wrong voltage." Next: measure directly at the *device's own connector*
(not through the wire/adapter) to see whether the device's TX pin is
producing any signal at all at its source, isolating the wire/adapter
entirely from the question.

**Breakthrough measurement, taken directly at the device's own DB9
connector (adapter disconnected):** pin 2→pin 5 (GND) = **-9.56V**,
essentially the same magnitude as the adapter's own TX idle (-9.89V) —
i.e. a real, actively-driven RS-232 transmitter at mark/idle. Pin 3→pin 5
= **0V** — a defined, driven-looking value rather than open, but still
nowhere near a genuine RS-232 mark (~-5 to -9V); consistent with an
*input* pin (nothing internally driving it out) sitting at whatever
baseline its own receiver circuit settles to when undriven from outside.

**This device transmits on pin 2 and receives on pin 3** — the opposite
of the standard PC/DTE assignment (pin 2 = RXD, pin 3 = TXD) that both
our USB-UART adapters and the "TX/RX crossed vs not" framing used so far
implicitly assumed. The device's port behaves like a **DCE** (modem-style)
pinout rather than DTE. The correct cable between a DTE (our adapter) and
a DCE (this device) is a **straight, same-pin-number** connection —
adapter pin 2 → device pin 2, adapter pin 3 → device pin 3, pin 5 → pin
5 — not a null-modem cross. This is a specific, unambiguous wiring by DB9
pin *number*; it's not established that either of the two
"crossed"/"uncrossed" configurations tried earlier actually matched it,
since those were set up and described by "TX/RX" labels, not confirmed
pin numbers on both ends together. **Decision:** wire it explicitly as
pin2↔pin2, pin3↔pin3, pin5↔pin5 and retest — this supersedes the earlier
"crossed"/"uncrossed" framing entirely.

**Tested the pin-matched (DCE-style straight) wiring: still nothing.**
Rewired exactly as decided (adapter pin2↔device pin2, pin3↔pin3,
pin5↔pin5) and repeated the full diagnostic battery on it: 9 bauds × 6
CRC variants (54 combos), 9 bauds × 3 parities × 2 stopbits (54 combos),
and two raw 0x55-burst visual checks (~5-6s each, user watched the
device both times) — **zero hits, zero reaction, all of it.** This is
the wiring that the device's own idle-voltage measurements said should
be correct (pin 2 actively driven at -9.56V matching the adapter's real
RS-232 transmitter; pin 3 reading a defined-but-different 0V consistent
with an input). That it still doesn't work means either (a) pin 3 isn't
actually the device's RXD despite the elimination logic — the 0V reading
only proved "not actively driven out," not "this is a working receiver
input," or (b) something else entirely is missing, most plausibly a
hardware handshake line (some RS-232 devices only listen on RXD once
DTR and/or RTS is asserted by the host as a "host present" signal — our
link is 3-wire TX/RX/GND only, so if the device's firmware gates on a
control line we haven't wired at all, no amount of correct TX/RX/baud/
CRC guessing will ever produce a response), or (c) wrong physical port
on the device (suspect #4, still unaddressed — never confirmed there
isn't a second/different connector meant for PC exchange specifically).

Next diagnostic idea, not yet tried: measure the *other* DB9 pins on the
device (1, 4, 6, 7, 8, 9) relative to GND, looking for one that reads
something other than a clean 0V/floating baseline — that would suggest a
control/handshake input the device is waiting on.

Asked about labeling and alternate ports: **no markings at all** near
the connector, and **only this one** connector looked like a
serial/data port *at the time asked* — user then remembered **the device
also has a USB port**, whose protocol isn't described in either vendor
doc at all (neither mentions USB anywhere). This wasn't caught earlier
because the question was scoped to "serial/data port" and a USB port
doesn't read as a DB9 lookalike.

**This changes the plan.** If the USB port enumerates as a USB-CDC
virtual serial port (very common for this class of device — a USB-UART
bridge chip *inside* the device, presenting a `/dev/ttyACM*`-style node),
we could plug straight into it and bypass the entire RS-232 electrical
layer we've been fighting (levels, pinout, handshake lines) — the
higher-level framing/CRC/command logic in `emon/` should be identical
regardless of which physical transport carries it, since the vendor docs
describe the command protocol, not the RS-232 wiring. Even if USB turns
out to need its own quirks, it's a much faster way to get a real
response and finally validate the CRC guess/frame decoding against
actual device output — after which the RS-232 path (needed for whatever
the eventual deployment transport is) becomes far more tractable, since
CRC would no longer be an unknown variable when debugging it.
Next: plug in USB, see what device node/class it enumerates as.

**Result:** the device's USB port enumerates as a genuine **FTDI FT232R
USB-UART** chip (`usb-FTDI_FT232R_USB_UART_A602YD53-if00-port0`), landing
at `/dev/ttyUSB0` once the RS-232 adapter was unplugged (freeing that
node) and the device's USB cable was plugged in directly. This means the
device has an FTDI bridge wired to its own internal UART and exposes it
straight over USB — no RS-232 voltage levels, no DTE/DCE pinout
questions, no handshake-line guessing; `pyserial` talks to it exactly
like any other USB-serial adapter. Baud rate still matters (FTDI passes
it through to real UART timing) but every other physical-layer complication from the RS-232
path (Step 3 above) is now moot for this connection. Immediately
re-running the baud/CRC probe against this port — if this responds,
`/dev/ttyUSB0` via USB becomes the primary link for the rest of
validation, and the RS-232 debugging can be revisited later only if a
non-USB deployment actually needs it.

**Result: also zero.** Same full battery on the USB/FTDI port — 9
bauds × 6 CRC variants (54), 9 bauds × 3 parities × 2 stopbits (54), two
raw 0x55-burst visual checks — **zero hits, zero reaction**, identical
to both RS-232 wiring attempts. The USB path didn't unstick anything.

**Important caveat surfaced here:** it's quite possible this device's
firmware gives *no visual feedback whatsoever* for received bytes unless
they form a complete, correctly-checksummed, recognized command — many
simple embedded UART command parsers just silently buffer/discard
anything that doesn't parse, with zero UI indication either way. If
that's true, the "watch the device for any reaction" burst test was
never capable of proving or disproving link health — it can only ever
show a positive (something happened) or an uninformative negative, never
a confident "the link is dead." That undercuts how much weight to put on
"zero reaction across every burst test" as evidence of anything.

Asked about a path to ground-truth: does the user have the **original
vendor PC software** for this meter (would let us capture/compare known-
good traffic and skip all this guessing)? — **not sure, would need to
check.** Is there any manual/label beyond the two `.doc` files with comms
settings? — **no, nothing else exists.** So there's no authoritative
source to fall back on right now beyond continued guessing, unless
vendor software turns up.

**Where this stands:** exhausted the practical combinatorial space on
two independent physical transports (RS-232 in two wiring topologies,
and USB/FTDI) — baud, parity, stop bits, and CRC variant all covered on
each, 270+ total attempts, zero response and zero observed reaction
throughout. This is a natural point to pause blind sweeping and consider
: (a) the user searching for vendor software (could exist online even if
not on hand), (b) trying a wider/more exotic baud list as a low-cost
long-shot, (c) getting a logic analyzer/oscilloscope on the line for
ground-truth signal observation (if available) rather than inferring
from software timeouts, or (d) setting hardware validation aside for now
and revisiting later, since it's genuinely blocked on information/tools
outside this session's reach.

**Decision: user found the vendor's original PC software, but it's
Windows-only — moving to a Windows machine to run it against the real
device and capture ground-truth traffic**, rather than continuing blind
sweeps here. This is the best available path: real known-good
request/response bytes make the CRC variant and framing solvable
directly (feed a captured frame into `emon.crc.guess_crc16`), and the
vendor tool's own connection-settings dialog will state the real baud
rate, sidestepping the whole RS-232/USB physical-layer investigation in
this file.

**Capture checklist for the Windows session** (leaving this here so
whichever session does the capture — this repo is already pushed to
`origin/master`, so `git pull` on the Windows machine gets the same
mindflow/plan/pseudocode context):

1. Note the **baud rate / parity / stop bits** from the vendor software's
   port-settings dialog — usually under a "Connection"/"Port
   settings"/"COM" menu.
2. Get a **raw byte-level capture** of at least one full request+response
   round trip while the vendor tool talks to the device (e.g. its
   "connect" or "read values" action). Options, roughly by ease:
   - A serial-port sniffer/monitor tool (e.g. Free Serial Port Monitor,
     Advanced Serial Port Monitor, HHD Software's Serial Port Monitor,
     or similar) attached to whichever COM port the vendor tool uses.
   - If the device's own USB/FTDI bridge is what's in use, a USB capture
     (e.g. Wireshark + USBPcap on Windows) on that device would also work
     and shows the same underlying UART bytes.
   - Even a **photo of the vendor tool's own raw-hex/log view**, if it
     has one, would do — anything that shows the actual bytes exchanged.
3. Bring back: the baud/framing settings, and the raw hex bytes of at
   least one request and its matching response — ideally for the
   cheapest command to trigger from the vendor UI (a "read info"/"connect"
   action is more likely to map to a short, simple request than a full
   measurement dump).
4. With that in hand: baud goes straight into `EMonDevice(...,
   baudrate=...)`/`SerialTransport`; the captured frame goes into
   `crc.guess_crc16(frame_without_last_2_bytes, last_2_bytes)` to solve
   the CRC variant in one shot; and the captured bytes can be sanity-
   checked against the frame layout in `pseudocode.md` to confirm (or
   correct) the reconstructed protocol before trusting it further.

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
