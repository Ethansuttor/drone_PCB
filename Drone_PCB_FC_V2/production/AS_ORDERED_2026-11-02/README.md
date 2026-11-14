# v2 (IRIS) as ordered, 2026-11-02. Do not edit.

This folder is the KiCad project exactly as it was when the fab files were exported (KiCad auto backup `Drone_PCB_FC-2026-11-02_185627.zip`, saved 6:56 pm; Gerbers exported 6:59 pm). `Drone_PCB_FC_gerbers_as_ordered.zip` is a copy of the Gerber and drill set that was ordered.

Verified 2026-11-09: all 917 track segments in this board file appear in the ordered F.Cu / B.Cu Gerbers (848 shared with the live file plus 69 unique to this one).

The live project one folder up (`Drone_PCB_FC_V2/`) was tidied after the order (about 10:15 pm, traces moved 1 to 2 mm, Ethan). Same 91 footprints, same net on every pad, same 153 vias; 72 track segments differ. Use this folder when the question is about copper on the boards that arrive. Pins, nets and parts are the same in both.

Libraries are not copied here: the project uses `../../libs` paths from the original location, so open it for reading and measuring only.
