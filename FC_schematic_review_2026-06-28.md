# Schematic Error Review — Drone FC (2026-06-28)

**Scope:** schematic only, pre-layout. **Method:** geometric netlist extracted from the `.kicad_sch` s-expressions (pin coords transformed by each symbol's rotation/mirror, unioned with wires/junctions/global-labels/power-symbols). Calibrated transform: 190/227 pins land on a connection point; power/VCAP/USB/SPI all resolve correctly, so connectivity is reliable. Pin functions re-checked against DS8626 (STM32F405 LQFP-64), the ICM-42605 and TPS5430 datasheets.

> **Source-of-truth note:** the working files in `Drone_PCB_FC/` are **corrupt** (see #0). This review was run on the newest intact backup: `Drone_PCB_FC-backups/Drone_PCB_FC-2026-06-27_204112.zip` (2026-06-27 20:41 local) — which is *newer* and more complete than the corrupted working copy.

---

## 0. FILE CORRUPTION — fix before anything else
`MCU-STM32F405.kicad_sch` and `LDOs.kicad_sch` on disk are **truncated mid-write** (unbalanced parens — MCU ends mid-`C13` property). The other sheets are intact but **NUL-padded**. The stale `~*.lck` files (dated Jun 25) mean KiCad exited ungracefully and a save got mangled (looks like a sync/crash). The working `.kicad_sch` files also carry an *older* mtime (11:42) than the latest backups (through 20:41), so they're both corrupt **and** stale.

**Fix:** Don't fab or ERC from the working folder as-is. Open the project in KiCad — if it loads clean, immediately `File → Save` (overwrites the corruption). If it errors on load, restore from `Drone_PCB_FC-2026-06-27_204112.zip`. Nothing newer than 20:41 was lost (the corrupt files are older content).

---

## CRITICAL / safety

### C1 — ESC connector CN1 is wired in REVERSE pin order vs your verified harness table  ⚠️ 25 V-onto-GPIO risk
Extracted CN1 signal pins (1→10): `GND, GND, +BATT, NC, M4(TIM4_CH2), M3(TIM4_CH1), M2(TIM3_CH4), M1(TIM3_CH3), CURR, NC`. (Pins 11/12 = shield → GND, fine.)
Your `schematic_capture_walkthrough.md` Block 5 table says pin1=TX, pin2=CRT, pin3=S1 … **pin8=VBAT**, pin9/10=GND. The schematic is that table **flipped end-for-end** — VBAT sits on CN1 pin 3, and a motor GPIO (PB0) sits on CN1 pin 8.

This is only correct if your ESC cable is a **reversed/crossover** SH1.0 type (or the footprint's pin-1 pad is at the far end). With a **straight 1:1 cable** + same-handed footprint, harness VBAT (ESC pin 8, up to **25.2 V** on 6S) lands on **CN1 pin 8 = PB0 = TIM3_CH3**, a 3.3 V GPIO → **instant MCU death**. Also CRT and TX would short to GND.

**This cannot be settled from the schematic.** Resolve it two ways, both:
1. Make the schematic match your documented intent (TX on pin 1 … VBAT on pin 8), *or* explicitly confirm the footprint pin-1 + reversed cable and update the Block-5 table so it's not contradictory.
2. **Before first power-up** do the continuity beep test (open item #1): VBAT + both GNDs from the *mated cable* to the ESC XT60 pads.

### C2 — Current-sense line has NO protection (required) and is on the wrong ADC pin
`CURR` net = `{CN1.9, U4.9/PC1}` only. The ESC CRT output runs **straight into PC1 (ADC123_IN11)** — no 1 kΩ series resistor, no 3.3 V Zener clamp. Your context doc marks "1 kΩ series + 3.3 V Zener clamp (**required**)" on this line; the walkthrough put it on PC2. Neither part exists in the netlist (no 1 kΩ R, no Zener anywhere in the BOM).
**Fix:** insert 1 kΩ in series CN1.9→ADC, add a 3.3 V Zener (or 3.3 V TVS) from the ADC node to GND, + ~100 nF. Pick PC1 *or* PC2 and set it in the target; PC2 is currently free.

### C3 — No VBAT voltage-sense divider exists (no battery voltage telemetry)
`+BATT` = `{CN1.3, U5.7 (buck VIN), C5.1}` — battery reaches the buck only. There is **no 100k/10k divider to any ADC pin** (PC0 and PC2 are free, both unconnected). Betaflight will have current sense but **no pack-voltage reading** → no voltage alarms, no sag compensation.
**Fix:** add 100 kΩ/10 kΩ from `+BATT` to a free ADC pin (PC0 = ADC123_IN10, or PC2) + 100 nF to GND. 100k/10k → 2.29 V at 25.2 V, safe; set `vbat_scale ≈ 110`.

---

## MAJOR / functional

### M1 — IMU VDD / VDDIO rails are SWAPPED (defeats the dedicated quiet LDO)
Extracted: `U6.8 VDD → +3V3` (main/noisy), `U6.5 VDDIO → +3V3_IMU` (quiet TLV733P). Decoupling follows the wrong rails (C10 2.2 µF + C11 0.1 µF on VDD/main; C9 10 nF on VDDIO/quiet).
This is **backwards from your own locked decision** (context v3, 2026-06-26: "VDD (pin 8) stays on the quiet +3V3_IMU … VDDIO (pin 5) now sits on the main +3V3"). Per the ICM-42605 datasheet, **VDD** is the device/MEMS/analog supply (the noise-sensitive one); **VDDIO** is just digital-I/O level. As drawn, the gyro's analog supply rides the shared MCU/flash rail and the clean LDO is wasted on the I/O pin → worse gyro noise floor.
**Fix:** swap the rails — VDD (pin 8) + C10 + C11 → `+3V3_IMU`; VDDIO (pin 5) + C9 → `+3V3`. (Also move the CS pull-up R3 to match VDDIO's rail.)

### M2 — Buck inductor value mismatch: schematic 15 µH vs locked BOM part 22 µH
`L2 = 15 µH` (symbol `Device:L_Iron`). Your locked BOM inductor is **22 µH, CKCS6028-22µH, LCSC C354622**. 15 µH is electrically valid (TPS5430 datasheet design example uses 15 µH, KIND=0.2, L_min≈12.5 µH; for 5 V/3 A/6S L_min≈13 µH), but the **schematic value and the part you'll order disagree** — the generated BOM would call out 15 µH.
**Fix:** set `L2 = 22 µH` to match C354622 (lower ripple, gentler), or re-spec the BOM to a 15 µH ≥3 A shielded part. Either way reconcile. (`L_Iron` is just a cosmetic symbol — assign the real shielded-inductor footprint.)

### M3 — Motor 4 (PB7/TIM4_CH2) DMA collides with blackbox SPI2_RX
Motors map PB0=TIM3_CH3 (DMA1 S7), PB1=TIM3_CH4 (S2), PB6=TIM4_CH1 (S0), **PB7=TIM4_CH2 (DMA1 S3)**. Flash SPI2_RX is also **DMA1 Stream 3** (RM0090 Table 42). They can't both run a DMA at once. Usually survivable (blackbox *writes* use SPI2_TX/S4; log download happens disarmed), but it constrains the custom target and can break DMA flash reads while armed.
**Fix (clean, optional):** move motor 4 to **PB5 = TIM3_CH2 (DMA1 S5)** — conflict-free vs all other motors, SPI2 and SPI1. PB5 is currently free.

---

## COMPLETENESS — the schematic is not finished
These blocks from your walkthrough are simply not in the netlist yet. Don't start layout until they're in, or you'll re-spin:

- **No SWD header.** PA13/PA14 dangle on unnamed stub nets; no `Conn` for SWD exists. You'd be DFU-only with no debugger. Add the 5-pin header (3V3/SWDIO PA13/SWCLK PA14/NRST/GND). *(Block 3)*
- **No buzzer circuit.** PC5 is a bare stub — no 2N7002, no buzzer pads, no 1N4148. *(Block 9)*
- **No status LED.** PB8 is a bare stub — no LED, no 330 Ω. *(Block 9)*
- **No spare UART3/USART6/I2C1 pads** (PB10/PB11, PC6/PC7, PB6/PB7). Optional, but they were in the plan. *(Block 8)*

## HOUSEKEEPING / BOM hygiene

- **`C1` value = `C1uF`** (malformed) → set to `1uF`. *(LDOs)*
- **`Y1` value = `Crystal_GND24`** (symbol name, not a value) → set `8 MHz` and assign a real crystal + confirm CL vs the 22 pF load caps (open item #2). *(MCU)*
- **`D3`/`D4` value = `D_Schottky`** (generic) — these are the USB-OR diodes; assign a real part (SS34/B5819W). *(LDOs)*
- **Battery-rail cap ratings unspecified.** `C5` (10 µF on +BATT) and any +BATT ceramic must be **≥50 V for 6S** (25.2 V). Plan called for **2×10 µF**; only one (C5) is placed. Add a 100 nF HF cap at VIN. `C8` (220 µF out) needs ≥10 V; it's correctly a polymer (`C_Polarized`) — keep it non-ceramic for TPS5430 loop stability.
- **Many unused MCU pins** sit on unnamed nets — add no-connect (`Q`) flags or ERC will bury real errors in warnings.
- **5 V rail is actually ~4.6 V:** both the buck (+5V_Buck) and USB (+5V_USB) feed +5V through Schottkys (D3/D4), so the receiver/buzzer see ≈4.6 V. Fine (FS-iA6B wants 4.0–8.4 V), just be aware. FB correctly taps +5V_Buck (pre-diode), so the buck still regulates a true 5.0 V.

---

## Already CORRECT / fixed since the 2026-06-27 review (don't re-touch)
- **VDDA** now properly filtered: `+3V3 → FB1 (600 Ω) → VDDA`, C18 (10 nF) + C19 (1 µF) VDDA→GND. (The old VDDA-to-GND short is gone.)
- **Both VCAP caps present:** C12 (2.2 µF) on VCAP_1, C13 (2.2 µF) on VCAP_2, each to GND.
- **Buck feedback R8 = 3.24 kΩ** (was the dangerous bare "3.24"); Vout = 1.221·(1+10k/3.24k) = 5.0 V. ✓
- **Receiver J3 wired:** Pin3→UARTx_RX→PA10, Pin4→UARTx_TX→PA9, Pin1→+5V, Pin2→GND (USART1, non-inverted i-BUS). ✓
- **USBLC6-2 fixed:** pin 2→GND, pin 5→VBUS(+5V_USB); D±routed *through* the array to PA11/PA12. ✓
- **VDD decoupling:** 4×100 nF (C24–C27) + 4.7 µF (C28) for the four VDD pins; VBAT pin1→+3V3. ✓
- IMU SPI1 (PA5/6/7, CS=PC4 + R3 pull-up, INT=PC3), flash SPI2 (PB12–15), crystal on PH0/PH1 w/ 22 pF, BOOT0/BOOT1/NRST straps — all correct.

---

## Verdict: **Proceed with changes — do not start layout yet**
The power core, MCU support, USB, SPI buses, clock and boot straps are clean, and the three old fab-blockers are fixed. But: **C1 (ESC pin-order / VBAT-on-GPIO)** is a genuine brick-the-board risk that must be resolved + continuity-verified, **C2/C3** leave required ADC protection and battery sensing missing, **M1** (IMU rail swap) wastes your quiet LDO, and the board is **functionally incomplete** (no SWD/buzzer/LED). Fix C1–C3 and M1–M2, finish the missing blocks, restore the file from backup, then run ERC and re-check before layout.

**Order:** #0 file restore → C1 → C2 → C3 → M1 → M2 → SWD/buzzer/LED → BOM values → ERC.
