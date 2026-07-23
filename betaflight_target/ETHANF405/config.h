/*
 * ETHANF405 — custom Betaflight target for the STM32F405RGT6 flight controller.
 *
 * PIN MAP MATCHES THE AS-ORDERED BOARD, taken from the STM32CubeMX report
 * "STM32 Program" (2026-07-23). Where CubeMX and the older schematic docs
 * disagreed, CubeMX (the board you actually ordered) wins.
 *
 * Build with the betaflight/config workflow: drop this at
 * config/configs/ETHANF405/config.h, then  make configs && make ETHANF405
 * See BUILD.md for the full procedure.
 *
 * Anything marked  >>> VERIFY  must be confirmed on the bench at bring-up.
 */

#pragma once

#define FC_TARGET_MCU     STM32F405

#define BOARD_NAME        ETHANF405
#define MANUFACTURER_ID   CUST          // local build only

/* ---------------------------------------------------------------------------
 * IMU / gyro — ICM-42605 populated (default BOM). SPI1. If you fit the
 * pin-compatible ICM-42688-P instead, swap the two driver pairs below.
 * ------------------------------------------------------------------------- */
#define USE_GYRO
#define USE_ACC
#define USE_GYRO_SPI_ICM42605
#define USE_ACC_SPI_ICM42605
// #define USE_GYRO_SPI_ICM42688P     // <- use these two instead if 42688-P fitted
// #define USE_ACC_SPI_ICM42688P

/* Blackbox flash — Winbond W25Q128JV on SPI2 (driver name is W25Q128FV) */
#define USE_FLASH
#define USE_FLASH_W25Q128FV

/* ---------------------------------------------------------------------------
 * Pin assignments — from the CubeMX pinout of the ordered board
 * ------------------------------------------------------------------------- */

/* Motors (CubeMX: PB0=TIM3_CH3, PB1=TIM3_CH4, PA3=TIM2_CH4, PB10=TIM2_CH3).
 * Betaflight motor 1..4 -> these pins; physical S1..S4 order is sorted in
 * Configurator after the spin test, no code change. Timer/DMA rows below. */
#define MOTOR1_PIN           PB0
#define MOTOR2_PIN           PB1
#define MOTOR3_PIN           PA3
#define MOTOR4_PIN           PB10

/* Gyro (SPI1) */
#define SPI1_SCK_PIN         PA5
#define SPI1_SDI_PIN         PA6        // MISO
#define SPI1_SDO_PIN         PA7        // MOSI
#define GYRO_1_SPI_INSTANCE  SPI1
#define GYRO_1_CS_PIN        PC4        // CubeMX GPIO_Output
#define GYRO_1_EXTI_PIN      PC3        // CubeMX GPIO_EXTI3

/* Blackbox flash (SPI2) — note MISO is on PC2 on this board, not PB14 */
#define SPI2_SCK_PIN         PB13
#define SPI2_SDI_PIN         PC2        // MISO  (CubeMX SPI2_MISO = PC2)
#define SPI2_SDO_PIN         PB15       // MOSI
#define FLASH_SPI_INSTANCE   SPI2
#define FLASH_CS_PIN         PB12       // CubeMX GPIO_Output

/* Receiver — FlySky FS-iA6B i-BUS on UART1 RX */
#define UART1_TX_PIN         PA9
#define UART1_RX_PIN         PA10

/* ADC — schematic-verified: CURRENT sense is on PA2 (net CurrentM1, D1 zener).
 * PA1 was allocated in CubeMX for battery-voltage sense, but the schematic
 * shows it at/near no-connect, so VBAT sensing may not exist on this board.
 * >>> VERIFY PA1: if there's no divider, delete ADC_VBAT_PIN — you'll have
 * current but no pack-voltage reading. */
#define ADC_CURR_PIN         PA2        // ADC1_IN2 — confirmed
#define ADC_VBAT_PIN         PA1        // >>> VERIFY: may be no-connect (no vbat sense)
#define ADC_INSTANCE         ADC1

/* Status LED — schematic shows the LED on PC13 (via R19 330Ω), NOT PB8 (PB8 is
 * unconnected on this board). WARNING: PC13 is in the F405 backup/power-switch
 * domain and can only sink ~3 mA (DS8626), so the LED will be dim but works. */
#define LED0_PIN             PC13

/* Beeper — PC5 GPIO driving a 2N7002 low-side switch + active buzzer (D8 1N914
 * flyback, R20 10k). Low-side FET => pin HIGH = buzzer ON = active-high, so
 * BEEPER_INVERTED is intentionally NOT defined. >>> VERIFY at bring-up. */
#define BEEPER_PIN           PC5
// #define BEEPER_INVERTED

/*
 * PA8 (pin 41) and PB3 (pin 55) are broken out to pads with no assigned
 * function on the schematic — spare/test points. Nothing to map in firmware.
 */

/* ---------------------------------------------------------------------------
 * Timer + DMA mapping for the motor outputs.
 *   PB0 / PB1 / PA3 rows are copied VERBATIM from OMNIBUSF4 (known-good DSHOT
 *   DMA). PB10 (TIM2_CH3) is unique to this board — best-effort, confirm below.
 *   >>> VERIFY at bring-up: in CLI run `resource`, `timer`, `dma show all`;
 *   confirm all four motors get a DMA stream with no clash against SPI1 (gyro)
 *   or SPI2 (flash), then `save`. Configurator's Motors/Timers/DMA tabs do the
 *   same with a GUI. Note PA3+PB10 share TIM2 (different channels) — fine for
 *   DShot, but that's exactly what `dma show all` verifies.
 * ------------------------------------------------------------------------- */
#define TIMER_PIN_MAPPING \
    TIMER_PIN_MAP( 0, PB0,  2, 0 ) \
    TIMER_PIN_MAP( 1, PB1,  2, 0 ) \
    TIMER_PIN_MAP( 2, PA3,  1, 1 ) \
    TIMER_PIN_MAP( 3, PB10, 1, 0 )

/* ---------------------------------------------------------------------------
 * Defaults
 * ------------------------------------------------------------------------- */
#define SYSTEM_HSE_MHZ       8

/* Gyro orientation — depends on how the chip faces vs the frame front.
 * >>> VERIFY at bring-up: watch the Setup-tab 3D model, adjust until it tracks. */
#define GYRO_1_ALIGN         CW0_DEG

#define DEFAULT_BLACKBOX_DEVICE      BLACKBOX_DEVICE_FLASH
#define DEFAULT_CURRENT_METER_SOURCE CURRENT_METER_ADC
#define DEFAULT_VOLTAGE_METER_SOURCE VOLTAGE_METER_ADC

/* Receiver defaults — i-BUS on UART1. Still enable "Serial RX" for UART1 in
 * Configurator > Ports at bring-up. */
#define DEFAULT_RX_FEATURE   FEATURE_RX_SERIAL
#define SERIALRX_PROVIDER    SERIALRX_IBUS
#define SERIALRX_UART        SERIAL_PORT_USART1
