# Custom Drone Flight Controller PCB — Project Context v3

**Date:** 2026-06-09. Supersedes v1 (`drone_fc_project_context.md`) and the lost v2. Incorporates the June 2026 audit (`FC_audit_2026-06-09.md`). Build is a **bench/resume project: no VTX, camera, or OSD**.

> ### ⚠ Update 2026-07-30 — IMU CHANGED: ICM-42605 → Bosch BMI270
>
> **Every ICM-42605 / ICM-42688-P / 426xx reference below this box is superseded.**
> The board is built around the **Bosch BMI270** (U7, LCSC C2836813, footprint
> `drone_lib:LGA-14_L3.0-W2.5-P0.50-BR`). The 2026-06-23 decision recorded below
> ("BMI270 evaluated and rejected", "stay with the ICM-42605 default") was
> **reversed on sourcing grounds**: the entire TDK 426xx family went reel-only
> (MOQ 1000+) or out of stock in single quantities at LCSC, DigiKey and Mouser.
>
> It is **not a drop-in.** Same 2.5×3.0mm LGA-14 outline, completely different
> pinout → new footprint and every IMU net re-routed. Decoupling is simpler:
> **100nF at VDD + 100nF at VDDIO**, not the 2.2µF/0.1µF/10nF the 426xx wanted.
> The unused-pin strapping is also different — ASDx(2)/ASCx(3) go to **VDDIO**
> and must *not* be grounded, and INT2(9)/OCSB(10)/OSDO(11) are left
> **unconnected**, where the 42605's RESV pins went to GND.
>
> **The power architecture did not change at all** — VDD still on the quiet
> TLV733P `+3V3_IMU` rail, VDDIO still on the main AP2112K `+3V3`. That's the
> payoff of the rail-split decision: a forced sourcing change cost a footprint
> and a re-route, not a power redesign.
>
> Firmware: Betaflight define is `BMI270`. **PID loop ceiling drops to 3.2 kHz**
> (BF clocks the BMI270 gyro at 3.2 kHz ODR; chip max is 6.4 kHz), SPI clock
> ceiling is **10 MHz** not 24 MHz, and the driver uploads an ~8 KB init blob at
> every boot. The BMI270 also ships uncalibrated — a few percent attitude error
> in Angle/Horizon, irrelevant in Acro. `GYRO_1_ALIGN` must be re-derived.
>
> Canonical pad-by-pad wiring table (verified against BMI270 datasheet rev 1.3
> Table 22, p.145): see **`VERIFIED_PINOUT.md` § IMU**.

**Update 2026-07-11 (SCHEMATIC COMPLETE — verified, entering layout):**
Full netlist re-verified pin-by-pin against the KiCad files (not the docs) this session. Status: **schematic done, ERC-clean pending, ready for layout.** Resolutions since 2026-06-28:
- **IMU rail swap DONE** — as-drawn is now correct: VDDIO (pin 5) → +3V3 (main), VDD (pin 8) → +3V3_IMU (quiet). All 14 ICM-42605 pins verified: MISO/MOSI/SCLK/CS on 1/14/13/12, INT1 (4) → PC3/EXTI3, RESV pin 7 + RESV 2/3/10/11 + INT2/FSYNC (9) → GND, CS 10k pull-up → +3V3. Decoupling: VDD 2.2µF+0.1µF, VDDIO 10nF.
- **Current-sense protection ADDED** — CN1 current pin → R5 1k series → PC1, D1 3.3V zener PC1→GND. Correct clamp (PC1 taps between R5 and zener). **Note: current sense is on PC1** (not PC2 as older pin table said).
- **VBAT divider ADDED** — +BATT → R6 100k → PC2, R13 10k + C29 100nF → GND. Ratio 0.091 (16.8 V → 1.53 V at PC2). Battery telemetry now available; calibrate vbat_scale at bring-up.
- **SWD header ADDED (J1, 5-pin)** — 3V3 / SWDIO=PA13 / SWDCLK=PA14 / GND / NRST. For emergency flash/debug if USB DFU fails.
- **Power LED ADDED** — +3V3 → R14 series resistor → D5 → GND. (Earlier IMU-rail LED D6 removed — do not load the quiet gyro rail.)
- **Buck confirmed complete** — TPS5430, L 15µH, Cin 10µF, Cout 220µF polymer, SS34 catch diode, 10nF BOOT cap, FB 10k/3.24k tied to +5V_Buck output. ENA left floating (TPS5430 enables when floating).
- **ORing diodes D3/D4 verified correct** (anodes at +5V_Buck / +5V_USB sources, cathodes at common +5V).
- **Motor pins confirmed in KiCad**: M1=PB0, M2=PB1, M3=PB6 (TIM4_CH1), M4=PB5 (TIM3_CH2). PA2/PA3/PB7 free.

**⚠ REMAINING BEFORE FAB (physical/process, not schematic):**
1. **ESC connector orientation** — CN1 is numbered reverse vs the manufacturer diagram (VBAT on pad 3, GND pads 1–2, current pad 9). Signal order matches the harness only if the connector mates reversed. **Mandatory: continuity-beep VBAT + both GND pads to the ESC XT60 before first power-up** — a mirror error puts 16.8 V on a 3.3 V GPIO.
2. Run **ERC** to zero before layout (authoritative check for unconnected nets).
3. Buzzer (PC5) and Betaflight status LED (PB8) intentionally not populated — spare UART/I2C pads (PB10/11, PC6/7) not broken out. All optional; add if wanted.
4. **File-sync corruption risk** — keep the project OFF the synced drive or pause sync while editing; a truncated `.kicad_sch` right before Gerber export is the worst-case. Confirm clean saves.

**Update 2026-06-28 (battery → 4S, motor remap, open items):**
- **Battery changed to 4S (14.8 V nominal / 16.8 V full charge)** — was 6S. Consequences: buck output inductor is now **15 µH** (TPS5430 datasheet value for this Vin/Vout; L_min ≈ 12 µH) — **the locked BOM 22 µH part C354622 is superseded; order a 15 µH ≥3 A shielded inductor instead.** Battery-rail/buck-input cap voltage rating requirement relaxes from ≥50 V (6S) to **≥25 V (use ≥35 V for transient margin)**. Any VBAT sense divider scales for 16.8 V.
- **Motor remap (done in schematic):** M4 moved **PB7 → PB5 (TIM3_CH2)** to clear the DMA1-Stream3 clash with SPI2_RX (blackbox). Motors are now M1=PB0/TIM3_CH3, M2=PB1/TIM3_CH4, M3=PB6/TIM4_CH1, M4=PB5/TIM3_CH2 → DMA1 streams S7/S2/S0/S5, all clear of SPI2 (S3/S4). PB7 freed. Verified against RM0090 Table 42.
- **IMU rail swap STILL PENDING in schematic:** as drawn, VDD(pin 8)→+3V3 and VDDIO(pin 5)→+3V3_IMU — **backwards** from the 2026-06-26 decision below. Must swap to VDD(8)→+3V3_IMU (quiet), VDDIO(5)→+3V3 (main). Caps follow their pins (C10 2.2µF + C11 0.1µF on VDD; C9 10nF on VDDIO).
- **Still open (ADC protection):** current sense (PC1) needs **1 kΩ series + 3.3 V Zener** to GND; **add a VBAT divider** (e.g. 100k/10k → free ADC PC0 or PC2, +100 nF) if battery voltage telemetry is wanted.
- **⚠ File corruption:** the working `*.kicad_sch` files keep getting truncated/NUL-padded on save (folder-sync race during KiCad's atomic save). The reliable clean copies are the `Drone_PCB_FC-backups/*.zip`. Fix the sync (pause it while editing, or move the project off the synced drive) and confirm a clean save before fab.

**Update 2026-06-26 (schematic capture in progress):** Buck (TPS5430) **fully wired**; both LDOs (AP2112K + TLV733P) **wired**; USB-C wired; **IMU power wired** (SPI logic still to do); ESC connector **J10** has VBAT+GND wired (signal lines pending). Two decisions refined this session: **(1) IMU rail split** — `VDD` (pin 8) stays on the quiet `+3V3_IMU`, but **`VDDIO` (pin 5) now sits on the main `+3V3`** (host logic rail) so digital I/O switching noise stays off the gyro supply and VDDIO matches the SPI host level. Caps per ICM-42605 BoM: 0.1µF+2.2µF at VDD, 10nF at VDDIO; RESV pin 7→GND mandatory, other RESV + INT2/FSYNC→GND. **(2) Receiver = FlySky FS-iA6B over i-BUS** (non-inverted, straight to UART1 RX; S.BUS rejected — inverted, no internal F405 inverter). Also confirmed: LQFP-64 has **no PDR_ON / no BYPASS_REG** pin (regulator + internal reset always on — nothing to wire); no 32.768kHz LSE needed (Betaflight doesn't use RTC). **Open ESD fix:** USBLC6-2 pin 2→GND, pin 5→VBUS were floating in the draft.

**Update 2026-06-23 (decisions locked):** Board grows to **~60×60mm** (oversized bench board for easy hand assembly; retains 30.5×30.5mm M3 mount to mate the BLS-04). **Assembly changed to self-reflow** — order **bare 4-layer PCBs + frameless stencil** from JLC (not PCBA), keep **all SMD single-sided (top)** for one hotplate pass, leaded (Sn63/Pb37) paste, hot-air station for LGA/rework. **MCU order part = STM32F405RGT6TR** (tape-and-reel/cut-tape, LCSC C15742, 600 in stock, buy-1). **IMU resolved — stay with the ICM-42605 default** (LCSC C2655099, buy-1 cut-tape, ~$9.38, ~1,168 in stock): Betaflight-preferred and shares the LGA-14 426xx footprint, so no design change. **BMI270 rejected** — Betaflight discourages it for new designs (uncalibrated gyro, 5–10% attitude error). ICM-42688-P (C1850418) stays as the drop-in alternate if its reel frees up. **Power architecture changed:** the external Matek MBEC6S is dropped — the FC now carries its **own onboard buck (TPS5430DDAR, C9864, VBAT→5V/3A)** fed from the ESC-harness VBAT, because the BLS-04 / Trinx G20 ESCs have no BEC. The old J5 external-5V-in header is removed.

## Project Goal
Custom ~60×60mm flight controller (oversized bench board for hand assembly; 30.5mm M3 mount retained) around the STM32F405RGT6, running Betaflight via a custom target (`config.h`), driving the existing Flycolor Raptor BLS-04 4-in-1 ESC through its original 10-pin harness.

## Open items (blocking schematic)
1. ~~ESC harness pinout UNVERIFIED~~ **RESOLVED 2026-06-16** — manufacturer pinout diagram obtained; cross-checks against the old v1 candidate (same physical layout, opposite numbering). See verified table below. Remaining: 30-sec continuity beep on VBAT + 2×GND to ESC XT60 pads to confirm pin-1 orientation isn't mirrored before first power-up.
2. Crystal load caps: confirm 22pF against the chosen crystal's CL spec (22pF fits CL≈12–14pF with ~2–3pF stray; CL=18pF would need ~30pF).

---

## Hardware Stack

### ESC — Flycolor Raptor BLS-04 4-in-1 (owned)
- BLHeli_S, EFM8BB21-family ESC MCU (per Flycolor S-Tower manual) → **Bluejay-flashable** for bidirectional DSHOT / RPM filtering (verify layout in ESC-Configurator first)
- 3–6S LiPo, 60–65A continuous / 70A burst, 500k eRPM limit
- DSHOT150/300/600, OneShot125/42, Multishot
- **No BEC** — exposes raw VBAT only
- **No serial telemetry** (BLHeli_S limitation; RPM comes via bidir DSHOT after Bluejay)
- 30.5×30.5mm M3 mounting (tower stack — matches FC hole pattern)
- FC interface: 10-pin JST SH1.0

### ESC connector — VERIFIED pinout (manufacturer diagram, 2026-06-16)
From the BLS-04 pinout diagram. Cross-checks against the v1 candidate (identical physical layout — GND/GND/VBAT cluster at one end, CRT/TX at the other, 4 motor signals in the middle — just numbered from the opposite end). Listed in diagram order (one end → other).

| Signal | FC destination | Protection |
|----|----|----|
| TX (ESC telemetry TX) | spare UART RX pad (USART6 RX = PC7) or NC | none — **non-functional on BLHeli_S** (RPM via bidir DSHOT after Bluejay) |
| CRT (current sense, analog) | PC2 (ADC123_IN12) | **1kΩ series + 3.3V Zener clamp (required)** |
| S1 (motor 1) | PB0 | — |
| S2 (motor 2) | PB1 | — |
| S3 (motor 3) | PA3 | — |
| S4 (motor 4) | PA2 | — |
| NC | — | — |
| VBAT (raw pack +, up to 25.2V/6S) | PC1 (ADC123_IN11) **+ TPS5430 buck VIN** | **100k/10k divider + 100nF (sense); also feeds onboard buck input via 50V caps (required)** |
| GND | Ground plane | — |
| GND | Ground plane | — |

**Pre-power continuity check (battery disconnected):** beep from the cable's VBAT and both GND pins to the ESC XT60 pads to confirm pin-1 orientation matches the FC footprint (1:1 SH1.0 cable mirrors pin order; a mirrored connector puts up to 25.2V on a 3.3V GPIO). Motor order (S1–S4) needs no verification — remapped in Betaflight.

### Power
```
LiPo (3–6S, ≤25.2V) → BLS-04/Trinx ESC → raw VBAT on 10-pin harness
   VBAT ─┬─→ sense divider 100k/10k → PC1 (ADC)
         └─→ TPS5430DDAR buck (VBAT→5V, 3A) ─┬─ 5V rail
USB-C VBUS → Schottky (SS34/B5819W) ─────────┘
5V rail → AP2112K-3.3 (600mA) → 3.3V: MCU, flash, LED, logic, BMI270 VDDIO (pin 5)
5V rail → TLV733P-3.3 (LCSC C134139) → [JP10] → 3.3V_IMU: BMI270 VDD (pin 8) only — dedicated quiet rail
5V rail → receiver header (FS-iA6B, 5V), active buzzer
```
- **Onboard buck (NEW 2026-06-23): TPS5430DDAR (LCSC C9864), 5.5–36V in → 5V/3A**, fed from the ESC-harness VBAT. Replaces the external Matek MBEC6S (removed) — the BLS-04 / Trinx G20 ESCs have **no BEC**, so the FC must make its own 5V like a normal stack FC. Support parts (locked to TI datasheet values): **22µH ≥3A shielded inductor (CKCS6028, C354622)**, **SS34 catch diode (PH→GND)** (= datasheet B340A), 10nF BOOT cap, feedback **R1=10k / R2=3.24k** (Vref 1.221V → 5.0V), input **2×10µF 50V X7R (GRM32ER71H106KA12L, C77102)**, output **220µF aluminum-polymer (~20–40mΩ ESR) — NOT pure ceramic** (the TPS5430 is internally compensated for some output-cap ESR; all-ceramic output can oscillate).
- **6S input caps must be ≥50V rated** (25.2V rail). Keep the buck switching loop (input cap → IC → inductor → diode) physically tight and the PH/switch node **away from the IMU and the VBAT/current ADC traces**; the TLV733P then filters residual switching ripple off the gyro rail.
- Schottky ORing lets USB power the MCU + receiver on the bench; the onboard buck dominates in flight. No backfeed into USB.
- AP2112K: 1µF in, 1µF out (ceramic X7R or X5R per datasheet Note 4). EN tied to 5V rail — always-on (VIH max = 6.0V, so 5V is safe). TLV733P: 1µF in/out + 100nF at IMU pins.

---

## MCU — STM32F405RGT6**TR**, **LQFP-64** (10×10mm, 0.5mm pitch; order LCSC C15742 — tape-and-reel/cut-tape, 600 in stock, buy-1)
Required support circuitry:
- 8MHz crystal (HSE) + 2× load caps (size per CL — open item 2)
- Decoupling: 100nF per VDD pin + one 4.7µF bulk; 1µF+10nF on VDDA
- **VCAP_1 (pin 31) and VCAP_2 (pin 47): 2.2µF X5R/X7R each, placed at the pin** (core regulator stability — mandatory)
- MCU VBAT pin → 3.3V + 100nF
- BOOT0: 10k pull-down + button to 3.3V (DFU entry)
- **BOOT1/PB2: 10k pull-down** (BOOT0=1 requires BOOT1=0 for system-memory DFU; floating = unreliable boot target)
- NRST: button to GND + 100nF
- USB FS on PA11/PA12: **no series resistors needed** (AN4879 — internal impedance). Optional USBLC6-2SC6 ESD array.
- USB-C: 5.1k on CC1 and CC2 to GND
- VBUS sense: 10k/10k divider from VBUS to PC0 (optional; Betaflight `USB_DETECT_PIN`)
- **SWD header: 3.3V, SWDIO (PA13), SWCLK (PA14), NRST, GND** — keep PA13/PA14 unassigned

## Pin Assignment

| Function | Pin | Notes |
|----|----|----|
| Motor pads A–D | PB0, PB1, PA3, PA2 | TIM3_CH3, TIM3_CH4, TIM2_CH4, TIM2_CH3. Map to S1–S4 in config.h after harness verification — copy OMNIBUSF4 `TIMER_PIN_MAP` rows verbatim (known-good DSHOT DMA) |
| IMU SPI1 | PA5/PA6/PA7 | SCK/MISO/MOSI |
| IMU CS | PC4 | **10k pull-up to 3.3V_IMU** (no floating CS during boot/DFU) |
| IMU INT | PC3 | EXTI3, keep trace short |
| Flash SPI2 | PB13/PB14/PB15 | SCK/MISO/MOSI |
| Flash CS | PB12 | |
| USB | PA11 (D−), PA12 (D+) | length-matched pair |
| VBUS sense | PC0 | optional |
| UART1 (receiver) | PA9 TX, PA10 RX | **FlySky FS-iA6B → i-BUS on RX (PA10, 5V-tolerant)** — non-inverted, no inverter; Betaflight provider IBUS. TX = optional i-BUS telemetry |
| UART3 (spare pads) | PB10 TX, PB11 RX | future GPS/MSP; **not** ESC telemetry (none on BLHeli_S) |
| USART6 (spare pads) | PC6 TX, PC7 RX | costs nothing now |
| I2C1 (spare pads) | PB6 SCL, PB7 SDA | future baro/mag |
| VBAT ADC | PC1 | ADC123_IN11 |
| Current ADC | PC2 | ADC123_IN12 |
| Buzzer | PC5 | GPIO → 2N7002 low-side |
| Status LED | PB8 | PC13 forbidden (power-switch domain, ~3mA, DS8626) |
| SWD | PA13/PA14 | header only |

## Component List

**MCU support:** STM32F405RGT6 (LQFP-64) · 8MHz crystal + load caps · decoupling per above · **2× 2.2µF VCAP** · 2× 10k (BOOT0, PB2) · BOOT0/NRST tact switches · USB-C receptacle + 2× 5.1k · optional USBLC6-2SC6

**Power:** **TPS5430DDAR buck (ESOP-8, C9864) — VBAT→5V/3A** + 22µH inductor (C354622) + SS34 catch diode + 10nF boot + 10k/3.24k FB + 2×10µF 50V Cin (C77102) + 220µF polymer Cout · AP2112K-3.3 (SOT-23-5) · **TLV733P-3.3 (SOT-23-5, C134139)** · SS34/B5819W Schottky (USB OR) · caps per power section · ~~5V/GND input pads (MBEC6S)~~ **removed — FC powered from ESC VBAT via onboard buck**

**IMU (CURRENT — updated 2026-07-30):** **Bosch BMI270** (U7, LCSC C2836813, ~$0.82–1.79, buy-1 in stock) on footprint `drone_lib:LGA-14_L3.0-W2.5-P0.50-BR` — **VDD (pin 8) on dedicated 3.3V_IMU with 100nF; VDDIO (pin 5) on main 3.3V with 100nF** · ASDx (2) + ASCx (3) → **VDDIO** (must NOT be grounded) · INT2 (9), OCSB (10), OSDO (11) → **unconnected** · INT1 (4) → PC3/EXTI3 · CSB (12) → PC4 with 10k pull-up to +3V3 · SCx (13) → PA5, SDx (14) → PA7/MOSI, SDO (1) → PA6/MISO · board center. Pad-by-pad table in `VERIFIED_PINOUT.md` § IMU.
- **`+3V3_IMU` reaches VDD through solder jumper JP10 (`SolderJumper_2_Open`) — bridge it, or the gyro is unpowered.**
- **Betaflight define = `BMI270`** (`USE_GYRO_SPI_BMI270` / `USE_ACC_SPI_BMI270`). PID loop ceiling **3.2 kHz** (not 8 kHz), SPI clock ceiling **10 MHz** (not 24 MHz), ~8 KB init blob uploaded at boot, chip boots in I2C and needs a CSB rising edge to enter SPI. `GYRO_1_ALIGN` must be re-derived — the 42605 guess does not carry over.
- **Known tradeoff (this was the original reason for rejecting it):** the BMI270 ships uncalibrated, which Betaflight cites when discouraging it for new designs — a few percent attitude error. Irrelevant in Acro freestyle, visible in Angle/Horizon and level trim. Accepted because the alternative was not building the board.

> **SUPERSEDED (kept for history — 2026-06-23 decision, reversed 2026-07-30):**
> ~~ICM-42605 (LCSC C2655099, ~$9.38) default populate; VDD 0.1µF+2.2µF, VDDIO 10nF; RESV pin 7→GND mandatory, RESV 2/3/10/11 + INT2/FSYNC→GND.~~
> ~~Footprint is the TDK 426xx LGA-14 shared with ICM-42688-P (C1850418), pin-compatible — populate whichever is in stock. BMI270 evaluated and rejected: BF discourages it for new designs (uncalibrated gyro); not worth leaving the 426xx footprint.~~
> ~~Gyro define = `ICM42605` or `ICM42688P` depending on which is fitted.~~
> Reversed because the whole 426xx family became reel-only (MOQ 1000+) / OOS at qty 1 across LCSC, DigiKey and Mouser. There is **no** pin-compatible alternate for the current footprint.

**Blackbox (CURRENT — updated 2026-07-30):** **GigaDevice GD25Q16E** (LCSC C2904431, SOIC-8 208mil, SPI2) + 100nF — **16 Mbit = 2 MB, not 16 MB.** Wrong part ordered vs the 128 Mbit spec; kept. Betaflight needs no code change (`USE_FLASH_W25Q128FV` only gates the generic `m25p16` driver, which detects by JEDEC ID; our chip is `{ 0xC84015, 104, 50, 32, 256 }` = 2,097,152 B). Configurator reporting 2 MB is correct. **Set `blackbox_sample_rate = 1/4`** at bring-up → 800 Hz logging, ~87 s per erase; at 1/1 you get ~22 s. Upgrade path: GD25Q128E (C2758105) or BY25Q128ES (C22471255), same JEDEC table, identical pinout, hot-air swap.
> ~~**Blackbox:** W25Q128JVSIQ (16MB SOIC-8, SPI2) + 100nF~~ — superseded (MOQ 12–14 @ ~$25–30), then superseded again by the ordering error above.

**Connectors:** JST SH1.0 10-pin (ESC) — SM10B-SRSS-TB, **LCSC C160409** (side-entry SMD; confirm vs top-entry against stack routing) · USB-C receptacle — TYPE-C-31-M-12, **LCSC C165948** (JLC-basic) · **4-pin receiver header: 5V, GND, TX, RX** (generic 2.54mm, KiCad stock) · SWD header (generic 2.54mm, KiCad stock) · spare pads: UART3, USART6, I2C1 (J5 external 5V-in pads removed — FC now powered from ESC VBAT via onboard buck)

**Misc:** **active** 5V magnetic buzzer — SUN-12095-5VPA7.6, **LCSC C360615** (active, built-in driving circuit, through-hole 12mm). **Hand-soldered → mark DNP in JLC BOM; place footprint + BZ+/BZ− pads only.** Betaflight beeper is on/off GPIO — passive buzzers only click. + 2N7002 + 1N4148 flyback · status LED + 330Ω on PB8 · VBAT divider 100k/10k + 100nF · current input 1k series + 3.3V Zener

### Locked LCSC part numbers (import via easyeda2kicad → `D:\Drone\drone_PCB\libs\drone_lib`)
| Part | LCSC | Notes |
|----|----|----|
| STM32F405RGT6**TR** | C15742 | tape-and-reel/cut-tape, 600 stock, buy-1; KiCad stock symbol `STM32F405RGTx` OK |
| **BMI270 (IMU)** | **C2836813** | **CURRENT PART (2026-07-30).** Bosch, buy-1 in stock. Footprint `LGA-14_L3.0-W2.5-P0.50-BR`. No pin-compatible alternate. |
| ~~ICM-42605 (IMU)~~ | ~~C2655099~~ | ~~default populate; shared 426xx footprint w/ ICM-42688-P (C1850418)~~ — **SUPERSEDED**, 426xx reel-only/OOS at qty 1 |
| **GD25Q16E (flash)** | **C2904431** | **AS-FITTED. 2 MB, not 16 MB — wrong part ordered, kept.** BF-supported via m25p16 JEDEC table, no code change. Set `blackbox_sample_rate = 1/4`. |
| ~~W25Q128JVSIQ (flash)~~ | ~~C97521~~ | superseded — MOQ 12–14 (~$25–30) |
| BY25Q128ES / GD25Q128E | C22471255 / C2758105 | not fitted — documented 16 MB upgrade path, identical SOIC-8 208mil pinout |
| TLV733P-3.3 | C134139 | IMU rail LDO |
| AP2112K-3.3 | C51118 | main 3.3V LDO |
| TPS5430DDAR (buck) | C9864 | onboard VBAT→5V/3A, ESOP-8; needs L1 + Cin + Cout + diode + FB below |
| 22µH inductor (buck L1) | C354622 | CKCS6028-22µH/M, 6028 shielded, ~3A; buck switch inductor |
| 10µF 50V X7R (buck Cin) | C77102 | Murata GRM32ER71H106KA12L, 1210; ×2 on buck VIN |
| 220µF polymer (buck Cout) | (assign at BOM) | aluminum-polymer ~20–40mΩ ESR; **not ceramic** (loop stability) |
| 3.24k 1% (buck FB R2) | (assign at BOM) | 0603; sets 5.0V with R1=10k |
| USB-C receptacle | C165948 | TYPE-C-31-M-12 |
| JST SH1.0 10-pin | C160409 | SM10B-SRSS-TB, ESC |
| USBLC6-2SC6 | C7519 | optional USB ESD |
| Buzzer | C360615 | **DNP / hand-solder** |
| Passives, headers | — | KiCad stock symbols; assign LCSC # at JLC BOM stage |

## Board
- **~60×60mm outline**, holes 30.5×30.5mm M3 (matches BLS-04 tower — board overhangs the stack; fine for a bench build, and the extra area eases hand assembly)
- **Single-sided assembly: ALL SMD on top (L1)** — one hotplate reflow pass, no flip. Do not place parts on the bottom.
- 4-layer: L1 signal/components · L2 solid GND (unbroken under IMU) · L3 power pours (3.3V / 3.3V_IMU / 5V) · L4 signal
- Layout priorities: IMU dead-center over solid ground, TLV733P adjacent; VCAP + decoupling at pins; crystal loop tight, away from USB; USB pair matched; motor traces away from IMU. Size to the hotplate's even-heat zone (~100mm plate)
- **Fab + assembly: order BARE 4-layer boards + frameless stencil from JLCPCB; self-populate** with leaded (Sn63/Pb37 T4) paste + stencil + ~100mm hotplate. Keep a hot-air station for the LGA IMU and bridge/rework. (No PCBA — F405 + all SMD reflowed by hand.) Bare 4-layer + stencil ≈ $20–40 / 5 boards.

## Build Order
1. **Verify ESC harness pinout** (protocol above) → update table → only then start KiCad
2. Power: **onboard TPS5430 buck (VBAT→5V)** + USB ORing → AP2112K + TLV733P rails
3. MCU: crystal, decoupling, **VCAP**, BOOT0 + **PB2**, NRST, VBAT pin
4. USB-C + CC resistors (+ ESD optional), VBUS divider
5. ESC connector per verified table; ADC protection (divider+cap, 1k+Zener)
6. IMU on SPI1 + CS pull-up + INT; flash on SPI2
7. Receiver header (5V!), spare UART/I2C pads, SWD header
8. Buzzer (active) + LED (PB8)
9. ERC → layout (single-sided, all parts on top) → DRC → JLCPCB DFM → **order bare boards + frameless stencil** → self-reflow (stencil + leaded paste + hotplate; hot-air for IMU/rework)
10. Bring-up (props off, ESC disconnected): rail smoke test → SWD/DFU connect → flash custom target (`make configs && make ETHANF405` — skeleton in audit §7) → Configurator sensor/flash check → connect verified harness → motor test no props → calibrate vbat scale (~110) and current scale (bench sweep, unknown V/A) → optional Bluejay flash for RPM filtering
