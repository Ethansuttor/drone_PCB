# Bring-up checklist — ETHANF405

**Board state:** reflowed, all SMD placed, FB1 bridged with a wire link, all
nine solder jumpers bridged except `CurrentM1`.

**Bring-up run 2026-08-18/19. Results recorded inline.** The open jumpers were
an advantage — they isolate the board into sections so each one can be verified
before it can hurt the next. Work top to bottom.

---

# BOARD STATUS as of 2026-09-14 evening: ALIVE, BLACKBOX FLASH RESOLVED

**The board works and the blackbox flash works.** It boots, enumerates, runs
Betaflight, reads the gyro and the ADC, drives four motors, and U3 now
enumerates as a 2 MB GD25Q16E.

**Root cause of the flash fault: a missing line in Betaflight's chip table.**
The GD25Q16E's JEDEC ID `0xC84015` is not present in `m25p16FlashConfig[]` in
`src/main/drivers/flash_m25p16.c` at the 4.5.5 tag. That table has twenty
entries covering Macronix, Micron, Winbond, PUYA, Zbit, Cypress and BergMicro,
and not one GigaDevice part. U3 was answering correctly on every attempt.
`m25p16_identify()` walked the table, found no match, returned false, and
`flashSpiInit()` discarded the answer and returned false. Fixed by one entry:

```c
// GigaDevice GD25Q16E
{ 0xC84015, 104, 50, 32, 256 },   // 32 sectors x 256 pages x 256 B = 2 MB
```

Geometry is copied from the Winbond W25Q16 entry `0xEF4015` directly above it,
which is the same 2 MB organisation, so it is a known-good shape and not a
guess. Clock limits are from the GD25Q16E datasheet.

**Everything the 2026-09-13 and 2026-09-14 diagnosis concluded about the
hardware was correct. Everything it concluded about the firmware was wrong.**
Four specific claims in the post mortem below have been retracted. They are
marked inline. Read part 9 first, then the corrections, and do not act on parts
3, 5 or 6 without reading part 9.

**C12 is fitted**, and every measurement recorded below was taken on a fully
decoupled core rail. Do not run this board without C12 for more than a bench
moment: VCAP_1 and VCAP_2 are two pins on the same internal 1.2 V core domain,
and C13 alone leaves the rail at roughly half its specified decoupling with
none local to pin 31.

## START HERE in a new session

Read this status block, then post mortem part 9, then `firmware/betaflight_target/BUILD.md`
section 7. Parts 3 to 6 are kept for the record but carry retractions. Nothing
important lives only in a chat log.

Outstanding work, in order:

- **Set `blackbox_sample_rate = 1/4` and `save`** before logging anything. The
  fitted chip is 2 MB, not the 16 MB the BOM called for. At the default 1/1 and
  a 3.2 kHz PID loop it fills in about 22 seconds. See BUILD.md section 7.
- **Motor mapping and direction are still unverified.** All four motors spin.
  Which output drives which arm, and which way, has not been checked. See the
  2026-09-12 update. First flight flipped and this is the leading suspect.
- **Failsafe: not started.**
- **Remove the debug probe when done.** `USE_FLASH_DEBUG_PROBE` in `config.h`
  and the diff in `firmware/betaflight_target/flash_probe_debug.patch`. It costs about
  12 bytes of flash and adds `flash_info debug` and `flash_info probe`. Keep it
  until logging is proven end to end, then drop the define. The GD25Q16E table
  entry is a separate change and must stay.

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
| **U3 GD25Q16E flash on SPI2** | **PASS after firmware fix** | Resolved 2026-09-14 evening. The chip always answered `C8 40 15`; JEDEC ID `0xC84015` was simply absent from Betaflight 4.5.5's `m25p16FlashConfig[]` table. One table entry fixed it. See post mortem part 9 |
| Current sense (PA2) | **PASS** | Reads correctly with motors under load |
| Receiver (i-BUS, USART1) | **PASS** | Bound to TX, live channel data in the Betaflight Receiver tab |
| Motors (M1–M4 DShot) | **PASS — all four** | All four outputs drive their motors; every motor spins (2026-09-12). **Mapping and direction still unverified** — see 2026-09-12 update |
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
- **U3 blackbox flash — verdict RETRACTED 2026-09-13. See the post mortem.**
  What was recorded on this date: re-verified every pin on the "Ruled out"
  list by hand, VCC, /HOLD, /WP, /CS, all four SPI2 continuity points, pin-1
  orientation, no MISO short, and concluded the die itself was bad.
  That conclusion does not hold. Three of those measurements pass identically
  on a healthy board and on a board with the fault, and two pin-adjacency
  candidates were never tested. Do not order or reflow a replacement U3 on the
  strength of this bullet.
- Failsafe: still untested — needs an RX signal-loss test.

---

## Update — 2026-09-12 — airframe built, all four motors spin, FIRST FLIGHT FLIPS

Airframe printed and assembled. Aircraft is complete and powers up on the pack.

- **Motors — PASS, all four.** Every output now drives its motor; all four spin
  on the bench with the ESC connected. This closes the "other three not yet
  exercised" item from 2026-09-03. Upgrades the summary row to PASS.
- **NEW BLOCKER — the aircraft flips as soon as it develops lift.** Consistent
  on every attempt, immediately, as the skids unload. Nothing else has been
  changed. This is a **control-polarity fault, not a frame fault** — no printed
  part and no mount change can fix it, and swapping airframes will not diagnose
  it.

### The three unverified items that all produce exactly this symptom

Every one of these is still open in this repo, and any one of them alone flips
the aircraft on liftoff. **They must be checked independently, in this order,
props off.** Do not fly again until all three pass.

1. **`GYRO_1_ALIGN` has never been re-derived.** `config.h` still carries
   `CW0_DEG`, and the comment there flags it as a placeholder that is
   *"very likely wrong"* — it was guessed for the ICM-42605 before the part
   changed to the BMI270, which has **different die axes AND a different
   footprint rotation (`-BR`)**. The CLI backup of 2026-08-18 contains no
   `gyro_1_sensor_align` override, so the aircraft is flying on the placeholder.
   A 180° error inverts roll and pitch while leaving yaw correct — an instant
   flip. A 90° error swaps the roll and pitch axes — also an instant flip.
2. **Motor mapping has never been verified.** `config.h` says outright that
   *"physical S1..S4 order is sorted in Configurator after the spin test"* —
   that sort was never done, because until today only one motor had ever been
   run. **"All four spin" proves the four outputs work; it says nothing about
   which physical motor each one drives.** Betaflight QUAD X is
   **M1 = rear-right, M2 = front-right, M3 = rear-left, M4 = front-left**;
   `../frame/designs/flight_E/output/Flight_E_overview.png` labels the physical positions
   the same way.
   - **Specific trap on this board:** `README.md` and
     `drone_fc_project_context_v3.md` §"ESC connector orientation" both record
     that **CN1 is numbered in reverse to the manufacturer's harness diagram and
     is only correct when the connector mates reversed.** Power clearly mates
     correctly (nothing released smoke, the buck runs off the pack), but the
     same reversal applies to the four signal lines. If the signal order ended
     up mirrored, **every motor is driven by its diagonal opposite** — which
     inverts roll and pitch, leaves yaw correct, and flips on liftoff. It
     presents identically to a 180° gyro error, which is exactly why both have
     to be tested separately rather than reasoned about.
   - The second half of the mapping is the builder's own wiring: which motor's
     three phase wires went to which ESC output pads. That is not recorded
     anywhere in this repo.
3. **Motor direction has never been verified.** Nothing in the CLI backup sets
   direction or `yaw_motors_reversed`. Diagonal pairs (M1+M4, M2+M3) must spin
   together and opposite to the other pair. Read the required arrows off the
   Configurator Motors-tab diagram for the installed firmware — do not work
   from memory, and do not assume props-in vs props-out.
   Then confirm each prop is fitted to match its motor's actual direction.

### Bench sequence to run before the next flight — props OFF, every time

- [ ] **Gyro axes.** Setup tab, watch the 3D model. Pitch the nose down → model
      pitches nose down. Roll right → model rolls right. Yaw right → model yaws
      right. **Wrong axis responds** = 90° error. **Right axis, wrong sign** =
      180° error. Iterate at runtime with `set gyro_1_sensor_align = CW90_DEG`
      (etc.) + `save` — no rebuild needed — then bake the winner into
      `config.h` so a full-chip-erase reflash cannot lose it.
- [ ] **Motor mapping.** Motors tab, spin **one motor at a time** at minimum
      throttle and write down which physical motor turns for BF 1, 2, 3, 4.
      Compare against M1=RR, M2=FR, M3=RL, M4=FL. Fix with the Motors-tab
      reorder tool or `resource MOTOR` — and if the fix turns out to be the
      full mirror, make it permanent in `config.h` rather than leaving it in
      CLI-only state.
- [ ] **Motor direction.** Same test, note each motor's rotation against the
      Configurator diagram. Correct in BLHeli_S / the Motors tab, not by
      swapping phase wires, so the mapping you just verified stays valid.
- [ ] **Closed-loop response test — the single highest-value check.** Props off,
      arm in Acro, **hold the aircraft by the frame, never by an arm**, and tilt
      it. Pitch the nose down: the **front** motors must spool up to push it
      back. Roll right: the **right** motors must spool up. If the wrong motors
      answer, you have found the fault before it costs you a set of props.
      **This single test catches all three faults above at once**, and it is the
      only one that proves gyro, mixer and wiring agree as a loop.
- [ ] **Failsafe** — still untested. RX signal-loss test. Do this before any
      further flight attempt, not after.

### Do not do these while debugging

- Do not tie the aircraft down and run it with props on. The restraint changes
  the dynamics, the aircraft fights the tether, and the result tells you nothing
  about free-flight stability. `../frame/designs/flight_E`'s own validation marks the
  restraint adapter as **unqualified**, and `../frame/designs/debug_D/README.md` states
  plainly that a new mount *"does not diagnose or fix the reported flip."*
- Do not touch PID gains or filter cutoffs. Nothing in this fault is a tuning
  problem, and changing gains will only mask which of the three faults it was.
- Do not print another frame revision hoping it helps. It will not.

### Also still open

- ~~**U3 blackbox flash does not respond**~~ **Resolved 2026-09-14 evening**, see
  post mortem part 9. Logging is available. Set `blackbox_sample_rate = 1/4`
  first, the chip is 2 MB. Log the next flight and diagnose the flip from data
  rather than from the bench tests alone. See
  `frame/printed_5inch_revC/VIBRATION_NOTES.md` §"Logging prerequisites".
- Once it flies: **hard-mount the FC on rigid standoffs while diagnosing.** The
  soft-mount isolator selection is unvalidated — `VIBRATION_NOTES.md` records no
  measured stiffness, damping or supported mass — and a taut harness bridging
  the moving and fixed sides bypasses the isolation anyway. Add isolation back
  as a deliberate, measured change *after* stable flight, one variable at a time.

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

## POST MORTEM — 2026-09-13 and 2026-09-14

Three separate threads got tangled over two days. Keep them apart when reading.

### 1. The board was never dead. What actually happened to it

On 2026-09-13, while probing U4's CS and MOSI pins with a handheld multimeter on
a **powered** board, it stopped enumerating: power LED on, D6 off, no COM port,
no DFU, and the F405 package hot to the touch.

Measurements taken, board unpowered, meter leads nulled:

| Point | Reading | Read at the time as |
|---|---|---|
| `+3V3` to GND | high | rail not shorted |
| VCAP_1 to `+3V3` | high | no VDD to VCAP bridge |
| VCAP_1 to GND, C12 fitted | about 3 Ohm | short on the VCAP_1 net |
| VCAP_1 to GND, C12 removed | about 3 Ohm | **short inside the die. WRONG** |

That last row was the mis-measurement, and it cost a day. **The probe was almost
certainly on the ground-side pad of the C12 footprint.** One pad of that
footprint goes to pin 31 and the other goes to ground, and probing the ground pad
against ground reads zero by definition. The numbers support it: 2.4 Ohm raw
against 1.6 Ohm of lead resistance is not a short, it is two probes on the same
net.

The board was declared dead on that reading. Nobody plugged it back in to check,
because the verdict had already been reached. On 2026-09-14 it was connected with
a scope attached and **it simply worked**, with C12 still off the board.

**What really happened: C12 failed short.** MLCCs flex-crack under board bending
or localised pressure, and silver migrates along the crack until a conductive
path forms. Voltage rating is irrelevant to this mechanism, which is why a 25 V
0805 died on a 1.2 V rail. Larger case sizes are more prone to it than smaller
ones. The sequence was: C12 shorts VCAP_1 to ground, the internal core regulator
drives into the short and dissipates everything it can source inside the die
(the heat), the core never reaches a valid 1.2 V (no boot, no LED, no USB).
Removing C12 fixed it at that instant.

**Still to do:** refit a 2.2 uF on C12, then confirm VCAP_1 at 1.2 V and total
draw at 20 to 50 mA. If the short returns with a new cap fitted, the cap was not
the cause and a whisker on that net becomes the suspect again.

### 2. Measurement discipline, the actual lesson of these two days

Three separate wrong turns, all measurement technique:

- **Probing a live 0.5 mm pitch package with a handheld meter.** This is what
  cracked C12 in the first place. Probe vias, jumper pads, passive terminals and
  connector pins instead, or tack a thin wire to the pin and probe the wire. A
  pin with no accessible test point is a reason to add one to the next revision,
  not a reason to freehand it.
- **Probing the wrong pad of a two-pad footprint.** Before trusting a resistance
  to ground, confirm which pad is the signal side. Ground to ground always reads
  zero and always looks like a catastrophe.
- **Taking a resistance reading on a powered board.** On 2026-09-14 SPI2 SCK
  measured about 22 Ohm to ground and looked like a short. It was PINIO driving
  the pin low, and 22 Ohm is roughly an STM32F4 GPIO's output-low on-resistance.
  Unpowered, the same pin read clean.

### 3. The flash fault. What is now positively cleared

The 2026-09-03 "dead die" verdict is **retracted**. It rested on three
measurements that pass identically on a healthy board and on a faulty one:

- `/CS = 3.3 V idle, R17 pull-up good`. With R17 fitted, CS reads 3.3 V whether
  the MCU drives it, never drives it, or it is shorted to a supply.
- Continuity from each U3 pin to its MCU pin. Proves the net is connected, not
  that it is connected to nothing else.
- `MISO to GND = 1.4 MOhm`, measured unpowered, so blind to any fault where MISO
  is clamped by an actively driven neighbour.

Work done 2026-09-14, all of it positive evidence rather than absence of a fault:

| Test | Result | What it clears |
|---|---|---|
| `dump all` | No duplicate claims on B12, B13, B15, C02. `flash_spi_bus = 2` | No resource stealing a pin |
| Adjacent-pin resistance checks | PB12 to +3V3 about 10 kOhm (R17). U3 pin 2 to PC3 open. J4 SCK to U3 pin 6 continuous | No VDD-to-CS bridge, no PC2-to-PC3 bridge |
| PINIO on B12, both polarities | 3.3 V and 0 V, clean toggle | PB12 pad, trace, joint and U3 pin 1 all good, MCU pin healthy |
| 10 kOhm from U3 pin 2 to U3 pin 8, then `flash_info` | MISO net measured 3.3 V, `flash_info` still `0x00000000` | U3 is not holding MISO low. The chip is not stuck-low or shorted |
| PC2 reassigned to `ADC_BATT`, then `status` | `Voltage: 3646 * 0.01V`, back-calculating to 3.3 V at the pin | **PC2 is connected and functional as an input.** Joint at LQFP pin 10 good, trace good, input buffer good |
| `status` `Devices detected: SPI:1` | Circular | Counts devices bound to buses via `spiBusDeviceRegister()`, so it only restates that the flash did not register |

**The one hard conclusion, which depends on no device cooperating:**

> The MISO net was measured at 3.3 V. PC2 was proven to read that 3.3 V through
> the ADC. `flash_info` still returned `0x00000000`. A transacting SPI2 sampling
> PC2 would have to return `0xFFFFFF`.
>
> **SPI2 does not sample PC2 during the flash read.**

**RETRACTED. The error is in the premise.** `flash_info`'s
`JEDEC ID=0x00000000` is not a measurement of the wire. `cliFlashInfo()` prints
`flashGetGeometry()`, which returns the static `noFlashGeometry` (all zeros)
whenever `flashDevice.vTable` is NULL, that is, whenever nothing was detected.
The raw bytes were never displayed anywhere. SPI2 was sampling PC2 correctly the
whole time and reading `C8 40 15`. Confirmed on the wire 2026-09-14 evening.
See part 9.

Three ways that can happen, all inside the firmware: no transfer occurs at all,
a transfer occurs but the receive path is not connected to the pin, or the driver
bails before transacting and `flash_info` prints a zeroed struct.

Those were the right three to consider, and the debug probe proposed in part 6
was built to separate them. The answer turned out to be a fourth:
**the transfer succeeded and the driver discarded the result.**

### 4. Firmware verified against the Betaflight 4.5.5 source

All checked directly, all correct. Do not re-derive these:

- **PC2 is a legal SPI2 MISO.** `bus_spi_pinconfig.c` lists SPI2 as SCK on PB10
  and PB13, MISO on PB14 and PC2, MOSI on PB15 and PC3. The long-standing open
  question in this file is **closed**. The `config.h` claim was right.
- **The `USE_FLASH_W25Q128FV` gate is real.** `common_post.h` contains exactly
  the block quoted in `config.h`, so `USE_FLASH_M25P16` gets defined.
- **`USE_SPI`, `USE_SPI_DEVICE_1/2/3`, `SPI_FULL_RECONFIGURABILITY` and
  `USE_SPI_DMA_ENABLE_EARLY` all come from `STM32F405/target.h`.** Their absence
  from `config.h` is correct, not an omission.
- **`USE_FLASHFS` is auto-defined from `USE_FLASH`** in `common_pre.h`, which
  matches `flash_info` being a valid command rather than `Unknown command`. That
  also proves the driver compiled and ran.
- **No DMA collision.** SPI2 wants DMA1 streams 3 and 4. The motors hold DMA1
  streams 7, 2, 6 and 1, and ADC1 sits on DMA2 stream 4.

### 5. Tests that did NOT discriminate, and why

Worth recording so they are not repeated:

- **`resource SPI_SDI 2 B14` swap.** Returned zeros, but PB14 is unconnected and
  a floating input with no pull can read low, so the result is consistent with
  both hypotheses.
- **Redirecting the flash driver onto SPI1 with the gyro's CS**
  (`GYRO_CS 1 NONE`, `FLASH_CS 1 C04`, `flash_spi_bus = 1`). Returned zeros. The
  redirect definitely applied, confirmed by the gyro disappearing from `status`.
  But with `GYRO_CS` freed the BMI270 is never initialised, and it powers up in
  I2C mode and only switches to SPI on a CSB rising edge from the driver, so it
  was almost certainly silent. PA6 has no pull-up, so silence reads as zeros.

> **RETRACTED 2026-09-14 evening.** That last sentence is wrong, and so is the
> rule drawn from it below.
>
> **Betaflight puts an internal pull-up on every SPI SDI pin.** From
> `src/main/drivers/bus_spi.h`:
>
> ```c
> #define SPI_IO_AF_SDI_CFG  IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_50MHz, GPIO_OType_PP, GPIO_PuPd_UP)
> ```
>
> `spiInitDevice()` applies that to MISO the moment the bus comes up, so an
> initialised Betaflight SPI bus with nothing answering reads **`0xFF`, not
> `0x00`**. Measured directly: the same SPI1 redirect, re-run 2026-09-14 evening
> with the debug probe fitted, reports `stage: 4 TRANSACTED` and
> `raw=ff ff ff ff`.
>
> So this test did discriminate after all. The bus transacted and the BMI270
> stayed silent, exactly as suspected. The `0x00000000` seen at the time was
> `noFlashGeometry`, not the wire. The evidence existed and `flash_info` hid it.

**The rule these imply:** a test whose negative result is "the line was not
driven" tells you nothing, because an undriven line reads low. **Every future
test must put a defined level on the line being read.** The 10 kOhm pull-up on
U3 pin 2 is the only test in this whole episode that produced a hard answer, and
that is exactly why.

> **AMENDED.** That rule holds for an uninitialised pin. On an initialised
> Betaflight SPI bus the SDI pull-up has already put a defined level on the line
> and it is high. The rule that actually generalises: **never reason about what a
> bus read unless you are looking at the raw bytes.** `flash_info` prints cached
> geometry and renders "not detected" as a zeroed struct, indistinguishable from
> a genuine zero read. That ambiguity, not the hardware, cost the two days.

### 6. Open hypothesis and next steps

> **CLOSED 2026-09-14 evening. The DMA hypothesis is falsified and the debug
> build was built and run. See part 9 for the result.**

**Untested hypothesis, and it rhymes with a bug already found on this board.**
The `ADC1_DMA_OPT` note in `config.h` documents `adc_stm32f4xx.c` silently giving
up when its DMA allocation loses a race: no error, no log line, every channel
reads a hard zero forever. `USE_SPI_DMA_ENABLE_EARLY` is defined for this MCU and
the equivalent path on SPI2 has not been traced. Grep
`src/main/drivers/bus_spi_stdperiph.c` for `USE_SPI_DMA_ENABLE_EARLY` and check
what happens on a bus with no DMA option assigned.

> **Result of that grep: the hypothesis is dead.** `USE_SPI_DMA_ENABLE_EARLY`
> does not appear in the SPI driver at all. It gates exactly one thing, the
> placement of `spiInitBusDMA()` in `src/main/fc/init.c`:
>
> ```
> init.c:801   flashInit(flashConfig());   <- the boot flash probe
> init.c:818   blackboxInit();
> init.c:988   spiInitBusDMA();            <- #if USE_SPI && USE_SPI_DMA_ENABLE_EARLY
> ```
>
> "Early" means early relative to `motorPostInit()`, not relative to flash. The
> boot probe runs about 190 lines before any SPI DMA stream is allocated, so it
> is polled by construction. A lost DMA race cannot explain it.
>
> Note also that the driver file has moved. It is
> `src/main/drivers/stm32/bus_spi_stdperiph.c` at the 4.5.5 tag, not
> `src/main/drivers/bus_spi_stdperiph.c`.

**Two ways to get the runtime visibility this now needs:**

1. **Scope on J4**, which breaks out SPI2 SCK and Flash /CS. Probes on both,
   ground on a J4 GND pin, trigger single shot on /CS falling edge, arm it, then
   type `exit` in the CLI to soft reset and catch the boot-time probe. Note that
   `flash_info` most likely prints geometry cached from boot rather than
   re-probing, so do not rely on typing it to generate bus activity. CS dropping
   with a 32-clock SCK burst means the bus runs. Nothing moving means SPI2 never
   transacts.
2. **A debug build.** Patch `flash_info` to re-probe on demand and print whether
   the bus instance was acquired, whether the CS IO was claimed, and the raw
   bytes off the wire. One build, one flash, and it separates all three cases in
   section 3.

> **Option 2 was taken and it answered the question in one boot.** The scope run
> was never needed. See part 9.

### 7. Two working notes that cost time

- **Betaflight 4.5 calls the SPI data resources `SPI_SDI` and `SPI_SDO`**, not
  `SPI_MISO` and `SPI_MOSI`. The old names return
  `###ERROR IN resource: INVALID RESOURCE NAME###`.
- **`wsl` opens `docker-desktop`, not Ubuntu**, because Docker Desktop installs
  itself as the default distro. That lands you in a BusyBox shell as root with an
  empty home and `-sh:` errors, which looks exactly like a missing source tree.
  Use `wsl -d Ubuntu`, or `wsl --set-default Ubuntu` to fix it permanently.

---

### 8. Changes for the next revision

- **Do NOT move SPI2 MISO from PC2 to PB14.** Settled 2026-09-14 evening. PC2 is
  a legal SPI2 MISO in Betaflight's pin table, it is correctly AF5-mapped at
  runtime (`moder=2 af=5` from the debug probe), and it clocked in a valid JEDEC
  ID. The fault was never PC2 and never the pin choice. Leave the routing alone.

  ```
  SDIO_D0   PC8       SDIO_CK   PC12
  SDIO_D1   PC9       SDIO_CMD  PD2
  SDIO_D2   PC10
  SDIO_D3   PC11
  ```

  Use a soldered push-push socket, **not a breakout on flying leads**. A card in
  a socket on a vibrating quad already has a known failure mode of intermittent
  contact and mid-flight write failures, and flying leads make it worse. Costs
  board area on the 80 × 80 mm board, a few grams, and the card must be FAT32.

  **Check the DMA before routing.** SDIO on the F4 uses DMA2, and so do SPI1 and
  ADC1 on this board. Work the stream and channel assignments out on paper, the
  way `ADC1_DMA_OPT 1` had to be. Do not assume it falls out clean.

  Note for reference: this could NOT have been retrofitted to the current board
  from the headers. J7 gives PC8, PC9, PC10, PC11, PA8, PB3, which is four of
  the six SDIO signals and missing clock and command. SPI3 is no better, J7 has
  SCK and MISO and no MOSI, and J4 carries only SPI2 SCK and Flash /CS. The one
  route that would have worked was depopulating U3 and wiring an SD breakout to
  the U3 footprint, where SPI2 SCK, MISO, MOSI, CS on PB12, 3.3 V and GND are
  all exposed at 1.27 mm pitch.

- **If keeping SPI flash instead, fit the part actually intended.** GD25Q128E
  (C2758105) or BY25Q128ES (C22471255), both in the same `m25p16` table, both
  the same SOIC-8 208 mil pinout. The fitted GD25Q16E is 2 MB, one eighth the
  intended log space.
- **Add test points** on FLASH /CS, SPI2 SCK, SPI2 MISO and SPI2 MOSI (or the
  SDIO lines, if the socket replaces the flash), so the bring-up above can be
  done without touching the package.
- **Break out the full bus on any spare header, not a partial one.** J4 exposing
  SPI2 SCK and /CS but not MISO or MOSI, and J7 exposing four of six SDIO pins,
  are both one signal short of being useful. A header that cannot carry a whole
  interface costs the same board area and buys nothing.
- **Consider assembly service for the fine-pitch parts.** Hand placing the
  LQFP-64 and the BMI270 is the step that constrains every repair option
  afterwards.

---

### 9. RESOLVED 2026-09-14 evening. The chip was never the problem

**The GD25Q16E's JEDEC ID is not in Betaflight 4.5.5's chip table.** That is the
whole fault. U3 answered correctly on every attempt going back to 2026-09-03.

#### How it was found

A debug build, option 2 from part 6. `flashSpiInit()` in
`src/main/drivers/flash.c` has three silent `return false` exits and no logging,
and `flash_info` prints geometry cached from boot, so a probe that bailed before
touching the bus looks identical to a probe that transacted and clocked in
zeros. The patch records which exit was taken, the pins `spiPinConfigure()`
resolved, the SPI peripheral and MISO GPIO register state, and the raw RDID
bytes, then prints them from `flash_info debug` (boot record) or
`flash_info probe` (re-run now).

First boot on the correct bus:

```
# flash_info debug
source: boot
stage: 4 TRANSACTED     a transfer was issued onto the bus
cs: pin=B12 freeOrPreinit=1 owner=PREINIT
bus: flash_spi_bus=2 spidev_idx=1
resolved pins: sck=B13 sdi=C02 sdo=B15 regbase=0x40003800
spi regs: cr1=0x0357 cr2=0x0000 sr=0x0002
  spe=1 mstr=1 br=2
sdi pin: moder=2 (2=alternate function) af=5 (5=SPI2 on F4)
rdid: issued=1 raw=c8 40 15 c8
```

`C8 40 15` is the GD25Q16E's JEDEC ID from the GigaDevice datasheet. Everything
about the bus is correct: SPI2 at `0x40003800`, enabled, master, PC2 in AF5, the
right three pins. The chip answered.

#### Why it was discarded

`m25p16FlashConfig[]` in `src/main/drivers/flash_m25p16.c` at the 4.5.5 tag has
twenty entries: Macronix, Micron, Winbond, PUYA, Zbit, Cypress, BergMicro. There
is **not one GigaDevice part in it.** `0xC84015` is absent. So
`m25p16_identify()` walked the table, matched nothing, returned false,
`flashSpiInit()` fell through to `spiPreinitByTag()` and returned false, and
`flash_info` printed `noFlashGeometry`, all zeros, forever.

**BUILD.md section 7 asserted the opposite** and quoted a table entry that does
not exist at this tag. That single wrong claim is what aimed two days of
diagnosis at the hardware. It has been corrected.

#### The fix

One line in `m25p16FlashConfig[]`, placed before the BergMicro entry:

```c
// GigaDevice GD25Q16E
// 32 sectors x 256 pages x 256 B = 2 MB. Same geometry as the Winbond
// W25Q16 entry above. 104 MHz fast read, 50 MHz for the standard 0x03 read.
{ 0xC84015, 104, 50, 32, 256 },
```

Geometry copied from the `0xEF4015` W25Q16 entry directly above, which is the
same 2 MB organisation, so it is a known-good shape rather than a guess.

#### Retractions this forces

1. **"SPI2 does not sample PC2"** (part 3) is wrong. It did, correctly, always.
2. **"an undriven line reads low"** (part 5) is wrong for an initialised
   Betaflight bus. `SPI_IO_AF_SDI_CFG` carries `GPIO_PuPd_UP`, so an unanswered
   bus reads `0xFF`. The SPI1 redirect test did discriminate.
3. **The DMA hypothesis** (part 6) is falsified. `spiInitBusDMA()` runs at
   `init.c:988`, long after `flashInit()` at `init.c:801`. The boot probe is
   polled.
4. **"the cause is firmware-side"** (status block, 2026-09-14 morning) was right,
   but for none of the reasons given.

#### Collateral found on the way

The SPI1 redirect from the part 5 experiment (`GYRO_CS 1 NONE`,
`FLASH_CS 1 C04`, `flash_spi_bus = 1`) had been **saved**, and survived the
reflash because the debug build was flashed without a full chip erase. The board
booted with no gyro: `Devices detected: SPI:0`, `NOGYRO`, `GYRO rate: 0`.
Restored with, in this order so the PC4 claim does not collide:

```
resource FLASH_CS 1 B12
resource GYRO_CS 1 C04
set flash_spi_bus = 2
save
```

Note that `Gyros detected: gyro 1 locked` in `status` is **not** evidence of a
live gyro. `gyrosDetected` is a persisted config field, so it reports a past
detection. `Devices detected: SPI:n` is the live number.

#### Why it probably failed on SPI2 before 2026-09-14

Not established, and not worth chasing now. The leading candidate is that the
PINIO test on PB12 left `resource PINIO 1 B12` saved, which makes
`IOIsFreeOrPreinit(PB12)` false and bails `flashSpiInit()` at the CS check before
the bus is touched. The current config has no PINIO resource line. If flash
detection ever disappears again, run `flash_info debug` first: `stage` names the
exit directly.

#### The debug patch

`firmware/betaflight_target/flash_probe_debug.patch`, against the 4.5.5 tag. Adds
`spiGetResolvedPins()` to `bus_spi.c`, `flashProbeDebug_t` and `flashReprobe()`
to `flash.c`, the report to `cli.c`, and the GD25Q16E table entry. All the
instrumentation is behind `USE_FLASH_DEBUG_PROBE`, defined in `config.h`. Costs
about 12 bytes of flash. **Remove the define once logging is proven end to end.
Keep the table entry.**

---

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
> `firmware/betaflight_target/BUILD.md` §3.1.

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

Full procedure in `firmware/betaflight_target/BUILD.md`.

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
