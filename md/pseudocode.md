# EMon — pseudocode / wire-format reference

Language-agnostic protocol reference, meant to be implementable directly
in any language (this repo currently has a Python skeleton; a C++ port is
planned). Rationale and open questions live in `mindflow.md` — this file
is just the "how to build a frame" reference distilled from it.

All multi-byte fields below are **little-endian** unless stated otherwise.

## Frame format

```
request:  byte0 = 0x02 (SYNC)
          byte1 = mode number (e.g. 0x03 for Mode 3)
          byte2..3 = payload length, uint16 LE (bytes, excludes CRC)
          byte4..4+len-1 = payload
          last 2 bytes = CRC16, uint16 LE, over every byte before it

response: byte0 = status: 0x00 = NORMA (OK), 0xFF = NOT NORMA (error)
          byte1 = 0x02 (SYNC, constant)
          byte2..3 = payload length, uint16 LE
          byte4..4+len-1 = payload
          last 2 bytes = CRC16, uint16 LE, over every byte before it
```

Mode byte == decimal mode number written in hex (Mode 2 → `0x02`, Mode 18
→ `0x12`, Mode 24 → `0x18`, Mode 25 → `0x19`, ...).

```
build_request(mode, payload):
    header = bytes(SYNC, mode) + len(payload) as uint16_LE
    body   = header + payload
    return body + crc16(body) as uint16_LE

parse_response(raw):
    status, sync = raw[0], raw[1]
    assert sync == SYNC
    length = uint16_LE(raw[2:4])
    body = raw[0 : 4+length]
    payload = raw[4 : 4+length]
    assert uint16_LE(raw[4+length : 6+length]) == crc16(body)
    return (status == 0x00), payload   # (ok, payload)
```

## CRC16 — UNKNOWN, must be determined empirically

Docs never state polynomial/init/reflection. Candidates to try, in rough
order of likelihood for this device class (default guess: MODBUS):

| name        | poly   | init   | refin | refout | xorout |
|-------------|--------|--------|-------|--------|--------|
| MODBUS      | 0x8005 | 0xFFFF | true  | true   | 0x0000 |
| CCITT-FALSE | 0x1021 | 0xFFFF | false | false  | 0x0000 |
| XMODEM      | 0x1021 | 0x0000 | false | false  | 0x0000 |
| KERMIT      | 0x1021 | 0x0000 | true  | true   | 0x0000 |
| CCITT-1D0F  | 0x1021 | 0x1D0F | false | false  | 0x0000 |
| ARC         | 0x8005 | 0x0000 | true  | true   | 0x0000 |

To determine: capture one real request+response pair, strip the trailing
2 CRC bytes from each, compute all candidates over the remaining bytes,
see which reproduces the captured CRC. (`scripts/00_find_crc.py`.)

## Numeric formats

**Unsigned fixed point** (`N` words, `frac_bits` fraction bits):
```
raw = concat_le(words)              # treat the N words as one unsigned int
value = raw / 2**frac_bits
```

**Signed fields** — standard two's complement, decoded the way the docs
describe it (negate the *whole* width when the sign bit is set, per their
own "X = 0 − X" rule — see `mindflow.md` "signed field decoding
correction" for why this is two's complement and not classic
sign-magnitude, and why the distinction matters at the `0x8000...`
boundary value):
```
raw = concat_le(words)
sign_bit = 1 << (total_bits - 1)
full_mask = (1 << total_bits) - 1
if raw & sign_bit:
    magnitude = (-raw) & full_mask          # doc's own "X = 0 - X" rule, full width
    value = -(magnitude / 2**frac_bits)
else:
    value = raw / 2**frac_bits
```

Confirmed per-field parameters:

| field                              | words | frac_bits | total_bits | signed? |
|-------------------------------------|-------|-----------|------------|---------|
| harmonic amplitude (field 1)        | 2     | 20        | 32         | no      |
| RMS phase/line value (fields 5–8)   | 2     | 20        | 32         | no      |
| RMS, clamp/ext-amp exception        | 2     | 16        | 32         | no      |
| mean/DC component (field 9)         | 2     | 20        | 32         | yes     |
| mean-rectified (field 10)           | 2     | 20        | 32         | no      |
| angle (fields 11/12, 30–32)         | 2     | 16        | 32         | yes     |
| cos(phi) (fields 18, 28)            | 1     | 12        | 16         | yes     |
| frequency (field 33)                | 2     | 16        | 32         | no      |
| K2u/K0u/I1/I2/I0/U2/U0/Uy* (34–44)  | 2     | 16        | 32         | no      |
| harmonic angle (field 47)           | 1     | 7         | 16         | yes     |

Note: `frac_bits`/`total_bits` fully determine decoding; the docs'
integer-bit-count claims are informational only (and one of them, for the
angle field, is arithmetically impossible — see `mindflow.md`).

**48-bit power field** (fields 13–17, 20–27, 4 words = 8 bytes):
```
sign_word = words[0]                 # 0x0000 = positive, 0xFFFF = negative
magnitude = concat_le(words[1:4])    # 48-bit unsigned, 28 int / 20 frac bits
value = (magnitude / 2**20) * (-1 if sign_word == 0xFFFF else 1)
```

**ASCII string quirk**: for any byte `b > 0xA0`, use `(b + 0x20) & 0xFF`
before interpreting as a character. (Rationale undocumented; only matters
for non-plain-ASCII bytes.)

**DAA-BCD date** (2 words, `word_year`, `word_month_day`), used at least
for the SW-version-date field:
```
year  = int(hex(word_year))          # e.g. 0x2017 -> "2017" -> 2017
month = int(hex(high_byte(word_month_day)))
day   = int(hex(low_byte(word_month_day)))
```
Example from the docs: `0x2017, 0x0113` → `2017-01-13`.

## Command reference (only the 4 functions currently targeted)

### connect / disconnect
No device-side session/handshake documented. `connect` = open the serial
port with the (still-unverified) parameters below; `disconnect` = close
it. No request/response involved.

```
port params (UNVERIFIED GUESS): 9600 baud, 8 data bits, no parity, 1 stop bit
```

### get_device_info — Mode 15 (Read FRAM)

Request: `mode=0x0F`, payload = `start_word(u16 LE) + count_words(u16 LE)`.
Use `start_word=0, count_words=256` to read the full 512-byte system area
(it's exactly one FRAM "block closed by CRC" — see `mindflow.md`).

Response payload = the raw 512 bytes; interesting fields (ЭМ‑3.1КМ
layout):

| byte offset | length | field                          | notes |
|-------------|--------|---------------------------------|-------|
| 0           | 16     | device type                     | ASCII, quirk above |
| 16          | 2      | serial number                   | plain u16 LE |
| 18          | 4      | SW version                      | **encoding unresolved**, see mindflow.md |
| 22          | 4      | SW version date                 | DAA `ГГГГ,ММДД`, ЭМ‑3.1КМ doc only |
| 26          | 2      | device type ID                  | 3 = EM3.3T, 2 = EM3.2 |
| 510         | 2      | CRC of this 512-byte block      | not the frame CRC |

### set_current_limits — Mode 2, ЭМ‑3.1КМ variant

Request: `mode=0x02`, payload (6 bytes / 3 words):
```
word0: current_limit_type   (0=transformer, 1=clamp, 2=UPTT)
word1: current_limit_index  (0..N-1 for that type)
word2: voltage_limit_index  (0..N-1)
```
Response payload on NORMA (10 bytes / 5 words):
```
word0: current_limit_type (echoed)
word1: current_limit_index (as applied)
word2: voltage_limit_index (as applied)
word3: access_level (0..2)
word4: wiring_scheme (0b001 / 0b010 / 0b100 = 3-phase-4-wire / 3-phase-3-wire / 1-phase-2-wire)
```
Relay switchover means the device may take up to ~1s to respond.

### get_analog_inputs — Mode 3 (full calc)

Request: `mode=0x03`, empty payload.
Response payload: fixed **4536 bytes** = `2 * (242*6 + 64 + 2 + 150 + 600)`,
laid out as 6 fixed-size channel blocks followed by one "footer" section.
Same layout is returned by Modes 4 and 5 — only *which* fields are
actually computed differs (see coverage table below); Mode 3 is used here
because it's the only one covering RMS + angle + frequency together.

**Channel blocks** (242 words = 484 bytes each), in this fixed order:

```
block index : channel
     0      : Ia
     1      : Ua
     2      : Ib
     3      : Ub
     4      : Ic
     5      : Uc

block_byte_offset(channel) = index(channel) * 484
```

Field word-offsets *within* a block (19 fields, sum = 242 words exactly;
every field reserves space in every block even where semantically N/A):

| field | word offset | words | meaning                              | only valid in |
|------:|------------:|------:|----------------------------------------|----------------|
| 1     | 0           | 98    | harmonics 2nd..50th                    | all |
| 2     | 98          | 2     | harmonic distortion coeff              | all |
| 3     | 100         | 102   | interharmonics 0.5..50.5                | all |
| 4     | 202         | 2     | interharmonic coeff                    | all |
| **5** | **204**     | **2** | **RMS phase value**                    | all — used for Ia/Ua/Ib/Ub/Ic/Uc |
| 6     | 206         | 2     | RMS phase @ 1st harmonic               | all |
| 7     | 208         | 2     | RMS line value                         | U blocks only |
| 8     | 210         | 2     | RMS line @ 1st harmonic                | U blocks only |
| 9     | 212         | 2     | mean/DC component                      | all |
| 10    | 214         | 2     | mean-rectified value                   | all |
| **11**| **216**     | **2** | **angle u^i**                          | I blocks only — used for phi(Ia/Ua) etc. |
| 12    | 218         | 2     | angle u^u                              | U blocks only |
| 13    | 220         | 4     | active power                           | all |
| 14    | 224         | 4     | apparent power                         | all |
| 15    | 228         | 4     | reactive power (method 2)              | all |
| 16    | 232         | 4     | reactive power (method 3)              | all |
| 17    | 236         | 4     | reactive power (method 1)               | all |
| 18    | 240         | 1     | cos(phi)                               | I blocks only |
| 19    | 241         | 1     | (empty)                                 | — |

**Footer** (816 words = 1632 bytes, starts right after the 6 blocks, i.e.
at byte offset `242*6*2 = 2904`):

| field | word offset (in footer) | words | meaning |
|------:|-------------------------:|------:|---------|
| 20–27 | 0                         | 32    | summed powers (active/apparent/reactive×3/seq×3) |
| 28    | 32                        | 1     | total cos(phi) |
| 29    | 33                        | 1     | (empty) |
| 30–32 | 34                        | 6     | angles I1^U1, I2^U2, I0^U0 |
| **33**| **40**                    | **2** | **frequency** |
| 34–44 | 42                        | 22    | K2u, K0u, I1, I2, I0, U2, U0, Uy, Uya, Uyb, Uyc |
| 45    | 64                        | 1     | applied current-limit code |
| 46    | 65                        | 1     | applied voltage-limit index |
| 47    | 66                        | 150   | harmonic angles 1..50, 3 phases |
| 48    | 216                       | 600   | harmonic powers 1..50, 3 phases, 4 words each |

**Mode field coverage** (which fields each mode actually computes; the
rest of the fixed-size payload is zero/stale):

| mode | computes fields |
|------|------------------|
| 3    | all (1–48) |
| 4    | 5, 7, 9–10, 13–24, 28, 45–46 |
| 5    | 1–4, 6, 8, 11–12, 25–27, 29–48 |

→ neither Mode 4 nor Mode 5 alone gives RMS (5) + angle (11) + frequency
(33) together; Mode 3 is required for `get_analog_inputs` as specified
(see `mindflow.md` "Mode coverage gap" for the tradeoff).

```
get_analog_inputs():
    payload = call(Mode.CALC_ALL)               # 4536 bytes
    for ch in (Ia, Ua, Ib, Ub, Ic, Uc):
        rms[ch] = decode_ufixed(payload[block_offset(ch)+408 : +4], frac_bits=20)
    for ch in (Ia, Ib, Ic):
        angle[ch] = decode_signed_fixed(payload[block_offset(ch)+432 : +4], frac_bits=16, total_bits=32)
    frequency = decode_ufixed(payload[2904+80 : +4], frac_bits=16)
    return AnalogInputs(rms.Ia, rms.Ib, rms.Ic, rms.Ua, rms.Ub, rms.Uc,
                         frequency, angle.Ia, angle.Ib, angle.Ic)
```
(byte offsets: field-word-offset × 2, e.g. field 5 at word 204 → byte 408
within its block; footer field 33 at word 40 → byte 80 within the footer.)
