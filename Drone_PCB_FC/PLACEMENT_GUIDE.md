# Placement guide — what goes where

**Generated 2026-08-11** from `Drone_PCB_FC/Drone_PCB_FC.kicad_pcb` (as-ordered board),
by reading the actual pad-to-net connections. Quantities are per board.

Organised **by part value**, because that's how you should assemble: open one
strip, place every position for that value, seal the strip, open the next.
Never have two strips out at once.

---

## Resistors — 0805

All 1%. The 4-digit marking on top tells you the value; **use it**, because
several of these sit next to each other and a swap is silent until power-up.

| Value | Marking | Qty | Where | Function |
|---|---|---|---|---|
| **10 kΩ** | `1002` | **9** | R3 | Pull-up on `IMU_nCS` (holds BMI270 CSB high through POR — required, this is what puts the chip in SPI mode) |
| | | | R4 | BOOT0 pull-down |
| | | | R7 | **Buck feedback upper** (+5V_Buck → VSENSE) — 1% is mandatory |
| | | | R9 | BOOT1 / PB2 pull-down |
| | | | R13 | **VBAT divider lower leg** → GND |
| | | | R15 | Flash IO2 (/WP) pull-up |
| | | | R17 | Flash /CS pull-up |
| | | | R18 | Flash IO3 (/HOLD) pull-up |
| | | | R20 | Buzzer MOSFET gate pull-down |
| **100 kΩ** | `1003` | 1 | R6 | **VBAT divider upper leg** (+BATT_after → node) |
| **5.1 kΩ** | `5101` | 2 | R1, R2 | USB-C CC1 / CC2 pull-downs (these are what make a host supply 5 V) |
| **3.24 kΩ** | `3241` | 1 | R8 | **Buck feedback lower** (VSENSE → GND). With R7 sets 5.00 V. 1% mandatory |
| **1 kΩ** | `1001` | 1 | R5 | Current-sense series limiter (`Current_meter` → clamp node) |
| **330 Ω** | `3300` | **2** | R19 | Status LED D6 series resistor (to PC14) |
| | | | R14 | Power LED D5 series resistor — **changed from 470 Ω, see note below** |

> **R14 changed 470 Ω → 330 Ω (2026-08-11, as built).**
> The 470 Ω value was sized for the red LED originally specified; the LEDs
> actually fitted are green (Vf ≈ 2.55 V), which cut D5 to ~1.6 mA. At 330 Ω
> D5 runs ~2.3 mA. No constraint applies on that path — D5 goes straight to
> ground, unlike D6 which must respect PC14's 3 mA backup-switch limit.
> **470 Ω is now unused on this board.** D5 will be visibly brighter than D6;
> that is expected, since D6 loses ~0.3 V inside the MCU's power switch.

### The two swaps that will bite you

- **R6 (`1003`) vs R13 (`1002`)** — adjacent, both in the VBAT divider. Swapped,
  PA1 sees 15.3 V instead of 1.53 V and the pin dies the first time you connect a pack.
- **R7 (`1002`) vs R8 (`3241`)** — adjacent, both buck feedback. Swapped, the buck
  outputs 1.221 × (1 + 3.24/10) = **1.62 V** instead of 5 V. Nothing downstream runs.

Place all resistors value-side-up so you can verify afterwards.

---

## Capacitors — 0805 unless noted

**These are unmarked.** Once two values are mixed on the bench you cannot tell
them apart. One strip at a time, no exceptions.

| Value | Qty | Where | Function |
|---|---|---|---|
| **100 nF** | **10** | C24, C25, C26, C27, C30, C31 | MCU VDD decoupling — one per VDD pin (U4 pins 1, 19, 32, 48, 64) plus a spare. Place these as close to the pins as the layout allows |
| | | C15 | **BMI270 VDDIO** decoupling (on `+3V3`) |
| | | C14 | **BMI270 VDD** decoupling (on `+3V3_IMU`) |
| | | C20 | NRST filter |
| | | C29 | VBAT divider filter (across R13) |
| **1 µF** | 5 | C1 | +5V rail bulk / AP2112K input |
| | | C2 | AP2112K **output** (on the JP5 side, before the jumper) |
| | | C3 | +5V decoupling — **low side returns to GND through JP6** |
| | | C4 | TLV733P output / IMU rail bulk (on `+3V3_IMU`, after JP10) |
| | | C19 | VDDA bulk (with FB1 and C18) |
| **2.2 µF** | **2** | C12 | **VCAP_1** (U4 pin 31) |
| | | C13 | **VCAP_2** (U4 pin 47) |
| **10 nF** | **2** | C7 (silk says `.01uF`) | **Buck BOOT cap** — U5 BOOT to PH switch node |
| | | C18 | VDDA HF decoupling |
| **22 pF** | 2 | C21, C22 | Crystal load caps, on PH0 / PH1. **Correct value — Y1 is CL = 12 pF** |
| **4.7 µF** | 1 | C28 | +3V3 rail bulk |
| **10 µF / 1210** | **1** | C5 | **Buck input** (on `+BATT_after`). C6 does not exist on this board |
| **220 µF POSCAP / 7343** | 1 | C8 | **Buck output** (on `+5V_Buck`). Polarity — see below |

### C12 / C13 are the ones that matter most

Both are VCAP. A 100 nF in either position and the F405 core regulator never
comes up — the chip is dead on the bench with no visible cause. Do these two
deliberately, on their own, and double-check before moving on.

---

## Diodes

| Part | Qty | Where | Function | Orientation |
|---|---|---|---|---|
| **SS34 / SMA** | **4** | D9 | **Buck catch diode** — cathode to PH switch node, anode to GND | Band → PH (pad 1) |
| | | D3 | Buck ORing — anode `+5V_Buck`, cathode → JP3 → `+5V` | Band → pad 1 (JP3 side) |
| | | D4 | USB ORing A — anode `+5V_USB`, cathode → USB5V1 → `+5V` | Band → pad 1 |
| | | D7 | USB ORing B — anode `+5V_USB`, cathode → USB5V2 → `+5V` | Band → pad 1 |
| **BZT52C3V3 / SOD-123** | 1 | D1 | 3.3 V clamp on the current-sense node | Band (cathode) → signal side, anode → GND |
| **LED 0805 (green)** | 2 | D5 | **Power indicator** — `+3V3 → R14 470 Ω → D5 → GND`. Always on | Cathode → GND (pad 1) |
| | | D6 | **Status LED** — `+3V3 → D6 → R19 330 Ω → PC14`. MCU sinks | Anode → +3V3 (pad 2) |
| **1N914 / DO-35 THT** | 1 | D8 | Buzzer flyback | **Band (cathode) → +5V side** |

D3, D4 and D7 are all the same SS34. D4 and D7 are in parallel off `+5V_USB` —
two ORing paths with their own jumpers.

---

## Everything else

| Ref | Part | Notes |
|---|---|---|
| **U4** | STM32F405RGT6, LQFP-64 | Pin 1 dot. Self-aligns well in reflow |
| **U7** | BMI270, LGA-14 | **±0.1 mm — no leads, no self-alignment.** Pads 2/3 (ASDx/ASCx) go to `+3V3` = VDDIO, correct per datasheet Table 22. Pad 8 = `+3V3_IMU` behind JP10 |
| **U3** | GD25Q16E, SOIC-8 | 2 MB, not 16 MB. Works as-is. Set `blackbox_sample_rate = 1/4` |
| **U5** | TPS5450DDAR, ESOP-8 | **Pad 5 (ENA) is NC — floating, which is correct**, the part enables when ENA floats. Pad 9 is the thermal pad — don't over-paste it or the IC floats |
| **U1** | AP2112K-3.3, SOT-23-5 | Pads 1 and 3 both on `+5V` = VIN and EN tied together, always-on. Output on pad 5 → JP5 |
| **IMU-U2** | TLV73333P, SOT-23-5 | Same pin arrangement as U1. **Don't mix these two up — both SOT-23-5, both 3.3 V.** Check the top marking |
| **U2** | USBLC6-2SC6, SOT-23-6 | USB ESD. Pads 1/3 to the connector side, 4/6 to the MCU |
| **Y1** | 8 MHz crystal, 3225 | Pins 1/3 are quartz, 2/4 are the grounded lid. **Orientation doesn't matter — 0° or 180° both work.** Only a 90° rotation is wrong |
| **L2** | 15 µH, 6×6 mm | **Test-fit first** — the Chilisin part you got has a different body height to the footprint's source part |
| **FB1** | 600 Ω bead, 0805 | **Not ordered. Bridge with a wire link** — this is the only VDDA supply path |
| **J2** | USB-C 16P | **Test-fit first** — you got a SHOU HAN part, board footprint is drawn for the Hroparts one |
| **CN1** | JST SH 10P | ESC harness. Pin 3 = +BATT, 8/7/6/5 = M1/M2/M3/M4, 9 = current sense, 1/2/11/12 = GND |
| **Q2** | 2N7000, **TO-92 THT** | Buzzer driver. Pad 1 = Source (GND), 2 = Gate, 3 = Drain. **Not ordered** |
| **BZ1** | Active buzzer, THT | **Not ordered** |
| **SW1** | Tact, THT | **NRST** → GND. Not ordered |
| **SW2** | Tact, THT | **BOOT0** → +3V3. Not ordered. Needed for the first DFU flash |
| **J1** | 1×05 header | SWD: +3V3, SWDIO, SWDCLK, GND, NRST |
| **J3** | 1×04 header | Receiver: +5V, GND, RX, TX |
| **J4** | 1×04 header | SPI2 breakout: SCK, Flash /CS, GND, GND |
| **J5** | 1×04 header | SPI1 breakout: SCK, IMU_nCS, EXTI3, GND |
| **J6** | 1×04 header | Motor breakout: M1, M2, M3, M4 |
| **J7** | 1×06 **socket** | Spares: PC8, PC9, PC10, PC11, PA8, PB3 |

---

## Polarity — the four that can destroy something

1. **C8, 220 µF POSCAP** — the stripe is the **ANODE (+)**, opposite of an
   aluminium electrolytic. It goes to `+5V_Buck`. Backwards on a 5 V rail it vents.
2. **D9, buck catch diode** — band to the PH switch node. Backwards is a dead
   short across the buck output.
3. **D3 / D4 / D7 ORing** — bands all to the `+5V` side (pad 1). Backwards
   defeats the ORing and back-feeds USB.
4. **D8, 1N914** — band to `+5V`. Backwards it conducts continuously across the buzzer.

---

## Reminder: nine solder jumpers ship open

None of them are optional-by-default. See `BOM_AS_ORDERED.md` § 2 for the full map.
Short version, before first power-up:

**JP5** (whole 3.3 V rail), **JP3** + **JP2** (battery path), **USB5V1** /
**USB5V2** (USB power), **JP6** (C3 ground return). Then **JP10** for the gyro,
**JP8** for battery voltage, **CurrentM1** for current sense.

One extra note on JP10: **C4 and C14 sit on the far side of it**, so with JP10
open the TLV733P has no output capacitor at all. It's unloaded so nothing bad
happens, but bridge JP10 early rather than leaving the LDO running open-loop.
