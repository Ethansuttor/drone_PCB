# Schematic Capture Walkthrough — Drone FC v3

Companion to `drone_fc_project_context_v3.md`. Follow this block by block in KiCad's schematic editor (Eeschema). Capture in this order — power first, MCU core next, peripherals last — so the rails and grounds exist before you hang anything off them.

## KiCad shortcuts you'll use constantly
- `A` — add symbol · `P` — add power symbol · `W` — draw wire · `L` — place net label
- `Q` — place no-connect (×) flag on unused pins · `E` — edit selected symbol/value · `V` — edit value
- `R` rotate · `M` move · `G` drag (keeps wires attached)
- Wires connect by **net name** even without a physical line — use net labels (`L`) liberally instead of dragging long wires across the sheet. Two pins with the same label = same net.

## Global conventions for this board
- **Three supply rails**, each its own power symbol: `+5V`, `+3V3`, `+3V3_IMU`. Grounds all tie to one `GND`.
- Put **decoupling caps physically next to the pin** they serve in layout — but in the schematic just label them clearly (e.g. `C_VDD1`).
- Every power *input* the board generates needs a **PWR_FLAG** (Eeschema → place it on `+5V`, `+3V3`, `+3V3_IMU`, and `GND` once each) or ERC will complain the rails are "driven by nothing."

---

## Capture progress (2026-06-26)
| Block | Status |
|---|---|
| 1 — Onboard buck (TPS5430) | **Done** — fully wired (VIN/Cin, PH+inductor, catch diode, BOOT cap, FB divider, Cout) |
| 1 — USB OR Schottky | **Done** (VBUS→+5V) |
| 2 — Both LDOs (AP2112K + TLV733P) | **Done** — both rails wired |
| 3 — MCU core | Power-in / decoupling reviewed & corrected (caps shunt rail→GND, VDD pins tap +3V3 directly). Clock/boot/SWD still to wire |
| 4 — USB-C | Receptacle (14P symbol), CC pulldowns, Schottky, D±, ESD placed. **Fix outstanding:** USBLC6-2 pin 2→GND and pin 5→VBUS were left floating |
| 5 — ESC connector **J10** | Placed; **VBAT + GND wired, signal/logic lines NOT yet** (M1–M4, CRT current, ESC_TLM) + their ADC protection |
| 6 — IMU power | **Done** (VDD, VDDIO, grounds, RESV, INT2). **IMU SPI logic NOT yet** (SCK/MISO/MOSI/CS/INT) |
| 7 — Flash | Not started |
| 8 — Receiver header / spares | Header placed; logic confirmed (FlySky i-BUS) |
| 9 — Buzzer / LED | Not started |
| 10 — ERC / footprints | Pending |

**Next up:** IMU SPI1 logic lines, J10 signal lines + ADC protection, MCU clock/boot/SWD, flash.

---

## Block 1 — Power input: onboard buck + USB ORing
Goal: generate a clean `+5V` rail **on-board** from the battery (the ESC's raw VBAT), ORed with USB, with no backfeed into USB.

> The BLS-04 / Trinx G20 ESCs have **no BEC** — they only pass raw pack voltage. So the FC makes its own 5V, exactly like a commercial stack FC. `VBAT_RAW` arrives on the ESC 10-pin harness (Block 5) and feeds both the VBAT sense divider *and* this buck.

**Buck — TPS5430DDAR** (`drone_lib:TPS5430DDAR`, ESOP-8, C9864). Pinout: 1 BOOT · 4 VSENSE · 5 ENA · 6 GND · 7 VIN · 8 PH · exposed pad = GND.
- **VIN (7)** ← `VBAT_RAW`. Decouple **2×10µF 50V X7R (GRM32ER71H106KA12L, C77102) + 100nF** from VIN→`GND`, right at the pin. *(50V parts — 6S is 25.2V.)*
- **GND (6)** + **exposed pad** → `GND`.
- **PH (8)** = switch node → one end of the **22µH (≥3A) shielded inductor (CKCS6028-22µH, C354622)**; other end → `+5V`.
- **Catch diode — SS34** (`Diode:D_Schottky`): cathode → `PH`, anode → `GND`.
- **BOOT (1)** → **10nF** cap to `PH`.
- **VSENSE (4)** ← feedback divider: **R1 = 10k** from `+5V` to VSENSE, **R2 = 3.24k** from VSENSE to `GND` (Vref 1.221V → 5.0V).
- **ENA (5)** → leave floating (internal pull-up enables it). *(Optional: a VBAT→ENA resistor divider sets a UVLO turn-on threshold.)*
- **Output cap** on `+5V`: **220µF aluminum-polymer (~20–40mΩ ESR) + 100nF** to `GND`. **Not pure ceramic** — the TPS5430 is internally compensated for some output-cap ESR, so an all-ceramic output can ring/oscillate (use polymer or POSCAP).

**USB OR:**
- **Schottky** SS34/B5819W — anode to USB `VBUS` (Block 4), cathode to `+5V`. ORs USB power onto the rail on the bench, blocks backfeed into USB.

Result: `+5V` comes from the onboard buck in flight and from USB through the Schottky on the bench. Drop a **PWR_FLAG** on `+5V` and on `GND`. *(No external 5V-in connector — that's removed.)*

**Layout note:** keep the buck loop (VIN cap → IC → inductor → catch diode → output cap) tight, the PH switch node small, and the whole thing **away from the IMU and the VBAT/CRT ADC traces**.

## Block 2 — 3.3V rails (two LDOs)
**Main 3.3V — AP2112K-3.3 (C51118, SOT-23-5):**
- VIN ← `+5V`, GND ← `GND`, VOUT → `+3V3`, EN → tie to VIN (`+5V`) so it's always on.
- Input caps: 10µF + 100nF from VIN to GND. Output caps: 10µF + 100nF from VOUT to GND.

**Quiet IMU 3.3V — TLV733P-3.3 (C134139, SOT-23-5):**
- VIN ← `+5V`, GND ← `GND`, VOUT → `+3V3_IMU`, EN → tie to VIN.
- Caps: 1µF in, 1µF out (plus the 100nF that sit at the IMU pins in Block 6).

Drop **PWR_FLAG** on `+3V3` and `+3V3_IMU`. The IMU rail is deliberately separate from everything else for low gyro noise.

---

## Block 3 — STM32F405RGT6 core (the big one)
Place symbol **`MCU_ST_STM32F4:STM32F405RGTx`** (`A`, type STM32F405). Wire its support circuitry:

**Power pins**
- All `VDD` pins → `+3V3`; all `VSS` pins → `GND`. LQFP-64 has four VDD/VSS pairs — put a **100nF next to each** plus one **4.7µF bulk** on `+3V3` near the chip.
- `VDDA` → `+3V3` through the analog filter: **1µF + 10nF** to GND at the pin (optionally a ferrite/small resistor from +3V3 to VDDA). `VSSA` → `GND`.
- `VBAT` pin → `+3V3` with a **100nF** to GND (not using a coin cell; just tie to 3V3).

**VCAP (mandatory — core regulator)**
- `VCAP_1` (pin 31) → **2.2µF X5R/X7R** to GND, placed at the pin.
- `VCAP_2` (pin 47) → **2.2µF X5R/X7R** to GND, placed at the pin.
- Skip these and the chip won't boot reliably.

**Clock (HSE)**
- 8MHz crystal between `PH0`/`OSC_IN` and `PH1`/`OSC_OUT`.
- One load cap from each crystal pin to GND. **Value 22pF** (open item: confirm against the crystal's CL — 22pF suits CL ≈ 12–14pF).

**Boot / reset**
- `BOOT0` → **10k pull-down to GND**, plus a tact switch from BOOT0 to `+3V3` (press = enter DFU).
- `PB2/BOOT1` → **10k pull-down to GND** (must be low for system-memory DFU; floating = unreliable boot).
- `NRST` → tact switch to GND + **100nF** to GND.

**SWD programming header**
- 5-pin header (`Conn_01x05`): `+3V3`, `PA13`/SWDIO, `PA14`/SWCLK, `NRST`, `GND`. Leave PA13/PA14 otherwise unassigned.

---

## Block 4 — USB-C (wired; one ESD fix outstanding)
Place **USB-C receptacle** — schematic uses the KiCad `USB_C_Receptacle_USB2.0_14P` symbol (maps to the TYPE-C-31-M-12, C165948). The 14P is USB-2.0-only and omits the SBU pins, which is correct — this link is only a USB-2.0 device port for DFU + Betaflight config.

- **Reversible-pin shorting:** tie the two `D+` positions together (A6 + B6) and the two `D−` together (A7 + B7) at the connector — Type-C is reversible, so each line is duplicated.
- `VBUS` (A4) → the Schottky anode in Block 1. Tie all VBUS/GND positions on the connector together; don't leave duplicates floating.
- `GND` / shield pins → `GND`.
- `CC1` (A5) → **5.1k to GND**. `CC2` (B5) → **5.1k to GND** (Rd — tells the host this is a device/sink; without these no VBUS).
- `D+` → MCU `PA12`, `D−` → MCU `PA11`. **No series resistors** (F405 has internal impedance per AN4879).
- **ESD — USBLC6-2SC6 (C7519, SOT-23-6)**, placed right at the connector. Force the trace *through* it by renaming across it: connector side `USBC_DP`/`USBC_DM` → device side `USB_DP`/`USB_DM`. Pairing: I/O1 = pins 1 (connector) & 6 (MCU); I/O2 = pins 3 (connector) & 4 (MCU).
  - **FIX OUTSTANDING:** **pin 2 → `GND`** and **pin 5 → `VBUS`** must be connected — they were left floating, which disables the clamp (the steering diodes need both rails). Wire these before ERC.
- **Optional VBUS sense:** 10k/10k divider from `VBUS` to `PC0` (Betaflight `USB_DETECT_PIN`).

## Block 5 — ESC connector **J10** (verified pinout — handle VBAT/CRT with care)
Place the **JST SH 1.0 10-pin** (SM10B-SRSS-TB, C160409, `drone_lib`). **Status (2026-06-26): VBAT + GND wired; signal lines (M1–M4, CRT, ESC_TLM) + their ADC protection still TODO.** Wire per the **verified** table:

| ESC pin | Net | MCU | Protection on the FC side |
|---|---|---|---|
| TX (telemetry) | `ESC_TLM` | `PC7` (USART6 RX) or leave NC | none — non-functional on BLHeli_S |
| CRT (current) | `CURR_SENSE` | `PC2` | **1kΩ in series + 3.3V Zener from PC2 to GND** |
| S1 | `M1` | `PB0` | — |
| S2 | `M2` | `PB1` | — |
| S3 | `M3` | `PA3` | — |
| S4 | `M4` | `PA2` | — |
| NC | — | — | place a no-connect `Q` |
| VBAT | `VBAT_RAW` | `PC1` | **100kΩ/10kΩ divider → PC1, + 100nF from PC1 to GND** |
| GND | `GND` | — | — |
| GND | `GND` | — | — |

- **VBAT divider:** 100k from `VBAT_RAW` to node, 10k from node to GND, node → `PC1`, 100nF node→GND. This scales up-to-25.2V down to ~2.3V max. Without it you put pack voltage on a 3.3V pin.
- **`VBAT_RAW` also feeds the onboard buck (Block 1):** the same harness VBAT net runs to the TPS5430 VIN. The FC's current draw is small, so the harness VBAT line carries it fine — this is how the FC is powered in the air now that the external BEC is gone.
- **Current clamp:** 1k from `CRT` to `PC2`, then a 3.3V Zener from `PC2` to GND. The 1k limits fault current into the clamp.
- Motor nets `M1–M4` go straight to the timer pins; you'll remap S1–S4 order in Betaflight, so exact assignment isn't critical.

---

## Block 6 — IMU (ICM-42605) — **power wired 2026-06-26; SPI logic still TODO**
Place **ICM-42605** (C2655099, `drone_lib`). LGA-14. Datasheet pin map (Table 9): 1 AP_SDO/SDO · 4 INT1 · 5 VDDIO · 6 GND · 7 RESV · 8 VDD · 9 INT2/FSYNC · 12 AP_CS · 13 SCLK · 14 SDI/MOSI · 2/3/10/11 RESV.

**Power (DONE) — split rails on purpose:**
- `VDD` (pin 8) → **`+3V3_IMU`** (quiet TLV733P rail), with **0.1µF + 2.2µF** X7R at the pin (datasheet BoM C1/C2). This is the noise-sensitive analog/MEMS supply.
- `VDDIO` (pin 5) → **`+3V3`** (main logic rail — NOT the quiet rail), with **10nF** X7R at the pin (datasheet BoM C3). VDDIO carries digital I/O switching noise and sets the SPI logic level, so it rides the host rail; keeping it off `+3V3_IMU` protects the gyro noise floor. *(Change from earlier "both on +3V3_IMU" plan — see context_v3 2026-06-26 note.)*
- `GND` (pin 6) → `GND`. `RESV` **pin 7 → GND (mandatory** per datasheet). `RESV` pins 2, 3, 10, 11 → `GND` (datasheet allows NC or GND; GND is cleanest over the L2 pour). `INT2/FSYNC` (pin 9) → `GND` (FSYNC unused).

**SPI1 logic (TODO):**
- `SCLK` (13) → `PA5`, `SDO`/MISO (1) → `PA6`, `SDI`/MOSI (14) → `PA7`.
- `AP_CS` (12) → `PC4`, **plus a 10k pull-up from PC4 to `+3V3_IMU`** (keeps CS high during boot/DFU so the IMU isn't selected accidentally).
- `INT1` (4) → `PC3` (EXTI3) — keep this trace short in layout.
- SPI clock ≤24MHz (datasheet Table 6): use SPI1 prescaler /4 = 21MHz, never /2 (42MHz exceeds rating).
- Layout note (not schematic): IMU dead-center, axes aligned to the frame, solid ground under it.

## Block 7 — Blackbox flash (W25Q128 on SPI2)
Place **W25Q128JVSIQ** (C97521, `drone_lib`, SOIC-8). Powered from `+3V3`.

- `VCC` → `+3V3` with **100nF**. `GND` → `GND`.
- SPI2: `CLK` → `PB13`, `DO`/MISO → `PB14`, `DI`/MOSI → `PB15`.
- `CS` → `PB12`.
- `/WP` and `/HOLD` → tie to `+3V3` (disabled) unless you have a reason not to.

---

## Block 8 — Receiver header & spare pads
- **Receiver header** (4-pin `Conn_01x04`): `+5V`, `GND`, `TX`=`PA9` (UART1 TX), `RX`=`PA10` (UART1 RX). **Receiver = FlySky FS-iA6B → use i-BUS** (single serial wire on UART1 RX). i-BUS is non-inverted, so it connects straight to RX — **no inverter needed**. *(Reject S.BUS: it's inverted and the F405 has no internal UART inverter — would need an external transistor.)* PA10 is a 5V-tolerant (FT) pin, good since the iA6B is powered at 5V. TX pin is for optional i-BUS telemetry later. Betaflight: Serial RX on UART1, provider = IBUS. **Power pin = 5V** (iA6B input range 4.0–8.4V; never battery). One 3-wire run (5V/GND/RX) to the receiver's i-BUS port both powers it and carries data.
- **Spare UART3 pads:** `PB10` (TX), `PB11` (RX) — future GPS/MSP.
- **Spare USART6 pads:** `PC6` (TX), `PC7` (RX) — note PC7 may also carry the ESC `ESC_TLM` net if you wired it in Block 5; pick one use.
- **Spare I2C1 pads:** `PB6` (SCL), `PB7` (SDA) — future baro/mag. Add 4.7k pull-ups to `+3V3` on each if you expect to use them.

## Block 9 — Buzzer & status LED
**Buzzer (hand-soldered, DNP in JLC BOM):**
- `PC5` → gate of **2N7002** (via the gate; source to `GND`).
- Buzzer `+` pad → `+5V`; buzzer `−` pad → 2N7002 drain. Place **1N4148 flyback** across the buzzer (cathode to +5V, anode to drain).
- Use the SUN-12095 footprint (C360615) for the pads but mark the buzzer **DNP** so JLC skips it.

**Status LED:**
- `PB8` → **330Ω** → LED anode; LED cathode → `GND` (or wire active-low to +3V3 if you prefer — just be consistent in config). Avoid PC13 (forbidden — power-switch domain).

---

## Block 10 — Finish & check
1. **Place PWR_FLAGs** (once each) on `+5V`, `+3V3`, `+3V3_IMU`, `GND` if not already.
2. **No-connect (`Q`)** every genuinely unused MCU pin and the ESC `NC` pin, or ERC will warn.
3. Run **Inspect → Electrical Rules Checker (ERC)**. Resolve real errors (unconnected pins, conflicting outputs). Expect a few "unconnected" warnings on spare pads — those are fine to leave or no-connect.
4. **Annotate** (assign reference designators) and **assign footprints** (Tools → Assign Footprints): your `drone_lib` parts already carry footprints; assign standard 0402/0603 footprints to passives and attach their LCSC part numbers in the symbol's field (this drives the JLC BOM later).
5. Save, commit to your `.history` git, and only then move to PCB layout.

## Order of attack (so grounds/rails exist first)
Power (1–2) → MCU core + VCAP + clock + boot (3) → USB-C (4) → ESC connector + ADC protection (5) → IMU (6) → flash (7) → headers/spares (8) → buzzer + LED (9) → ERC (10).

## Watch-outs
- VCAP caps and the VBAT-pin 100nF are easy to forget and both cause "won't boot" symptoms.
- The VBAT divider and CRT Zener are safety-critical — never wire ESC VBAT/CRT straight to a GPIO.
- IMU rails are split on purpose: **`VDD` (pin 8) on `+3V3_IMU`** (quiet), **`VDDIO` (pin 5) on `+3V3`** (host logic rail). Don't put VDD on the main rail or VDDIO on the quiet rail.
- **USBLC6-2 pin 2 → `GND`, pin 5 → `VBUS`** — if either floats the ESD array does nothing (caught this in the draft).
- Single ground only — there is no separate "digital"/"power" GND net. Everything returns to one `GND` pour (L2); isolation is by split *power* rails + layout, not split grounds.
- Receiver header power pin = **5V**. Use the 5V-tolerant RX pin (PA10) for the i-BUS signal.
- Buck **input caps must be ≥50V** (6S = 25.2V); keep the buck switch node/loop tight and away from the IMU and ADC lines, or it shows up as gyro noise.
- The FC has **no external 5V input anymore** — power comes from `VBAT_RAW` (ESC harness) through the onboard TPS5430 buck.
