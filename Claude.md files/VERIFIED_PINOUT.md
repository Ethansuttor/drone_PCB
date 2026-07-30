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
| Gyro SCK | PA5 | SPI1_SCK | ICM-42605 |
| Gyro MISO | PA6 | SPI1_MISO | |
| Gyro MOSI | PA7 | SPI1_MOSI | |
| Gyro CS | PC4 | GPIO (net `IMU_nCS`) | |
| Gyro INT | PC3 | EXTI3 (net `IMU_EXTI3`) | data-ready |
| Flash SCK | PB13 | SPI2_SCK | W25Q128JV |
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

## Not connected (spare)

PA0, PA1(see above), PA4, PA15, PC0, PC1, PC6–PC12, PC14, PC15, PB4–PB9, PB11,
PB14, PD2.

## Firmware mapping consequences

- Motors → `MOTOR1..4_PIN = PB0, PB1, PA3, PB10`. PA3+PB10 share TIM2 (different
  channels); PB0+PB1 share TIM3. Confirm DShot DMA with `dma show all` at bring-up.
- `ADC_CURR_PIN PA2` confirmed. `ADC_VBAT_PIN PA1` unconfirmed (may be absent).
- `LED0_PIN PC13` (not PB8).
- Physical motor S1–S4 order is set in Configurator after the spin test, not here.
