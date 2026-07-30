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
2. **Bridge JP10** — the solder jumper between the TLV733P output and the
   `+3V3_IMU` net. It's a `SolderJumper_2_Open` footprint, so it ships open and
   the IMU's VDD (BMI270 pin 8) is dead until you blob it. Symptom if missed:
   board enumerates fine, no gyro detected.
3. **Rail smoke test:** bench PSU, current-limited, no MCU load first — confirm
   5 V and both 3.3 V rails before trusting the board. Measure `+3V3_IMU`
   specifically, downstream of JP10.
4. **Connect in Configurator** (normal USB, no BOOT0). Land on the **Setup** tab.
5. **Move the board** — the 3D model should follow. That proves the IMU + SPI +
   firmware all work. Then fix `GYRO_1_ALIGN` in config.h: the committed value is
   a placeholder guessed for the old ICM-42605, so expect to change it.
6. **Ports tab:** enable **Serial RX** on UART1 (your i-BUS receiver).
7. **Receiver tab:** power the RX, move TX sticks, confirm channels map right.
8. **Modes tab:** assign an **ARM** switch.
9. **Motors tab (still props off):** set protocol **DShot600**, spin each motor
   with the sliders, fix direction/order here (not in copper). This is where the
   S1–S4 physical order gets sorted.
10. **CLI checks for this custom board:** run `resource`, `timer`, and
    `dma show all` — confirm all four motors have a DMA stream and nothing clashes
    with SPI1/SPI2, then `save`. (The `config.h` timer/DMA row for **PB10** is
    best-effort and flagged to verify here; PA3+PB10 share TIM2.)
11. **Calibrate:** accelerometer (board flat), then `vbat_scale` (~110 start) and
    the current-meter scale by bench sweep — both are unknown until measured.
12. **Set the blackbox log rate** — see the flash note below. In CLI:
    `set blackbox_sample_rate = 1/4` then `save`. Skipping this gives you ~22
    seconds of log per erase.
13. Failsafe, then — and only then — props on.

Optional after it flies: flash **Bluejay** onto the ESC via ESC-Configurator for
bidirectional-DShot RPM filtering (BLHeli_S has no serial telemetry).

---

## Quick reference — what's pinned in config.h (matches the CubeMX board pinout)

| Function | Pin(s) |
|---|---|
| Gyro/Acc | **Bosch BMI270**, SPI1 (PA5/PA6/PA7), CS PC4, INT PC3 |
| Flash (blackbox) | **GD25Q16E — 2 MB**, SPI2 (SCK PB13 / **MISO PC2** / MOSI PB15), CS PB12 |
| Motors 1–4 | PB0, PB1, **PA3, PB10** (TIM3_CH3/CH4, TIM2_CH4/CH3) |
| Receiver | UART1 (TX PA9 / RX PA10), i-BUS |
| Current ADC | **PA2** (ADC1_IN2) — confirmed (zener-clamped) |
| VBAT ADC | PA1 — **verify a divider exists**, may be no-connect |
| LED / Beeper | **PC13** (via 330Ω, dim — see note) / PC5 |
| PA8, PB3 | spare test-point pads, no firmware function |

Full canonical pin table with net names: `../VERIFIED_PINOUT.md`.

### Gyro note (changed 2026-07-30)

The IMU is the **Bosch BMI270**, not the ICM-42605 the earlier docs describe —
the TDK 426xx family went reel-only in single quantities. Driver defines are
`USE_GYRO_SPI_BMI270` / `USE_ACC_SPI_BMI270`. There is no pin-compatible
alternate part for this footprint; the old "swap to ICM-42688-P" note no longer
applies.

Two things this changes at bring-up:

- **Max PID loop is 3.2 kHz**, not 8 kHz. Betaflight clocks the BMI270 gyro at a
  3.2 kHz ODR (chip max 6.4 kHz). Setting 8k in Configurator won't do what you
  expect.
- **`GYRO_1_ALIGN` must be re-derived** at step 4 below. The value in `config.h`
  was guessed for the 42605's die orientation and the BMI270 sits on a different
  footprint rotation, so treat it as unknown.

If the Setup tab reports **no gyro detected**, suspect the SPI1 connection or an
unbridged **JP10** before you suspect the chip: the BMI270 driver uploads an
~8 KB init blob over SPI at boot, so any marginal link fails as a hard
"not present" rather than as noisy data.

### Blackbox flash note — the fitted chip is 2 MB, not 16 MB

**U3 is a GigaDevice GD25Q16E (LCSC C2904431) = 16 Mbit = 2 MB.** The BOM called
for a 128 Mbit part; the wrong one was ordered. It's being kept.

**No firmware change is needed.** `USE_FLASH_W25Q128FV` in `config.h` is only a
gate that pulls in Betaflight's generic `m25p16` driver (via `common_post.h`),
and that driver detects chips by JEDEC ID at runtime. Our chip is in its table:

```c
// GigaDevice GD25Q16E
{ 0xC84015, 104, 50, 32, 256 },   // 32 sectors x 256 pages x 256 B = 2 MB
```

Configurator will report **2 MB** on the Blackbox tab. That is correct, not a
fault. Don't chase it.

**What you must do:** budget the log rate. Roughly 30 KB/s per kHz of logging
rate for a standard quad field set:

| `blackbox_sample_rate` | Log rate @ 3.2 kHz PID | ~Bitrate | Time to fill 2 MB |
|---|---|---|---|
| `1/1` | 3.2 kHz | ~96 KB/s | **~22 s** — unusable |
| `1/2` | 1.6 kHz | ~48 KB/s | ~44 s |
| **`1/4`** | **800 Hz** | **~24 KB/s** | **~87 s** — use this |
| `1/8` | 400 Hz | ~12 KB/s | ~175 s |

800 Hz logging still resolves everything below 400 Hz, which covers the motor
and frame noise peaks that filter tuning actually depends on. Tuning runs are
30–60 s regardless; the practical loss is that you can't log a whole pack.

To claw back more space, disable fields you have no hardware for — this board
has no GPS, no mag, no baro, and no RSSI pin. Run `get blackbox` in the CLI to
list the exact setting names for your firmware version (they're a bitmask
exposed as individual `blackbox_disable_*` booleans), then turn off the ones
that apply. Leave `debug` on only while you're actually reading a debug mode.

**Upgrade path** if 90 seconds stops being enough: GD25Q128E (C2758105) or
BY25Q128ES (C22471255). Both are in the same `m25p16` JEDEC table, both are
SOIC-8 208mil with identical pinout, so it's a hot-air swap with no firmware
change.
