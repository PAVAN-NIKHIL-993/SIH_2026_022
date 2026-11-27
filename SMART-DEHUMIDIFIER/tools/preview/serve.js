#!/usr/bin/env node
// Browser preview of the dryer website WITHOUT an ESP32.
//
//   node tools/preview/serve.js          (from SMART-DEHUMIDIFIER/)
//   -> http://localhost:8080/            dashboard   (src/webui.h INDEX_HTML)
//      http://localhost:8080/display     kiosk page  (DISPLAY_HTML)
//      http://localhost:8080/online      online UI through the ESP bridge page
//      http://localhost:8080/docs/       online UI (docs/index.html) directly
//
// The pages are read straight from src/webui.h on every request (edit ->
// refresh) and served exactly as the firmware serves them. /api/* returns a
// simulated drying cycle (temperature climbing to the setpoint, humidity
// falling, weight dropping) so every tile, gauge and the live chart has data.
// Zero dependencies (Node >= 18). PORT=9000 node tools/preview/serve.js
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..', '..');
const PORT = +process.env.PORT || 8080;
const T0 = Date.now();

function pages() {
  const src = fs.readFileSync(path.join(ROOT, 'src', 'webui.h'), 'utf8');
  const out = {};
  const re = /static const char (\w+)\[\] PROGMEM = R"HTML\(([\s\S]*?)\)HTML";/g;
  let m;
  while ((m = re.exec(src))) out[m[1]] = m[2];
  return out;
}

// ---- simulated cycle ------------------------------------------------------
const SET = {
  setTemp: 60, tempHyst: 1.5, maxTemp: 95, humHigh: 60, humLow: 40, humTarget: 35,
  requireHum: false, dryMinutes: 120, fanMin: 0, fanIn: 100, fanOut: 100, fanSlope: 6,
  targetG: 1900, fanTrigRH: 60, fanTrigMin: 1, fanBurstS: 60, mode: 0,
  stickCount: 400, stickWetG: 2.5, pasteWaterPct: 35, targetMoistPct: 10,
  heaterMax: 100, cooldownSec: 45, bypassPct: 20, cutoffPct: 10, battType: 0,
  tzMinutes: 330, smartVent: true, boostHeat: true, kp: 10, ki: 0.2, kd: 5,
  requireWeight: false, weightRateG: 2, weightMinY: 10,
};
const DEFS = Object.assign({}, SET, { setTemp: 60, targetG: 0 });
let running = true;
let manual = null;                       // {pct, until}

function sim() {
  const el = Math.floor((Date.now() - T0) / 1000) + 1500;   // start 25 min in
  const k = 1 - Math.exp(-el / 900);                       // 0 -> 1 warm-up
  const wob = Math.sin(el / 17) * 0.4;
  const tAvg = +(28 + (SET.setTemp - 28) * k + wob).toFixed(1);
  const hAvg = +(78 - 36 * k + Math.sin(el / 23)).toFixed(1);
  const g = Math.round(2400 - 520 * k);
  return { el, tAvg, hAvg, g };
}

function status() {
  const { el, tAvg, hAvg, g } = sim();
  const now = Math.floor(Date.now() / 1000);
  const man = manual && manual.until > Date.now()
    ? { pct: manual.pct, left: Math.round((manual.until - Date.now()) / 1000) } : null;
  return {
    state: running ? 'DRYING' : 'IDLE', fw: '2.0.23', fault: '', faultRec: null, warnRec: null,
    s1: { ok: true, t: +(tAvg - 0.3).toFixed(1), h: +(hAvg + 0.4).toFixed(1) },
    s2: { ok: true, t: +(tAvg + 0.3).toFixed(1), h: +(hAvg - 0.4).toFixed(1) },
    tAvg, hAvg, hMax: +(hAvg + 0.4).toFixed(1),
    heat: running ? Math.max(18, Math.round(90 - tAvg)) : 0, boost: running && tAvg < SET.setTemp - 5,
    fan: running ? 35 : 0, fanIn: 0, fanOut: running ? 35 : 0, man,
    kp: { ok: true, last: 'A' },
    scale: { ok: true, g, rate: 1.2, cal: true },
    supply: { name: 'SOLAR MODE', solar: true, live: true, mode: 0 },
    dht: { ok: true, t: 31.5, h: 62 },
    wtStart: 2400, finalG: 0, moistureG: 0, cycleSecs: 0,
    clockSet: true, now, tz: 330,
    door: { fitted: true, locked: false, closed: true, phase: running ? 'RUNNING' : 'READY', batch: 2400 },
    relay: running, elapsed: running ? el : 0, remaining: running ? Math.max(0, SET.dryMinutes * 60 - el) : 0,
    logCount: Math.min(600, Math.floor(el / 10)), eelog: { ok: true, used: 42, slots: 817 }, heap: 183000,
    rtc: 'DS1307 OK (coin-cell)',
    wx: { ok: true, t: 31.2, h: 71, r: 20, w: 9.5, code: 1, loc: 'Vijayawada', age: 3,
          manual: false, fresh: true, src: 'live', outT: 30.4, outH: 69 },
    bat: { v: 12.9, pct: 84, bypass: false, valid: true, name: '4S LiFePO4' },
    set: SET, defs: DEFS,
  };
}

function history() {
  const { el } = sim();
  const n = Math.min(600, Math.floor(el / 10));
  const t = [], temp = [], hum = [];
  for (let i = 0; i < n; i++) {
    const s = el - (n - 1 - i) * 10, k = 1 - Math.exp(-s / 900);
    t.push(s);
    temp.push(+(28 + (SET.setTemp - 28) * k + Math.sin(s / 17) * 0.4).toFixed(1));
    hum.push(+(78 - 36 * k + Math.sin(s / 23)).toFixed(1));
  }
  return { t, temp, hum };
}

const CYCLES = [
  { file: 'cycle20260924-101500-041.csv', started: '2026-09-24 10:15:00', ended: '2026-09-24 12:17:40',
    reason: 'completed', setTemp: 60, dryMin: 120, elapsedMin: 122.7, remainMin: 0 },
  { file: 'cycle20260923-093000-040.csv', started: '2026-09-23 09:30:00', ended: '2026-09-23 11:02:10',
    reason: 'stopped', setTemp: 80, dryMin: 120, elapsedMin: 92.2, remainMin: 27.8, note: 'user stopped' },
];

function send(res, code, type, body) {
  res.writeHead(code, { 'Content-Type': type, 'Cache-Control': 'no-store',
                        'Access-Control-Allow-Origin': '*' });
  res.end(body);
}

http.createServer((req, res) => {
  const url = new URL(req.url, 'http://x');
  const p = url.pathname;
  let body = '';
  req.on('data', (c) => { body += c; });
  req.on('end', () => {
    try {
      const P = pages();
      if (p === '/') return send(res, 200, 'text/html; charset=utf-8', P.INDEX_HTML);
      if (p === '/display') return send(res, 200, 'text/html; charset=utf-8', P.DISPLAY_HTML);
      if (p === '/online') return send(res, 200, 'text/html; charset=utf-8',
        P.ONLINE_LOADER_HTML.replace('__ONLINE_URL__', '/docs/'));
      if (p === '/docs/' || p === '/docs/index.html') return send(res, 200, 'text/html; charset=utf-8',
        fs.readFileSync(path.join(ROOT, 'docs', 'index.html'), 'utf8'));
      if (p === '/api/data') return send(res, 200, 'application/json', JSON.stringify(status()));
      if (p === '/api/history') return send(res, 200, 'application/json', JSON.stringify(history()));
      if (p === '/api/cycles') return send(res, 200, 'application/json', JSON.stringify(CYCLES));
      if (/\.csv$/.test(p) || p === '/api/cycle')
        return send(res, 200, 'text/csv', 'sec,temp_avg_C,hum_avg_RH\r\n0,28.0,78.0\r\n600,41.9,62.4\r\n');
      if (p === '/update') return send(res, 200, 'text/html', '<p>OTA upload page (firmware only)</p>');
      if (req.method === 'POST' && p.startsWith('/api/')) {
        if (p === '/api/start') running = true;
        if (p === '/api/stop') running = false;
        if (p === '/api/heat') {
          try { const j = JSON.parse(body || '{}'); manual = { pct: +j.pct || 0, until: Date.now() + 60000 }; }
          catch (e) { manual = { pct: 100, until: Date.now() + 60000 }; }
        }
        return send(res, 200, 'text/plain', 'ok');
      }
      send(res, 404, 'text/plain', 'not found');
    } catch (e) {
      send(res, 500, 'text/plain', String(e && e.stack || e));
    }
  });
}).listen(PORT, '0.0.0.0', () => {
  console.log(`dryer UI preview: http://localhost:${PORT}/  (/display, /online, /docs/)`);
});
