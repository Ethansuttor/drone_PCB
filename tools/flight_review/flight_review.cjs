#!/usr/bin/env node
// Blackbox flight review: decode a Betaflight .BBL and write fixed, comparable metrics.
// Usage: node tools/flight_review/flight_review.cjs <log.BBL> [--out DIR] [--bin SECONDS] [--active-throttle N] [--compare DIR]
// Output (default D:\Drone\output\flight-<stamp>\): REPORT.md (facts only), summary.json, logN.csv.
// Interpretation is not done here. The reviewer reads REPORT.md and applies .ai/LESSONS.md.
'use strict';
const fs = require('fs'), path = require('path'), vm = require('vm');
const TOOL_VERSION = 1;

// ---------- args
const argv = process.argv.slice(2);
if (!argv.length || argv.includes('-h') || argv.includes('--help')) {
  console.log(fs.readFileSync(__filename, 'utf8').split('\n').slice(1, 5).join('\n')); process.exit(argv.length ? 0 : 1);
}
const opt = { bin: 5, activeThrottle: 1100 };
let bbl = null;
for (let i = 0; i < argv.length; i++) {
  const a = argv[i];
  if (a === '--out') opt.out = argv[++i];
  else if (a === '--bin') opt.bin = +argv[++i];
  else if (a === '--active-throttle') opt.activeThrottle = +argv[++i];
  else if (a === '--compare') opt.compare = argv[++i];
  else bbl = a;
}
const full = fs.readFileSync(bbl);
const stamp = (path.basename(bbl).match(/(\d{8}_\d{6})/) || [null, new Date().toISOString().replace(/\D/g, '').slice(0, 14)])[1];
const outRoot = path.resolve(__dirname, '../../../output');
const out = path.resolve(opt.out || path.join(outRoot, 'flight-' + stamp));
fs.mkdirSync(out, { recursive: true });

// ---------- load the vendored Blackbox Explorer parser into a sandbox
const cmp = (a, b) => { const p = s => String(s).split(/[.-]/).map(n => parseInt(n, 10) || 0); const x = p(a), y = p(b);
  for (let i = 0; i < 3; i++) { if ((x[i] || 0) !== (y[i] || 0)) return (x[i] || 0) - (y[i] || 0); } return 0; };
const semver = { gte: (a, b) => cmp(a, b) >= 0, gt: (a, b) => cmp(a, b) > 0, lt: (a, b) => cmp(a, b) < 0, lte: (a, b) => cmp(a, b) <= 0, eq: (a, b) => cmp(a, b) === 0 };
const quiet = { log: (...a) => { if (!/^Ignoring unsupported header/.test(String(a[0]))) console.log(...a); }, warn: console.warn, error: console.error, info: () => {}, debug: () => {} };
const ctx = vm.createContext({ console: quiet, semver, FlightLogFieldPresenter: { adjustDebugDefsList() {} }, TextDecoder, TextEncoder });
for (const f of ['tools.js', 'flightlog_fielddefs.js', 'datastream.js', 'decoders.js', 'flightlog_parser.js']) {
  const code = fs.readFileSync(path.join(__dirname, 'vendor', f), 'utf8').replace(/^import[\s\S]*?;\s*/gm, '').replace(/\bexport /g, '');
  vm.runInContext(code, ctx, { filename: f });
}
const evName = Object.fromEntries(Object.entries(vm.runInContext('FlightLogEvent', ctx)).map(([k, v]) => [v, k]));
const disarmReasons = vm.runInContext('FLIGHT_LOG_DISARM_REASON', ctx);

// ---------- helpers
const r2 = v => v == null || !isFinite(v) ? null : Math.round(v * 100) / 100;
const mean = a => a.length ? a.reduce((s, v) => s + v, 0) / a.length : NaN;
const rms = a => a.length ? Math.sqrt(a.reduce((s, v) => s + v * v, 0) / a.length) : NaN;
const sd = a => { const m = mean(a); return Math.sqrt(mean(a.map(v => (v - m) ** 2))); };
const corr = (a, b) => { const ma = mean(a), mb = mean(b); let n = 0, da = 0, db = 0;
  for (let i = 0; i < a.length; i++) { n += (a[i] - ma) * (b[i] - mb); da += (a[i] - ma) ** 2; db += (b[i] - mb) ** 2; }
  return da && db ? n / Math.sqrt(da * db) : null; };
const modeNames = (mask, names) => names.filter((_, i) => (mask >>> i) & 1).join('+') || 'none';

// ---------- decode every recording in the file
const starts = []; let p = -1;
while ((p = full.indexOf('H Product:', p + 1)) >= 0) starts.push(p);
if (!starts.length) { console.error('No "H Product:" header found: not a Betaflight blackbox file?'); process.exit(2); }

const recs = [];
for (let n = 0; n < starts.length; n++) {
  ctx.data = full.subarray(starts[n], starts[n + 1] || full.length);
  vm.runInContext(`var parser = new FlightLogParser(data); parser.parseHeader();
    var rows = [], slow = [], events = [], invalid = 0;
    parser.onFrameReady = (ok, frame, type) => { if (!ok) { invalid++; return; }
      if (type === 'I' || type === 'P') rows.push(Array.from(frame));
      else if (type === 'S') slow.push({ time: rows.length ? rows[rows.length - 1][1] : null, frame: Array.from(frame) });
      else if (type === 'E') events.push({ event: frame.event, data: frame.data, time: rows.length ? rows[rows.length - 1][1] : null }); };
    parser.parseLogData(false);
    var modeNameList = Array.from(FLIGHT_LOG_FLIGHT_MODE_NAME);`, ctx);
  const P = ctx.parser, fields = P.frameDefs.I.name, sf = P.frameDefs.S ? P.frameDefs.S.name : [];
  recs.push({ n: n + 1, fields, slowFields: sf, sys: P.sysConfig, stats: P.stats, invalid: ctx.invalid,
    rows: ctx.rows, slow: ctx.slow, events: ctx.events, modeNameList: ctx.modeNameList });
  fs.writeFileSync(path.join(out, `log${n + 1}.csv`), [fields.join(','), ...ctx.rows.map(r => r.join(','))].join('\n'));
}

// ---------- analysis
function analyze(rec) {
  const ix = Object.fromEntries(rec.fields.map((f, i) => [f, i]));
  const col = f => ix[f] == null ? null : rec.rows.map(r => r[ix[f]]);
  const has = f => ix[f] != null;
  const rows = rec.rows; if (!rows.length) return { n: rec.n, empty: true };
  const t = col('time'), t0 = t[0], sec = t.map(v => (v - t0) / 1e6);
  const acc1G = rec.sys.acc_1G || 2048, motorMax = (rec.sys.motorOutput || [])[1];
  const nMot = [0, 1, 2, 3, 4, 5, 6, 7].filter(i => has(`motor[${i}]`)).length;
  const thr = col('rcCommand[3]');
  const gyro = [0, 1, 2].map(i => col(`gyroADC[${i}]`));
  const setp = [0, 1, 2].map(i => col(`setpoint[${i}]`));
  const mot = [...Array(nMot)].map((_, i) => col(`motor[${i}]`));
  const vbat = has('vbatLatest') ? col('vbatLatest').map(v => v / 100) : null;
  const amp = has('amperageLatest') ? col('amperageLatest').map(v => v / 100) : null;
  const accN = has('accSmooth[0]') ? rows.map(r => Math.hypot(r[ix['accSmooth[0]']], r[ix['accSmooth[1]']], r[ix['accSmooth[2]']]) / acc1G) : null;
  const active = thr ? thr.map(v => v > opt.activeThrottle) : rows.map(() => true);
  const pick = (a, mask) => a.filter((_, i) => mask[i]);

  // whole recording (active frames only for flight metrics)
  const nAct = active.filter(Boolean).length;
  const motMeans = mot.map(m => mean(pick(m, active)));
  const avgMot = mean(motMeans);
  const motorPct = motMeans.map(m => r2((m / avgMot - 1) * 100));
  const hi = motMeans.indexOf(Math.max(...motMeans));
  const others = motMeans.filter((_, i) => i !== hi);
  const highVsOthers = { motor: hi + 1, pct: r2((motMeans[hi] / mean(others) - 1) * 100) };
  const sat = motorMax ? mot.map(m => r2(100 * pick(m, active).filter(v => v >= 0.98 * motorMax).length / Math.max(nAct, 1))) : null;
  const motSum = rows.map((_, i) => mot.reduce((s, m) => s + m[i], 0));
  let currentCheck = null;
  if (amp && vbat && nAct > 10) {
    const a = pick(amp, active), v = pick(vbat, active), ms = pick(motSum, active);
    const ratio = a.map((x, i) => x / v[i]).filter(isFinite);
    currentCheck = { meanA: r2(mean(a)), corrWithMotorSum: r2(corr(a, ms)), corrWithVbat: r2(corr(a, v)),
      ampsPerVolt: r2(mean(ratio)), ampsPerVoltSdPct: r2(100 * sd(ratio) / mean(ratio)) };
  }
  let accPeak = null;
  if (accN) { let k = 0; accN.forEach((v, i) => { if (v > accN[k]) k = i; }); accPeak = { g: r2(accN[k]), atS: r2(sec[k]) }; }

  // bins
  const bins = [];
  for (let s = 0; s < sec[sec.length - 1]; s += opt.bin) {
    const m = sec.map(x => x >= s && x < s + opt.bin);
    const cnt = m.filter(Boolean).length; if (!cnt) continue;
    const ma = m.map((v, i) => v && active[i]), act = ma.filter(Boolean).length;
    bins.push({ fromS: s, toS: r2(Math.min(s + opt.bin, sec[sec.length - 1])), frames: cnt, activeFrames: act,
      throttle: thr ? { min: Math.min(...pick(thr, m)), max: Math.max(...pick(thr, m)), mean: r2(mean(pick(thr, m))) } : null,
      gyroRms: gyro.map(g => g && r2(rms(pick(g, m)))),
      errorRms: gyro.map((g, a) => g && setp[a] && r2(rms(pick(g.map((v, i) => v - setp[a][i]), m)))),
      motorMean: mot.map(x => r2(mean(pick(x, m)))),
      vbat: vbat && r2(mean(pick(vbat, m))), amps: amp && r2(mean(pick(amp, m))),
      accMaxG: accN && r2(Math.max(...pick(accN, m))) });
  }

  // modes, failsafe, events
  const sx = Object.fromEntries(rec.slowFields.map((f, i) => [f, i]));
  const modes = [], fs_ = []; let lastMode = null, lastFs = null;
  for (const s of rec.slow) {
    const tS = s.time == null ? 0 : r2((s.time - t0) / 1e6);
    if (sx.flightModeFlags != null && s.frame[sx.flightModeFlags] !== lastMode) {
      lastMode = s.frame[sx.flightModeFlags]; modes.push({ atS: tS, mask: lastMode, names: modeNames(lastMode, rec.modeNameList) }); }
    if (sx.failsafePhase != null && s.frame[sx.failsafePhase] !== lastFs) { lastFs = s.frame[sx.failsafePhase]; fs_.push({ atS: tS, phase: lastFs }); }
  }
  const events = rec.events.map(e => ({ atS: e.time == null ? null : r2((e.time - t0) / 1e6), type: evName[e.event] || e.event,
    detail: e.event === 15 && e.data ? (disarmReasons[e.data.reason] || e.data.reason) : undefined }));
  return {
    n: rec.n, durationS: r2(sec[sec.length - 1]), frames: rows.length, invalidFrames: rec.invalid,
    corruptFrames: rec.stats.totalCorruptFrames, activeFrames: nAct,
    endedCleanly: events.some(e => e.type === 'LOG_END'), disarmed: events.find(e => e.type === 'DISARM') || null,
    config: { firmware: `${rec.sys.firmwareType === 3 ? 'Betaflight' : 'fw' + rec.sys.firmwareType} ${rec.sys.firmwareVersion}`,
      craft: rec.sys['Craft name'] || null, looptimeUs: rec.sys.looptime, pidDenom: rec.sys.pid_process_denom,
      logRate: rec.sys.frameIntervalPDenom ? `${rec.sys.frameIntervalPNum || 1}/${rec.sys.frameIntervalPDenom}` : null,
      acc1G, motorOutput: rec.sys.motorOutput, vbatscale: rec.sys.vbatscale },
    flight: { gyroRms: gyro.map(g => g && r2(rms(pick(g, active)))),
      errorRms: gyro.map((g, a) => g && setp[a] && r2(rms(pick(g.map((v, i) => v - setp[a][i]), active)))),
      motorMean: motMeans.map(r2), motorPctVsAvg: motorPct, highVsOthers, motorSaturationPct: sat, accPeak, currentCheck },
    modes, failsafe: fs_, events, bins };
}

const results = recs.map(analyze);
const summary = { toolVersion: TOOL_VERSION, source: path.resolve(bbl), generated: new Date().toISOString(),
  options: { binS: opt.bin, activeThrottle: opt.activeThrottle }, recordings: results };
fs.writeFileSync(path.join(out, 'summary.json'), JSON.stringify(summary, null, 2));

// ---------- comparison with a previous run (same tool version only)
function findPrev() {
  if (opt.compare) return path.resolve(opt.compare);
  if (!fs.existsSync(outRoot)) return null;
  const c = fs.readdirSync(outRoot).filter(d => d.startsWith('flight-') && path.join(outRoot, d) !== out)
    .map(d => path.join(outRoot, d)).filter(d => { try { return JSON.parse(fs.readFileSync(path.join(d, 'summary.json'))).toolVersion === TOOL_VERSION; } catch { return false; } })
    .sort(); return c.length ? c[c.length - 1] : null;
}
const prevDir = findPrev();
let prev = null; try { prev = prevDir && JSON.parse(fs.readFileSync(path.join(prevDir, 'summary.json'))); } catch {}

// ---------- REPORT.md (facts only)
const L = [], f3 = a => a ? a.map(v => v ?? 'n/a').join(' / ') : 'n/a';
L.push(`# Flight data: ${path.basename(bbl)}`, '', `Tool v${TOOL_VERSION}. Facts only; interpret with .ai/LESSONS.md. "Active" = throttle command > ${opt.activeThrottle}.`, '');
L.push(`Recordings in file: ${results.length}. The newest is recording ${results.length}.`, '');
for (const r of results) {
  if (r.empty) { L.push(`## Recording ${r.n}: empty`, ''); continue; }
  const F = r.flight, c = r.config;
  L.push(`## Recording ${r.n}`, '',
    `- ${c.firmware}, craft ${c.craft ?? 'n/a'}, looptime ${c.looptimeUs} us, log rate ${c.logRate ?? 'n/a'}, motorOutput ${f3(c.motorOutput)}`,
    `- Duration ${r.durationS} s, ${r.frames} frames (${r.activeFrames} active), invalid ${r.invalidFrames}, corrupt ${r.corruptFrames}`,
    `- Ended cleanly (LOG_END): ${r.endedCleanly ? 'yes' : 'NO'}; disarm event: ${r.disarmed ? `at ${r.disarmed.atS} s, ${r.disarmed.detail ?? ''}` : 'none'}`,
    `- Modes: ${r.modes.map(m => `${m.atS} s ${m.names}`).join('; ') || 'no slow frames'}`,
    `- Failsafe phase changes: ${r.failsafe.map(x => `${x.atS} s phase ${x.phase}`).join('; ') || 'none'}`,
    `- Gyro RMS roll/pitch/yaw (active, deg/s): ${f3(F.gyroRms)}`,
    `- Gyro minus setpoint RMS roll/pitch/yaw (active): ${f3(F.errorRms)}`,
    `- Motor mean (active): ${f3(F.motorMean)}; percent vs 4 motor average: ${f3(F.motorPctVsAvg)}; highest is M${F.highVsOthers.motor} at ${F.highVsOthers.pct} % over the mean of the others`,
    `- Motor time at 98 % of max output (active): ${F.motorSaturationPct ? f3(F.motorSaturationPct) + ' %' : 'n/a'}`,
    `- Accelerometer peak: ${F.accPeak ? `${F.accPeak.g} g at ${F.accPeak.atS} s` : 'n/a'}`);
  if (F.currentCheck) { const k = F.currentCheck;
    L.push(`- Current check: mean ${k.meanA} A; correlation with motor sum ${k.corrWithMotorSum}, with VBAT ${k.corrWithVbat}; A/V ${k.ampsPerVolt} (sd ${k.ampsPerVoltSdPct} %)`);
    if ((k.corrWithMotorSum != null && k.corrWithMotorSum < 0.3) || (k.ampsPerVoltSdPct != null && k.ampsPerVoltSdPct < 3)) L.push('  - FLAG: current does not rise with motor output, or is a fixed multiple of VBAT. Treat the current reading as invalid (see LESSONS rule 8).');
  }
  L.push('', `| s | thr mean | gyro RMS r/p/y | err RMS r/p/y | motors mean | VBAT | A | acc max g |`, '|---|---|---|---|---|---|---|---|');
  for (const b of r.bins) L.push(`| ${b.fromS} to ${b.toS} | ${b.throttle?.mean ?? ''} | ${f3(b.gyroRms)} | ${f3(b.errorRms)} | ${f3(b.motorMean)} | ${b.vbat ?? ''} | ${b.amps ?? ''} | ${b.accMaxG ?? ''} |`);
  L.push('');
}
if (prev) {
  const a = results[results.length - 1], b = prev.recordings[prev.recordings.length - 1];
  if (a && b && !a.empty && !b.empty) {
    const d = (x, y) => x == null || y == null ? 'n/a' : `${y} to ${x}`;
    L.push(`## Newest recording vs previous run (${path.basename(prevDir)}, recording ${b.n})`, '',
      `- Gyro RMS r/p/y: ${[0, 1, 2].map(i => d(a.flight.gyroRms[i], b.flight.gyroRms[i])).join('; ')}`,
      `- Error RMS r/p/y: ${[0, 1, 2].map(i => d(a.flight.errorRms[i], b.flight.errorRms[i])).join('; ')}`,
      `- Motor percent vs average: ${a.flight.motorPctVsAvg.map((v, i) => `M${i + 1} ${d(v, b.flight.motorPctVsAvg[i])}`).join('; ')}`,
      `- Accelerometer peak g: ${d(a.flight.accPeak?.g, b.flight.accPeak?.g)}`,
      '', 'Different flights, pilots inputs and durations: a change here is not a controlled experiment.', '');
  }
} else L.push('No previous run from this tool version to compare against.', '');
fs.writeFileSync(path.join(out, 'REPORT.md'), L.join('\n'));
console.log(`Wrote ${out}${path.sep}REPORT.md, summary.json, ${results.length} CSV file(s).`);
