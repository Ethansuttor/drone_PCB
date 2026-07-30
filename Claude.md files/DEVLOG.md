# Development Log — Custom STM32F405 Flight Controller

A running engineering log for the drone flight-controller PCB project. Entries are
dated and describe what was done, why, and what it unblocked. Newest entries are
added at the **bottom**.

> Log started 2026-07-11. Entries dated before that were reconstructed from git
> history, the dated design-review documents, and the versioned project-context
> notes, so early dates reflect when the work landed in the repo/docs rather than
> a live daily log.

**Project:** ~60×60 mm, 4-layer, STM32F405RGT6 flight controller for a 4S freestyle
drone. Runs Betaflight (custom target). Single-sided SMD, self-reflow. Mates to an
existing Flycolor Raptor BLS-04 4-in-1 ESC (no BEC → onboard buck required).

---

## 2026-05-20 — Project kickoff / architecture v1
- Defined the goal: design a flight controller from scratch as a bench/résumé
  project around the STM32F405 with native Betaflight support.
- Initial power architecture assumed an external Matek MBEC6S for 5 V; IMU
  candidate and rail strategy scoped. Target battery: 6S.
- Committed initial project skeleton and README.

## 2026-06-09 — First design audit (v3 context)
- Ran a full design audit (`FC_audit_2026-06-09.md`) and consolidated all decisions
  into `drone_fc_project_context_v3.md` (part numbers, pin map, power tree).

## 2026-06-16 — ESC harness pinout verified
- Obtained the manufacturer pinout diagram for the BLS-04 10-pin SH1.0 harness and
  cross-checked it against the v1 candidate. Locked the signal order
  (TX, current, S1–S4, NC, VBAT, GND, GND). Flagged a mandatory pre-power
  continuity check to rule out a mirrored connector.

## 2026-06-23 — Major architecture decisions locked
- Board grows to ~60×60 mm (oversized bench board, retains 30.5 mm M3 mount).
- Assembly changed to self-reflow: bare 4-layer PCB + frameless stencil, all SMD
  single-sided (top), leaded paste + hotplate, hot-air for the LGA IMU.
- **Power architecture change:** dropped the external MBEC6S; the FC now carries its
  own onboard buck (TPS5430DDAR, VBAT→5V/3A) because the BLS-04 ESC has no BEC.
- IMU resolved: stay with ICM-42605 (Betaflight-preferred, available, shares the
  426xx LGA-14 footprint with the ICM-42688-P alternate). BMI270 evaluated and
  rejected for new designs.

## 2026-06-26 — Schematic capture in progress
- Buck (TPS5430) fully wired; both LDOs (AP2112K main + TLV733P quiet IMU rail)
  wired; USB-C wired; IMU power wired.
- Decided the IMU rail split: VDD on the quiet +3V3_IMU, VDDIO on the main +3V3 so
  digital I/O switching noise stays off the gyro supply.
- Receiver locked: FlySky FS-iA6B over i-BUS straight into UART1 RX (non-inverted;
  S.BUS rejected — no internal F405 inverter).

## 2026-06-27 — Independent design review
- Second-pass review (`FC_independent_review_2026-06-27.md`) to pressure-test the
  power tree and pin assignments.

## 2026-06-28 — Battery → 4S, motor remap
- Switched target battery from 6S to 4S (14.8 V nom / 16.8 V full). Buck output
  inductor revised to 15 µH per the TI datasheet for this Vin/Vout; input-cap
  voltage rating requirement relaxed.
- Motor remap: moved M4 PB7→PB5 (TIM3_CH2) to clear a DMA1 stream clash with
  SPI2_RX (blackbox). Final: M1=PB0, M2=PB1, M3=PB6, M4=PB5. Verified against
  RM0090 DMA tables.
- Schematic review captured in `FC_schematic_review_2026-06-28.md`.

## 2026-07-10 — Continued schematic work
- Iterated on the MCU support circuitry and I/O sheet (multiple project backups
  saved during editing).

## 2026-07-11 — Schematic completed and verified; entering layout
- Completed the remaining analog/protection and debug circuitry:
  - **Current-sense clamp** on the ESC current line: 1 kΩ series + 3.3 V zener to
    GND, with the ADC (PC1) tapped between the resistor and the clamp.
  - **VBAT divider**: 100 kΩ / 10 kΩ + 100 nF from raw +BATT to PC2 (0.091 ratio;
    16.8 V → 1.53 V), enabling battery-voltage telemetry.
  - **SWD debug header** (J1): 3V3 / SWDIO (PA13) / SWDCLK (PA14) / GND / NRST, as
    insurance for flashing/recovery if USB DFU fails.
  - **Power-good LED** on +3V3 with a series resistor; removed an earlier LED from
    the quiet +3V3_IMU rail to avoid loading the gyro supply.
  - Completed the buck: added the 220 µF polymer output cap, 10 nF BOOT cap, and
    the 10k/3.24k feedback divider tied to the output.
- Ran a full pin-by-pin netlist verification directly against the KiCad files:
  MCU core, IMU (all 14 pins), flash, USB + ESD, dual LDOs, ORing diodes, boot/
  reset, current sense, VBAT divider, and SWD all verified correct.
- **Open before fab:** confirm ESC connector orientation with a continuity-beep
  test (the schematic numbers CN1 reverse to the manufacturer diagram — correct
  only if the connector mates reversed); run ERC to zero; keep the project off the
  synced drive to avoid save-corruption.
- Repo hygiene: committed the schematic edits to git (branch `main`).
- **Next:** PCB layout — 4-layer, solid L2 ground under the IMU, tight buck
  switching loop kept away from the IMU and analog ADC traces, matched USB pair.

## 2026-07-23 — Custom Betaflight target created + pinout verified against fab board
- Wrote the custom Betaflight target `betaflight_target/ETHANF405/config.h` plus
  a build/flash/bring-up guide (`betaflight_target/BUILD.md`).
- **Reconciled the firmware pin map against the as-ordered board** (CubeMX report
  + KiCad schematic), which diverged from the v3 context doc's pin table. The
  board is fabricated, so the schematic is now the source of truth. Captured it in
  the new canonical `VERIFIED_PINOUT.md` and referenced it from `CLAUDE.md`.
- Key corrections vs. the old docs: motors are PB0/PB1/**PA3/PB10**
  (TIM3_CH3/CH4, TIM2_CH4/CH3), flash SPI2 **MISO on PC2** (not PB14), ADC current
  on **PA2** (VBAT on PA1 unconfirmed — possibly no-connect → maybe no pack-voltage
  telemetry), status LED on **PC13** via 330Ω (dim, ~3mA backup-domain limit; PB8
  unconnected). PA8/PB3 are spare pads.
- **Open before flashing:** confirm PA1 VBAT divider exists; verify motor DShot
  DMA (`dma show all`) since PA3+PB10 share TIM2 and PB0+PB1 share TIM3.
- Unblocks: building the `.hex` (`make configs && make ETHANF405`) ahead of the
  board arriving.

## 2026-07-30 — IMU changed: ICM-42605 → Bosch BMI270 (sourcing-forced); docs + firmware reconciled

- **Why:** the entire TDK InvenSense 426xx family (ICM-42605, ICM-42688-P,
  IIM-42652) became unobtainable in single quantities — reel-only MOQ 1000+ at
  LCSC, OOS/backordered at DigiKey and Mouser. This **reverses the 2026-06-23
  decision** to stay with the 42605 and the rejection of the BMI270 recorded in
  that entry.
- **New part:** Bosch **BMI270**, U7, LCSC **C2836813**, ~$0.82–1.79, buy-1 in
  stock. Betaflight gyro define `BMI270`.
- **Not a drop-in.** Same 2.5×3.0mm LGA-14 outline, completely different pinout →
  new footprint (`drone_lib:LGA-14_L3.0-W2.5-P0.50-BR`) and every IMU net
  re-routed. Decoupling simplified to **100nF at VDD + 100nF at VDDIO** (the
  2.2µF/0.1µF/10nF set was a 426xx requirement). Unused-pin strapping inverted:
  ASDx(2)/ASCx(3) now go to **VDDIO and must not be grounded**, and
  INT2(9)/OCSB(10)/OSDO(11) are left **unconnected**, where the 42605's RESV pins
  went to GND.
- **Power architecture absorbed the swap with zero change** — VDD still on the
  quiet TLV733P `+3V3_IMU` rail, VDDIO still on the main AP2112K `+3V3`. This is
  the concrete payoff of the 2026-06-26 rail-split decision: a forced sourcing
  change cost a footprint and a re-route, not a power redesign.
- **Verified this session:** all 14 pads checked against BMI270 datasheet rev 1.3
  Table 22 (p.145) — wiring is correct. IMU nets confirmed routed in the PCB
  (SPI1_SCK 22 segments, IMU_nCS 18, EXTI3 15, MOSI 9, MISO 7). The production
  export in `Drone_PCB_FC/production/` already carries U7 = BMI270, so the fab
  package is the BMI270 board.
- **Firmware updated:** `config.h` swapped from `USE_GYRO/ACC_SPI_ICM42605` to
  `USE_GYRO/ACC_SPI_BMI270`. As shipped it would have failed WHO_AM_I and
  reported "no gyro detected." Behavioural consequences documented in the file:
  **PID loop ceiling drops 8 kHz → 3.2 kHz** (BF clocks the BMI270 gyro at a
  3.2 kHz ODR; chip max 6.4 kHz), **SPI clock ceiling 24 MHz → 10 MHz**, driver
  uploads an ~8 KB init blob at every boot, and the chip boots in I2C mode
  needing a CSB rising edge to enter SPI (R3's 10k pull-up + BF's CS toggle
  handle this).
- **`GYRO_1_ALIGN` flagged as unknown** — the committed `CW0_DEG` was guessed for
  the 42605's die orientation; the BMI270 has different die axes *and* a
  different footprint rotation. Must be re-derived on the bench.
- **Accepted tradeoff:** the BMI270 ships uncalibrated, which is exactly why
  Betaflight discourages it for new designs (few-percent attitude error).
  Irrelevant for Acro freestyle, visible in Angle/Horizon and level trim.
- **Docs reconciled:** `CLAUDE.md`, `VERIFIED_PINOUT.md` (new § IMU with the
  pad-by-pad table), `drone_fc_project_context_v3.md` (superseding header box +
  IMU/power/BOM sections, old text struck through rather than deleted),
  `schematic_capture_walkthrough.md` Block 6 (rewritten), `betaflight_target/BUILD.md`,
  and `datasheets/lcsc.txt` (marked stale, points at `libs/lcsc.txt`).
- **New bring-up item found while auditing: JP10.** The `+3V3_IMU` rail reaches
  the BMI270's VDD through solder jumper **JP10**, which is a
  `SolderJumper_2_Open` footprint — it ships open. Unbridged, the board
  enumerates over USB and reports no gyro. Added to the BUILD.md bring-up
  sequence ahead of the rail smoke test.
- **Found while auditing — BOM drift across three files.** `production/bom.csv`
  (2026-07-19, the export the boards were ordered from) lists U3 =
  **GD25Q16ETIGR C2904431, a 16 Mbit part**, plus TPS5430DDAR (C9864) and
  L2 = C354622 (22µH), where `Drone_PCB_FC_LCSC_BOM.csv` says BY25Q128ESSIG
  (128 Mbit), TPS5450 and 15µH C83374. Flash resolved below; **TPS5430-vs-5450
  and the 22µH-vs-15µH inductor are still open** — confirm which was actually
  ordered before populating the buck.
- **Also fixed:** the LCSC BOM had the IMU as U6; schematic, PCB and production
  files all say **U7**. Corrected in the BOM.
- **Next:** bridge JP10, confirm the buck parts, then bench bring-up per
  BUILD.md §4.

## 2026-07-30 — Blackbox flash is 2 MB, not 16 MB (wrong part ordered) — keeping it

- **What happened:** U3 as ordered and fitted is a **GigaDevice GD25Q16E
  (C2904431) — 16 Mbit = 2 MB**. The BOM specified a 128 Mbit part
  (BY25Q128ESSIG, C22471255). Wrong part made it onto the order; caught while
  reconciling the three BOM files during the BMI270 doc pass.
- **No firmware change required** — verified against Betaflight master, not
  assumed. `USE_FLASH_W25Q128FV` is not a chip selector; it's only a gate that
  pulls in the generic `m25p16` driver via `common_post.h`:
  `#if (USE_FLASH_W25M512 || USE_FLASH_W25Q128FV || USE_FLASH_PY25Q128HA) && !USE_FLASH_M25P16 -> define USE_FLASH_M25P16`.
  That driver detects chips by **JEDEC ID at runtime**, and ours is in its
  table: `{ 0xC84015, 104, 50, 32, 256 }` — GigaDevice GD25Q16E, 32 sectors ×
  256 pages × 256 B = **2,097,152 bytes**. Configurator will report 2 MB and
  that is correct behaviour, not a fault.
- **Decision: keep the 2 MB part.** The cost is log duration, not function.
  At ~30 KB/s per kHz of logging rate: 3.2 kHz → ~22 s (unusable), 1.6 kHz →
  ~44 s, **800 Hz → ~87 s**. Tuning runs are 30–60 s regardless, and 800 Hz
  still resolves everything below 400 Hz, which is where the motor/frame noise
  peaks that filter tuning depends on actually live. The real loss is the
  ability to log a whole pack.
- **Mandatory bring-up step added:** `set blackbox_sample_rate = 1/4`. Without
  it the log fills in ~22 seconds. Added to BUILD.md §4 as step 12.
- Documented the full rate table plus field-disable advice (no GPS/mag/baro/RSSI
  on this board) in `betaflight_target/BUILD.md`, and the reasoning inline in
  `config.h` next to the misleading-but-correct `USE_FLASH_W25Q128FV` define so
  nobody "fixes" it later.
- **Upgrade path if 90 s stops being enough:** GD25Q128E (C2758105) or
  BY25Q128ES (C22471255). Both are in the same `m25p16` JEDEC table, both
  SOIC-8 208mil with identical pinout — hot-air swap, no firmware change.

---

### Log format (for future entries)
```
## YYYY-MM-DD — <short milestone title>
- What I did and why.
- Any decision made + the reason / tradeoff.
- What it unblocked, or what's next.
```
