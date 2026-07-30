# Prompt for Google Jules — Update README.md

Copy everything below the line into Jules.

---

## Task

Update `README.md` in this repository. It has gone stale: several key components changed during the design, and the project has moved from "schematic complete" to "boards ordered from JLCPCB." Rewrite the affected sections using the facts below.

**Preserve the existing structure, headings, and first-person engineering-narrative voice.** This is a portfolio/resume repo, so the writing should stay technical and specific — explain *why* decisions were made, not just *what* the parts are. Do not invent specifications that aren't listed here. Do not change sections that aren't mentioned.

## 1. Add a board render at the top

Immediately under the `# Custom Drone Flight Controller PCB` heading and before `## Overview`, insert:

```markdown
![Flight controller PCB, top view](images/pcb-top.png)
```

The image file will be added separately — just place the reference.

## 2. IMU changed: ICM-42605 → Bosch BMI270

**What changed:** The design now uses the **Bosch BMI270** (LCSC C2836813) instead of the ICM-42605.

**Why:** The entire TDK InvenSense 426xx family (ICM-42605, ICM-42688-P, IIM-42652) became unobtainable in single quantities — reel-only minimum orders of 1000+ at LCSC, and out of stock or backordered at DigiKey and Mouser. The BMI270 is stocked at quantity 1 and is a current-generation gyro used on the majority of new commercial flight controllers, with mature Betaflight support.

**Engineering impact worth writing up:** The BMI270 is *not* a drop-in. Although it shares the same 2.5×3mm 14-pin LGA package outline, its pinout is completely different, which required a new footprint and re-routing every IMU net. Its decoupling requirement is also simpler (100nF at VDD and 100nF at VDDIO, versus the 2.2µF/0.1µF/10nF arrangement the 426xx wanted).

**The narrative angle to emphasize:** The existing dual-LDO architecture absorbed this mid-design part swap without any change to the power design — VDD still sits on the dedicated quiet TLV733P rail and VDDIO still sits on the main AP2112K logic rail. This is a concrete payoff of the isolation decision described in "Why This Approach." Rewrite that first bullet so it reflects the BMI270 and makes this point: designing for rail isolation up front meant a forced sourcing change cost only a footprint and a re-route, not a power redesign.

**Also update:** Betaflight gyro define is now `BMI270`.

## 3. Blackbox flash changed: W25Q128JVSIQ → BOYAMICRO BY25Q128ESSIG

**What changed:** Blackbox flash is now the **BOYAMICRO BY25Q128ES** (LCSC C22471255), 16MB SPI NOR.

**Why:** The Winbond W25Q128JVSIQ hit a minimum order quantity of 12–14 units (~$25–30). The BY25Q128ES is explicitly named in Betaflight's supported flash list, and its SOIC-8 208-mil package matches the W25Q128 footprint and pinout exactly, making it a true drop-in at roughly $0.36. Backup part is the GigaDevice GD25Q128ESIG (C2758105), also Betaflight-supported.

## 4. Buck regulator changed: TPS5430DDAR → TPS5450DDAR

**What changed:** The onboard buck is now the **TPS5450DDAR** (LCSC C114425).

**Why:** The TPS5430DDAR (C9864) went out of stock. The TPS5450 is a pin-for-pin drop-in — same ESOP-8/DDA package, same pinout, same 1.221V feedback reference (so the existing 10k/3.24k divider still yields 5.0V), same 5.5–36V input range — with a 5A rating instead of 3A. All buck support passives are unchanged.

## 5. Battery changed: 3–6S → 4S

**What changed:** The design target is now a **4S LiPo (14.8V nominal, 16.8V full charge)**, not 3–6S up to 25.2V.

**Consequences to reflect:** the buck output inductor is 15µH (was 22µH), and input capacitor voltage ratings could relax from ≥50V to ≥25V (50V parts are still used for transient margin).

## 6. Update "Current Status"

Replace the entire "Current Status (Work in Progress)" section. The new status:

**Boards ordered from JLCPCB on 2026-07-19 — awaiting delivery (~2 weeks).**

- Schematic and PCB layout complete; **ERC and DRC both clean**.
- 4-layer stackup: L1 signal/components, L2 solid ground, L3 power pours, L4 signal. All SMD on the top layer for a single hotplate reflow pass.
- Ordered as bare 4-layer boards (ENIG finish, chosen for reliable soldering of the LGA gyro and fine-pitch parts) plus a frameless top-side stencil, quantity 5.
- Next step is assembly and staged bring-up once boards arrive.

## 7. Add a new section: "Design for Bring-Up"

Add a new section (place it after "Interfaces"). This is a differentiating detail for a portfolio repo — the board was deliberately designed to be debuggable:

- **Staged power isolation:** normally-open solder jumpers split the power tree at four points — buck 5V output to the rest of the 5V rail, 5V to each of the two LDOs, and the quiet 3.3V rail to the IMU. Each stage is brought up and verified on a current-limited bench supply before the next jumper is closed, so a fault in one stage cannot propagate downstream.
- **Test points** on every rail (VBAT, 5V, 3.3V, 3.3V_IMU) plus multiple grounds, the buck switch node and feedback node for scoping, and both ADC sense lines for calibration. Dedicated ground loops near the buck and IMU for short scope-ground connections.
- **Spare GPIO broken out:** all 16 unused STM32 pins are routed to a labelled header, including timer-capable pins for motor reassignment, a spare UART, spare ADC channels, and SWO — insurance against a pin-level mistake requiring a full respin.

## 8. Optional — add a "Notable Design Details" section

If it fits naturally, add a short section covering:

- **Motor output DMA conflict:** motor 4 was moved from PB7 to PB5 (TIM3_CH2) to clear a DMA1 Stream 3 collision with SPI2_RX (the blackbox flash). Final motor mapping is M1=PB0, M2=PB1, M3=PB6 (TIM4_CH1), M4=PB5 (TIM3_CH2), verified against RM0090's DMA request mapping table.
- **Buck output capacitor:** a Panasonic POSCAP 10TPB220M (220µF, 10V, 40mΩ ESR) — the exact part named in TI's datasheet worked example. A pure-ceramic output would have too little ESR and risks loop instability, since the TPS545x is internally compensated assuming some output-cap ESR.
- **ADC input protection:** the ESC current-sense line is clamped with a 1kΩ series resistor and a 3.3V zener to protect the STM32 ADC pin.

## 9. Update the "Build Instructions" section

Adjust step 1 to reflect that boards and stencil are already ordered, and fold the staged-jumper bring-up sequence into step 2.
