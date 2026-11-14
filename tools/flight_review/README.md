# flight_review

Decodes a Betaflight blackbox `.BBL` and writes the same metrics every time, so flights can be compared.

```
node tools/flight_review/flight_review.cjs "D:\Drone\Flight logs\BTFL_BLACKBOX_LOG_<stamp>_ETHANF405.BBL"
```

Options: `--out DIR` (default `D:\Drone\output\flight-<stamp>\`), `--bin 5` (seconds per table row), `--active-throttle 1100` (frames above this throttle command count as flying), `--compare DIR` (default: the newest earlier output folder made by the same tool version).

Output:
- `REPORT.md`: facts only, one section per recording in the file, plus a comparison of the newest recording with the previous run. Read this, not the CSVs.
- `summary.json`: the same numbers, machine readable.
- `logN.csv`: every decoded frame, for spreadsheets or one off questions.

What it reports per recording: firmware and log rate, duration, frame counts, corrupt frames, clean end and disarm reason, flight mode changes (names from Blackbox Explorer), failsafe phases, gyro RMS and gyro minus setpoint RMS per axis, motor means (percent vs the four motor average, and highest motor vs the mean of the others), time at 98 % output, accelerometer peak, a current sensor sanity check, and a per bin table.

Checks: tested 2026-10-23 against the hand made reviews. Log `20261020_021020` recording 6 matches the old review (15.42 s, 12665 frames, 1 corrupt, ANGLE at 3.53 s, 10 to 15 s gyro RMS 7.79 / 24.84 / 1.94). Log `20261021_012021` gives A/V 2.05 (sd 1.16 %) and M4 13.6 % over the others, as in DEVLOG 2026-10-21.

Units: gyro in deg/s as logged, VBAT `vbatLatest / 100`, current `amperageLatest / 100`, acceleration in g using the header `acc_1G`.

Extending: add a metric to `analyze()` and the report, and bump `TOOL_VERSION` if an existing number changes meaning (comparisons only run between runs of the same version). Do not write one off analysis scripts in output folders.

Parser: `vendor/` holds five unchanged files from Blackbox Explorer 2026.6.0 (GPL v3), with a small built in semver replacement, so no npm install is needed. Requires Node 18 or newer.
