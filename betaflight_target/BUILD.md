# ETHANF405 — build, flash, and bring-up

Everything you need to turn `ETHANF405/config.h` into a `.hex` and get your board
flying. Written assuming you've never built Betaflight before.

The board isn't in Configurator's dropdown (it's custom), so you can't "download
firmware" for it — you compile your own `.hex` from the config in this folder,
then flash that file. Do the build now, while you wait for parts, so bring-up day
is just flash-and-go.

---

## 1. Set up the build environment (one time)

Betaflight builds cleanly on Linux. On Windows the least-painful path is **WSL2**
(a real Ubuntu inside Windows). macOS and native Linux work the same way from
step 1.2.

### 1.1 Windows only — install WSL
In an **Administrator** PowerShell:
```powershell
wsl --install
```
Reboot if it asks, let it finish setting up Ubuntu, and pick a username/password.
From now on, run everything in the **Ubuntu** terminal, not PowerShell.

### 1.2 Install the basic tools
```bash
sudo apt update
sudo apt install -y git make curl
```
You do **not** need to hunt down an ARM compiler — Betaflight downloads the exact
one it wants into its own `tools/` folder on the first build.

---

## 2. Get the source and build (one time, then repeat on any config change)

```bash
# 2.1 clone Betaflight firmware
git clone https://github.com/betaflight/betaflight.git
cd betaflight

# 2.2 pull in the configs (creates src/config/…) and the ARM toolchain
make configs
make arm_sdk_install
```

Now drop your target in and build it:
```bash
# 2.3 copy your custom config into the config tree
mkdir -p src/config/configs/ETHANF405
cp /path/to/betaflight_target/ETHANF405/config.h src/config/configs/ETHANF405/config.h

# 2.4 build
make ETHANF405
```

If it succeeds you'll get a file like:
```
obj/betaflight_<version>_ETHANF405.hex
```
**That `.hex` is your firmware.** Copy it somewhere easy to find on the Windows
side (e.g. `cp obj/betaflight_*_ETHANF405.hex /mnt/d/Drone/drone_PCB/`).

> On Windows/WSL your D: drive is at `/mnt/d/` inside Ubuntu, so the config for
> this project is at `/mnt/d/Drone/drone_PCB/betaflight_target/ETHANF405/config.h`.

Re-run only step 2.4 whenever you edit `config.h`.

---

## 3. Flash the board (once it arrives)

1. Open Betaflight Configurator → **Firmware Flasher** tab.
2. Put the board in DFU: hold **BOOT0**, plug in USB, release. Windows shows a
   "STM32 BOOTLOADER" device. First time, run **Zadig** and set that device's
   driver to **WinUSB**, or Configurator won't see it.
3. Tick **Full chip erase** (first flash only).
4. Click **Load Firmware [Local]**, pick your `ETHANF405…hex`.
5. **Flash Firmware.** Wait for the verify to finish.

---

## 4. Bring-up order (props off, ESC unplugged)

Do these in order — each depends on the one before. Full detail lives in
`drone_fc_project_context_v3.md` §Build Order step 10; the short version:

1. **Continuity-beep the ESC harness FIRST** (before any power): VBAT + both GND
   pads from the mated cable to the ESC XT60. This is the mandatory check that
   the connector isn't mirrored — a reversed cable puts 16.8 V on a 3.3 V GPIO.
   (context v3, remaining item #1.)
2. **Rail smoke test:** bench PSU, current-limited, no MCU load first — confirm
   5 V and both 3.3 V rails before trusting the board.
3. **Connect in Configurator** (normal USB, no BOOT0). Land on the **Setup** tab.
4. **Move the board** — the 3D model should follow. That proves the IMU + SPI +
   firmware all work. Fix `GYRO_1_ALIGN` in config.h if the model faces wrong.
5. **Ports tab:** enable **Serial RX** on UART1 (your i-BUS receiver).
6. **Receiver tab:** power the RX, move TX sticks, confirm channels map right.
7. **Modes tab:** assign an **ARM** switch.
8. **Motors tab (still props off):** set protocol **DShot600**, spin each motor
   with the sliders, fix direction/order here (not in copper). This is where the
   S1–S4 physical order gets sorted.
9. **CLI checks for this custom board:** run `resource`, `timer`, and
   `dma show all` — confirm all four motors have a DMA stream and nothing clashes
   with SPI1/SPI2, then `save`. (The `config.h` timer/DMA rows for PB6/PB5 are
   best-effort and flagged to verify here.)
10. **Calibrate:** accelerometer (board flat), then `vbat_scale` (~110 start) and
    the current-meter scale by bench sweep — both are unknown until measured.
11. Failsafe, then — and only then — props on.

Optional after it flies: flash **Bluejay** onto the ESC via ESC-Configurator for
bidirectional-DShot RPM filtering (BLHeli_S has no serial telemetry).

---

## Quick reference — what's pinned in config.h (matches the CubeMX board pinout)

| Function | Pin(s) |
|---|---|
| Gyro/Acc | ICM-42605, SPI1 (PA5/PA6/PA7), CS PC4, INT PC3 |
| Flash (blackbox) | W25Q128JV, SPI2 (SCK PB13 / **MISO PC2** / MOSI PB15), CS PB12 |
| Motors 1–4 | PB0, PB1, **PA3, PB10** (TIM3_CH3/CH4, TIM2_CH4/CH3) |
| Receiver | UART1 (TX PA9 / RX PA10), i-BUS |
| Current ADC | **PA2** (ADC1_IN2) — confirmed (zener-clamped) |
| VBAT ADC | PA1 — **verify a divider exists**, may be no-connect |
| LED / Beeper | **PC13** (via 330Ω, dim — see note) / PC5 |
| PA8, PB3 | spare test-point pads, no firmware function |

Full canonical pin table with net names: `../VERIFIED_PINOUT.md`.

Swap the two gyro `#define`s to the ICM-42688-P pair if you populate that chip
instead.
