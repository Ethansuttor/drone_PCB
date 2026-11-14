# routing_check

Routing review checks for a KiCad board file, written during the 2026-10-29 v2 routing review.

```
python routing_check.py ../../Drone_PCB_FC_V2/Drone_PCB_FC.kicad_pcb --png review.png --hl CURR_ADC,VBAT_ADC
```

Reports net lengths by layer, copper near the M3 mounting holes (anything within 1.4 mm of the hole edge sits under a nut or standoff), tracks near the TPS5450 switch node and under L3, and tight parallel runs between nets. `--png` draws F.Cu red, B.Cu blue and the `--hl` nets green.

Edit `SW_NETS` and `BODIES` at the top if the buck parts are renamed or moved. Run KiCad's own DRC as well (`kicad-cli pcb drc --refill-zones`); this script does not replace it. Run it on the saved file, not the autosave, unless you mean to check unsaved edits.
