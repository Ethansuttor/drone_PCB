# ETHANF405 — build, flash, and bring-up

Everything you need to turn `ETHANF405/config.h` into a `.hex` and get your
board flying.

**Revised 2026-08-19, after the procedure was actually executed end to end.**
The earlier version of this file was written from theory and got several things
wrong. Every command below has been run on this project's hardware.

The board isn't in Configurator's dropdown (it's custom), so you can't
"download firmware" for it — you compile your own `.hex` from the config in
this folder, then flash that file with **Load Firmware [Local]**.

---

## 0. Read this before you start — the four things that cost hours

1. **Build 4.5.5, not `master`.** Betaflight Configurator **10.10.0** supports
   firmware **4.5.x** and flatly refuses anything newer ("The configurator
   version used does not support firmware 26.12.0"). `master` currently builds
   `2026.12.0-alpha`, which will flash fine and then be rejected on connect.
2. **The gyro define is `USE_ACCGYRO_BMI270`.** Not the split
   `USE_GYRO_SPI_BMI270` / `USE_ACC_SPI_BMI270` pair. Already fixed in
   `config.h`; noted here because the failure mode looks catastrophic.
3. **`ADC1_DMA_OPT 1` must stay in `config.h`.** Without it VBAT and current
   read a permanent, silent zero. See §6.
4. **The blackbox flash needs a source patch that is not upstream.** The fitted
   GD25Q16E's JEDEC ID `0xC84015` is missing from `m25p16FlashConfig[]` in
   4.5.5, so the chip answers and the driver discards it. Apply
   `flash_probe_debug.patch` after every `make clean`, branch switch or fresh
   clone, or the flash silently stops enumerating. See §7 and §2.3.

---

## 1. Set up the build environment (one time)

> **`wsl` opens `docker-desktop`, not Ubuntu.** Docker Desktop installs itself as
> the default WSL distro, so a bare `wsl` drops you into a BusyBox shell as root
> with an empty home and `-sh:` errors, which looks exactly like a missing source
> tree. Use **`wsl -d Ubuntu`**, or `wsl --set-default Ubuntu` to fix it for good.
> `wsl -l -v` lists what you actually have.


Betaflight builds cleanly on Linux. On Windows use **WSL2** — a real Ubuntu
inside Windows. It is a lightweight VM: no separate window to manage, boots in
about a second, shares your filesystem, removable with `wsl --uninstall`.
Budget ~3 GB of disk.

### 1.1 Windows only — install WSL

In an **Administrator** PowerShell (the folder you run it from does not
matter):

```powershell
wsl --install
```

Reboot if asked, let Ubuntu finish setting up, pick a username/password.

> **Then open "Ubuntu" from the Start menu**, not PowerShell. Launching WSL out
> of an admin PowerShell inherits `C:\WINDOWS\system32` as the working
> directory, and `git clone` there fails with
> `chmod on .../.git/config.lock failed: Operation not permitted`.
> If that happens: `cd ~` and start again.

### 1.2 Install the tools

```bash
sudo apt update
sudo apt install -y git make curl build-essential
```

`build-essential` is not optional — without it the Makefile prints
`/bin/sh: 1: gcc: not found` on every invocation. You do **not** need to
install an ARM compiler yourself; Betaflight fetches its own.

---

## 2. Get the source and build

### 2.1 Clone — into your home directory

```bash
cd ~
git clone https://github.com/betaflight/betaflight.git
cd betaflight
```

> **Do not clone into `/mnt/d` or any Windows path.** Building across the
> `/mnt/` translation layer is dramatically slower — thousands of small
> compiler reads is the worst case for it. Clone and build in the Linux home,
> and reach across to `/mnt/d/` only to copy the config in and the hex out.

### 2.2 Check out 4.5.5

```bash
git checkout 4.5.5
```

("detached HEAD" is expected and fine.) Use `4.5-maintenance` instead if you
want the latest 4.5 fixes.

### 2.3 Toolchain and configs

```bash
make configs
make arm_sdk_install
```

`make arm_sdk_install` downloads ~140 MB. **Re-run it after switching
branches** — different Betaflight versions pin different toolchain versions,
and the symptom is:

```
make: arm-none-eabi-gcc: No such file or directory
mk/tools.mk:266: *** **ERROR** arm-none-eabi-gcc not in the PATH.
```

### 2.3b Apply the local source patch (after every clean or branch switch)

```bash
git apply /mnt/d/Drone/drone_PCB/firmware/betaflight_target/flash_probe_debug.patch
```

This adds the GD25Q16E entry to the flash chip table, which is **required** for
blackbox to work, plus the `flash_info debug` / `flash_info probe` bring-up
instrumentation behind `USE_FLASH_DEBUG_PROBE`. Check whether the table entry
has landed upstream before applying by hand:

```bash
grep -n C84015 src/main/drivers/flash_m25p16.c
```

Reverse it with `git apply -R` on the same file. The patch touches `src/main`
only, so it carries no submodule pointer and applies cleanly to a fresh 4.5.5
checkout.

### 2.4 Drop the config in and build

```bash
mkdir -p src/config/configs/ETHANF405
cp /mnt/d/Drone/drone_PCB/firmware/betaflight_target/ETHANF405/config.h \
   src/config/configs/ETHANF405/config.h

make ETHANF405
```

Your D: drive is at `/mnt/d/` inside Ubuntu.

Success looks like a memory-region table and:

```
Creating HEX ./obj/betaflight_4.5.5_STM32F405_ETHANF405.hex
Building target config ETHANF405 succeeded.
```

Expect roughly **53% of FLASH1 and 69% of RAM** used. Copy it somewhere
Windows can see:

```bash
cp obj/betaflight_4.5.5*ETHANF405.hex /mnt/d/Drone/drone_PCB/firmware/hex/
```

**Re-run only §2.4 after editing `config.h`.** After switching branches, re-run
§2.3 first, and `make clean` as well.

---

## 3. Flash the board

### 3.1 One-time: the USB driver

The STM32 ROM bootloader enumerates as `STM32 BOOTLOADER` (USB ID
**0483:DF11**) but Windows binds no driver to it — it lands under **Other
devices** with a yellow triangle, and Configurator shows no DFU port.

Download **Zadig** from <https://zadig.akeo.ie> (also linked on Configurator's
Welcome page). It is a standalone `.exe`, no installer.

1. Board plugged in with **BOOT0 shorted**
2. Right-click Zadig → **Run as administrator**
3. **Options → List All Devices**
4. Select **STM32 BOOTLOADER**, target driver **WinUSB**, **Install Driver**

**ImpulseRC Driver Fixer** does the same thing with no options if Zadig is
awkward.

> Zadig's WinUSB and ST's own STM32CubeProgrammer driver overwrite each other.
> Pick one toolchain. Betaflight needs WinUSB.

### 3.2 Flash

1. Short **BOOT0** (SW2 isn't fitted — short the pads), plug USB in. The port
   dropdown should read **DFU - STM32 BOOTLOADER**.
2. **Firmware Flasher** tab. Ignore **Choose a Board** and **Choose a Firmware
   version** entirely — those query `build.betaflight.com` for cloud builds and
   your target will never be there. The "Build server failure / 404" banner is
   irrelevant to you.
3. Tick **Full chip erase**. Mandatory when changing firmware version.
4. **Load Firmware [Local]** — bottom right, maximize the window if you can't
   see it — and pick your `.hex`. **Check the loaded-firmware bar names the
   version you meant**; old builds accumulate in that folder.
5. **Flash Firmware.** 30–60 s including verify.
6. **Remove the BOOT0 jumper**, replug. Leaving it shorted just re-enters DFU.

### 3.3 Connecting

Board enumerates as a COM port. Hit **Connect**.

**These two log lines are normal and permanent for this board:**

```
Configurator has detected the board: ETHANF405 but no official Betaflight target was found
Configurator failed to verify the board...
```

That's Configurator failing to find ETHANF405 in Betaflight's online database.
It never will. Nothing is wrong.

**`MultiWii API version: 1.48.0`** confirms 4.5.x — that's the number to check
if the version warning reappears.

---

## 4. Bring-up order (props off, ESC unplugged)

See `BRINGUP_CHECKLIST.md` for what each step actually returned on this board.

1. **Continuity-beep the ESC harness FIRST**, before any battery power: VBAT
   and both GND pads from the mated cable to the ESC XT60. A mirrored cable
   puts 16.8 V on a 3.3 V GPIO.
2. **USB first, battery later.** Bridge JP5, USB5V1, USB5V2, JP6 and run the
   whole digital bring-up on 5 V from USB before the pack ever goes near it.
3. **Bridge FB1** with a wire link — it was never ordered and it is the only
   VDDA supply path. Confirm with `status`: `Vref` should read ~3.3 V. If VDDA
   were dead you'd get nothing sane there.
4. **Bridge JP10** for the gyro. `+3V3_IMU` is dead until you do, and C4/C14
   sit on the far side of it. Symptom if missed: board enumerates fine, no gyro.
5. **Connect in Configurator**, land on **Setup**.
6. **Move the board** — the 3D model should follow. Then fix `GYRO_1_ALIGN`:
   the committed value is a placeholder guessed for the old ICM-42605. Iterate
   at runtime with `set gyro_1_sensor_align = CW90_DEG` etc. and `save` — no
   rebuild needed — then bake the winner into `config.h`.
7. **Ports tab:** enable **Serial RX** on UART1 (i-BUS).
8. **Receiver tab:** power the RX, move sticks, confirm channel mapping.
9. **Modes tab:** assign an **ARM** switch.
10. **Motors tab (props off):** protocol **DShot600**, spin each motor, fix
    direction and S1–S4 order here rather than in copper.
11. **Battery side last.** Current-limited bench supply, not a pack, the first
    time. Bridge JP2, verify `+5V_Buck` = 5.00 V, then JP3, then JP8 and
    CurrentM1.
12. **Calibrate:** accelerometer flat, then `vbat_scale`, then current meter
    scale by bench sweep.
13. **Set the blackbox log rate** — `set blackbox_sample_rate = 1/4`, `save`.
14. Failsafe, then — and only then — props on.

Optional after it flies: flash **Bluejay** onto the ESC via ESC-Configurator
for bidirectional-DShot RPM filtering (BLHeli_S has no serial telemetry).

---

## 5. CLI commands that matter for this board

```
status                      # rails, clock, Vref, sensors, voltage, arming flags
resource                    # confirms every pin mapping
timer                       # confirms motor timer/channel assignment
dma                         # confirms DMA streams per motor pin
dma ADC 1                   # ADC1's DMA assignment  <-- see §6
flash_info                  # blackbox chip JEDEC ID and geometry
get vbat                    # every vbat_* setting at once
get battery_meter           # must be ADC
get current_meter           # must be ADC
get adc                     # adc_device etc.
```

> `dma show all` is **not** valid — it returns `BAD DEVICE: show`. The command
> is plain `dma`.

`get <partial>` lists every setting containing that substring. Far quicker than
guessing exact names.

---

## 6. The ADC DMA collision — read this before debugging VBAT

**Symptom:** `status` reports a healthy `Vref=3.28V` and a live `Core temp`,
but `Voltage: 0 * 0.01V (0S battery - NOT PRESENT)` — with correct voltage on
the ADC pin, a verified divider, `battery_meter = ADC`, and the resource
mapped.

**Cause:** on the F405, ADC1's regular-conversion DMA defaults to **DMA2 Stream
0 Channel 0**. SPI1_RX — the gyro — defaults to **DMA2 Stream 0 Channel 3**.
Same stream. SPI1 is allocated first and wins, and `adc_stm32f4xx.c` then does:

```c
if (!dmaAllocate(dmaGetIdentifier(adc.dmaResource), OWNER_ADC, 0)) {
    return;
}
```

It gives up **silently**. No error, no boot warning. Every external ADC channel
reads a hard zero forever — VBAT *and* current.

**Why it's so well disguised:** Vref and core temperature are read through
**injected channels with software polling**, which need no DMA. So the ADC
looks perfectly healthy in `status` while the channels you care about are dead.

**Fix (permanent, already in `config.h`):**

```c
#define ADC1_DMA_OPT 1      // ADC1 -> DMA2 Stream 4, clear of SPI1
```

**Fix (runtime, does not survive a full-chip-erase reflash):**

```
dma ADC 1 1
save
```

This is also why the "verify timers and DMA at bring-up" note existed — but
checking the four motor streams isn't enough. **Check ADC against SPI1 too.**

---

## 7. Blackbox flash — REQUIRED source patch, and the fitted chip is 2 MB

**U3 is a GigaDevice GD25Q16E (LCSC C2904431) = 16 Mbit = 2 MB.** The BOM
called for a 128 Mbit part; the wrong one was ordered and kept.

**A firmware change IS needed, and this section got that wrong until
2026-09-14.** `USE_FLASH_W25Q128FV` in `config.h` is indeed only a gate that
pulls in Betaflight's generic `m25p16` driver via `common_post.h`, and that
driver does detect chips by JEDEC ID at runtime. But **ours was not in its
table.**

`m25p16FlashConfig[]` in `src/main/drivers/flash_m25p16.c` at the 4.5.5 tag has
twenty entries covering Macronix, Micron, Winbond, PUYA, Zbit, Cypress and
BergMicro, and not a single GigaDevice part. `0xC84015` is absent. The chip
answered `C8 40 15` correctly on every attempt from 2026-09-03 onward and the
driver threw the answer away. The earlier claim here that the entry existed is
what sent two days of diagnosis at the hardware.

**The fix, added 2026-09-14**, one entry placed before the BergMicro line:

```c
// GigaDevice GD25Q16E
// 32 sectors x 256 pages x 256 B = 2 MB. Same geometry as the Winbond
// W25Q16 entry above. 104 MHz fast read, 50 MHz for the standard 0x03 read.
{ 0xC84015, 104, 50, 32, 256 },
```

Geometry is copied from the `0xEF4015` W25Q16 entry directly above it, which is
the same 2 MB organisation, so it is a known-good shape and not a guess.

**This does not come from upstream.** It lives in
`firmware/betaflight_target/flash_probe_debug.patch` and must be re-applied after any
`make clean`, branch switch or fresh clone, or the flash silently stops
enumerating again. Check newer Betaflight before re-applying by hand, the entry
may have landed upstream since.

Configurator reporting **2 MB** is correct, not a fault.

**What you must do:** budget the log rate. Roughly 30 KB/s per kHz of logging
rate for a standard quad field set:

| `blackbox_sample_rate` | Rate @ 3.2 kHz PID | ~Bitrate | Time to fill 2 MB |
|---|---|---|---|
| `1/1` | 3.2 kHz | ~96 KB/s | **~22 s** — unusable |
| `1/2` | 1.6 kHz | ~48 KB/s | ~44 s |
| **`1/4`** | **800 Hz** | **~24 KB/s** | **~87 s** — use this |
| `1/8` | 400 Hz | ~12 KB/s | ~175 s |

800 Hz still resolves everything below 400 Hz, which covers the noise peaks
filter tuning depends on. To claw back more space, disable fields you have no
hardware for — no GPS, no mag, no baro, no RSSI pin. `get blackbox` lists the
exact setting names for your firmware version.

**Upgrade path:** GD25Q128E (C2758105) or BY25Q128ES (C22471255). Same
`m25p16` table, same SOIC-8 208mil pinout, hot-air swap, no firmware change.

> **U3 WORKS as of 2026-09-14 evening.** `flash_info` reports the chip and a
> FLASHFS partition. The hardware was never at fault. The missing table entry
> above was the entire problem.
>
> Proven on the wire by the debug probe, first boot on the correct bus:
>
> ```
> # flash_info debug
> stage: 4 TRANSACTED
> resolved pins: sck=B13 sdi=C02 sdo=B15 regbase=0x40003800
> spi regs: cr1=0x0357  spe=1 mstr=1 br=2
> sdi pin: moder=2 (alternate function) af=5 (SPI2 on F4)
> rdid: issued=1 raw=c8 40 15 c8
> ```
>
> **Two things this file previously said that are now known to be wrong:**
>
> - **"SPI2 does not sample PC2."** It did, correctly, the whole time. That
>   conclusion rested on `flash_info` returning `JEDEC ID=0x00000000`, but that
>   zero is `noFlashGeometry` printed when nothing was detected, not a reading
>   off the wire. `flash_info` never showed the raw bytes. Now it can, with
>   `flash_info debug`.
> - **"a transacting SPI2 sampling PC2 would have to return 0xFFFFFF."** True as
>   far as it goes, and the reason it would is worth knowing:
>   `SPI_IO_AF_SDI_CFG` in `bus_spi.h` carries `GPIO_PuPd_UP`, so
>   `spiInitDevice()` puts an internal pull-up on every SDI pin. An initialised
>   Betaflight bus with nothing answering reads `0xFF`, not `0x00`. Do not reason
>   from a driver-level "not detected" as though it were a wire measurement.
>
> **PC2 is a legal SPI2 MISO** and is correctly AF5-mapped at runtime. Verified
> in `bus_spi_pinconfig.c` at the 4.5.5 tag and confirmed live. `config.h` was
> right. Do not move MISO to PB14 on the next revision.
>
> **The 2026-09-03 "dead die" verdict is retracted**, and so is the 2026-09-13
> claim that the board itself died. See post mortem part 9 in
> `BRINGUP_CHECKLIST.md` for the full account.
>
> A healthy GD25Q16E returns JEDEC ID **`C8 40 15`**, verified against the
> GigaDevice datasheet and now observed.
>
> **Debug commands**, available while `USE_FLASH_DEBUG_PROBE` is defined in
> `config.h`:
>
> - `flash_info debug` prints the record the boot probe left. No bus activity.
> - `flash_info probe` re-runs `flashSpiInit()` now and prints a fresh record.
>
> `stage` names which of the three silent `return false` exits in
> `flashSpiInit()` was taken, so a future failure is one command to localise.
> Remove the define once logging is proven end to end. **Keep the table entry.**
>
> **Resource names:** Betaflight 4.5 uses `SPI_SDI` and `SPI_SDO`, not
> `SPI_MISO` and `SPI_MOSI`.

---

## 8. Gyro note — Bosch BMI270

Driver define is **`USE_ACCGYRO_BMI270`** (single combined define).

- **Max PID loop is 3.2 kHz**, not 8 kHz. Betaflight clocks the BMI270 at a
  3.2 kHz ODR (chip max 6.4 kHz). Confirmed on hardware: `GYRO rate: 3225`.
  Setting 8k in Configurator won't do what you expect.
- SPI clock ceiling 10 MHz (`BMI270_MAX_SPI_CLK_HZ`).
- **`GYRO_1_ALIGN` must be re-derived.** The committed value was guessed for
  the ICM-42605's die orientation; the BMI270 sits on a different footprint
  rotation (`-BR`). Treat it as unknown.

If the Setup tab reports **no gyro detected**, suspect SPI1 or an unbridged
**JP10** before you suspect the chip: the BMI270 driver uploads an ~8 KB init
blob over SPI at every boot, so any marginal link fails as a hard "not present"
rather than as noisy data. R3 (10k to +3V3) holds CSB high through POR, which
is what puts the chip in SPI mode at all.

---

## Quick reference — what's pinned in config.h

| Function | Pin(s) |
|---|---|
| Gyro/Acc | **Bosch BMI270**, SPI1 (PA5/PA6/PA7), CS PC4, INT PC3 |
| Flash (blackbox) | **GD25Q16E — 2 MB**, SPI2 (SCK PB13 / **MISO PC2** / MOSI PB15), CS PB12 |
| Motors 1–4 | PB0, PB1, PA3, PB10 (TIM3_CH3/CH4, TIM2_CH4/CH3) — **verified** |
| Receiver | UART1 (TX PA9 / RX PA10), i-BUS |
| Current ADC | **PA2** (ADC1_IN2), behind jumper `CurrentM1` |
| VBAT ADC | **PA1** (ADC1_IN1), 11:1 divider, behind jumper **JP8** — **verified** |
| LED / Beeper | **PC14** (via R19 330Ω, dim) / PC5 |
| ADC1 DMA | **DMA2 Stream 4** via `ADC1_DMA_OPT 1` — **not** the default |
| PA8, PB3 | spare test-point pads, no firmware function |

Full canonical pin table with net names: `../VERIFIED_PINOUT.md`.
