// Headless smoke test of the dryer dashboard (jsdom + mocked fetch)
// Headless smoke test of the dryer dashboard website.
//   npm install jsdom   (once, in this folder)
//   node ui-test.js
// Extracts the embedded page from ../../src/webui.h, mocks the /api
// endpoints, and checks that every value lands in the right DOM slot.
const fs = require('fs');
const { JSDOM } = require('jsdom');

const src = fs.readFileSync(__dirname + '/../../src/webui.h', 'utf8');
// first R"HTML(...)HTML" literal = the embedded dashboard (there is also
// a second one for the /online loader page)
const html = src.split('R"HTML(')[1].split(')HTML"')[0];
// segment [3] = the /display kiosk page (INDEX first, ONLINE_LOADER second)
const dispHtml = src.split('R"HTML(')[3] ? src.split('R"HTML(')[3].split(')HTML"')[0] : '';

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
  fault: 'E07 DOOR OPEN - door opened during the cycle',
  faultRec: { code: 7, name: 'DOOR OPEN', critical: true, active: true, sinceS: 12, val: 0, limit: 0 },
  wtStart: 2400, finalG: 1900, moistureG: 500, cycleSecs: 7200,
  warnRec: { code: 13, name: 'LOW SUPPLY VOLTAGE', active: true },
  clockSet: true, now: 1789012500, tz: 330,
  door: { fitted: true, locked: false, closed: true, phase: 'READY', batch: 1234 },
  relay: true, elapsed: 1234, remaining: 5966,
  logCount: 741, eelog: { ok: true, used: 123, slots: 817 }, heap: 183000, now: 1780000000, clockSet: true, tz: 330,
  wx: { ok: true, t: 31.2, h: 78, r: 40, w: 12.5, code: 2, loc: 'Razam',
        age: 4, manual: false, fresh: true, src: 'live',
        outT: 29.8, outH: 81 },
  bat: { v: 12.31, pct: 78, bypass: false, valid: true, name: '3S Li-ion' },
  set: { setTemp: 60, tempHyst: 1.5, maxTemp: 95, humHigh: 60, humLow: 40,
         humTarget: 35, requireHum: false, dryMinutes: 120, fanMin: 0, fanIn: 100, fanOut: 100, fanSlope: 6,
         targetG: 1000, fanTrigRH: 60, fanTrigMin: 1, fanBurstS: 60, mode: 0,
         stickCount: 400, stickWetG: 2.5, pasteWaterPct: 35, targetMoistPct: 10,
         heaterMax: 100, cooldownSec: 45, bypassPct: 20, cutoffPct: 10,
         battType: 0, tzMinutes: 330, smartVent: true, boostHeat: true, kp: 10, ki: 0.2, kd: 5,
         requireWeight: false, weightRateG: 2, weightMinY: 10 },
  defs: { setTemp: 80, tempHyst: 1.5, maxTemp: 95, humHigh: 60, humLow: 40,
          humTarget: 35, requireHum: false, dryMinutes: 120, fanMin: 20, fanIn: 50, fanOut: 80, fanSlope: 3,
          heaterMax: 100, cooldownSec: 45, bypassPct: 20, cutoffPct: 10,
          battType: 0, tzMinutes: 330, smartVent: true, boostHeat: true, kp: 10, ki: 0.2, kd: 5,
          requireWeight: false, weightRateG: 2, weightMinY: 10 },
});

const calls = [];
const dom = new JSDOM(html, {
  runScripts: 'dangerously',
  beforeParse(window) {
    window.fetch = async (url, opts) => {
      calls.push(url + ' ' + (opts ? opts.method : 'GET'));
      let body = '{}';
      if (url === '/api/data') body = JSON.stringify(fakeStatus());
      else if (url === '/api/cycles')
        body = JSON.stringify([
          { file: 'cycle20260902-140000-001.csv', started: '2026-09-02 14:00:00',
            ended: '2026-09-02 16:02:11', reason: 'completed', setTemp: 80,
            dryMin: 120, elapsedMin: 122.2, remainMin: 0 },
          { file: 'cycle20260901-090000-002.csv', started: '2026-09-01 09:00:00',
            ended: '2026-09-01 10:12:00', reason: 'stopped', setTemp: 80,
            dryMin: 180, elapsedMin: 72, remainMin: 108, note: 'user stopped' }]);
      else if (url === '/api/history')
        body = JSON.stringify({ t: [0, 10, 20], temp: [30, 40, 44], hum: [70, 62, 57] });
      else body = 'ok';
      return { ok: true, text: async () => body, json: async () => JSON.parse(body) };
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
let failures = 0;
const check = (name, cond) => {
  console.log((cond ? 'PASS' : 'FAIL') + '  ' + name);
  if (!cond) failures++;
};

setTimeout(async () => {
  try {
    check('polled /api/data', calls.some(c => c.startsWith('/api/data')));
    check('seeded /api/history', calls.some(c => c.startsWith('/api/history')));
    check('state badge DRYING', $('stBadge').textContent === 'DRYING');
    if ($('stBadge').textContent !== 'DRYING') console.log('   badge=', JSON.stringify($('stBadge').textContent));
    check('temp avg shown 44.5', $('tAvg').textContent === '44.5');
    check('hum avg shown 56.7', $('hAvg').textContent === '56.7');
    check('sensor t1 44.2', $('t1').textContent === '44.2');
    check('heater 34%', $('heat').textContent === '34');
    check('fan 20%', $('fan').textContent === '20');
    check('weight target + diff shown', $('wtTgt').textContent.includes('1000') && $('wtTgt').textContent.includes('234'));
    check('MAX heat-up badge visible', $('boostBadge').style.display === 'inline-block');
    check('manual badge with countdown', $('manBadge').textContent === 'MANUAL 42s');
    check('manual knob at override value', $('heatKnob').value === '80');
    check('exhaust duty shown', $('fanOut').textContent === '16');
    check('battery 78%', $('bat').textContent === '78');
    check('battery V 12.31', $('bv').textContent === '12.31');
    check('remaining mmss', $('rem').textContent === '1h 39m');
    check('elapsed mmss', $('el').textContent === '20:34');
    check('target temp 60.0', $('setT').textContent === '60.0');
    check('RH band', $('setH').textContent === '40\u201360');
    check('log count', $('logN').textContent === '741');

    // switch to custom slide and check the form filled from live settings
    window.showTab('Set');
    check('parameters page visible', $('secSet').classList.contains('on'));
    check('form target temp 60', $('f_setTemp').value === '60');
    check('weight params filled', $('f_weightRateG').value === '2' && $('f_weightMinY').value === '10');
    check('form hours 2', $('f_hrs').value === '2');
    check('form kp', $('f_kp').value === '10');

    // one-click defaults on the SAME page
    await window.applyDefaults();
    check('defaults POST sent', calls.some(c => c.startsWith('/api/defaults')));
    check('defaults table on parameters page', $('defTable').rows.length >= 10);
    check('form still filled after reset', $('f_setTemp').value === '60');

    // chart drew points (2 polls + 3 history seeds)
    check('chart series populated', window.eval('T.length') >= 4);

    // canvas draw did not throw
    window.draw();
    check('draw() no exception', true);

    // clock + history features
    check('live clock formatted', /\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}/.test($('clock').textContent));
    check('past-cycles table filled', $('cycTable').rows.length === 3);
    check('eelog registry line renders', $('eeN').textContent === '123/817');
    check('eelog CSV download link', html.includes('/eelog.csv'));
    check('home page cycle-data row', html.includes('/lastcycle.csv') &&
          html.includes('ALL cycles'));
    check('kiosk display has data download', dispHtml.includes('/eelog.csv'));
    check('virtual keypad drawer on main page',
          html.includes('id="vpDrawer"') && html.includes("vpKey('B')"));
    check('kiosk landscape keypad', dispHtml.includes('id="vpPad"') &&
          dispHtml.includes('/api/key'));
    window.vpOpen();
    check('keypad drawer opens',
          window.document.getElementById('vpDrawer').classList.contains('on'));
    window.document.querySelector('#vpDrawer .vpPad button').click();
    check('virtual key posts to /api/key',
          calls.some(c => c.startsWith('/api/key?k=1')));
    check('history badge completed', $('cycTable').rows[1].cells[2].textContent.trim() === 'completed');
    check('history download link', $('cycTable').rows[1].cells[4].innerHTML.includes('/api/cycle?file='));
    check('history shows time left', $('cycTable').rows[2].cells[3].textContent.includes('min left'));
    check('tz field filled', $('f_tz').value === '330');

    // weather card (live forecast relayed by the phone)
    check('outdoor temp from forecast', $('wxT').textContent === '29.8');
    check('outdoor RH from forecast', $('wxH').textContent === '81');
    check('weather badge LIVE', $('wxBadge').textContent === 'LIVE');
    check('weather icon set', $('wxIcon').textContent.length > 0);
    check('weather location + age', $('wxWhen').textContent.includes('Razam') && $('wxWhen').textContent.includes('min ago'));
    check('smartVent checkbox filled', $('f_smartVent').checked === true);
    check('boostHeat checkbox filled', $('f_boostHeat').checked === true);
    check('airflow + burst fields filled', $('f_fanOut').value === '100' && $('f_fanSlope').value === '6' &&
      $('f_fanTrigRH').value === '60' && $('f_fanTrigMin').value === '1' && $('f_fanBurstS').value === '60' &&
      $('f_targetG').value === '750');   // stick calculator overrode 1000
    check('stick calculator fills target (400x2.5g, 35->10%)',
      $('f_stickCount').value === '400' && $('f_targetG').value === '750' &&
      $('stickPrev').textContent.includes('750') && $('stickPrev').textContent.includes('250'));
    $('heatKnob').value = '100';
    $('heatKnob').dispatchEvent(new dom.window.Event('input', {bubbles:true}));
    $('heatKnob').dispatchEvent(new dom.window.Event('change', {bubbles:true}));
    check('knob posts manual heat 100', calls.some(c => c.startsWith('/api/heat?d=100')));
    check('knob readout live', $('knobPct').textContent === '100%');
    check('knob badge pending MANUAL', $('manBadge').textContent.includes('MANUAL'));
    check('smartVent paused hint', $('wxWhen').textContent.includes('venting paused'));
    check('dummy 5-day forecast strip', $('wxDays').children.length === 5);
    check('OTA update link in footer', window.document.body.innerHTML.includes('/update'));
    check('firmware version shown', $('fw').textContent === '2.0.0');
    check('brand shown', $('brand') && $('brand').textContent.includes('ARCHITECTS OF SOLUTIONS'));
    check('supply pill SOLAR live', $('supPill').textContent.includes('SOLAR'));
    check('DHT22 measured outdoor shown', $('wxOut').textContent.includes('31.5') && $('wxOut').textContent.includes('62'));
    check('display-view link present', window.document.body.innerHTML.includes('/display'));
    check('kiosk /display page exists in firmware', dispHtml.includes('SMART DEHUMMIDIFIER - display') && dispHtml.includes('api/data') && dispHtml.includes('wakeLock'));
    check('kiosk clock shown + settable (tap)', dispHtml.includes('id="clk"') && dispHtml.includes('/api/settime') && dispHtml.includes('kdt'));
    check('dashboard manual clock form', html.includes('f_dt') && html.includes('setClockManual'));
    check('E-code fault card present', html.includes('faultRec') && html.includes('warnBox'));
    check('E13 warning banner shown', $('warnBox').style.display === 'block' &&
      $('warnBox').innerHTML.includes('E13'));
    check('batch report card markup', html.includes('doneCard') && html.includes('moisture removed'));
    check('kiosk DONE summary markup', dispHtml.includes('dWm') && dispHtml.includes('DRYING COMPLETE'));
    check('mode buttons present', window.document.querySelectorAll('[data-mdbtn]').length === 3);
    check('keypad last key shown', $('kpLast').textContent === 'A');
    check('weight tile 1234g', $('wt').textContent === '1234');
    check('door READY pill with batch', $('doorPill').textContent.includes('READY') && $('doorPill').textContent.includes('1234'));
    check('weight rate 1.2 g/min', $('wtRate').textContent === '1.2');
    check('weight mode off', $('wtMode').textContent === 'off');
    check('fire tab class', $('tabDash').className.includes('fire'));
    check('water tab class', $('tabSet').className.includes('water'));
    check('background swatches rendered', $('bgSwatches').children.length === 6);
    window.bgApply('#0b1026', false);
    check('background applied', window.document.body.style.getPropertyValue('--bg') === '#0b1026' && window.document.body.classList.contains('nolines'));

    console.log(failures === 0 ? '\nALL UI TESTS PASSED' : `\n${failures} TESTS FAILED`);
    process.exit(failures === 0 ? 0 : 1);
  } catch (e) {
    console.error('EXCEPTION:', e);
    process.exit(1);
  }
}, 500);
