# Custom Drone Flight Controller PCB - Revised Project Context

## Project Goal
Design a custom flight controller PCB built around the STM32F405RGT6, compatible with Betaflight firmware. The board connects to an existing Flycolor Raptor BLS-04 4-in-1 ESC via its original cable. 

## Why This Approach
- STM32F405 is the industry standard FC chip.
- Directly relevant to drone/embedded companies.
- Real firmware work: writing a custom Betaflight target file.
- More resume impact than a module-based approach.

---

## Hardware Stack

### ESC
- **Flycolor Raptor BLS-04** 4-in-1 ESC
- 3-6S LiPo input, 65A continuous / 70A burst
- BLHeli_S firmware
- Supports DSHOT150/300/600 and OneShot125
- **No onboard BEC**
- Connects to FC via 10-pin JST SH1.0 cable

### Power
- **Matek MBEC6S** external buck converter (already built)
  - Input: 6-30V (raw LiPo)
  - Output: 5V, 1.5A continuous / 2.5A burst
- **On FC board:** AP2112K-3.3 LDO (5V in → 3.3V out). Chosen for strict stability with low-ESR ceramic capacitors.

### Power Chain
`LiPo → BLS-04 ESC (raw LiPo passthrough) → Matek MBEC6S → 5V → FC board → AP2112K-3.3 → 3.3V → STM32 + IMU`

---

## MCU: STM32F405RGT6

- 168MHz Cortex-M4 with FPU
- LQFP-100 package, 0.5mm pitch
- Native Betaflight support
- Required support circuitry:
  - 8MHz external crystal (clock source)
  - Decoupling caps on every VDD/VDDA pin (strictly routed: Plane → Cap → Pin)
  - BOOT0 pin with 10k pull-down + button to 3.3V
  - NRST button to GND with 100nF cap
  - **22Ω series resistors** on USB D+ and D- lines
  - **VBUS sensing circuit** (voltage divider to GPIO)

---

## ESC Connector & Protection

**JST SH1.0 10-pin female receptacle on FC board**

| Pin | Signal | FC Destination | Protection / Filtering |
|-----|--------|----------------|------------------------|
| 1 | GND | Ground plane | None |
| 2 | GND | Ground plane | None |
| 3 | VBAT | ADC pin (PC1) | 100k/10k divider + **0.1μF cap to GND** |
| 4 | NC | No connection | N/A |
| 5 | Motor 4 | PB1 (TIM3_CH4) | None |
| 6 | Motor 3 | PB0 (TIM3_CH3) | None |
| 7 | Motor 2 | PA3 (TIM2_CH4) | None |
| 8 | Motor 1 | PA2 (TIM2_CH3) | None |
| 9 | CRT (Current) | ADC pin (PC2) | **1kΩ series resistor** + optional 3.3V Zener |
| 10 | NC | No connection | N/A |

---

## Full Pin Assignment Table

| Function | STM32F405 Pin | Notes |
|----------|--------------|-------|
| Motor 1 | PA2 | TIM2_CH3, DMA capable for DSHOT |
| Motor 2 | PA3 | TIM2_CH4 |
| Motor 3 | PB0 | TIM3_CH3 |
| Motor 4 | PB1 | TIM3_CH4 |
| IMU SCK | PA5 | SPI1 |
| IMU MISO | PA6 | SPI1 |
| IMU MOSI | PA7 | SPI1 |
| IMU CS | PC4 | Any GPIO |
| IMU INT | PC3 | Any GPIO, interrupt input |
| Flash SCK | PB13 | SPI2 |
| Flash MISO | PB14 | SPI2 |
| Flash MOSI | PB15 | SPI2 |
| Flash CS | PB12 | Any GPIO |
| USB D- | PA11 | USB FS (Requires 22Ω series resistor) |
| USB D+ | PA12 | USB FS (Requires 22Ω series resistor) |
| **VBUS Sense** | **PC0** | **Detects USB connection (10k/10k divider from 5V USB)** |
| UART1 TX | PA9 | Receiver input |
| UART1 RX | PA10 | **ELRS Recommended** (SBUS requires external hardware inverter) |
| UART3 TX | PB10 | ESC telemetry (optional) |
| UART3 RX | PB11 | |
| VBAT ADC | PC1 | ADC12_IN11 (Requires 0.1μF bypass cap) |
| Current ADC | PC2 | ADC12_IN12 (Requires 1kΩ series protection resistor) |
| Buzzer | PC5 | GPIO → MOSFET → buzzer |
| Status LED | PC13 | GPIO |

---

## Full Component List

### MCU Support
- STM32F405RGT6 (LQFP-100)
- 8MHz crystal + 2x 22pF load caps
- Decoupling: 1uF + 100nF on each VDD pin, 1uF + 10nF on VDDA
- USB Series Resistors: 2x 22Ω (0402 or 0603)
- BOOT0 / NRST tactile buttons

### Power
- **AP2112K-3.3** LDO (SOT-23-5)
- 10uF + 100nF input caps on LDO
- 10uF + 100nF output caps on LDO

### IMU
- **ICM-42688-P** (SPI, modern Betaflight standard, currently manufactured)
- 100nF decoupling on VDD and VDDIO
- Mount at center of board, away from switching noise

### Blackbox
- W25Q128JVSIQ SPI NOR flash (16MB, SPI2)
- 100nF decoupling

### Connectors
- JST SH1.0 10-pin female (ESC interface)
- 3-pin 2.54mm header: UART1 RX/TX/GND (Receiver)
- 3-pin 2.54mm header: UART3 RX/TX/GND (ESC telemetry)
- USB-C connector with 5.1k resistors on CC1 and CC2 to GND

### Misc & Protection
- Passive buzzer + 2N7002 MOSFET + 1N4148 flyback diode
- Status LED + 330Ω series resistor
- VBAT divider: 100kΩ + 10kΩ + **0.1μF capacitor**
- Current sense protection: **1kΩ series resistor**
- **VBUS divider**: 10kΩ + 10kΩ (5V USB to PC0)

---

## Board Specs
- Target size: ~40x40mm
- **4-layer stackup**
  - Layer 1: Signal + components (top)
  - Layer 2: Ground plane (solid, unbroken — especially under IMU)
  - Layer 3: Power plane (3.3V and 5V copper pours)
  - Layer 4: Signal (bottom)
- Mounting holes: 30.5x30.5mm pattern (standard drone stack)
- Fab: JLCPCB

---

## Schematic Build Order (Start here in KiCad)
1. **Power Section:** 5V input → AP2112K-3.3 → 3.3V rail with MLCC decoupling.
2. **STM32F405:** Place chip, add crystal, route decoupling caps (ensuring tight physical placement), BOOT0/NRST circuits.
3. **USB & VBUS:** PA11/PA12 with 22Ω resistors, CC pull-downs, VBUS 10k/10k divider to PC0.
4. **ESC Connector:** 10-pin JST SH1.0, motor signals to timer pins.
5. **ADC Protection:** VBAT divider (add 0.1μF cap), current ADC (add 1kΩ resistor).
6. **IMU:** ICM-42688-P on SPI1 (PA5/6/7), CS on PC4, INT on PC3.
7. **Flash:** W25Q128 on SPI2 (PB13/14/15), CS on PB12.
8. **Receiver Header:** UART1 RX (PA10), TX (PA9), GND. (Plan for ELRS to avoid hardware inversion).
9. **Peripherals:** Buzzer circuit, Status LED.
10. **Review:** Run ERC/DRC, inspect ground paths, export Gerbers.