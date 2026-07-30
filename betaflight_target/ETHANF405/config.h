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
 * IMU / gyro — Bosch BMI270 on SPI1 (U7, LCSC C2836813).
 *
 * CHANGED 2026-07-30: was ICM-42605. The whole TDK 426xx family went reel-only
 * / OOS in single quantities, so the board was re-spun onto the BMI270. It is
 * NOT a drop-in — same 2.5x3.0mm LGA-14 outline, completely different pinout —
 * but that was absorbed in the schematic/PCB (footprint ...-BR, every IMU net
 * re-routed). Wiring verified against BMI270 datasheet rev 1.3 Table 22 (p.145).
 * Nothing below this comment changes for the pin map; only the driver.
 *
 * Behavioural differences vs the 42605, all handled by the BF driver but worth
 * knowing at bring-up:
 *   - Gyro ODR is 3.2 kHz in Betaflight (chip max 6.4 kHz), NOT 8 kHz. The PID
 *     loop ceiling is 3.2k. Don't try to force 8k in Configurator.
 *   - SPI clock ceiling 10 MHz (BMI270_MAX_SPI_CLK_HZ) vs 24 MHz on the 42605.
 *   - The driver uploads an ~8 KB init blob over SPI at every boot. A marginal
 *     SPI1 connection shows up as "no gyro detected", not as noise.
 *   - The chip powers up in I2C mode and only switches to SPI on a CSB rising
 *     edge. R3 (10k to +3V3) holds CSB high through POR; the BF driver toggles
 *     CS to make the transition. Relevant only if you ever drive it from bare
 *     STM32 code.
 *   - The BMI270 ships uncalibrated (this is why BF discourages it for new
 *     designs). Irrelevant in Acro; costs you a few percent attitude accuracy
 *     in Angle/Horizon and on level trim.
 * ------------------------------------------------------------------------- */
#define USE_GYRO
#define USE_ACC
#define USE_GYRO_SPI_BMI270
#define USE_ACC_SPI_BMI270

/* ---------------------------------------------------------------------------
 * Blackbox flash — SPI2.
 *
 * FITTED PART IS **GigaDevice GD25Q16E (LCSC C2904431) — 16 Mbit = 2 MB**, not
 * the 128 Mbit part the BOM called for. Ordered by mistake; kept deliberately.
 *
 * The define below looks wrong but IS correct. Betaflight's flash detection is
 * JEDEC-ID based at runtime, and `USE_FLASH_W25Q128FV` is only a gate that
 * pulls in the generic m25p16 driver via common_post.h:
 *
 *     #if (defined(USE_FLASH_W25M512) || defined(USE_FLASH_W25Q128FV) \
 *          || defined(USE_FLASH_PY25Q128HA)) && !defined(USE_FLASH_M25P16)
 *     #define USE_FLASH_M25P16
 *     #endif
 *
 * That driver's device table contains our chip:
 *     // GigaDevice GD25Q16E
 *     { 0xC84015, 104, 50, 32, 256 },   // 32 sectors x 256 pages x 256 B = 2 MB
 * so it enumerates and works with no code change. Do NOT "fix" this to some
 * GD25Q16 define — no such define exists.
 *
 * CONSEQUENCE: 2 MB, i.e. 1/8th the intended log space. Rule of thumb is
 * ~30 KB/s of log per kHz of logging rate for a standard quad field set:
 *
 *     3.2 kHz (1/1)  ~96 KB/s  ->  ~22 s   <- unusable
 *     1.6 kHz (1/2)  ~48 KB/s  ->  ~44 s
 *     800 Hz  (1/4)  ~24 KB/s  ->  ~87 s   <- recommended
 *
 * So set `blackbox_sample_rate = 1/4` at bring-up (see BUILD.md). 800 Hz still
 * resolves everything below 400 Hz, which covers the noise peaks that matter
 * for filter and PID tuning. Tuning runs are 30-60 s anyway; this board just
 * can't log a whole pack. Upgrade path if that ever bites: GD25Q128E
 * (C2758105) or BY25Q128ES (C22471255) — both in the same m25p16 table, both
 * identical SOIC-8 208mil pinout, hot-air swap.
 * ------------------------------------------------------------------------- */
#define USE_FLASH
#define USE_FLASH_W25Q128FV     // gate for the m25p16 driver; actual chip is GD25Q16E, 2 MB

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
 * >>> VERIFY at bring-up: watch the Setup-tab 3D model, adjust until it tracks.
 * This value is a PLACEHOLDER and is very likely wrong since the IMU changed:
 * the BMI270 has different die axes AND sits on a different footprint
 * orientation (-BR) than the 42605 this was originally guessed for. Re-derive
 * it on the bench, don't assume CW0 carried over. */
#define GYRO_1_ALIGN         CW0_DEG

#define DEFAULT_BLACKBOX_DEVICE      BLACKBOX_DEVICE_FLASH
#define DEFAULT_CURRENT_METER_SOURCE CURRENT_METER_ADC
#define DEFAULT_VOLTAGE_METER_SOURCE VOLTAGE_METER_ADC

/* Receiver defaults — i-BUS on UART1. Still enable "Serial RX" for UART1 in
 * Configurator > Ports at bring-up. */
#define DEFAULT_RX_FEATURE   FEATURE_RX_SERIAL
#define SERIALRX_PROVIDER    SERIALRX_IBUS
#define SERIALRX_UART        SERIAL_PORT_USART1
