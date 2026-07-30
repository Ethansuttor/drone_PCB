# ETHANF405 — VERIFIED Pin Map (canonical)

**Source of truth for firmware.** Verified against the KiCad schematic
(2026-07-23) and the STM32CubeMX report "STM32 Program". This is the **as-ordered
board** — where this table disagrees with older docs (`drone_fc_project_context_v3.md`,
audits), **this table wins**. The board is fabricated; pins are not changing.

MCU: STM32F405RGT6, LQFP-64. HSE 8 MHz.

## Active functions

| Function | Pin | Peripheral / net | Notes |
|---|---|---|---|
| Motor 1 | PB0 | TIM3_CH3 | net `TIM3_CH3-M1` |
| Motor 2 | PB1 | TIM3_CH4 | net `TIM3_CH4-M2` |
| Motor 3 | PA3 | TIM2_CH4 | net `TIM2_CH4-M3` |
| Motor 4 | PB10 | TIM2_CH3 | net `TIM2_CH3-M4` |
| Gyro SCK | PA5 | SPI1_SCK | **BMI270** (U7) — see IMU note below |
| Gyro MISO | PA6 | SPI1_MISO | |
| Gyro MOSI | PA7 | SPI1_MOSI | |
| Gyro CS | PC4 | GPIO (net `IMU_nCS`) | |
| Gyro INT | PC3 | EXTI3 (net `IMU_EXTI3`) | data-ready |
| Flash SCK | PB13 | SPI2_SCK | **GD25Q16E, 2 MB** — see note below |
| Flash MISO | PC2 | SPI2_MISO | **on PC2, not PB14** |
| Flash MOSI | PB15 | SPI2_MOSI | |
| Flash CS | PB12 | GPIO (net `/CS`) | |
| Current sense | PA2 | ADC1_IN2 | net `CurrentM1`, D1 zener clamp to GND |
| Receiver TX | PA9 | USART1_TX | |
| Receiver RX | PA10 | USART1_RX | FS-iA6B i-BUS in |
| USB D− | PA11 | USB_OTG_FS_DM | |
| USB D+ | PA12 | USB_OTG_FS_DP | |
| SWDIO | PA13 | SWD | debug header |
| SWCLK | PA14 | SWD | debug header |
| Buzzer | PC5 | GPIO → 2N7002 + D8 (1N914) flyback + R20 10k | active buzzer |
| Status LED | PC13 | GPIO → R19 330Ω | see WARNING below |
| Crystal | PH0 / PH1 | RCC_OSC_IN/OUT | 8 MHz HSE |
| BOOT0 | 60 | boot strap | |
| BOOT1 | PB2 | boot strap | |
| NRST | 7 | reset | |
| VCAP_1 / VCAP_2 | 31 / 47 | C12 / C13, 2.2µF each | core reg |

## IMU — BMI270, not ICM-42605 (changed 2026-07-30)

The gyro is a **Bosch BMI270** (U7, LCSC C2836813, footprint
`drone_lib:LGA-14_L3.0-W2.5-P0.50-BR`). The 426xx family went reel-only / OOS in
single quantities. The **STM32 pin map above did not change** — only the chip and
its footprint. Pad-to-net wiring verified against BMI270 datasheet rev 1.3
Table 22 (p.145):

| Pad | Name | Net | Datasheet (SPI 4-wire) |
|---|---|---|---|
| 1 | SDO | `MISO` → PA6 | SDO ✓ |
| 2 / 3 | ASDx / ASCx | `+3V3` (= VDDIO) | VDDIO ✓ — must **not** go to GND |
| 4 | INT1 | `EXTI3` → PC3 | INT1 ✓ |
| 5 | VDDIO | `+3V3` (main AP2112K rail) | ✓ |
| 6 / 7 | GNDIO / GND | `GND` | ✓ |
| 8 | VDD | `+3V3_IMU` (quiet TLV733P rail, **via JP10**) | ✓ |
| 9 / 10 / 11 | INT2 / OCSB / OSDO | unconnected | DNC ✓ |
| 12 | CSB | `IMU_nCS` → PC4, R3 10k → +3V3 | CSB ✓ |
| 13 | SCx | `SPI1_SCK` → PA5 | SCK ✓ |
| 14 | SDx | `MOSI` → PA7 | SDI ✓ |

Decoupling is C14 100nF (VDD) + C15 100nF (VDDIO) — correct for the BMI270, and
deliberately **not** the 2.2µF/0.1µF/10nF arrangement the 426xx required.

Firmware consequences: driver = `USE_GYRO_SPI_BMI270` / `USE_ACC_SPI_BMI270`;
gyro ODR 3.2 kHz in Betaflight (chip max 6.4 kHz) so the **PID loop ceiling is
3.2 kHz, not 8 kHz**; SPI clock ceiling 10 MHz (was 24 MHz); the driver uploads an
~8 KB init blob at every boot; the chip boots in I2C mode and needs a CSB rising
edge to enter SPI (R3 holds CSB high through POR, BF's driver toggles CS).

## Blackbox flash — 2 MB, not 16 MB

U3 as fitted is a **GigaDevice GD25Q16E (C2904431), 16 Mbit = 2 MB**. The BOM
specified a 128 Mbit part; the wrong one was ordered. Keeping it.

No firmware change needed: `USE_FLASH_W25Q128FV` only gates Betaflight's generic
`m25p16` driver (via `common_post.h`), and that driver detects by JEDEC ID at
runtime. Our chip is in its table as `{ 0xC84015, 104, 50, 32, 256 }` =
32 sectors × 256 pages × 256 B = 2,097,152 bytes. **Configurator reporting 2 MB
is correct, not a fault.**

Cost is log duration: at ~30 KB/s per kHz of logging rate, 3.2 kHz fills it in
~22 s. **Set `blackbox_sample_rate = 1/4`** (800 Hz, ~87 s) at bring-up. Full
table and field-disable advice in `../betaflight_target/BUILD.md`.

## Unresolved / confirm at bring-up

- **PA1 (ADC1_IN2 region / ADC1_IN1):** CubeMX allocated PA1 as an ADC input
  (intended battery-voltage sense), but the schematic view shows it at/near
  no-connect. **Battery VOLTAGE sensing may not exist on this board.** Confirm
  whether a divider actually lands on PA1. If not: Betaflight gets current but no
  pack voltage (no voltage sag/alarms). Current sense (PA2) is confirmed present.
- **Status LED on PC13 — WARNING:** PC13 is in the F405 backup/power-switch
  domain and can only sink ~3 mA (DS8626), so the LED will be dim. It works, but
  it's not a normal GPIO. PB8 (the pin the old docs called for) is **not
  connected** on this board.
- **PA8 (pin 41):** broken out to a pad, labelled only `PA8`, no assigned
  function. Spare/test point. (CubeMX had it as EXTI8; hardware = spare.)
- **PB3 (pin 55):** broken out, labelled `PB3`. Spare/test point.
- **JP10 (`SolderJumper_2_Open`)** sits between the TLV733P output and the
  `+3V3_IMU` net, so it ships **open** and the BMI270's VDD is unpowered until
  it's solder-bridged. Bridge it before first power-up. Symptom if missed: board
  enumerates over USB, "no gyro detected."
- **`GYRO_1_ALIGN`** in `config.h` is a placeholder guessed for the old ICM-42605.
  The BMI270 has different die axes and a different footprint rotation — re-derive
  it on the bench, don't assume it carried over.

## Not connected (spare)

PA0, PA1(see above), PA4, PA15, PC0, PC1, PC6–PC12, PC14, PC15, PB4–PB9, PB11,
PB14, PD2.

## Firmware mapping consequences

- Motors → `MOTOR1..4_PIN = PB0, PB1, PA3, PB10`. PA3+PB10 share TIM2 (different
  channels); PB0+PB1 share TIM3. Confirm DShot DMA with `dma show all` at bring-up.
- `ADC_CURR_PIN PA2` confirmed. `ADC_VBAT_PIN PA1` unconfirmed (may be absent).
- `LED0_PIN PC13` (not PB8).
- Physical motor S1–S4 order is set in Configurator after the spin test, not here.
