# Independent Design Review — STM32F405 Flight Controller
**Date:** 2026-06-27   **Scope:** schematic only (PCB layout not reviewed)

**Method:** Connectivity was extracted geometrically from the `.kicad_sch` s-expressions — each symbol's pin coordinates transformed by its instance rotation/mirror, then wires/junctions/labels/power-symbols unioned into real nets (not inferred from layout appearance). Pin functions were re-derived from the STM32F405 datasheet (DS8626) and RM0090 for the **LQFP-64** package; every IC was checked against its own datasheet. Quoted nets are the actual extracted nets.

**Transform caveat (important for trust):** a geometric parser can place a 2-pin part (diode, polarized cap) in either of two mirror orientations when both pads land on wires, which swaps *pin identity* (K↔A) without changing *which nodes* connect. I therefore re-verified every polarity-sensitive part (D2/D3/D4) with a transform **calibrated against a known multi-pin part (U5 TPS5430)**. Net connectivity (what connects to what) is reliable throughout; the diode polarities below are the calibrated results.

## VERDICT: ⛔ NO-GO for fabrication
Three fabrication-blocking errors (VDDA shorted to GND, VCAP_1 missing its capacitor, buck feedback resistor value) plus an unrouted receiver link. None are layout-fixable — they require schematic changes. Fix the Critical + M1 items, close KiCad and re-run ERC (the on-disk files are mid-save/truncated), then re-review.

---

## CRITICAL — will not work / damages parts

### C1. VDDA (pin 13) is shorted to GND
**File:** `MCU-STM32F405.kicad_sch` · **Pin/net:** `U4` pin 13 `VDDA` → net **GND**
**Evidence:** VDDA's only wire is (147.32,52.07)→(147.32,48.26)→(157.48,48.26), where a **junction** ties it to both the C18/C19 bottom-plate rail and a `power:GND` symbol at (157.48,50.8). The ferrite output FB1.2 feeds only C18.1/C19.1 (the cap **top** plates) — a dead node that never reaches VDDA. Confirmed with the calibrated transform (U4 is rot 0/no-mirror; VDD/VSS/SPI pins all land correctly) and by the GND symbol identity.
**Datasheet rule:** DS8626 power-supply scheme — *"VDDA and VSSA must be connected to VDD and VSS respectively"*; *"a maximum difference of 300 mV between VDD and VDDA."* Here VDD = 3.3 V, VDDA = 0 V → 3.3 V delta (~11× the limit). On LQFP-64 there is **no separate VREF+ pin** (VREF+ is bonded to VDDA internally), so this also kills the ADC reference.
**Consequence:** ADC (VBAT/current sense) dead; analog domain unpowered; the 3.3 V→0 V delta can forward-bias internal protection → possible latch-up/damage. Board will not run correctly.
**Fix:** Connect VDDA (pin 13) to the **ferrite-output node** (FB1.2 = C18.1 = C19.1). End state: `+3V3 → FB1 → VDDA`, with C18 (10 nF) and C19 (1 µF) from **VDDA to GND**. As drawn, the VDDA pin is tapped onto the cap-bottom/GND rail instead of the cap-top/ferrite rail.

### C2. VCAP_1 (pin 31) is floating — missing 2.2 µF core-regulator capacitor
**File:** `MCU-STM32F405.kicad_sch` · **Pin/net:** `U4` pin 31 `VCAP_1` → stub to (99.06,133.35), no component. Only VCAP_2 (pin 47) has its cap (C13, 2.2 µF → GND). VCAP_1 is not tied to VCAP_2.
**Datasheet rule:** DS8626 §5.3.2 / power-supply scheme — *"Two external ceramic capacitors should be connected on VCAP_1 & VCAP_2 pin,"* **2.2 µF** low-ESR each. LQFP-64 has two independent VCAP pins; each needs its own cap.
**Consequence:** The internal 1.2 V core regulator on the VCAP_1 domain is undecoupled → unstable core rail; the MCU may not start or may run erratically.
**Fix:** Add a 2.2 µF X7R from **VCAP_1 to GND**, placed at the pin (mirror C13).

### C3. Buck feedback resistor R8 is annotated "3.24" (reads as 3.24 Ω; must be 3.24 kΩ)
**File:** `buck-converter.kicad_sch` · **Net:** VSENSE divider R7 (10k, top) / R8 (bottom) into `U5` pin 4
**Rule:** TPS5430 Vref = **1.221 V**; TI's typical 5 V design (datasheet figure) is **R1 = 10 kΩ, R2 = 3.24 kΩ** → Vout = 1.221·(1 + 10k/3.24k) = 5.0 V. The `Value` field is the bare string **`3.24`**, which KiCad/BOM read as 3.24 Ω.
**Consequence (if built as drawn):** R7/R8 = 10000/3.24 ≈ 3086 → the regulator drives Vout toward battery voltage trying to reach 1.221 V on VSENSE → the "5 V" rail rises to ~VBAT → destroys both LDOs, USB, the ESD device, and everything downstream of 5 V.
**Fix:** Set R8 = **3.24 kΩ** explicitly in the symbol Value and BOM. (Likely just a missing "k", but it must be corrected before BOM export.)

---

## MAJOR — functional risk

### M1. Receiver / UART header (J3) is not connected to the MCU
**File:** `IO.kicad_sch` · **Nets:** `UARTx_RX` = `{J3.3}` only; `UARTx_TX` = `{J3.4}` only — neither label appears on any MCU pin (verified against the full MCU label set).
**Consequence:** No path for the FS-iA6B iBUS (or any RX protocol) into the MCU → no radio-control input. iBUS is non-inverted, so it only needs a direct USART RX — but that RX is unrouted. (May reflect a work-in-progress sheet, but it is a hard blocker for a flyable board.)
**Fix:** Wire `UARTx_RX`/`UARTx_TX` to a free USART — e.g. PA10/PA9 (USART1), PB11/PB10 (USART3), or PC7/PC6 (USART6). All are currently unused and RX-capable.

### M2. Blackbox-flash SPI2_RX DMA collides with motor-4 (TIM4_CH2) on DMA1 Stream 3
**Files:** `MCU-STM32F405.kicad_sch`, `Blackbox-flash.kicad_sch`, `IO.kicad_sch`
**Evidence + RM0090 Table 42:** The four motors map to PB0=TIM3_CH3 (DMA1 **S7**), PB1=TIM3_CH4 (**S2**), PB6=TIM4_CH1 (**S0**), PB7=TIM4_CH2 (**S3**). The blackbox flash is on SPI2: **SPI2_RX = DMA1 Stream 3**, SPI2_TX = DMA1 Stream 4. So **motor-4 DSHOT DMA (TIM4_CH2 → Stream 3) and SPI2_RX (Stream 3) are the same stream** — they cannot both run.
**Consequence:** A DMA conflict the firmware target must resolve. In practice blackbox *writes* use SPI2_TX (Stream 4, no conflict) and log *download* uses RX with motors disarmed, so it's often survivable — but it constrains the target and can break DMA-based flash reads while armed.
**Fix (optional but clean):** Drive motor 4 from **PB5 (TIM3_CH2 = DMA1 Stream 5)** instead of PB7 — conflict-free against SPI2 (S3/S4), SPI1 (DMA2) and the other three motors (S0/S2/S7). PB5 is currently free. This requires re-routing the CN1 motor-4 net and its label from TIM4_CH2 to TIM3_CH2.

### M3. IMU VDD / VDDIO rails are assigned backwards (defeats the quiet LDO)
**File:** `IMU-ICM42605-p.kicad_sch` · **Pins/nets:** `U6` pin 8 `VDD` → **+3V3** (main, shared/noisy); pin 5 `VDDIO` → **+3V3_IMU** (quiet TLV73333 LDO). Confirmed with the calibrated transform; decoupling follows the rails (C10 2.2 µF + C11 0.1 µF on VDD/main per the ICM-42605 datasheet typical circuit; C9 10 nF on VDDIO/quiet).
**Why it's wrong:** VDD powers the MEMS + analog front-end (the actual sensing); VDDIO only sets digital-I/O levels. The dedicated TLV73333 exists to give the gyro a clean supply, but only VDDIO sits on it while VDD shares the main rail with the MCU and flash. The clean LDO is wasted on the I/O pin.
**Consequence:** Gyro analog supply sees main-rail noise → worse noise floor / more filtering needed.
**Fix:** Power **VDD (pin 8) + C10 + C11 from +3V3_IMU**. Put VDDIO on +3V3_IMU as well (simplest — the 300 mA LDO trivially carries the <10 mA gyro) or leave VDDIO on main 3V3.

### M4. No VBAT voltage-sense divider to any ADC pin
**Files:** `IO.kicad_sch` / `buck-converter.kicad_sch` · **Net:** `+BATT` members = `{C5.1, CN1.8, U5.7}` — battery voltage reaches only the buck input. Current sense exists (`Current_meter` → PC1 = ADC123_IN11 ✓); battery-voltage sense does not.
**Consequence:** Betaflight cannot measure pack voltage on the FC (no VBAT telemetry/alarms/sag compensation).
**Fix:** Add a divider from `+BATT` to a free ADC pin (PC0/ADC123_IN10, PC2/IN12, or PA4/ADC12_IN4), scaled for **16.8 V (4S)** — e.g. ~10k/1k (≈11:1) keeps the ADC node ≤1.5 V — plus a 100 nF filter and a series/clamp consideration. (Depends on C1 — the ADC reference is dead until VDDA is fixed.)

### M5. Battery-rail capacitor voltage ratings unspecified — must be ≥25 V for 4S
**File:** `buck-converter.kicad_sch` · **Parts:** C5 (10 µF on `+BATT`) and any other `+BATT`-side ceramic. `Value` fields carry no voltage; passives are not in `libs/lcsc.txt`.
**Rule:** 4S full charge = **16.8 V** (transients higher). A 16 V-rated cap is **under-rated** at 16.8 V — a classic 4S trap. The original 6S intent *may* mean parts are already ≥35 V (safe), but confirm.
**Fix:** Specify `+BATT` ceramics at **≥25 V** (≥35 V keeps 6S headroom). C8 (220 µF) on the 5 V node needs ≥10 V. The TPS5430 (5.5–36 V in) and D2 (SS34, 40 V) are fine for both 4S and 6S.

### M6. ESC connector CN1 pin-1 keying must be verified on the PCB (16.8 V-onto-GPIO risk)
**File:** `IO.kicad_sch` (schematic mapping is correct, see below). This is a **layout/footprint** check the schematic can't settle.
**Risk:** `+BATT` (16.8 V) is on CN1 pin 8. If the SM10B-SRSS-TB footprint/keying is mirrored vs the Flycolor harness, a 180° reversal lands harness VBAT (pin 8) onto CN1 pin 3 = **TIM3_CH3 = PB0**, a 3.3 V GPIO → instant MCU death.
**Fix:** On the PCB, confirm CN1 pad-1 (silk + connector key) maps to harness pin 1 (TX) so VBAT lands on pad 8. Verify against the harness before first powered connection.

---

## MINOR — optimization / housekeeping
- **VDD decoupling count:** only 3×100 nF (C25–C27) + 1×4.7 µF (C28) for four VDD pins (19/32/48/64). DS8626 wants one 100 nF per VDD pin + the bulk; add a 4th 100 nF and ideally a 100 nF on VBAT (pin 1). *File: `MCU-STM32F405.kicad_sch`.*
- **ESC telemetry unused:** `CN1.1` (ESC TX) is unconnected — no ESC telemetry. Acceptable (bidirectional DSHOT still gives RPM); wire to a spare UART RX if wanted.
- **ADC vs gyro DMA:** ADC1 (DMA2 Str0/4) and SPI1-RX gyro (DMA2 Str0/2) both live on DMA2 — resolvable in firmware (pick non-overlapping streams), no board change needed. Worth noting for the custom target.
- **USB VBUS sense:** PA9/OTG_FS_VBUS unconnected — fine for a bus/self-powered F405 in Betaflight.
- **Flash exposed pad:** `U7` pin 9 (EP) → GND, but W25Q128JVSIQ (C97521) is SOIC-8 with no thermal pad — harmless; confirm the footprint so ERC doesn't flag a phantom pad.
- **TPS5430 ENA floating:** `U5` pin 5 (ENA) floats = enabled per datasheet (*"Float the pin to enable"*) — works; an optional VIN-referenced UVLO divider gives defined turn-on/brown-out.

---

## Verified CORRECT (checked against datasheets / calibrated geometry — no action)
- **VDDA filter parts present** (ferrite FB1 600 Ω + 10 nF + 1 µF) — only the VDDA pin connection is wrong (C1).
- **HSE crystal:** Y1 ↔ PH0(OSC_IN, pin5)/PH1(OSC_OUT, pin6) with C21/C22 (22 pF) to GND — correctly wired.
- **Reset/boot:** NRST = 100 nF (C20) + button to GND ✓; BOOT0 = 10k pulldown (R4) + button to +3V3 for DFU ✓; BOOT1/PB2 = 10k pulldown (R9) ✓.
- **Buck topology:** catch diode **D2 correctly oriented** (cathode→PH switch node, anode→GND — calibrated-transform verified); bootstrap C7 (0.01 µF) BOOT→PH per datasheet; L2 15 µH; 10 µF in / 220 µF out matches TI's 5 V reference design.
- **5 V diode-OR correctly oriented:** D3 (anode→+5V_Buck, cathode→+5V) and D4 (anode→+5V_USB, cathode→+5V) — Schottky OR into +5V; LDO inputs ≈4.6–4.9 V, within AP2112K / TLV73333 limits.
- **USB 2.0 FS:** D− = PA11 (pin 44), D+ = PA12 (pin 45) — no swap, no stray series R (F405 has internal series R). A6+B6 (D+) and A7+B7 (D−) tied for both C orientations. USBLC6-2SC6 in series, connector-side ESD; VBUS on pin 5; CC1/CC2 each 5.1 kΩ to GND (correct UFP pulldowns).
- **SPI bus separation:** gyro on **SPI1** (PA5/PA6/PA7 = AF5, soft CS = PC4), flash on **SPI2** (PB13/PB14/PB15 = AF5, soft CS = PB12) — separate peripherals, good practice. AFs confirmed in DS8626 Table 9.
- **Flash:** /CS (R10), /WP (R11), /HOLD (R12) all 10k pull-ups to +3V3; decoupled (C23 100 nF); EP→GND.
- **IMU strap/reserved pins (ICM-42605 datasheet Table):** pin 7 RESV→GND (required ✓), pins 2/3/10/11 RESV→GND (allowed ✓), pin 9 INT2/FSYNC→GND (correct for unused FSYNC ✓), INT1→PC3/EXTI3 ✓. Custom symbol pin-numbering matches the datasheet.
- **ESC connector CN1 mapping (vs Flycolor harness TX, CRT, S1–S4, NC, VBAT, GND, GND):** pin-for-pin correct — CRT→Current_meter→PC1; S1→TIM3_CH3/PB0; S2→TIM3_CH4/PB1; S3→TIM4_CH1/PB6; S4→TIM4_CH2/PB7; VBAT→+BATT; GND×2. **All four motors are connected.** (See M2 for the DMA-stream caveat on motor 4, and M6 for the keying check.)

---

## Ordered must-fix list (before re-spin / fab)
1. **C1 — VDDA short:** reconnect VDDA (pin 13) to the FB1 output node; C18/C19 from VDDA→GND. *(damages part / no ADC)*
2. **C2 — VCAP_1:** add 2.2 µF VCAP_1→GND. *(may not boot)*
3. **C3 — R8 value:** set to **3.24 kΩ** (not 3.24 Ω). *(5 V overvoltage)*
4. **M1 — RX link:** wire J3 `UARTx_RX/TX` to a free USART. *(no control input)*
5. **M3 — IMU rails:** move VDD (pin 8) + C10/C11 to **+3V3_IMU**. *(gyro noise)*
6. **M4 — VBAT sense:** add a 4S-scaled divider to a free ADC pin. *(no battery telemetry)*
7. **M5 — cap ratings:** specify +BATT-side caps ≥25 V. *(4S margin)*
8. **M2 — motor-4 DMA:** prefer PB5/TIM3_CH2 for motor 4 to clear the SPI2_RX/Stream-3 clash. *(firmware DMA conflict)*
9. **M6 — CN1 keying:** verify pad-1 orientation on the PCB. *(16.8 V onto PB0 if mirrored)*
10. Close KiCad (on-disk `MCU-` and `LDOs` sheets are mid-save/truncated), confirm a clean save, **run ERC** — it will independently flag C1, C2 and M1 as power-input / unconnected-pin errors — then re-review.
