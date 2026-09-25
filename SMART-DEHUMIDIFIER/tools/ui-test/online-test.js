// Headless test of the GitHub-Pages dashboard (docs/index.html):
//   - standalone mode renders the same dashboard via espFetch
//   - "Back up cycles to GitHub" uploads every ESP cycle CSV to
//     api.github.com with the token header and base64 content
// Run: node online-test.js   (needs `npm install` in this folder)
'use strict';
const fs = require('fs');
const path = require('path');
const { JSDOM } = require('jsdom');

const html = fs.readFileSync(path.join(__dirname, '../../docs/index.html'), 'utf8');

const fakeStatus = () => ({
  state: 'DRYING', fw: '2.0.0', fault: '',
  s1: { ok: true, t: 44.2, h: 57.1 },
  s2: { ok: true, t: 44.8, h: 56.4 },
  tAvg: 44.5, hAvg: 56.7, hMax: 57.1,
  heat: 34, boost: true, fan: 20, fanIn: 10, fanOut: 16,
  man: { pct: 80, left: 42 },
  kp: { ok: true, last: 'A' },
  scale: { ok: true, g: 1234, rate: 1.2, cal: true },
  supply: { name: 'SOLAR MODE', solar: true, live: true, mode: 0 },
  dht: { ok: true, t: 31.5, h: 62 },
  fault: '', faultRec: { code: 0, name: '', critical: false, active: false, sinceS: 0, val: 0, limit: 0 },
  warnRec: { code: 0, name: '', active: false },
  wtStart: 0, finalG: 0, moistureG: 0, cycleSecs: 0,
  clockSet: true, now: 1789012500, tz: 330,
  door: { fitted: true, locked: false, closed: true, phase: 'READY', batch: 1234 },
  relay: true, elapsed: 1234, remaining: 5966,
  logCount: 741, heap: 183000, now: 1780000000, clockSet: true, tz: 330,
  wx: { ok: false, src: 'none', outT: null, outH: null },
  kp: { ok: false, last: '' },
  bat: { v: 12.31, pct: 78, bypass: false, valid: true, name: '3S Li-ion' },
  set: { setTemp: 80, tempHyst: 1.5, maxTemp: 95, humHigh: 60, humLow: 40,
         humTarget: 35, requireHum: false, dryMinutes: 120, fanMin: 20, fanIn: 50, fanOut: 80, fanSlope: 3,
         heaterMax: 100, cooldownSec: 45, bypassPct: 20, cutoffPct: 10,
         eelog: { ok: true, used: 123, slots: 817 },
  battType: 0, tzMinutes: 330, smartVent: true, boostHeat: true, kp: 10, ki: 0.2, kd: 5,
         stickCount: 0, stickWetG: 2.5, pasteWaterPct: 35, targetMoistPct: 10 },
  defs: { setTemp: 80, tempHyst: 1.5, maxTemp: 95, humHigh: 60, humLow: 40,
          humTarget: 35, requireHum: false, dryMinutes: 120, fanMin: 20, fanIn: 50, fanOut: 80, fanSlope: 3,
          heaterMax: 100, cooldownSec: 45, bypassPct: 20, cutoffPct: 10,
          battType: 0, tzMinutes: 330, smartVent: true, boostHeat: true, kp: 10, ki: 0.2, kd: 5 },
});

const cycles = [
  { file: 'cycle20260902-140000-001.csv', started: '2026-09-02 14:00:00',
    ended: '2026-09-02 16:02:11', reason: 'completed', setTemp: 80,
    dryMin: 120, elapsedMin: 122.2, remainMin: 0 },
  { file: 'cycle20260901-090000-002.csv', started: '2026-09-01 09:00:00',
    ended: '2026-09-01 10:12:00', reason: 'stopped', setTemp: 80,
    dryMin: 180, elapsedMin: 72, remainMin: 108 },
];

const ghPuts = [];
let failures = 0;
const check = (name, cond) => {
  console.log((cond ? 'PASS' : 'FAIL') + '  ' + name);
  if (!cond) failures++;
};

const dom = new JSDOM(html, {
  url: 'https://pavan-nikhil-993.github.io/arena/',
  runScripts: 'dangerously',
  beforeParse(window) {
    window.localStorage.setItem('ghRepo', 'test/dryer-data');
    window.localStorage.setItem('ghToken', 'github_pat_TEST');
    window.fetch = async (url, opts) => {
      url = String(url);
      if (url.startsWith('http://192.168.4.1/api/data'))
        return { ok: true, status: 200, text: async () => JSON.stringify(fakeStatus()),
                 json: async () => fakeStatus() };
      if (url.startsWith('http://192.168.4.1/api/cycles'))
        return { ok: true, status: 200, text: async () => JSON.stringify(cycles),
                 json: async () => cycles };
      if (url.startsWith('http://192.168.4.1/api/cycle')) {
        const f = decodeURIComponent(url.split('file=')[1] || '');
        const csv = '# started,2026-09-02\nsec,temp\n0,30\n';
        return { ok: true, status: 200, text: async () => csv, json: async () => ({}) };
      }
      if (url.startsWith('http://192.168.4.1/api/history'))
        return { ok: true, status: 200, text: async () => '{"t":[0],"temp":[30],"hum":[60]}',
                 json: async () => ({ t: [0], temp: [30], hum: [60] }) };
      if (url.startsWith('https://api.github.com/')) {
        ghPuts.push({ url, opts });
        return { ok: true, status: 201,
                 text: async () => '{"content":{"sha":"abc"}}',
                 json: async () => ({ content: { sha: 'abc' } }) };
      }
      // open-meteo etc: no internet in this test
      throw new TypeError('offline');
    };
    window.devicePixelRatio = 1;
    // canvas 2D stub: every method works; gradient factories return fillable objects
    const grad = { addColorStop() {} };
    const ctx2d = new Proxy({}, {
      get: (t, p) => {
        if (p === 'createLinearGradient' || p === 'createRadialGradient') return () => grad;
        if (p === 'measureText') return () => ({ width: 10 });
        if (typeof p === 'string') return () => {};
        return undefined;
      },
      set: () => true,
    });
    window.HTMLCanvasElement.prototype.getContext = () => ctx2d;
  },
});

const { window } = dom;
const $ = (id) => window.document.getElementById(id);

setTimeout(async () => {
  try {
    check('standalone banner shown', $('modeTxt').textContent.includes('standalone'));
    check('banner points to /online link', $('modeTxt').innerHTML.includes('192.168.4.1/online'));
    check('live data rendered', $('tAvg').textContent === '44.5');
    check('state badge DRYING', $('stBadge').textContent === 'DRYING');
    check('dummy weather badge TYP', $('wxBadge').textContent === 'TYP.');
    check('dummy outdoor temp filled', /^\d+(\.\d)?$/.test($('wxT').textContent));
    check('dummy 5-day forecast strip', $('wxDays').children.length === 5);
    check('manual heat knob rendered', $('manBadge').textContent === 'MANUAL 42s' && $('heatKnob').value === '80');
    check('weight tile filled', $('wt').textContent === '1234');
    check('door pill rendered', $('doorPill').textContent.includes('READY'));
    check('cycle table filled', $('cycTable').rows.length === 3);
    check('github section present', !!$('ghRepo'));
    check('repo loaded from storage', $('ghRepo').value === 'test/dryer-data');

    // run the GitHub backup
    await window.ghBackup();
    check('two files uploaded', ghPuts.length === 2);
    check('uploaded to contents API',
      ghPuts.every(p => p.url === 'https://api.github.com/repos/test/dryer-data/contents/dryer-cycles/' +
        (p.url.includes('-001') ? 'cycle20260902-140000-001.csv' : 'cycle20260901-090000-002.csv')));
    check('token header sent',
      ghPuts.every(p => p.opts.headers['Authorization'] === 'Bearer github_pat_TEST'));
    const body = JSON.parse(ghPuts[0].opts.body);
    check('content is base64 csv', body.content === window.btoa('# started,2026-09-02\nsec,temp\n0,30\n'));
    check('commit message set', body.message.includes('dryer cycle'));
    check('status line reports done', $('ghStat').textContent.includes('2 uploaded'));

    console.log(failures === 0 ? '\nALL ONLINE-UI TESTS PASSED' : `\n${failures} TESTS FAILED`);
    process.exit(failures === 0 ? 0 : 1);
  } catch (e) {
    console.error('EXCEPTION:', e);
    process.exit(1);
  }
}, 600);
