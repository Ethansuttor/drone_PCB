# Custom Drone Flight Controller PCB

## Overview
This project is a custom-designed, STM32-based flight controller for FPV drones, running Betaflight firmware. It is primarily built as a personal engineering challenge to design a complex embedded system from scratch and see it successfully operate in the real world.

Currently a work in progress (WIP), this board is designed as a bench/resume project. It focuses on core flight controller functionalities without video system peripherals (no VTX, camera, or OSD) to prioritize power architecture, IMU integration, and MCU logic.

## Why This Approach?
- **Component Availability and Cost Optimization:** Supply chain realities require flexibility. By utilizing a dual-LDO power architecture (two distinct 3.3V LDOs: AP2112K and TLV733P), I was able to strictly isolate the noisy logic rails from the sensitive IMU power rail. This architectural choice allowed me to confidently design around a more cost-effective and readily available IMU (the ICM-42605) instead of relying on scarce, premium alternatives, all while maintaining clean gyro performance.
- **Real-World Constraints:** I chose to design the flight controller to mate directly with a pre-existing ESC (the Flycolor Raptor BLS-04 4-in-1). This introduced unique constraints, such as the lack of an onboard BEC on the ESC, which required engineering an onboard power buck converter (TPS5430) directly from the raw LiPo voltage.
- **Industry Standard Hardware:** Utilizing the STM32F405 MCU provides native Betaflight support, which requires writing a custom Betaflight target configuration file—a highly relevant exercise in embedded systems development.
- **Self-Assembly:** Designed as an oversized (~60x60mm) bench board with single-sided SMD components to allow for complete self-reflow and hand assembly.

## Hardware Specifications

### MCU & Core Logic
- **Microcontroller:** STM32F405RGT6 (168MHz Cortex-M4 with FPU)
- **Firmware:** Betaflight (Custom target `ETHANF405`)
- **Blackbox:** W25Q128JVSIQ (16MB SPI NOR Flash)

### Sensors
- **IMU:** ICM-42605 (SPI1), chosen for availability, mounted dead-center for optimal flight dynamics.

### Power Architecture
- **Input:** 3–6S LiPo (up to 25.2V) passed through the ESC harness
- **Onboard Buck:** TPS5430DDAR stepping raw VBAT down to 5V/3A
- **Regulators:**
  - Main 3.3V LDO (AP2112K-3.3) for the MCU, flash, LED, logic, and IMU digital I/O (`VDDIO`).
  - Dedicated quiet 3.3V LDO (TLV733P-3.3) purely for the IMU sensor rail (`VDD`), ensuring minimal switching noise.

### Interfaces
- **ESC Connection:** 10-pin JST SH1.0 harness (mating to Flycolor Raptor BLS-04)
- **Receiver:** FlySky FS-iA6B over i-BUS (UART1)
- **USB:** USB-C for Betaflight configurator and DFU flashing
- **Expansion:** Spare UART3, USART6, I2C1 pads for future GPS/Mag integration

## Current Status (Work in Progress)
**Schematic complete (2026-07-11) — verified, entering PCB layout.**
- **Completed:** Full schematic — power architecture (TPS5430 buck + dual LDOs), IMU wiring/isolation, MCU core (decoupling, VCAP, VDDA filter, crystal, boot/reset), SPI2 flash, USB-C + ESD, ESC interface, current-sense clamp, VBAT divider, SWD header, power LED. Netlist verified pin-by-pin against the KiCad files.
- **Next Steps:**
  - Run ERC to zero; confirm ESC connector orientation with a continuity-beep test before power-up.
  - PCB layout and routing (solid L2 ground plane under IMU, tight buck switching loop away from analog/IMU, matched USB pair, decoupling at pins).
  - Order bare 4-layer boards + frameless stencil (JLCPCB); staged self-assembly and bring-up.

## Build Instructions
*(To be detailed upon board completion)*
1. **PCB Assembly:** Order bare 4-layer boards and a stencil. Apply leaded paste and use a hotplate for single-sided SMD reflow.
2. **Bring-up:** Perform rail smoke tests before connecting the MCU via SWD to flash the custom Betaflight target.
3. **Integration:** Connect the verified ESC harness, perform motor spin tests (props off), and calibrate ADC scales for voltage and current.
