# IRIS: Custom STM32F405 Flight Controller PCB

![IRIS V2 render, top view](images/iris-v2-render.png)

This repo holds the KiCad projects, production files, and tools for my custom flight controller: an STM32F405 board running Betaflight on a custom target I wrote. I built it to find out whether I could take an embedded system from a blank schematic all the way to something that actually flies.

There are two board revisions in this repo:

- **V1** (`Drone_PCB_FC/`): an 80 x 80 mm board. Built, brought up, and mounted in a printed 5-inch frame. All four motors spin, but it flips on every liftoff, and finding out why is the open problem.
- **V2, "IRIS"** (`Drone_PCB_FC_V2/`): a 55 x 55 mm board with a 16 MB blackbox flash, a barometer, and more connectors for peripherals. Ordered from JLCPCB on 2026-11-02.

## V2 (IRIS)

Four copper layers, 55 x 55 mm, assembled from JLCPCB with parts from LCSC.

| Function | Part |
|---|---|
| MCU | STM32F405RGT6, 168 MHz Cortex-M4F, LQFP-64, 8 MHz crystal |
| IMU | Bosch BMI270, LGA-14 |
| Barometer | SPA06-003, LGA-8 |
| Blackbox | Winbond W25Q128JVSIQ, 16 MB SPI NOR, SOIC-8 |
| Buck | TPS5450DDAR with a 15 µH inductor, SS54 Schottky diodes |
| LDO | AP2112K-3.3 for the 3.3 V rail |
| USB | USB-C receptacle with USBLC6-2SC6 ESD protection |
| Other | Buzzer (BZ1), test points TP1 to TP5 |

Connectors:

- CN1: 10-pin JST SM10B for the ESC
- J1: 8-pin JST SH
- J2: USB-C
- J3: 1 x 4 header for the receiver
- J4: 6-pin JST GH for GPS and compass
- J5: 4-pin JST SH for optical flow
- J6: 4-pin JST GH, spare I2C
- J7: 5-pin header for SWD, with NRST, GND, CLK, IO, and 3V3 labelled on the silkscreen

### What changed from V1

- **Blackbox:** the 2 MB GD25Q16E is replaced by a 16 MB W25Q128JVSIQ, which is eight times the log capacity. The chip stays in the same SOIC-8 footprint family.
- **Barometer:** added (SPA06-003).
- **Power:** one AP2112K-3.3 now supplies the 3.3 V rail. V1 had a second quiet LDO for the IMU, and V2 drops it.
- **Connectors and silkscreen:** GPS and compass, optical flow, spare I2C, SWD, camera (CAM), and video (TV) labels.
- **Size:** 80 x 80 mm down to 55 x 55 mm.

### Files

- `Drone_PCB_FC_V2/`: the live KiCad project.
- `Drone_PCB_FC_V2/production/AS_ORDERED_2026-11-02/`: the exact project and Gerbers that were ordered. Use this folder when a question is about the copper on the boards that arrive. Do not edit it.
- `Drone_PCB_FC_V2/production/`: BOMs, placement positions, and order CSVs.

### Status

Ordered 2026-11-02. Bring-up notes will go here as they happen.

## V1

### Why it's built this way

The board mates with a Flycolor Raptor BLS-04 4-in-1 ESC I already owned, using its existing 10-pin harness. The BLS-04 has no BEC. It hands you raw pack voltage and nothing else, so the board needs its own buck converter running off 16.8 V fully charged (14.8 V nominal).

The board is oversized on purpose. At 80 x 80 mm, with a standard 30.5 mm mounting pattern, it overhangs the stack. Everything is on the top layer of a four-layer stackup and hand placed for a hotplate reflow pass, so the extra room was worth more to me than the elegance.

### What went wrong

- **The IMU disappeared mid-design.** The ICM-42605 went reel-only at LCSC and out of stock at DigiKey and Mouser. I switched to a Bosch BMI270, which shares the 2.5 x 3.0 mm LGA-14 outline and nothing else. Splitting the 3.3 V rail earlier kept VDD on a quiet LDO, so the swap cost a footprint and a re-route instead of a power redesign. The BMI270 ships uncalibrated and caps the PID loop at 3.2 kHz instead of 8 kHz, which matters little for Acro freestyle.
- **The flash chip was the wrong size.** The BOM called for a 128 Mbit part, and a 16 Mbit (2 MB) part arrived. Betaflight identifies SPI NOR by JEDEC ID at runtime, and the GD25Q16E's ID (0xC84015) was missing from Betaflight 4.5.5's device table. The chip answered every read correctly and the driver discarded the replies. I blamed the chip, then the solder joints, then the STM32's SPI peripheral. The fix was one table entry in the target patch. The lesson: don't reason about what a bus returned until you're looking at the raw bytes.
- **A DMA collision moved a motor output.** Motor 4 originally sat on PB7, which wanted DMA1 Stream 3, the same stream SPI2_RX uses for the blackbox flash. The final mapping is M1 = PB0 and M2 = PB1 on TIM3, and M3 = PA3 and M4 = PB10 on TIM2, checked against the RM0090 DMA request table. I'll confirm it with `dma show all` at bring-up.

### Bring-up

Four normally-open solder jumpers split the power tree: buck output to the 5 V rail, 5 V to each LDO, and the quiet 3.3 V rail to the IMU. Each stage comes up on a current-limited bench supply before the next jumper closes. Test points sit on every rail and on both ADC sense lines. Before anything is powered, the ESC cable gets a continuity check on VBAT and both grounds against the ESC's XT60, because the schematic numbers that connector in the reverse order of the manufacturer's diagram.

### Where it's at

Brought up through the jumpers on 2026-08-19. Rails, USB, gyro, receiver, and both sense lines came up. The only failure was the blackbox flash, which was the firmware problem above. The board is mounted on the BLS-04 with EMAX 2205 motors and a 4S pack, and it flips on liftoff every time.

Next: verify motor mapping and spin direction, set up failsafe, then log a flight at a 1/4 blackbox sample rate and diagnose the flip from data.
