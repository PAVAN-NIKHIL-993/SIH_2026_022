#!/usr/bin/env python3
"""Generates docs/design-workflow.drawio (2 pages, 1080x1920 = 9:16) and
matching SVG previews. Run from smart-dehumidifier/:  python3 tools/make-design-diagram.py
"""
import os
from xml.sax.saxutils import escape

W, H = 1080, 1920

COLORS = {   # kind -> (fill, stroke)
    'process':   ('#dae8fc', '#6c8ebf'),
    'decision':  ('#ffe6cc', '#d79b00'),
    'terminator':('#d5e8d4', '#82b366'),
    'note':      ('#f5f5f5', '#666666'),
    'danger':    ('#f8cecc', '#b85450'),
    'gold':      ('#fff2cc', '#d6b656'),
    'purple':    ('#e1d5e7', '#9673a6'),
    'green':     ('#d5e8d4', '#82b366'),
}

def B(i, label, x, y, w, h, kind='process', bold_first=True, dashed=False):
    return dict(id=i, label=label, x=x, y=y, w=w, h=h, kind=kind,
                bold=bold_first, dashed=dashed)

def E(src, dst, label='', dashed=False, color='#595959', via=None):
    return dict(src=src, dst=dst, label=label, dashed=dashed, color=color, via=via)

# ================= PAGE 1 : SYSTEM DESIGN =================
p1_boxes = [
 B('title', 'SMART DEHUMIDIFIER PILLAR — FULL SYSTEM DESIGN\nv2.1 max-PCF architecture · one ESP32-S3 does everything', 40, 16, 1000, 46, 'note'),
 # power chain
 B('solar', '550 W SOLAR PANEL\n24 V nominal', 40, 76, 220, 54),
 B('mppt', 'MPPT CONTROLLER\n20 A+ · 12 V side', 292, 76, 220, 54),
 B('batt', 'LiFePO4 4S 50–100 Ah\nBMS ≥ 50 A', 544, 76, 220, 54),
 B('fuse', '50 A FUSE\nheater rail', 796, 76, 220, 54),
 B('latch', 'P-MOSFET LATCH — hard power-off\nhold = GPIO14 · BUTTON-1: 3 s off / 10 s reboot', 380, 160, 320, 64, 'gold'),
 B('bus', '12 V POWER BUS', 200, 252, 680, 36, 'note', bold_first=False),
 B('bts', 'BTS7960 43 A + 555 shifter\nRPWM = 12 · EN = 13\n→ 500 W coil (0.32 Ω)', 60, 320, 280, 64),
 B('l298', 'L298N\nENB = 17 (fan PWM)\nIN3/IN4 tied on the module (v2.0.10)\n→ OUTLET fan (burst law)', 400, 320, 280, 64),
 B('buck', '5 V BUCK (set 5.0 V first)\n→ ESP32 · relays · HX711 · keypad', 740, 320, 280, 64),
 # brain
 B('s3', 'ESP32-S3 DevKitC\n(the brain · zero libraries)\n\nBOOT btn = 0 (start/stop)\nlatch hold = 14\nI2C0 = 8 / 9 (I2C1 free)\nHX711 = 1 / 2\nbattery = ADC 4 · gate 5\nheater = RPWM 12 · EN 13\nfan ENB = 17\ndoor = 21 · DHT11 = 41\nDHT22 = 10 · opto = 18\nrelays = 6 / 7 · toggle = 39\nbuttons = 15 / 16\nbuzzer = 38 · pixel = 48\nRTC DS1302 = 40 / 42 / 47\n\n(no display — the website\nis the screen, v2.0.7)', 390, 430, 300, 560),
 # left column — I2C0
 B('i2c0', 'I2C0 BUS — GPIO 8 / 9', 40, 430, 300, 34, 'purple', bold_first=False),
 B('aht1', 'AHT10 (0x38)\nchamber TOP · temp/RH', 40, 474, 300, 54),
 B('pcf1', 'PCF8574 #1 @ 0x20 — KEYPAD 4×4\nB menu · 2/4/6/8 arrows · A OK\nC mode · D run', 40, 538, 300, 64, 'purple'),
 B('pcf2', 'PCF8574 #2 @ 0x21 — FRONT PANEL (mixed)\nP0/P1 → supply relay CH1/CH2 (active-LOW)\nP2/P3 → BUTTON-1 / BUTTON-2\nP4 → SOLAR toggle · P5 → opto\nP6 → door limit switch · P7 → buzzer\n(10 kΩ pull-ups on all pins)', 40, 612, 300, 100, 'purple'),
 B('freed', 'SPARE S3 GPIOs (no display)\nfrees 6 · 7 · 38 · 15 · 16 · 39 · 18 · 21\nafter PCF #2 lands\ntoday free: 3 · 11\n40/42/47 = DS1302 RTC (v2.0.21)\n41 = DHT11 · 48 = pixel · 35/36/37 = R8 PSRAM', 40, 722, 300, 94, 'gold'),
 B('loads2', 'DRIVEN VIA PCF #2\n2-CH SUPPLY RELAY — CH1 solar / CH2 bypass\n(never both · switch only with loads quiet)\nBUZZER KY-012 (low-side: + to 3V3)\nBUTTONS / TOGGLE / OPTO / DOOR read back', 40, 828, 300, 110, 'gold'),
 # right column
 B('aht2', 'DHT22 · cool-return path\nGPIO10 + 10k · chamber source #2\n(I2C1 free, GPIO11 spare)', 760, 430, 280, 54),
 B('hx711', 'HX711 + 2×5 kg load cells\nCLK = 1 · DOUT = 2 · dry-to-weight\ncells OUTSIDE the hot chamber', 760, 494, 280, 74),
 B('divider', 'BATTERY DIVIDER\n100k / 15k → ADC 4\ngate = GPIO5 (2N7000)', 760, 578, 280, 64),
 B('phone', 'ANY PHONE (browser only)\nWiFi AgarbattiDryer / dryer1234\nhttp://192.168.4.1\ndashboard · parameters · history', 760, 866, 280, 84, 'green'),
 B('pcbox', 'PC — Arduino IDE\nnetwork OTA:\nsmart-dehumidifier at 192.168.4.1\nor USB serial 115200', 760, 960, 280, 74, 'green'),
 # bottom band
 B('safety', 'SAFETY CHAIN — always armed\n95 °C hard cut · both sensors dead 15 s · heater failure (temp flat 3 min) · fan error (RH ≥ 60 % for 5 min)\ndoor opened mid-cycle = instant FAULT · battery ≤ 10 % safe shutdown · purge before every power cut\nOTA refused while RUNNING · START refused until the scale is calibrated · unused pins parked safe', 40, 1140, 1000, 130, 'danger'),
 B('modes', 'MODES — C key / BUTTON-2 / website\nAGARBATTI 60 °C · USER DEFINED · SILICAGEL 80 °C\nsetTemp clamped 40–80 °C (hard cut 95 °C)', 40, 1290, 480, 80, 'gold'),
 B('beeps', 'BEEP LANGUAGE\nstart 3 s · end 5 s · power-on 2/s × 3 s\nerror 5 s · door 1 s · mode change 3 s', 560, 1290, 480, 80, 'gold'),
 B('legend', 'BUILD NOTES\n• Buy PCF8574 — never PCF8574A (0x38 = AHT10 clash) · addresses 0x20 / 0x21 / 0x22 · AT24C256 0x50 (cycle registry)\n• DevKitC N8/N16/N16R8 all work — pin map avoids GPIO 35/36/37 (PSRAM on R8)\n• All grounds common at ONE star point · buck set to 5.0 V before the ESP32 connects\n• Battery gate (GPIO5), latch hold (14), RPWM (12), ADC (4), HX711 (1/2) stay DIRECT on the S3 —\n  everything slow (clicks, beeps, switches, presses) lives on the PCFs', 40, 1390, 1000, 150, 'note'),
 B('p2ref', 'OPERATING WORKFLOW  (power-on → calibrate → load → dry → done)  →  PAGE 2 of this file', 40, 1560, 1000, 44, 'note', bold_first=False),
]
p1_edges = [
 E('solar', 'mppt'), E('mppt', 'batt'), E('batt', 'fuse'), E('fuse', 'latch'),
 E('latch', 'bus'), E('bus', 'bts'), E('bus', 'l298'), E('bus', 'buck'),
 E('aht1', 's3'), E('pcf1', 's3'), E('pcf2', 's3'),
 E('pcf2', 'loads2'),
 E('aht2', 's3'), E('hx711', 's3'), E('divider', 's3'),
 E('s3', 'phone', 'hotspot', color='#82b366'),
 E('s3', 'pcbox', 'OTA', color='#82b366'),
]

# ================= PAGE 2 : OPERATING WORKFLOW =================
p2_boxes = [
 B('t1', 'POWER ON — press BUTTON-1\nP-MOSFET latch feeds the pillar', 390, 30, 300, 56, 'terminator'),
 B('p1', 'ALL PINS LOW\noutputs safe · unused pads parked', 390, 112, 300, 56),
 B('p2', 'SPLASH + POWER-ON BEEPS\nSELF-TEST [diag]\n10 s INIT + CALIBRATION window', 390, 194, 300, 72),
 B('p3', 'ENGAGE — supply relay = toggle\n(start gate per calibration state)', 390, 292, 300, 56),
 B('d1', 'scale\ncalibrated?', 410, 374, 260, 110, 'decision'),
 B('cal', 'START LOCKED — CALIBRATE\ntare, then known weight (e.g. 1000 g)\nwebsite Calibrate / serial cal 1000', 60, 384, 280, 90, 'gold'),
 B('p4', 'GATE OPEN — LOAD THE TRAYS\nthen CLOSE the door', 390, 516, 300, 56),
 B('p5', 'BATCH WEIGHED (diff vs tare)\nREADY: n g on screen + site\n(empty / overload warned)', 390, 598, 300, 72),
 B('p6', 'START — D key · website ·\nBUTTON-2 · BOOT button', 390, 696, 300, 56, 'green'),
 B('p7', 'RUNNING — OPEN DOOR = FAULT\nfull-power heat-up → PID hold\nsetTemp 40–80 °C', 390, 778, 300, 72),
 B('p8', 'FAN BURST LAW\nRH ≥ 60 % for 1 min → fan 100 % for 60 s', 390, 880, 300, 56),
 B('p9', 'TARGET WEIGHT\ncur / tgt / diff live · warn at T-5 min', 390, 962, 300, 56),
 B('fault', 'FAULTS — beep 5 s\n95 °C cut\nheater failure (flat 3 min)\nfan error (RH ≥ 60 % 5 min)\ndoor opened mid-cycle\nboth sensors dead\nbattery ≤ 10 %', 60, 778, 280, 170, 'danger'),
 B('d3', 'time up?', 410, 1044, 260, 100, 'decision'),
 B('d4', 'within 5 %\nof target?', 410, 1176, 260, 110, 'decision'),
 B('done1', 'DONE — purge 45 s (fan 100 %)', 390, 1322, 300, 56, 'terminator'),
 B('done2', 'DONE-WITH-WARNING — target missed\nflag on site/CSV + warn beep', 700, 1322, 320, 56, 'gold'),
 B('p10', 'POWER CUT (relay) · UNLOCK\nUNLOAD — CSV saved (wt_g · mode · target)', 390, 1404, 300, 72),
 B('p11', 'NEXT BATCH — door already unlocked', 390, 1502, 300, 56),
 B('offbox', 'BUTTON-1: hold 3 s = HARD OFF (latch drops) · hold 10 s = REBOOT\nFIRMWARE UPDATE: website → Firmware update (.bin)  or  Arduino IDE → smart-dehumidifier at 192.168.4.1 — refused while RUNNING', 60, 1600, 960, 90, 'note'),
 B('modesnote', 'MODES: AGARBATTI 60 °C (default) · USER DEFINED · SILICAGEL 80 °C — cycle with the C key / website / BUTTON-2', 60, 1710, 960, 56, 'gold', bold_first=False),
]
p2_edges = [
 E('t1', 'p1'), E('p1', 'p2'), E('p2', 'p3'), E('p3', 'd1'),
 E('d1', 'cal', 'NO', color='#d79b00'), E('cal', 'd1', 'done ✓', color='#82b366'),
 E('d1', 'p4', 'YES', color='#82b366'),
 E('p4', 'p5'), E('p5', 'p6'), E('p6', 'p7'), E('p7', 'p8'), E('p8', 'p9'), E('p9', 'd3'),
 E('d3', 'p7', 'no — keep drying', color='#d79b00', via=372),
 E('p7', 'fault', 'any fault', dashed=True, color='#b85450'),
 E('fault', 'p10', 'purge → power off', dashed=True, color='#b85450', via=28),
 E('d3', 'd4', 'YES', color='#82b366'),
 E('d4', 'done1', 'YES', color='#82b366'), E('d4', 'done2', 'NO', color='#d79b00'),
 E('done1', 'p10'), E('done2', 'p10'),
 E('p10', 'p11'), E('p11', 'p4', 'reload trays', via=352),
]

# ================= PAGE 3 : CODE WORKFLOW =================
p3_boxes = [
 B('t1_', 'SMART DEHUMIDIFIER — CODE WORKFLOW\nfirmware v2.x · zero libraries · every step mapped to its component', 40, 16, 1000, 46, 'note'),
 B('boothdr', '① BOOT — setup() → initSystem()', 60, 76, 300, 40, 'purple', bold_first=False),
 B('b1', 'supply::begin — LATCH HOLD GPIO14\nasserted within ms (or power dies)', 60, 130, 300, 50),
 B('b2', 'pinsUnusedSafe — every unused pad\nparked (pull-up / input)', 60, 192, 300, 50),
 B('b3', 'relay outputs OFF — ALL PINS LOW', 60, 254, 300, 50),
 B('b4', 'printBanner + loadSettings — NVS v7', 60, 316, 300, 50),
 B('b5', 'sensors.begin — AHT10 top +\ndht.begin — DHT22 return + DHT11 out', 60, 378, 300, 50),
 B('b6', 'battery.begin — divider ADC4 + gate 5', 60, 440, 300, 50),
 B('b7', 'buzzer.begin + bz::powerOn — KY-012\n2 beeps/s × 3 s', 60, 502, 300, 50),
 B('b8', 'keypad.begin — PCF #1 @ 0x20', 60, 564, 300, 50),
 B('b9', 'web::begin — hotspot + site\n(ARCHITECTS OF SOLUTIONS)', 60, 626, 300, 50),
 B('b10', 'scale.begin — HX711 1/2\n(saved calibration restored)', 60, 688, 300, 50),
 B('b11', 'door::begin(false) — limit switch up,\nlock stays LOW (init window)', 60, 750, 300, 50),
 B('b12', 'txdisp::begin — UART GPIO3 → UNO', 60, 812, 300, 50),
 B('b13', 'web::begin — hotspot + site\ndoor begin(false) + clockBoot (NVS)', 60, 874, 300, 50),
 B('b14', 'ArduinoOTA + mDNS — "smart-dehumidifier"', 60, 936, 300, 50),
 B('b15', '10 s INIT + CALIBRATION window\nsensors/battery/scale settle · splash', 60, 998, 300, 56, 'gold'),
 B('b16', 'supply::engage + door::engage\nrelay = toggle · lock = state', 60, 1066, 300, 50),
 B('b17', 'dryer.powerOn → IDLE (ready)', 60, 1128, 300, 50, 'terminator'),
 B('loophdr', '② loop() — every pass (cooperative, 0 = DRYER_RTOS)', 420, 130, 620, 40, 'purple', bold_first=False),
 B('looptasks', 'web::handle — site + captive DNS\nArduinoOTA.handle — IDE upload\nsensors.update — AHT10 + DHT22 avg\nbattery.update — ADC (5 s)\ndryer.tick — control core (1 s)\nkeypad.update → menu::key\nscale.update — HX711 (2 Hz)\ndoor::update — lock/limit workflow\nsupply::update — buttons/toggle/opto\ndht.update — DHT11 outdoor (10 s)\nrtc.sync — DS1302 boot restore · set-mirror\nserviceSerial — console\nserviceButton — BOOT btn (S3)\nbuzzer.update — beep edges\nmaybeAutoStart · maybeDiag (15 s)', 420, 182, 300, 440, 'process', bold_first=False),
 B('compmap', 'COMPONENTS DRIVEN\n\nphone (hotspot site)\nPC (OTA upload)\nAHT10 + DHT22 (chamber)\nbattery divider\nBTS7960 + 500 W coil\nL298N + outlet fan\nPCF #1 keypad\nPCF #2 relays · buzzer · buttons\ntoggle · opto · door switch\nHX711 + load cells\nAT24C256 cycle registry\ndoor limit switch\nDHT11 outdoor sensor\nDS1302 RTC (CR2032 coin cell)\nphone/tablet (web display\n+ full control page)', 760, 182, 280, 440, 'gold', bold_first=False),
 B('tickhdr', '③ dryer.tick() — the 1-second control core', 420, 640, 620, 36, 'purple', bold_first=False),
 B('t1', 'state machine — IDLE → RUNNING →\nCOOLDOWN → DONE / FAULT', 420, 692, 290, 56),
 B('t2', 'SAFETY: 95 °C hard cut ·\nsensors dead 15 s → FAULT', 420, 762, 290, 56, 'danger'),
 B('t3', 'WATCHDOGS: heater failure\n(flat 3 min) · fan error (RH 5 min)', 420, 832, 290, 56, 'danger'),
 B('t4', 'heatStep — PID/boost duty\n→ RPWM 12 (BTS7960 → coil)', 420, 902, 290, 56),
 B('t5', 'fanStep — burst law\n→ ENB 17 (L298N → fan)', 420, 972, 290, 56),
 B('t6', 'target weight — T-5 min warn\n+ suggestion (HX711 g/min)', 420, 1042, 290, 56),
 B('t7', 'completion gates —\ntime · RH · weight-rate', 420, 1112, 290, 56),
 B('t8', 'cyclelog record (10 s)\n→ LittleFS CSV (wt_g · mode)', 420, 1182, 290, 56),
 B('states', 'EVENTS INTO THE MACHINE\n\nstart() — refused until the scale is\ncalibrated (door gate) · captures\nbatch weight\nstop() — user stop → purge\npowerOn() — DONE/FAULT → IDLE\naddMinutes(+15) — live\napplyMode(0/1/2) — AGARBATTI /\nUSER / SILICAGEL presets\nfaultNow("door opened!")\nsetManualHeat(knob %, 60 s)', 740, 692, 300, 546, 'note', bold_first=False),
 B('webbox', '④ web::handle — the website on the hotspot\n/api/data 1 Hz JSON (everything) · /api/start 403-gated · /api/scale tare+cal\n/api/mode presets · /api/settime phone clock · /api/heat knob (60 s)\n/update OTA (refused while RUNNING) · /api/log.csv history · captive DNS', 420, 1330, 620, 150, 'green', bold_first=False),
 B('dataflow', 'DATA FLOW\nAHT10 → PID → PWM → coil\nRH → burst law → fan\ngrams → gates + target\nfaults → beep + site + CSV\nclock → NVS 30 min', 60, 1330, 340, 150, 'note', bold_first=False),
 B('rtos', 'DRYER_RTOS 1 = the same work split into 5 FreeRTOS tasks: web @ core 0 · sensors / control / panel / power @ core 1 · Wire mutex protects the shared I2C bus', 60, 1250, 1000, 60, 'note', bold_first=False),
 B('files', 'FILE MAP — main.cpp (boot · loop · console · watchdogs) · control.* (state machine · PID · fan law · gates) · sensors/aht10.* · battery.* · buzzer.* · keypad.* (PCF #1) ·\nscale.* (HX711) · door.* (workflow) · supply.* (latch · selector) · menu.* (keypad UI) · dht.* (DHT22 return + DHT11 out) · rtc.* (DS1302 clock) · web/webui (site — incl. /display kiosk) · cyclelog (CSV)', 60, 1500, 1000, 110, 'note', bold_first=False),
]
p3_edges = (
  [E(f'b{i}', f'b{i+1}') for i in range(1, 17)] +
  [E('b17', 'loophdr', 'then', color='#82b366'),
   E('looptasks', 't1', '1 s', color='#d79b00')] +
  [E(f't{i}', f't{i+1}') for i in range(1, 8)]
)

PAGES = [
    dict(name='1 · System Design', boxes=p1_boxes, edges=p1_edges, svg='design-system.svg'),
    dict(name='2 · Operating Workflow', boxes=p2_boxes, edges=p2_edges, svg='design-workflow.svg'),
    dict(name='3 · Code Workflow', boxes=p3_boxes, edges=p3_edges, svg='design-code.svg'),
]

# ---------------- draw.io emitter ----------------
def esc(s):
    return escape(s, {'"': '&quot;'})

def dvalue(box):
    lines = box['label'].split('\n')
    html = []
    for i, ln in enumerate(lines):
        e = esc(ln)
        if i == 0 and box.get('bold', True) and len(lines) > 1:
            e = f'<b>{e}</b>'
        html.append(e)
    return esc('<br>'.join(html)) if len(lines) > 1 else esc(lines[0])

def dstyle(box):
    fill, stroke = COLORS[box['kind']]
    if box['kind'] == 'decision':
        s = 'rhombus;whiteSpace=wrap;html=1;'
    else:
        s = 'rounded=1;whiteSpace=wrap;html=1;'
    s += f'fillColor={fill};strokeColor={stroke};fontSize=12;'
    if box.get('dashed'):
        s += 'dashed=1;'
    return s

def make_drawio(path):
    pages_xml = []
    for n, page in enumerate(PAGES):
        cells = ['<mxCell id="0" />', '<mxCell id="1" parent="0" />']
        for b in page['boxes']:
            cells.append(
                f'<mxCell id="{b["id"]}" value="{dvalue(b)}" style="{dstyle(b)}" vertex="1" parent="1">'
                f'<mxGeometry x="{b["x"]}" y="{b["y"]}" width="{b["w"]}" height="{b["h"]}" as="geometry" />'
                f'</mxCell>')
        for i, e in enumerate(page['edges']):
            st = ('edgeStyle=orthogonalEdgeStyle;rounded=1;html=1;strokeWidth=2;'
                  f'endArrow=block;strokeColor={e["color"]};')
            if e['dashed']:
                st += 'dashed=1;'
            lbl = escape(e['label']) if e['label'] else ''
            cells.append(
                f'<mxCell id="e{n}_{i}" value="{lbl}" style="{st}" edge="1" parent="1" '
                f'source="{e["src"]}" target="{e["dst"]}">'
                f'<mxGeometry relative="1" as="geometry" /></mxCell>')
        pages_xml.append(
            f'<diagram id="page{n}" name="{escape(page["name"])}">'
            f'<mxGraphModel dx="1400" dy="900" grid="1" gridSize="10" guides="1" tooltips="1" '
            f'connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="{W}" pageHeight="{H}" '
            f'math="0" shadow="0"><root>{"".join(cells)}</root></mxGraphModel></diagram>')
    xml = ('<mxfile host="app.diagrams.net" version="24.7.5">'
           + ''.join(pages_xml) + '</mxfile>')
    open(path, 'w').write(xml)
    print('written', path, f'({len(xml)/1024:.1f} KB, {len(PAGES)} pages)')

# ---------------- SVG emitter (preview) ----------------
def rect_point(b, tx, ty):     # border intersection of center->(tx,ty)
    cx, cy = b['x'] + b['w'] / 2, b['y'] + b['h'] / 2
    dx, dy = tx - cx, ty - cy
    if dx == 0 and dy == 0:
        return cx, cy
    t = min((b['w'] / 2) / abs(dx) if dx else 1e9, (b['h'] / 2) / abs(dy) if dy else 1e9)
    return cx + dx * t, cy + dy * t

def make_svg(page, path):
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" '
           f'font-family="Segoe UI, Arial, sans-serif">',
           '<defs><marker id="arr" markerWidth="9" markerHeight="9" refX="8" refY="4" '
           'orient="auto"><path d="M0,0 L8,4 L0,8 z" fill="#444"/></marker></defs>',
           f'<rect width="{W}" height="{H}" fill="#ffffff"/>']
    byid = {b['id']: b for b in page['boxes']}
    for e in page['edges']:
        s, t = byid[e['src']], byid[e['dst']]
        dash = ' stroke-dasharray="6 4"' if e['dashed'] else ''
        col = e['color'] if e['color'] != '#595959' else '#444'
        if e['via'] is not None:                     # left-side loop route
            x = e['via']
            a = (s['x'], s['y'] + s['h'] / 2)
            b_ = (t['x'], t['y'] + t['h'] / 2)
            pts = [a, (x, a[1]), (x, b_[1]), b_]
            pts_s = ' '.join(f'{p[0]:.0f},{p[1]:.0f}' for p in pts)
            out.append(f'<polyline points="{pts_s}" fill="none" stroke="{col}" '
                       f'stroke-width="2" marker-end="url(#arr)"{dash}/>')
            lx, ly = x, (a[1] + b_[1]) / 2
        else:
            sc, tc = (s['x'] + s['w'] / 2, s['y'] + s['h'] / 2), (t['x'] + t['w'] / 2, t['y'] + t['h'] / 2)
            x1, y1 = rect_point(s, *tc)
            x2, y2 = rect_point(t, *sc)
            out.append(f'<line x1="{x1:.0f}" y1="{y1:.0f}" x2="{x2:.0f}" y2="{y2:.0f}" '
                       f'stroke="{col}" stroke-width="2" marker-end="url(#arr)"{dash}/>')
            lx, ly = (x1 + x2) / 2, (y1 + y2) / 2
        if e['label']:
            out.append(f'<text x="{lx:.0f}" y="{ly - 6:.0f}" font-size="12" text-anchor="middle" '
                       f'fill="#333" stroke="#ffffff" stroke-width="4" paint-order="stroke">{escape(e["label"])}</text>')
    for b in page['boxes']:
        fill, stroke = COLORS[b['kind']]
        dash = ' stroke-dasharray="6 4"' if b.get('dashed') else ''
        if b['kind'] == 'decision':
            cx, cy = b['x'] + b['w'] / 2, b['y'] + b['h'] / 2
            pts = f'{cx},{b["y"]} {b["x"]+b["w"]},{cy} {cx},{b["y"]+b["h"]} {b["x"]},{cy}'
            out.append(f'<polygon points="{pts}" fill="{fill}" stroke="{stroke}" stroke-width="2"{dash}/>')
        else:
            out.append(f'<rect x="{b["x"]}" y="{b["y"]}" width="{b["w"]}" height="{b["h"]}" '
                       f'rx="10" fill="{fill}" stroke="{stroke}" stroke-width="2"{dash}/>')
        lines = b['label'].split('\n')
        lh = 17
        y0 = b['y'] + b['h'] / 2 - (len(lines) - 1) * lh / 2 + 5
        for i, ln in enumerate(lines):
            weight = ' font-weight="bold"' if (i == 0 and b.get('bold', True) and len(lines) > 1) else ''
            out.append(f'<text x="{b["x"] + b["w"] / 2:.0f}" y="{y0 + i * lh:.0f}" font-size="13" '
                       f'text-anchor="middle" fill="#1a1a1a"{weight}>{escape(ln)}</text>')
    out.append('</svg>')
    open(path, 'w').write(''.join(out))
    print('written', path)

if __name__ == '__main__':
    root = os.path.join(os.path.dirname(__file__), '..')
    make_drawio(os.path.join(root, 'docs', 'design-workflow.drawio'))
    for page in PAGES:
        make_svg(page, os.path.join(root, 'docs', page['svg']))