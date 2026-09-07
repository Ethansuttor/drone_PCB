# Bring-up checklist — ETHANF405

**Board state:** reflowed, all SMD placed, FB1 bridged with a wire link, all
nine solder jumpers bridged except `CurrentM1`.

**Bring-up run 2026-08-18/19. Results recorded inline.** The open jumpers were
an advantage — they isolate the board into sections so each one can be verified
before it can hurt the next. Work top to bottom.

---

## Results summary — 2026-08-19

| Subsystem | Status | Evidence |
|---|---|---|
| USB-C (J2), USBLC6, D+/D− | **PASS** | Enumerated as `STM32 BOOTLOADER`, USB ID `0483:DF11` |
| AP2112K → `+3V3` (JP5) | **PASS** | 3.3 V rail up, MCU runs |
| **FB1 link / VDDA** | **PASS** | `Vref=3.28V` in `status` — only readable if VDDA is live |
| **Y1 8 MHz + C21/C22** | **PASS** | `Clock=168MHz (PLLP-HSE)` — PLL locked to the external crystal |
| **C12/C13 VCAP** | **PASS** | Core regulator running; chip would be dead otherwise |
| **Status LED D6** | **PASS** | Blinks — after correcting `LED0_PIN` to **PC14** |
| **TLV733P → `+3V3_IMU` (JP10)** | **PASS** | Gyro came up as soon as it was bridged |
| **BMI270 (U7) on SPI1** | **PASS** | `GYRO=BMI270, ACC=BMI270`, `GYRO rate: 3225` |
| **Motor timers PB0/PB1/PA3/PB10** | **PASS** | TIM3_CH3/CH4, TIM2_CH4/CH3 — as designed |
| **Motor DMA** | **PASS** | DMA1 S7, S2, S6, S1 — four distinct streams, no clash. *Closes the "PB10 best-effort" flag* |
| **TPS5450 buck (U5, L2, C8, D9)** | **PASS** | **4.98 V** from a 15.58 V pack — 0.4% off target |
| **VBAT divider + JP8 → PA1** | **PASS** | 1.4 V at PA1 from 15.58 V (11.13:1). Reads 15.45 V at `vbat_scale 110`; trimmed to **111** |
| **ADC1** | **PASS after fix** | Required `ADC1_DMA_OPT 1` — see the DMA section below |
| **U3 GD25Q16E flash on SPI2** | **FAIL — DEAD CHIP** | `flash_info` → `JEDEC ID=0x00000000`. Every pin re-verified good 2026-09-03 — chip itself is bad. See below |
| Current sense (PA2) | **PASS** | Reads correctly with motors under load |
| Receiver (i-BUS, USART1) | **PASS** | Bound to TX, live channel data in the Betaflight Receiver tab |
| Motors (M1–M4 DShot) | **PARTIAL PASS** | One motor spun at full throttle on the bench — ran clean. Other three not yet exercised individually |
| Battery + USB simultaneous (ORing D3/D4/D7) | **PASS** | Motor spun with the LiPo and USB-C both connected at once — no short, no fighting between the two 5V sources |
| Failsafe | **NOT STARTED** | — |

---

## Update — 2026-09-03

Board fully assembled, all subsystems from the 2026-08-19 run still good.

- **Power ORing (D3/D4/D7) — PASS.** Powered the board from the LiPo (CN1)
  and USB-C (J2) at the same time and spun a motor. No short, no fighting
  between the two 5V rails — confirms the ORing diodes and the `+5V` plane
  hold up with both sources hot simultaneously.
- **Receiver — PASS.** i-BUS receiver bound to the transmitter, live
  channel data confirmed in the Betaflight Receiver tab.
- **Motors — PARTIAL PASS.** DShot timer/DMA lines were confirmed clean at
  bring-up (Stage 10); now bench-tested one motor at full throttle with the
  ESC connected — spun up clean, no issues. Other three motors not yet
  spun individually.
- **Current sense (PA2) — PASS.** Reads correctly with motors under load.
- **U3 blackbox flash — CONFIRMED DEAD CHIP.** Re-verified every pin on the
  "Ruled out" list below by hand — VCC, /HOLD, /WP, /CS, all four SPI2
  continuity points, pin-1 orientation, no MISO short. Every one of them
  checks out exactly as it should. With the SPI2 plumbing, the pull-ups and
  the pinout all confirmed good, the only thing left that explains
  `JEDEC ID=0x00000000` is the die itself — replace U3 with one of the five
  spares.
- Failsafe: still untested — needs an RX signal-loss test.

---

## THE ONE THAT WILL WASTE YOUR DAY — ADC DMA collision

**Read this before debugging any zero ADC reading.**

**Symptom:** `status` shows a perfectly healthy `Vref=3.28V` and a live
`Core temp=39degC` climbing over time, while directly underneath it sits
`Voltage: 0 * 0.01V (0S battery - NOT PRESENT)` — with 1.4 V confirmed on PA1,
the divider verified, JP8 verified, `battery_meter = ADC`, `vbat_scale = 110`,
`adc_device = 1`, and `resource ADC_BATT 1 A01` all correct.

**Cause:** on the F405, ADC1's regular-conversion DMA defaults to **DMA2 Stream
0 Channel 0**. SPI1_RX — the gyro — defaults to **DMA2 Stream 0 Channel 3**.
Same stream. SPI1 is allocated first and wins. `adc_stm32f4xx.c` then does:

```c
if (!dmaAllocate(dmaGetIdentifier(adc.dmaResource), OWNER_ADC, 0)) {
    return;
}
```

It gives up **silently** — no error, no boot warning, no log line. The regular
conversion sequence never runs and **every external ADC channel reads a hard
zero forever**: VBAT *and* current, both.

**Why it hides so well:** on F4, Vref and core temperature are read through
**injected channels triggered by software polling**, which need no DMA at all.
So the ADC looks completely healthy in `status` while the channels you actually
care about are dead. Everything downstream of the pin measures perfect and you
will blame the solder jumper, the divider, the resistors, the ADC pin map and
the chip itself before you blame DMA. All of those were measured good on this
board while the reading stayed at zero.

**Fix — permanent, now in `config.h`:**

```c
#define ADC1_DMA_OPT 1      // ADC1 -> DMA2 Stream 4, clear of SPI1
```

**Fix — runtime, does NOT survive a full-chip-erase reflash:**

```
dma ADC 1 1
save
```

**This is why "verify DMA at bring-up" isn't just about the motors.** The four
DShot streams were clean on the first check; the collision was ADC against
SPI1, which nothing in the original checklist told you to look at.

---

## CONFIRMED FAULT — U3 blackbox flash chip is dead

```
# flash_info
Flash sectors=0, sectorSize=0, pagesPerSector=0, pageSize=0, totalSize=0 JEDEC ID=0x00000000
```

`Devices detected: SPI:1` — that one device is the gyro. The flash never
registered.

**`0x00000000` rather than `0xFFFFFF` matters:** an open MISO usually floats
high and reads all ones. All zeros means the MCU clocked out the read command
and saw the data line held low the whole time — the chip never answered.

**Ruled out, all measured on the board:**

- [x] VCC (pin 8) = 3.3 V
- [x] /HOLD (pin 7) = 3.3 V — R18 pull-up good
- [x] /WP (pin 3) = 3.3 V — R15 pull-up good
- [x] /CS (pin 1) = 3.3 V idle — R17 pull-up good
- [x] Continuity pin 2 → PC2 (U4 pin 10), pin 5 → PB15, pin 6 → PB13, pin 1 → PB12
- [x] Pin-1 dot orientation correct
- [x] **No short:** MISO to GND reads 1.4 MΩ
- [x] **PC2 is a valid SPI2 MISO** in Betaflight's F4 `spiHardware[]` table
- [x] `flash_spi_bus = 2`
- [x] SPI plumbing proven good — the BMI270 works on SPI1

**Confirmed 2026-09-03:** every pin re-tested by hand — VCC, /HOLD, /WP,
/CS, all four SPI2 continuity points, pin-1 orientation, no MISO-to-GND
short. All good, exactly as the list above states. Nothing left to blame but
the die itself.

**Next action:** replace U3 — there are five spares. Keep the hot-air nozzle
narrow and away from U7; the BMI270 is the one part on this board that
shouldn't be reflowed twice. No need to re-attempt reflow-and-retest on the
old chip first — the pin-level verification already rules everything else
out.

Not a blocker for anything else. Blackbox logging is the only casualty.

---

## Stage 0 — Visual, before any meter touches it

- [ ] Magnify every LQFP-64 lead, all four sides. Corners first — that's where bridges hide.
- [ ] U7 BMI270 — check it isn't rotated or offset. You can't see the joints; check the package edges against the pad outlines.
- [ ] U3 SOIC-8, U1 / IMU-U2 SOT-23-5, U2 SOT-23-6 — pin 1 orientation.
- [ ] **U1 vs IMU-U2 are not swapped.** Both SOT-23-5, both 3.3 V regulators, different parts. Read the top marking.
- [ ] Nothing tombstoned.

**Polarity — the four that destroy things:**

- [ ] **C8** stripe (= ANODE, +) faces `+5V_Buck`, not GND
- [ ] **D9** band faces the PH switch node (pad 1), not GND
- [ ] **D3 / D4 / D7** bands all face pad 1 (the jumper side)
- [ ] **D6** anode to `+3V3`; **D5** cathode to GND

Photograph the board and zoom in on your phone. You will catch something.

---

## Stage 1 — Shorts check, no power

DMM in resistance mode. **Readings will drift upward** as the meter charges the
decoupling — C8 is 220 µF and C5 is 10 µF, so give them time to settle.

| Between | Expected | Red flag |
|---|---|---|
| `+BATT` ↔ GND | Open (JP2 is open) | Any low reading |
| `+BATT_after` ↔ GND | Climbs high (C5 charging) | Settles < 50 Ω |
| `+5V_Buck` ↔ GND | **See note below** | — |
| `+5V` ↔ GND | Climbs high; isolated from all sources | Settles < 50 Ω |
| `+3V3` ↔ GND | Climbs high, > 1 kΩ settled | Settles < 50 Ω |
| `+3V3_IMU` ↔ GND | Climbs high (only C4, C14, U7 pad 8) | Settles < 50 Ω |
| GND corner ↔ GND corner | < 1 Ω | Anything higher = via stitching problem |

> **`+5V_Buck` to GND will read a diode drop in one direction — this is normal.**
> The path is `+5V_Buck → L2 → PH → D9 → GND`. In diode mode with the red probe
> on GND you'll see roughly **0.25–0.35 V** forward through the Schottky.
> Don't mistake it for a short. Reverse the probes and it should read open.

- [ ] Verify all nine jumpers are still open: JP2, JP3, JP5, JP6, JP8, JP10, USB5V1, USB5V2, CurrentM1.

> **Correction, 2026-08-19:** the original version of this file warned that
> reflow paste could bridge the jumpers. It can't — all nine
> `SolderJumper_2_Open` footprints carry `F.Cu` + `F.Mask` only, **no
> `F.Paste`**, confirmed by parsing the PCB file. There is no stencil aperture
> over them. That warning only applies if you hand-paste.

**Stop here if anything reads low.** Find it before applying power.

---

## Stage 2 — Bridge FB1

- [ ] Solder a wire link across FB1 (resistor lead offcut or a single wire strand).
- [ ] **Verify continuity `+3V3` ↔ U4 pin 13 (VDDA).** Must beep.

Without this the MCU will not run and you risk exceeding the 300 mV VDD−VDDA
limit on every power cycle. Do not skip it.

> **Confirmed working 2026-08-19** — `Vref=3.28V` in `status`. That reading
> comes through ADC1's internal reference channel and is only possible with
> VDDA powered, so it's a positive test of the FB1 link, not just an absence
> of symptoms.

---

## Stage 3 — USB connector only, nothing else live

Both ORing jumpers still open, so `+5V` is isolated from everything. **Zero risk
to any IC at this step.**

- [ ] Plug in USB (use a current-limited supply or a USB meter).
- [ ] Measure at the `+5V_USB` side of USB5V1 or USB5V2 (or at D4/D7 anode): **~5.0 V**.
- [ ] Current draw: essentially zero.

If there's no 5 V here, the USB-C connector is the problem — the SHOU HAN part
you fitted isn't the one the footprint was drawn for. Check CC1/CC2 have their
5.1 k pulldowns (R1, R2) and that the shell soldered properly.

> **PASS** — the SHOU HAN connector dropped into the HRO footprint without
> issue.

---

## Stage 4 — Bring up +5V, test the LDOs in isolation

- [ ] Power off. Bridge **USB5V1**, **USB5V2**, **JP6**.
- [ ] Power on via USB, current limit ~200 mA.

| Measure | Expected |
|---|---|
| `+5V` | **~4.6–4.7 V** (5 V minus the SS34 forward drop) |
| Current draw | A few mA — only LDO quiescent |
| **JP5 pad 1** (AP2112K output, at C2) | **3.30 V ±3%** |
| **JP10 pad 1** (TLV733P output) | **3.30 V ±3%** |

`+3V3` is still isolated by JP5, so both LDOs are running unloaded. **This
verifies both regulators before you connect them to anything.**

Anything warm at this point — stop and investigate.

---

## Stage 5 — Bring up the 3.3 V rail

- [ ] Power off. Bridge **JP5**.
- [ ] Power on, current limit ~200 mA.

| Measure | Expected |
|---|---|
| `+3V3` | **3.30 V ±3%** |
| **VDDA** (U4 pin 13, through your FB1 link) | **3.30 V** — must match `+3V3` within 300 mV |
| Current draw | ~20–50 mA (MCU idle, flash, BMI270) |
| `+3V3_IMU` | **0 V** — correct, JP10 still open |
| D5 power LED | **Lit** (~2.3 mA, dim-ish green) |

A blank F405 with BOOT0 low won't enumerate — that's expected, not a fault. It
hard-faults on empty flash and sits there, which looks identical to "dead" from
outside. **Judge by VCAP_1/VCAP_2 reading ~1.2 V and by DFU enumeration, not by
"nothing happens".**

---

## Stage 6 — Prove the MCU is alive

**USB DFU is the highest-value single test.** It proves the 3.3 V rail, the core
regulator, reset release, **the crystal**, the USB-C connector, D+/D− routing,
the USBLC6, and that the CPU executes — all at once.

- [ ] SW2 isn't fitted — short the **BOOT0** pads to `+3V3` by hand.
- [ ] Replug USB.
- [ ] Host enumerates **`STM32 BOOTLOADER`**, USB ID **`0483:DF11`**.

> It will appear under **Other devices** with a yellow triangle — that's a
> missing driver, **not** a fault. Run Zadig and bind **WinUSB** to it. See
> `betaflight_target/BUILD.md` §3.1.

> **The F4 DFU bootloader clocks USB off HSE**, so enumeration is itself proof
> that Y1, C21 and C22 are good. Confirmed later by `Clock=168MHz (PLLP-HSE)`.

**SWD as an alternative** — independent of USB, and worth wiring if DFU fails.
J1: pin 1 `+3V3`, 2 `SWDIO`, 3 `SWDCLK`, 4 `GND`, 5 `NRST`. Expect IDCODE
**`0x413`** for STM32F405.

---

## Stage 7 — IMU rail

- [ ] Power off. Bridge **JP10**.
- [ ] Power on. Measure `+3V3_IMU` = **3.30 V**.

Note C4 and C14 sit on the far side of JP10, so until now the TLV733P has been
running with no output capacitor. Bridging JP10 fixes that.

> **PASS.** The BMI270 appeared immediately on the next connect:
> `Gyros detected: gyro 1 locked dma`, `GYRO=BMI270, ACC=BMI270`,
> `GYRO rate: 3225` (the documented 3.2 kHz ceiling), CPU 19%, cycle time
> 310 µs. The LGA-14 placement, the re-drawn `-BR` footprint and every
> re-routed IMU net are all good.

> **LED diagnostic worth knowing:** before JP10, D6 gave two quick blinks at
> boot then went dark. That's `indicateFailure(FAILURE_MISSING_ACC, 2)` — the
> firmware reporting no gyro, not a generic startup flash. After JP10 it
> switched to continuous blinking. A repeating burst pattern forever means
> failure mode; one short burst then quiet at power-up is normal.

---

## Stage 8 — Buck converter

**Bench supply, not a LiPo, the first time.** You want a current limit. A 4S
pack will push hundreds of amps into a reversed diode; a bench supply just sits
in current limit and tells you something's wrong.

- [ ] USB unplugged. Bridge **JP2** first, leaving **JP3** open so the buck
      output stays isolated from `+5V` and the MCU.
- [ ] Bench supply to CN1 pin 3 (`+BATT`) and GND. Start at **12–14.8 V, limit
      200–300 mA.**
- [ ] **If it slams into current limit, kill it** — that's D9 or C8 backwards.

| Measure | Expected |
|---|---|
| `+5V_Buck` | **5.00 V ±2%** (R7 10 k / R8 3.24 k, V_ref 1.221 V) |
| `+5V` | ~4.65 V (after the D3 drop) |
| `+3V3` | 3.30 V |
| Input current | ~30–60 mA at 14.8 V |

- [ ] Then bridge **JP3**.
- [ ] **Scope the PH node** (D9 cathode / L2 pad 1). Clean switching at ~500 kHz.
- [ ] **Scope `+5V_Buck` for ripple.** More than ~50 mV pk-pk suggests the output cap ESR is wrong.
- [ ] Feel L2 and U5 after a minute. Warm is fine, hot is not.

If `+5V_Buck` is **1.62 V**, R7 and R8 are swapped.

> **PASS — 4.98 V measured off a 15.58 V pack.** 0.4% from target, well inside
> the 1% feedback resistors. U5, L2, C8, D9 and R7/R8 all confirmed.

---

## Stage 9 — Telemetry jumpers

- [ ] Bridge **JP8** (VBAT sense → PA1).
- [ ] With 14.8 V applied, measure PA1: should be **1.35 V** (14.8 / 11).
      At 16.8 V, PA1 = **1.53 V**.

> **PASS — 1.4 V at PA1 from a 15.58 V pack** = 11.13:1 against a nominal 11:1.
> Betaflight reported 15.45 V at the stock `vbat_scale = 110`, trimmed to
> **111** (`110 × 15.58 ÷ 15.45`).
>
> **This only worked after `ADC1_DMA_OPT 1`.** Before that it read a flat zero
> with 1.4 V sitting on the pin. See the DMA section at the top of this file.

**Before bridging `CurrentM1`, verify the ESC side is safe:**

- [ ] Identify the pads — one beeps to **U4 pin 16 (PA2)**, the other goes
      through R5 toward **CN1 pin 9**. Measure the CN1 side.
- [ ] Unpowered: that pad to GND should read **high**. D1 is a 3.3 V zener
      sitting reverse-biased. Near zero means D1 is dead or backwards.
- [ ] Unpowered: ~**1 kΩ** from that pad to CN1 pin 9, through R5.
- [ ] Powered, motors stopped: **0 V to a few hundred mV.** Anything near 3.3 V
      or at pack voltage means stop — that's a mirrored harness, and bridging
      would put it on PA2.

> A working sensor and a dead signal path both read ~0 V with motors stopped.
> This test says it's **safe** to bridge, not that it **works**. You can't
> confirm the current sense until something draws real current — the shunt is
> inside the ESC, so a dummy load across the battery bypasses it entirely.

---

## Stage 10 — Firmware

Full procedure in `betaflight_target/BUILD.md`.

- [ ] Build **4.5.5**, not `master` — Configurator 10.10.0 rejects anything newer.
- [ ] `USE_ACCGYRO_BMI270`, not the split gyro/acc defines.
- [ ] `LED0_PIN` = **PC14**. PC13 is physically unconnected.
- [ ] **`ADC1_DMA_OPT 1`** — or VBAT and current read a silent zero.
- [ ] Flash with **Full chip erase** whenever the firmware version changes.
- [ ] `set blackbox_sample_rate = 1/4` — the fitted flash is 2 MB, not 16 MB.
- [ ] Confirm gyro detected (BMI270). If "no gyro," check JP10 first.
- [ ] `resource`, `timer`, `dma` — confirm the TIM2/TIM3 DShot streams are clear.
      **The command is `dma`. `dma show all` returns `BAD DEVICE: show`.**
- [ ] `dma ADC 1` — confirm ADC1 is **not** sharing a stream with SPI1.
- [ ] Trim `vbat_scale` against a DMM.
- [ ] Re-derive `GYRO_1_ALIGN` on the bench. Iterate at runtime with
      `set gyro_1_sensor_align = ...` + `save` — no rebuild needed — then bake
      the winner into `config.h`.
- [ ] **Motor test with the ESC unplugged.** Scope PB0 / PB1 / PA3 / PB10 for
      DShot activity before anything can spin.

> **Expected and permanent on this board:** `Configurator has detected the
> board: ETHANF405 but no official Betaflight target was found` and
> `Configurator failed to verify the board`. ETHANF405 isn't in Betaflight's
> online database and never will be. Not a fault.

---

## Solder jumper failure mode — check all of them

A jumper blob can look bridged, hold on mechanically, and still have wetted
only **one** pad. Beep continuity **across** each jumper's two pads rather than
trusting the look of it. A half-wetted jumper that works today can open up from
vibration later, which is a much worse day to find out.

---

## Jumper summary

| Bridge at stage | Jumpers |
|---|---|
| 2 | FB1 (wire link) |
| 4 | USB5V1, USB5V2, JP6 |
| 5 | JP5 |
| 7 | JP10 |
| 8 | JP2, JP3 |
| 9 | JP8, CurrentM1 |
