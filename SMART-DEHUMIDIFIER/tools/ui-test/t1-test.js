// Headless test of the esp32_T1 web page (jsdom + mocked fetch).
//   npm install   (once, in this folder)      node t1-test.js
// Extracts the page from ../../arduino-ide/esp32_T1/esp32_T1.ino, feeds it
// /api snapshots for every state and checks what lands on screen and which
// requests the buttons send (relative URLs, POST only).
const fs = require('fs');
const { JSDOM } = require('jsdom');

const src = fs.readFileSync(__dirname + '/../../arduino-ide/esp32_T1/esp32_T1.ino', 'utf8');
const html = src.split('R"HTML(')[1].split(')HTML"')[0];

const base = { state: 'IDLE', phase: '', t: 31.2, h: 70.5, t1: 31.6, h1: 69.8, t2: 30.8, h2: 71.2,
  set: 60, min: 120, el: 0, left: 7200, heat: 0, msg: '', fw: 'esp32_T1 v1.0' };
let api = { ...base };
let reply = { ok: true, status: 200, text: 'Started' };
let offline = false;
const calls = [];

const dom = new JSDOM(html, {
  url: 'http://192.168.4.1/',
  runScripts: 'dangerously',
  beforeParse(w) {
    w.fetch = async (url, opts = {}) => {
      calls.push({ url: String(url), method: opts.method || 'GET' });
      if (offline) throw new Error('network down');
      if (String(url) === 'api') return { ok: true, status: 200, json: async () => ({ ...api }) };
      return { ok: reply.ok, status: reply.status, text: async () => reply.text };
    };
    w.alerts = [];
    w.alert = (m) => w.alerts.push(String(m));
    w.setInterval = () => 0;                      // polls are driven by the test
  },
});
const w = dom.window;
const $ = (id) => w.document.getElementById(id);
const settle = () => new Promise((r) => setTimeout(r, 30));
let fails = 0;
function check(ok, name) {
  console.log((ok ? 'PASS  ' : 'FAIL  ') + name);
  if (!ok) fails++;
}
async function show(over) { api = { ...base, ...over }; await w.poll(); await settle(); }
const lastPost = () => calls.filter((c) => c.method === 'POST').pop() || {};

(async () => {
  await settle();                                  // the page polls once on load
  check(calls.length && calls[0].url === 'api' && calls[0].method === 'GET', 'polls GET api (relative URL)');
  check($('st').textContent === 'IDLE' && $('st').className === 'pill IDLE', 'IDLE state pill');
  check($('t').textContent === '31.2' && $('h').textContent === '70.5' && $('ts').textContent === '60',
    'chamber temperature, humidity, target');
  check($('t1').textContent === '31.6' && $('h1').textContent === '69.8' &&
        $('t2').textContent === '30.8' && $('h2').textContent === '71.2', 'both DHT22 cards');
  check($('hp').textContent === '0' && $('hb').style.width === '0%' && $('ph').textContent === '',
    'heater 0 %, no phase');
  check($('tm').textContent === 'Set the target and time, then Start', 'idle hint');
  check($('msg').hidden === true, 'no message box when msg is empty');
  check($('go').textContent === 'Start' && $('fs').value === '60' && $('fm').value === '120',
    'Start button, inputs filled from the device');
  check($('fw').textContent === 'esp32_T1 v1.0', 'firmware name in the footer');

  await show({ state: 'RUNNING', phase: 'HEAT-UP', heat: 100, el: 0, left: 7200, t: 35.4 });
  check($('st').className === 'pill RUNNING' && $('hb').style.width === '100%' &&
        $('ph').textContent === '\u00b7 heating up', 'RUNNING: heat-up at 100 %');
  check($('tm').textContent === '2h 00m left of 2h 00m' && $('go').textContent === 'Update',
    'RUNNING: time left, button becomes Update');

  await show({ state: 'RUNNING', phase: 'HOLD', heat: 38, el: 2670, left: 4530, t: 60.1, t1: 60.1,
    h1: 31.0, t2: null, h2: null, h: 31.0, msg: 'DHT22 #2 offline - running on #1 only' });
  check($('ph').textContent === '\u00b7 holding' && $('hp').textContent === '38' &&
        $('tm').textContent === '1h 15m left of 2h 00m', 'HOLD: 38 %, 1h 15m left');
  check($('t2').textContent === '--' && $('h2').textContent === '--' && $('c2').classList.contains('off') &&
        !$('c1').classList.contains('off'), 'offline sensor shows -- and is dimmed');
  check(!$('msg').hidden && $('msg').textContent === 'DHT22 #2 offline - running on #1 only',
    'warning shown in the message box');

  await show({ state: 'FAULT', heat: 0, msg: 'Over-temperature 90.4 C - heater cut.' });
  check($('st').className === 'pill FAULT' && $('msg').textContent.startsWith('Over-temperature') &&
        $('go').textContent === 'Start' && $('tm').textContent === '', 'FAULT: reason shown, Start again');

  await show({ state: 'DONE', heat: 0, el: 7200, left: 0, msg: 'Done - 120 min completed' });
  check($('st').className === 'pill DONE' && $('msg').textContent === 'Done - 120 min completed', 'DONE');

  // typing is not overwritten by the 2 s refresh
  $('fs').value = '72';
  $('fs').dispatchEvent(new w.Event('input'));
  await show({ set: 60 });
  check($('fs').value === '72', 'an input being edited is not overwritten by the refresh');

  // buttons
  $('fs').value = '65'; $('fm').value = '90';
  reply = { ok: true, status: 200, text: 'Started' };
  $('go').click(); await settle();
  check(lastPost().url === 'start?set=65&min=90', 'Start posts start?set=65&min=90');
  check(calls[calls.length - 1].url === 'api', 'page refreshes after a command');
  check($('fs').value === '60', 'after a command the inputs show the device values again');

  await show({ state: 'RUNNING', phase: 'HOLD', heat: 40 });
  $('fs').value = '70';
  $('go').click(); await settle();
  check(lastPost().url === 'set?set=70&min=120', 'Update (while running) posts set?...');

  $('stop').click(); await settle();
  check(lastPost().url === 'stop' && lastPost().method === 'POST', 'Stop posts stop');

  reply = { ok: false, status: 409, text: 'Too hot to start - wait until the chamber is below 85 C.' };
  await show({ state: 'FAULT' });
  $('go').click(); await settle();
  check(w.alerts.pop() === reply.text, 'a refused Start shows the reason');

  offline = true;
  await w.poll(); await settle();
  check($('st').textContent === 'OFFLINE' && $('st').className === 'pill FAULT', 'unreachable -> OFFLINE pill');
  offline = false;

  // look: royal blue + golden brown backdrop, never black; nothing absolute
  check(/body::before\{[^}]*#2352c9[^}]*#c4914a/.test(html.replace(/\s+/g, ' ').replace(/\{ /g, '{')),
    'backdrop = royal blue sky + golden-brown horizon');
  check(!/#000\b|#000000|\bblack\b|rgb\(0, ?0, ?0\)/i.test(html), 'no black anywhere in the page');
  check(!/fetch\('\//.test(html) && !/https?:\/\//.test(html), 'only relative URLs (works under any path)');
  check(calls.every((c) => c.url === 'api' ? c.method === 'GET' : c.method === 'POST'),
    'commands are POST, polling is GET');

  console.log(fails ? `${fails} ESP32_T1 PAGE TEST(S) FAILED` : 'ALL ESP32_T1 PAGE TESTS PASSED');
  process.exit(fails ? 1 : 0);
})();
