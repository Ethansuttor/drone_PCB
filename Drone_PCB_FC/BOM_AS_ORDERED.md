# BOM as ordered vs. board as fabricated

**Generated 2026-08-11** from LCSC order `WM2607190116` (2026-08-12) cross-checked
against `Drone_PCB_FC/Drone_PCB_FC.kicad_pcb` (the as-ordered board).

Per-board quantities in this file come from **counting footprints in the PCB
file**, not from `Drone_PCB_FC_LCSC_BOM.csv`. Where the two disagree, the PCB
wins — several designators in the CSV (C6, C9, C10, D2) do not exist on the
board, and several on the board (C31, D7, D9, R20, Q2, J4–J7) are absent from
the CSV.

---

## 1. Blockers — fix before assembly

### FB1 not ordered, and it is not optional

`FB1` (600 Ω @ 100 MHz ferrite bead, 0805, LCSC C139475) **is placed on the
board** and **was not ordered**.

It sits between `+3V3` and `Net-(U4-VDDA)` — i.e. it is the **only path
supplying VDDA (U4 pad 13)** on the STM32F405. C18 (10 nF) and C19 (1 µF)
decouple that node. With FB1 absent, VDDA floats: the ADC has no reference, the
PLL has no clean supply, and the internal reset behaves unpredictably. **The MCU
will not run reliably.**

**Workaround, good enough for bring-up and for flight:** bridge the FB1 pads
with a 0 Ω 0805 or a solder blob. You lose HF isolation on the analog supply,
nothing else. Order the real bead when convenient.

### Not ordered, needed for a complete board

| Ref | Part | Package | Consequence if omitted |
|---|---|---|---|
| **FB1** | 600 Ω @ 100 MHz bead (C139475) | 0805 | **VDDA unpowered — see above. Bridge it.** |
| **Q2** | 2N7000 | **TO-92 THT** | No buzzer driver. Note the board uses TO-92 through-hole, *not* the SOT-23 2N7002 the CSV lists. |
| **D8** | 1N914 | **DO-35 THT** | Buzzer flyback clamp. Omit only if BZ1 is also omitted. |
| **BZ1** | SUN-12095-5VPA7.6 (C360615) | 12 mm THT | No beeper. Not a bring-up blocker. |
| **SW1** | 6 mm tact — **NRST** | THT | Short the pads by hand instead. |
| **SW2** | 6 mm tact — **BOOT0** | THT | Needed to enter DFU on a blank F405. Short by hand for the first flash. |
| **J1** | 1×05 header 2.54 mm | THT | SWD (+3V3, SWDIO, SWDCLK, GND, NRST). Solder wires directly. |
| **J3** | 1×04 header 2.54 mm | THT | Receiver (+5V, GND, RX, TX). |
| **J4** | 1×04 header 2.54 mm | THT | SPI2 flash breakout. Optional. |
| **J5** | 1×04 header 2.54 mm | THT | SPI1/IMU breakout. Optional. |
| **J6** | 1×04 header 2.54 mm | THT | Motor M1–M4 breakout. Optional (CN1 carries motors to the ESC). |
| **J7** | 1×06 **socket** 2.54 mm | THT | Spare pads PC8–PC11, PA8, PB3. Optional. |

A single 1×40 header strip (e.g. C2337) covers J1, J3, J4, J5, J6. J7 wants a
female socket strip.

---

## 2. Solder jumpers — nine of them, all ship OPEN

**This is the most important section in this file.** The board carries **nine**
`SolderJumper_2_Open` footprints. `VERIFIED_PINOUT.md` documents only JP10. The
other eight are undocumented and several of them break the board completely if
left open.

| Jumper | Connects | Function | Must bridge? |
|---|---|---|---|
| **JP5** | AP2112K VOUT (U1.5, + C2) ↔ `+3V3` | **Main 3.3 V rail** | **YES — the entire 3.3 V rail is dead without it. MCU, IMU, flash, everything.** |
| **JP3** | D3 cathode ↔ `+5V` | Buck 5 V into common +5V rail | **YES for battery power** |
| **JP2** | `+BATT` ↔ `+BATT_after` | Pack feed to buck VIN, C5, R6 | **YES for battery power** |
| **USB5V1** | D4 cathode ↔ `+5V` | USB 5 V ORing, path A | **YES (or USB5V2) for USB power** |
| **USB5V2** | D7 cathode ↔ `+5V` | USB 5 V ORing, path B | at least one of the two |
| **JP10** | TLV733P VOUT ↔ `+3V3_IMU` | BMI270 VDD | **YES — "no gyro detected" without it** |
| **JP8** | 100 k/10 k divider node ↔ **PA1** | **Battery voltage sense** | YES if you want pack voltage |
| **CurrentM1** | R5/D1 node ↔ **PA2** | Current sense | YES for current telemetry |
| **JP6** | C3 low side ↔ `GND` | Ground return for C3 (1 µF on +5V) | Yes — decoupling only, but free |

**Practical bring-up order:** bridge JP5, JP3, JP2, USB5V1, USB5V2 and JP6
before first power-up. Bridge JP10 after you've confirmed the 3.3 V rail is
sane. Bridge JP8 and CurrentM1 whenever you want telemetry.

D3, D4 and D7 are all SS34 Schottkys in ORing configuration; D9 is the buck
catch diode. Both USB paths (D4 and D7) land on `+5V_USB` from J2, so they are
parallel — bridging both is fine.

---

## 3. Firmware corrections this cross-check produced

### Status LED is on PC14, not PC13

`U4 pad 2 (PC13)` is **unconnected**. The status LED chain is:

```
+3V3 → D6 (anode) → D6 cathode → R19 330Ω → U4 pad 3 = PC14
```

`VERIFIED_PINOUT.md` and `firmware/betaflight_target/ETHANF405/config.h` both say
`LED0_PIN PC13`. **Change to `PC14`.** PC14 is OSC32_IN, but no LSE crystal is
fitted, so it is free as GPIO. Same backup-domain drive limitation applies
(~3 mA sink, DS8626) — the LED will be dim regardless.

D5 is a separate always-on power indicator: `+3V3 → R14 470Ω → D5 → GND`.

### Battery voltage sense EXISTS — open item closed

`VERIFIED_PINOUT.md` lists PA1 as *"unconfirmed — may be no-connect"* and warns
that pack-voltage sensing may not exist. **It does.** The board has:

```
+BATT_after → R6 100k → node → R13 10k → GND
                         ├── C29 100nF → GND
                         └── JP8 (open) → U4 pad 15 = PA1
```

- Divider ratio is **exactly 11:1**. At 16.8 V full charge PA1 sees **1.53 V** — safe.
- Betaflight's default **`vbat_scale = 110`** corresponds to an 11:1 divider, so
  the default is already correct. Verify against a bench supply anyway.
- **Bridge JP8** or PA1 stays disconnected and you get no voltage telemetry.

`ADC_VBAT_PIN PA1` in `config.h` is therefore correct as written — it just needs
the jumper.

### Current sense confirmed

`ESC current → R5 1k → node (D1 3.3 V zener to GND) → CurrentM1 jumper → U4 pad 16 = PA2`.
`ADC_CURR_PIN PA2` is correct. Bridge CurrentM1.

---

## 4. Line-by-line: ordered vs. required

Per-board counts are actual footprint counts from the PCB file.

| LCSC | Part | Ordered | Per board | Designators | Boards |
|---|---|---|---|---|---|
| C15742 | STM32F405RGT6 | 5 | 1 | U4 | 5 |
| C2836813 | BMI270 | 5 | 1 | U7 | 5 |
| C2904431 | GD25Q16E (2 MB) | 5 | 1 | U3 | 5 |
| C114425 | TPS5450DDAR | 5 | 1 | U5 | 5 |
| C51118 | AP2112K-3.3 | 10 | 1 | U1 | 10 |
| C134139 | TLV73333P | 5 | 1 | IMU-U2 | 5 |
| C7519 | USBLC6-2SC6 | 10 | 1 | U2 | 10 |
| C2682775 | 8 MHz crystal, **CL 12 pF** | 5 | 1 | Y1 | 5 |
| C280546 | 15 µH inductor | 10 | 1 | L2 | 10 |
| C242173 | **220 µF POSCAP** | 5 | 1 | C8 | 5 |
| C5156756 | 10 µF 1210 | 5 | **1** | C5 only | 5 |
| C49678 | **100 nF 0805** | **20** | **10** | C14,C15,C20,C24–C27,C29,C30,C31 | **2** |
| C6119929 | 1 µF 0805 | 20 | 5 | C1–C4, C19 | 4 |
| C19110 | 2.2 µF 0805 | 10 | **2** | C12, C13 (VCAP) | 5 |
| C83170 | 10 nF 0805 | 50 | **2** | C7 (".01uF"), C18 | 25 |
| C376875 | 22 pF C0G 0805 | 50 | 2 | C21, C22 | 25 |
| C1779 | 4.7 µF 0805 | 20 | 1 | C28 | 20 |
| C2907219 | 10 kΩ 0805 | 100 | **9** | R3,R4,R7,R9,R13,R15,R17,R18,R20 | 11 |
| C27834 | 5.1 kΩ 0805 | 100 | 2 | R1, R2 | 50 |
| C2907232 | 1 kΩ 0805 | 100 | 1 | R5 | 100 |
| C149504 | 100 kΩ 0805 | 100 | 1 | R6 | 100 |
| C2933414 | 3.24 kΩ 1% 0805 | 100 | 1 | R8 | 100 |
| C114564 | 470 Ω 0805 | 100 | 1 | R14 | 100 |
| C17630 | 330 Ω 0805 | 100 | 1 | R19 | 100 |
| C8678 | SS34 SMA | 20 | **4** | D3, D4, D7, D9 | 5 |
| C2112 | BZT52C3V3 SOD-123 | 50 | 1 | D1 | 50 |
| C25170728 | LED 0805 (green) | 50 | 2 | D5, D6 | 25 |
| C160409 | JST SH 10P | 5 | 1 | CN1 | 5 |
| C2765186 | USB-C 16P | 20 | 1 | J2 | 20 |
| — | **FB1 bead** | **0** | **1** | FB1 | **0** |
| — | **2N7000 TO-92** | **0** | 1 | Q2 | **0** |
| — | **1N914 DO-35** | **0** | 1 | D8 | **0** |
| — | **Buzzer** | **0** | 1 | BZ1 | **0** |
| — | **6 mm tact** | **0** | 2 | SW1, SW2 | **0** |
| — | **2.54 mm headers** | **0** | 5+1 socket | J1, J3–J7 | **0** |
| C2925419 | KH-2012-HM1 2.4 GHz antenna | 10 | **0** | — | not in design |

### Board count

**Two complete boards, with zero spare 100 nF caps.** 100 nF is the sole binding
constraint at 20 pieces / 10 per board. Everything else stretches to four boards
or more. 0805s get lost off tweezers constantly — **order more 100 nF** (and
they cost nothing).

---

## 5. Substitutions vs. `Drone_PCB_FC_LCSC_BOM.csv`

### Needs a physical test-fit before paste

**L2 — Chilisin LVS606028-150M-N (C280546)** instead of Sunlord SWPA6045S150MT
(C83374).

- Value correct: 15 µH, 6.0 × 6.0 mm.
- **Ratings lower:** 1.8 A Irms / 2 A Isat / 95 mΩ, vs 2 A / 2.7 A / 71 mΩ.
- At 16.8 V in, 5 V out, 500 kHz, 15 µH: ΔI ≈ **0.47 A**. FC load is well under
  1 A, so peak inductor current ≈ 1.2 A — comfortably below the 2 A saturation
  point. **Fine in normal operation; no margin under fault or inrush.**
- Body height differs (2.8 mm vs 4.5 mm), which usually means a different land
  pattern. Board footprint is `drone_lib:IND-SMD_L6.0-W6.0`. **Test-fit.**

**J2 — SHOU HAN "TYPE-C 16PIN 2MD(073)" (C2765186)** instead of Hroparts
TYPE-C-31-M-12 (C165948).

- Both are **16P, SMD, right-angle Type-C** — these are near-universal clones of
  one land pattern, so it very likely drops in.
- Board footprint is specifically `Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12`.
- Lower ratings (3 A / 5 V vs 5 A / 20 V) are irrelevant for bench USB.
- **Test-fit.** A mismatch here is unrecoverable after reflow.

### Accept as-is

| Item | Change | Verdict |
|---|---|---|
| C5/C5156756 | 10 µF **100 V** 1210 instead of 50 V | Fine — *better* DC-bias retention at 16.8 V. Same 1210 land. |
| D5/D6 | **Green** 525 nm instead of red | Vf 2.4–3.1 V. At 330 Ω / 470 Ω on 3.3 V that's ~1.5–2 mA. Both LEDs will be dim but visible. PC14 can't sink more than ~3 mA anyway, so leave R19 alone; drop R14 to 220 Ω if you want a brighter power LED. |
| D1/C2112 | JSCJ instead of MDD BZT52C3V3 | Identical spec |
| 10 nF, 1 µF, 2.2 µF, 22 pF | Different manufacturers | All same value/dielectric/rating class. 2.2 µF at 25 V is fine — VCAP runs at 1.2 V. |
| Resistors | FOJAN / YAGEO instead of UNI-ROYAL | All 1%. **R8 3.24 kΩ is 1%** — confirmed, that's the one that matters (sets buck Vout). |
| U5 | TPS5450 ordered; board silk says TPS5430 | Pin-for-pin drop-in, already documented. Same 1.221 V ref, so 10 k/3.24 k still gives 5.0 V. |

### Resolved: crystal load caps

`C2682775` is spec'd **CL = 12 pF**. The open item carried since 2026-06-09
(*"confirm 22 pF against the chosen crystal's CL"*) is **closed** — 22 pF suits
CL 12–14 pF with 2–3 pF stray. C21/C22 are correct.

### C6 does not exist on the board

The CSV called for two 10 µF buck input caps (C5 + C6) and flagged *"C6 must
still be added to the schematic/board."* It never was. **The board has one**
(C5, on `+BATT_after`).

TPS5450 datasheet minimum input capacitance is 4.7 µF, so one 10 µF 100 V X7R
clears it. Input RMS ripple is ≈ Iout/2 — under 0.5 A at this board's load, which
a single 1210 handles. Expect slightly higher input ripple than the two-cap
design. **Acceptable. Note it and move on.**

---

## 6. Quick shopping list

Minimum to complete two boards:

- **600 Ω @ 100 MHz ferrite bead, 0805** — C139475 ×10 *(or bridge with 0 Ω / solder)*
- **100 nF 50 V X7R 0805** — C49678, another 50
- **2N7000 TO-92** ×10 and **1N914 DO-35** ×10 *(buzzer driver)*
- **SUN-12095-5VPA7.6 buzzer** — C360615 ×2
- **6 mm 4-pin THT tact switch** ×10
- **2.54 mm pin header strip 1×40** — C2337 ×2
- **2.54 mm female socket strip 1×40** ×1 *(for J7)*

Not needed: C2925419 (2.4 GHz ceramic antenna) is not part of this design.
