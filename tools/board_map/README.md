# board_map

Plots each footprint's courtyard (or pad extents if it has none) on the top and bottom of a `.kicad_pcb`, with the Edge.Cuts outline. Use it to find free areas for silkscreen, logos or new parts.

```
python tools/board_map/board_map.py Drone_PCB_FC_V2/Drone_PCB_FC.kicad_pcb output.png
```

Needs matplotlib. Rotated parts are drawn as rotated bounding boxes, so the map shows the board as fuller than it is (2026-10-30: it read the top as full, but an 11 mm logo fit in the bottom left corner). Traces and vias are not drawn.
