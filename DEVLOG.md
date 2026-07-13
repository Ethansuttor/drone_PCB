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

---

### Log format (for future entries)
```
## YYYY-MM-DD — <short milestone title>
- What I did and why.
- Any decision made + the reason / tradeoff.
- What it unblocked, or what's next.
```
