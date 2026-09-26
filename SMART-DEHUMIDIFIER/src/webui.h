/**
 * @file webui.h
 * @brief The dryer dashboard, embedded in flash (PROGMEM).
 *
 * Two slides:
 *   [Dashboard ]  live dump of all data + gauges + multi-series graph + controls
 *   [Parameters]  EVERY setting editable on one page + one-click reset to
 *                 factory defaults + manually-set drying time + Save & Start
 *
 * 100 % offline: no CDN, no internet - the ESP32 hotspot IS the network.
 * Pro toolkit: SVG ring gauges, canvas chart engine with dual axes,
 * hover crosshair + tooltip, legend toggles, past-cycle graph viewer.
 *
 * NOTE FOR MAINTAINERS: many element ids and a few exact markup anchors
 * (the <nav> tag, the btnClearCyc controls block, 'use strict'; ) are
 * depended on by tools/online/sync-online.js and tools/ui-test - keep them.
 */

#pragma once

static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Dehumidifier</title>
<style>
:root{
--rb:#1b45b5;--rb2:#2a58d0;--rbd:#10307f;--gb:#996515;--gb2:#b8862b;
--gold:#e9c46a;--gold2:#f6dd9c;
--bg:#1b45b5;--bg2:rgba(8,24,80,.5);
--card:rgba(19,48,142,.88);--card2:rgba(30,64,170,.92);
--line:rgba(233,196,106,.24);--line2:rgba(233,196,106,.46);
--tx:#fff8ea;--dim:#d6def4;--dim2:#b2c0e4;
--ok:#4ade80;--warn:#f7c552;--hot:#ff7b7b;--cy:#8fd6ff;--am:#e9c46a;
--acc:#7aa7ff;--acc2:#2f5fe0;--vio:#c4b5fd;
--sh:0 10px 28px rgba(6,16,58,.42);--r:16px;
--lines:repeating-linear-gradient(180deg,rgba(255,255,255,.035) 0 1px,transparent 1px 4px);
/* v2.0.23 ROYAL & GOLD: royal-blue sky over a golden-brown horizon, joined
   by a warm gold seam (a plain blue->brown blend turns muddy grey). Never
   black. The swatches in the header swap --bgimg (themes below). */
--bgimg:radial-gradient(110% 40% at 50% 74%,rgba(247,201,111,.34),rgba(247,201,111,0) 70%),
 linear-gradient(180deg,#2352c9 0%,#1d48b8 36%,#3560c8 55%,#c4914a 68%,#a8741d 80%,#8a5a14 100%)}
body.th-blue{--bgimg:radial-gradient(85% 55% at 100% 108%,rgba(212,162,76,.62),rgba(212,162,76,0) 70%),
 radial-gradient(90% 60% at 0% -5%,#3f6ae6,rgba(63,106,230,0) 62%),linear-gradient(170deg,#2654d0,#1b45b5 55%,#16389a)}
body.th-brown{--bgimg:radial-gradient(90% 55% at 0% 0%,rgba(80,120,235,.55),rgba(80,120,235,0) 60%),
 linear-gradient(180deg,#1d48b8 0%,#3560c8 13%,#c4914a 25%,#a8741d 46%,#8a5a14 100%)}
body.th-diag{--bgimg:linear-gradient(152deg,#2352c9 0%,#1b45b5 44%,#e1b35a 49.4%,#b98526 51%,#996515 72%,#7d5112 100%)}
body.th-sapphire{--bgimg:radial-gradient(85% 45% at 100% 108%,rgba(196,145,74,.55),rgba(196,145,74,0) 70%),
 radial-gradient(90% 60% at 0% 0%,#3561dc,rgba(53,97,220,0) 62%),linear-gradient(170deg,#1a3fa8,#132f8c 60%,#10297a)}
body.th-bronze{--bgimg:radial-gradient(80% 50% at 100% 0%,rgba(246,221,156,.35),rgba(246,221,156,0) 62%),
 radial-gradient(70% 45% at 0% 0%,rgba(42,88,208,.6),rgba(42,88,208,0) 60%),linear-gradient(170deg,#b8862b,#996515 50%,#6e4610)}
body.th-custom{--bgimg:radial-gradient(100% 45% at 50% 105%,rgba(212,162,76,.45),rgba(212,162,76,0) 70%),linear-gradient(var(--bg),var(--bg))}
body.nolines{--lines:linear-gradient(transparent,transparent)}
*{box-sizing:border-box;margin:0;padding:0}
html{-webkit-text-size-adjust:100%;background:#1b45b5}
body{color:var(--tx);font:14px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
padding-bottom:56px;font-variant-numeric:tabular-nums;min-height:100vh}
/* fixed backdrop layer (background-attachment:fixed is ignored on iOS) */
body::before{content:"";position:fixed;inset:0;z-index:-1;pointer-events:none;
background:var(--lines),var(--bgimg)}
button,input,select{font:inherit}
/* ---------- header ---------- */
header{display:flex;flex-wrap:wrap;gap:10px 16px;align-items:center;
justify-content:space-between;padding:16px 20px 14px;
border-bottom:1px solid rgba(233,196,106,.5);
background:linear-gradient(180deg,rgba(12,34,104,.66),rgba(12,34,104,.3));
box-shadow:0 6px 22px rgba(6,16,58,.22)}
.brand{display:flex;gap:12px;align-items:center}
.logo{width:42px;height:42px;border-radius:13px;display:grid;place-items:center;
font-size:21px;background:linear-gradient(135deg,#f6dd9c,#b8862b 55%,#7a4f12);
box-shadow:inset 0 0 0 1px rgba(255,248,234,.6),var(--sh)}
header h1{font-size:17px;font-weight:750;letter-spacing:.5px;color:var(--gold2);
text-shadow:0 1px 3px rgba(6,16,58,.4)}
header .sub{color:var(--dim);font-size:11.5px;margin-top:1px}
.hstat{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.pill{display:inline-flex;align-items:center;gap:7px;padding:6px 13px;border-radius:999px;
font-size:12px;font-weight:700;letter-spacing:.6px;border:1px solid var(--line2);
background:rgba(10,30,96,.58);color:var(--dim);white-space:nowrap}
button.pill{cursor:pointer}button.pill:hover{border-color:var(--gold);color:var(--tx)}
.pill .dot{width:8px;height:8px;border-radius:50%;background:currentColor;
box-shadow:0 0 10px currentColor;animation:pulse 2s infinite}
@keyframes pulse{50%{opacity:.45}}
.p-idle{color:var(--dim)}.p-run{color:var(--ok);border-color:rgba(74,222,128,.55)}
.p-purge{color:var(--warn);border-color:rgba(247,197,82,.6)}.p-done{color:#c9dbff;border-color:rgba(122,167,255,.65)}
.p-fault{color:#ffc4c4;border-color:rgba(255,123,123,.75);background:rgba(120,18,34,.55)}.p-by{color:var(--warn)}
#clock{font-size:12.5px;color:var(--tx);border:1px solid var(--line);
border-radius:10px;padding:5px 11px;background:rgba(10,30,96,.58);white-space:nowrap}
/* ---------- tabs: golden-brown + royal-blue, on a floating glass bar ---------- */
nav{display:flex;gap:10px;padding:12px 16px 12px;max-width:940px;margin:0 auto;
position:sticky;top:0;z-index:40;background:rgba(14,38,116,.62);
-webkit-backdrop-filter:blur(10px);backdrop-filter:blur(10px);
border:1px solid rgba(233,196,106,.28);border-top:0;border-radius:0 0 18px 18px;
box-shadow:0 8px 22px rgba(6,16,58,.28)}
nav button{flex:1;padding:12px 8px;border:1.5px solid rgba(246,221,156,.85);border-radius:13px;
font-size:13.5px;font-weight:750;cursor:pointer;transition:.18s;letter-spacing:.3px;opacity:.86;
box-shadow:0 0 10px rgba(233,196,106,.3),inset 0 0 10px rgba(255,255,255,.2)}
nav button.fire{background:linear-gradient(120deg,#b8862b,#e3b75f 30%,#f6dd9c 50%,#e3b75f 70%,#b8862b);
background-size:200% 100%;color:#2b1a02}
nav button.water{background:linear-gradient(120deg,#6f95f5,#9db8ff 30%,#dbe6ff 50%,#9db8ff 70%,#6f95f5);
background-size:200% 100%;color:#06163f}
nav button:hover{transform:translateY(-1px);opacity:1;filter:brightness(1.08);
box-shadow:0 0 16px rgba(233,196,106,.55)}
nav button.on{opacity:1;animation:shine 3s linear infinite;
box-shadow:0 0 18px rgba(233,196,106,.7),0 0 26px rgba(122,167,255,.35),inset 0 0 14px rgba(255,255,255,.3)}
@keyframes shine{0%{background-position:0% 0}100%{background-position:200% 0}}
.bgsw{display:inline-block;width:30px;height:30px;border-radius:9px;margin:3px;cursor:pointer;
border:1px solid var(--line2);box-shadow:inset 0 0 7px rgba(255,255,255,.25),0 0 6px rgba(233,196,106,.3)}
.bgsw.on{outline:2px solid var(--gold);outline-offset:2px}
/* ---------- layout ---------- */
section{display:none;padding:14px 16px;max-width:940px;margin:0 auto;animation:fadein .25s}
section.on{display:block}
section>h2{text-shadow:0 1px 3px rgba(6,16,58,.45)}
@keyframes fadein{from{opacity:0;transform:translateY(4px)}to{opacity:1}}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:12px}
.card{background:linear-gradient(180deg,var(--card2),var(--card));border:1px solid var(--line);
border-radius:var(--r);padding:15px 16px;box-shadow:var(--sh);position:relative;overflow:hidden}
.card::before,.chartbox::before,.fsec::before{content:"";position:absolute;left:12%;right:12%;top:0;height:1px;
background:linear-gradient(90deg,transparent,rgba(246,221,156,.85),transparent)}
.card h3{color:var(--gold2);font-size:10.5px;text-transform:uppercase;letter-spacing:1.2px;
font-weight:750;margin-bottom:8px;display:flex;align-items:center;gap:7px}
.card h3 .sp{flex:1}
.big{font-size:31px;font-weight:750;letter-spacing:-.5px;white-space:nowrap}
.unit{font-size:13px;color:var(--dim);font-weight:500}
.sml{font-size:11.5px;color:var(--dim);margin-top:5px}
.sml b{color:var(--tx);font-weight:650}
.dot{display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--ok);
margin-right:4px;box-shadow:0 0 8px rgba(74,222,128,.7)}
.dot.off{background:var(--hot);box-shadow:0 0 8px rgba(255,123,123,.7)}
/* ---------- gauges ---------- */
.gwrap{display:flex;align-items:center;gap:14px}
.gring{width:100px;height:100px;flex:none}
.gring .bgc{fill:none;stroke:rgba(255,255,255,.14);stroke-width:9}
.gring .fgc{fill:none;stroke-width:9;stroke-linecap:round;
transition:stroke-dashoffset .8s cubic-bezier(.22,1,.36,1)}
.gring .mk{stroke:var(--tx);stroke-width:2.5;stroke-linecap:round;opacity:.85}
.gval{font-size:27px;font-weight:750;line-height:1.05;white-space:nowrap}
.gmeta{font-size:11px;color:var(--dim);margin-top:3px}
/* ---------- bars ---------- */
.bar{height:9px;border-radius:6px;background:rgba(6,18,64,.55);border:1px solid var(--line);
overflow:hidden;margin-top:9px}
.bar i{display:block;height:100%;border-radius:6px;transition:width .8s}
.bar.heat i{background:linear-gradient(90deg,#b8862b,#ff9a76);box-shadow:0 0 12px rgba(255,154,118,.45)}
.bar.fan i{background:linear-gradient(90deg,#4f7df0,var(--cy));box-shadow:0 0 12px rgba(143,214,255,.4)}
/* ---------- controls ---------- */
.controls{display:flex;flex-wrap:wrap;gap:10px;margin:14px 0}
button.act{padding:12px 20px;border:1px solid var(--line2);border-radius:13px;font-size:14px;font-weight:750;
cursor:pointer;transition:.16s;letter-spacing:.3px;color:var(--tx);
background:linear-gradient(180deg,rgba(50,88,200,.96),rgba(26,60,164,.96));box-shadow:0 6px 16px rgba(6,16,58,.35)}
button.act:hover:not(:disabled){transform:translateY(-2px);filter:brightness(1.12)}
button.act:active:not(:disabled){transform:translateY(0)}
button.act:disabled{cursor:not-allowed;box-shadow:none!important;background:rgba(8,24,80,.45)!important;
color:rgba(255,248,234,.45)!important;border-color:rgba(233,196,106,.2)!important}
#btnStart,#btnGo{background:linear-gradient(135deg,#f6dd9c,#d4a24c 45%,#b8862b);color:#2b1a02;
border-color:rgba(255,248,234,.65);box-shadow:0 6px 18px rgba(184,134,43,.45)}
#btnStop{background:linear-gradient(135deg,#f04262,#be123c);color:#fff;border-color:rgba(255,200,205,.5)}
#btnPower{background:linear-gradient(135deg,#4f7df0,#1d48b8);color:#eef4ff}
#btnDef{background:linear-gradient(135deg,#c9973a,#996515 55%,#7a4f12);color:#fff8ea;border-color:var(--gold2)}
.tabbtn{padding:10px 8px;border-radius:11px;border:1px solid var(--line2);background:rgba(8,24,80,.45);
color:var(--dim);font-weight:750;font-size:12.5px;letter-spacing:.4px;cursor:pointer;transition:.15s}
.tabbtn:hover{color:var(--tx);border-color:var(--gold)}
.tabbtn.on{background:linear-gradient(135deg,#f6dd9c,#d4a24c 50%,#b8862b);color:#2b1a02;
border-color:rgba(255,248,234,.7);box-shadow:0 0 12px rgba(233,196,106,.45)}
.wtctl{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:9px}
.wtctl input{grid-column:1/-1;padding:8px 11px}
.wtctl button.act{padding:9px 10px;font-size:13px}
/* ---------- chart ---------- */
.chartbox{background:linear-gradient(180deg,var(--card2),var(--card));border:1px solid var(--line);
border-radius:var(--r);padding:15px;margin-top:12px;box-shadow:var(--sh);position:relative}
.chartbox>b{font-size:13px;color:var(--gold2)}
.legend{display:flex;flex-wrap:wrap;gap:6px;margin:10px 0 6px}
.lg{display:inline-flex;align-items:center;gap:6px;padding:5px 11px;border-radius:999px;
border:1px solid var(--line2);background:var(--bg2);color:var(--dim);font-size:11.5px;
font-weight:650;cursor:pointer;transition:.15s;user-select:none}
.lg:hover{color:var(--tx)}
.lg i{width:9px;height:9px;border-radius:3px;background:var(--c)}
.lg.on{color:var(--tx);border-color:color-mix(in srgb,var(--c) 60%,transparent);
background:color-mix(in srgb,var(--c) 22%,rgba(8,24,80,.5))}
canvas{width:100%;display:block;border-radius:10px}
#chart,#cycChart{height:250px}
.tip{position:absolute;pointer-events:none;background:rgba(10,28,90,.96);border:1px solid var(--line2);
border-radius:10px;padding:8px 11px;font-size:11.5px;line-height:1.7;opacity:0;
transition:opacity .12s;z-index:60;white-space:nowrap;box-shadow:var(--sh)}
.tip b{font-weight:700}
/* ---------- tables ---------- */
table{width:100%;border-collapse:collapse;font-size:13px}
td,th{padding:9px 8px;border-bottom:1px solid var(--line);text-align:left}
tr:last-child td{border-bottom:none}
td:last-child,th:last-child{text-align:right}
th{color:var(--gold2);font-size:10px;text-transform:uppercase;letter-spacing:1px;opacity:.9}
tbody tr{transition:.12s}
tbody tr:hover{background:rgba(233,196,106,.07)}
td:last-child{font-weight:700}
.badge{display:inline-block;padding:3px 10px;border-radius:20px;font-size:11px;
font-weight:750;letter-spacing:.4px;white-space:nowrap}
.b-idle{background:rgba(255,255,255,.14);color:var(--dim)}.b-run{background:rgba(122,167,255,.26);color:#dfe9ff}
.b-purge{background:rgba(233,196,106,.24);color:var(--gold2)}.b-done{background:rgba(122,167,255,.26);color:#dfe9ff}
.b-fault{background:rgba(255,123,123,.24);color:#ffcaca}.b-by{background:rgba(247,197,82,.24);color:var(--warn)}
a.dl{color:var(--cy);font-size:13px;text-decoration:none;font-weight:650}
a.dl:hover{text-decoration:underline}
.vbtn{background:rgba(8,24,80,.45);border:1px solid var(--line2);color:var(--cy);border-radius:9px;
padding:5px 10px;font-size:11.5px;font-weight:700;cursor:pointer}
.vbtn:hover{border-color:var(--gold)}
/* ---------- forms ---------- */
form{background:none;border:none;padding:0}
.fsec{background:linear-gradient(180deg,var(--card2),var(--card));border:1px solid var(--line);
border-radius:var(--r);padding:15px;margin-bottom:12px;box-shadow:var(--sh);position:relative}
.fsec>h4{font-size:11px;text-transform:uppercase;letter-spacing:1.1px;color:var(--gold2);
margin-bottom:11px;display:flex;gap:8px;align-items:center}
.frow{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px;margin-bottom:10px}
.frow:last-child{margin-bottom:0}
label{font-size:11px;color:var(--dim);display:block;margin-bottom:5px;font-weight:600;
letter-spacing:.2px}
input,select{width:100%;padding:10px 12px;border-radius:11px;border:1px solid var(--line2);
background:rgba(8,24,80,.6);color:var(--tx);font-size:14.5px;transition:.15s}
input:focus,select:focus{outline:none;border-color:var(--gold);
box-shadow:0 0 0 3px rgba(233,196,106,.22)}
input[type=number]::-webkit-inner-spin-button{opacity:.5}
input[type=range]{padding:0;border:0;background:none;box-shadow:none}
input[type=color]{width:46px;height:30px;padding:2px;flex:none}
input[type=checkbox]{width:18px;height:18px;flex:none;accent-color:#e9c46a}
select option{background:#10307f;color:var(--tx)}
details{margin:10px 0 0;padding:11px 13px;border:1px dashed var(--line2);border-radius:12px;
background:rgba(8,24,80,.3)}
summary{color:var(--dim);cursor:pointer;font-size:12.5px;font-weight:600}
summary:hover{color:var(--tx)}
.chk{display:flex;align-items:center;gap:11px;font-size:13.5px;color:var(--tx);
padding:10px 2px;cursor:pointer;user-select:none}
.chk input{appearance:none;-webkit-appearance:none;width:42px;height:24px;border-radius:999px;
background:rgba(255,255,255,.18);border:1px solid var(--line2);position:relative;cursor:pointer;
transition:.2s;flex:none;padding:0}
.chk input::after{content:"";position:absolute;top:2px;left:2px;width:18px;height:18px;
border-radius:50%;background:#dfe6f7;transition:.2s}
.chk input:checked{background:linear-gradient(135deg,#f6dd9c,#b8862b);border-color:transparent}
.chk input:checked::after{left:20px;background:#fff8ea}
.chk input:checked+span{color:var(--tx)}
/* ---------- misc ---------- */
.fault{background:linear-gradient(135deg,rgba(158,22,48,.94),rgba(98,10,30,.94));border:1px solid #ff8a9a;
color:#ffe3e7;padding:12px 15px;border-radius:13px;margin-bottom:12px;display:none;
font-weight:650;box-shadow:var(--sh)}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%) translateY(8px);
background:linear-gradient(135deg,#f6dd9c,#b8862b);color:#2b1a02;padding:11px 22px;
border-radius:999px;font-weight:750;font-size:13.5px;opacity:0;transition:.25s;
pointer-events:none;z-index:99;box-shadow:0 10px 30px rgba(184,134,43,.45)}
#toast.err{background:linear-gradient(135deg,#f04262,#be123c);color:#fff}
.note{font-size:11.5px;color:var(--dim2);margin-top:8px}
.note b{color:var(--dim)}
/* notes that sit straight on the backdrop get a glass strip (readable on gold too) */
section>.note{background:rgba(14,38,116,.86);border:1px solid var(--line);border-radius:12px;
padding:8px 12px;color:var(--dim)}
section>.note b{color:var(--tx)}
.wxbox{display:flex;gap:13px;align-items:flex-start}
.wxicon{font-size:37px;line-height:1;filter:drop-shadow(0 4px 10px rgba(0,0,0,.4))}
.wxrows{font-size:12px;color:var(--dim);line-height:1.85;margin-top:2px}
.wxrows b{color:var(--tx);font-weight:650}
/* v2.0.19: virtual keypad drawer (touch displays) */
#vpDrawer{display:none;position:fixed;top:0;right:0;bottom:0;width:min(400px,100vw);
background:linear-gradient(180deg,#1d48b8,#10307f 70%,#7a4f12);border-left:1px solid var(--line2);box-shadow:var(--sh);
z-index:80;flex-direction:column;padding:14px;gap:10px;overflow:auto}
#vpDrawer.on{display:flex}
#vpHead{display:flex;justify-content:space-between;align-items:center}
#vpText{flex:1;overflow:auto;font-size:13px;line-height:2;color:var(--dim);background:rgba(8,24,80,.4);
border:1px dashed var(--line2);border-radius:10px;padding:10px;min-height:110px}
.vpRow{display:flex;justify-content:space-between;gap:10px}
.vpSel{color:var(--gold2);font-weight:700}
.vpPad{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.vpPad button{padding:12px 0;font-size:17px;font-weight:700;border-radius:12px;
border:1px solid var(--line2);background:rgba(8,24,80,.55);color:var(--tx);cursor:pointer}
.vpPad button:active{transform:scale(.93);background:rgba(233,196,106,.28)}
.vpPad button small{display:block;font-size:9px;font-weight:600;color:var(--dim)}
@media(max-width:560px){.big{font-size:26px}.gval{font-size:23px}.gring{width:88px;height:88px}
header{padding:13px 14px}nav{padding:10px 12px}nav button{font-size:12.5px;padding:10px 4px}}
</style></head><body>
<header>
  <div class="brand"><div class="logo">&#127807;</div>
    <div><h1 id="brand">ARCHITECTS OF SOLUTIONS</h1>
    <div class="sub">v<span id="fw">--</span> &middot; solar &middot; offline &middot; yours &mdash; 192.168.4.1</div></div></div>
  <div class="hstat">
    <span id="stBadge" class="pill p-idle"><span class="dot"></span>IDLE</span>
    <span id="clock">&#9201; --</span>
    <span id="doorPill" class="pill p-purge" style="display:none">&#128274; CALIBRATE</span>
    <span id="supPill" class="pill p-purge">&#9728; SOLAR</span>
    <a href="/display" target="_blank" class="pill p-run" style="text-decoration:none">&#128421; Display view</a>
    <button class="pill" onclick="vpOpen()" title="Virtual 4x4 keypad - the on-device menu, by touch">&#9000; Keypad</button>
    <span style="position:relative">
      <button class="pill" onclick="toggleBg()" title="Theme / background">&#127912; Theme</button>
      <div id="bgPanel" style="display:none;position:absolute;right:0;top:44px;z-index:60;
        background:linear-gradient(180deg,#2350c8,#12318a);border:1px solid var(--line2);border-radius:14px;padding:12px;
        box-shadow:var(--sh);width:250px">
        <div style="font-size:12px;color:var(--gold2);margin-bottom:8px;font-weight:700">THEME &middot; royal blue &amp; golden brown</div>
        <div id="bgSwatches"></div>
        <label style="display:flex;gap:7px;align-items:center;font-size:12px;color:var(--dim);margin-top:9px">
          <input type="checkbox" id="bgLines" onchange="bgSet()"> hair lines</label>
        <label style="display:flex;gap:7px;align-items:center;font-size:12px;color:var(--dim);margin-top:6px">
          custom colour <input type="color" id="bgColor" onchange="bgSet(this.value)"></label>
      </div>
    </span>
  </div>
</header>

<nav>
  <button id="tabDash" class="on fire" onclick="showTab('Dash')">&#128293; Dashboard</button>
  <button id="tabSet" class="water" onclick="showTab('Set')">&#128167; Parameters</button>
</nav>

<!-- ================= SLIDE: DASHBOARD ================= -->
<section id="secDash" class="on">
  <div id="faultBox" class="fault"></div>
  <div id="warnBox" class="fault" style="background:linear-gradient(135deg,rgba(184,134,43,.95),rgba(122,79,18,.95));border-color:#f6dd9c;color:#fff8ea;display:none"></div>
  <div id="doneCard" class="fault" style="background:linear-gradient(135deg,rgba(22,110,58,.94),rgba(10,66,36,.94));border-color:#5ee89a;color:#eafff1;display:none"></div>
  <div class="controls">
    <button class="act" id="btnStart" onclick="api('/api/start')">&#9654; Start</button>
    <button class="act" id="btnStop" onclick="api('/api/stop')">&#9632; Stop</button>
    <button class="act" id="btnAdd" onclick="api('/api/addtime?min=15')">+15 min</button>
    <button class="act" id="btnPower" onclick="api('/api/power')">&#9211; Power On</button>
  </div>
  <!-- v2.0.18: cycle-data downloads right on the home page -->
  <div class="note" style="margin:8px 0 12px">&#128190; <b>Cycle data:</b>
    <a class="dl" href="/eelog.csv" download>&#11015; ALL cycles &mdash; EEPROM registry (<span id="eeN2">--</span>)</a>
    &nbsp;&middot;&nbsp;
    <a class="dl" href="/lastcycle.csv" download>&#11015; latest cycle &mdash; full detail</a>
    &nbsp;&middot;&nbsp; per-batch files: &ldquo;Past cycles&rdquo; below</div>

  <div class="grid">
    <!-- gauge: temperature -->
    <div class="card"><h3>&#127777; Chamber temp</h3>
      <div class="gwrap">
        <svg class="gring" viewBox="0 0 120 120">
          <circle class="bgc" cx="60" cy="60" r="52"/>
          <circle class="fgc" id="g_t" cx="60" cy="60" r="52" stroke="#f2c14e"
            stroke-dasharray="326.7" stroke-dashoffset="326.7"
            transform="rotate(-90 60 60)"/>
          <line class="mk" id="g_tmark" x1="60" y1="6" x2="60" y2="17"
            transform="rotate(0 60 60)" style="display:none"/>
        </svg>
        <div><div class="gval"><span id="tAvg">--</span><span class="unit"> &deg;C</span></div>
        <div class="gmeta">target <b id="setT">--</b> &deg;C</div></div>
      </div>
      <div class="sml" style="margin-top:9px"><span class="dot" id="d1"></span>S1 <b id="t1">--</b>&deg;
      &nbsp; <span class="dot" id="d2"></span>S2 <b id="t2">--</b>&deg;</div>
    </div>
    <!-- gauge: humidity -->
    <div class="card"><h3>&#128167; Humidity</h3>
      <div class="gwrap">
        <svg class="gring" viewBox="0 0 120 120">
          <circle class="bgc" cx="60" cy="60" r="52"/>
          <circle class="fgc" id="g_h" cx="60" cy="60" r="52" stroke="#4cc3ff"
            stroke-dasharray="326.7" stroke-dashoffset="326.7"
            transform="rotate(-90 60 60)"/>
          <line class="mk" id="g_hlo" x1="60" y1="6" x2="60" y2="17" style="display:none"/>
          <line class="mk" id="g_hhi" x1="60" y1="6" x2="60" y2="17" style="display:none"/>
        </svg>
        <div><div class="gval"><span id="hAvg">--</span><span class="unit"> %RH</span></div>
        <div class="gmeta">band <b id="setH">--</b> %RH</div></div>
      </div>
      <div class="sml" style="margin-top:9px">S1 <b id="h1">--</b> &middot; S2 <b id="h2">--</b>
      &middot; peak <b id="hMax">--</b></div>
    </div>
    <!-- gauge: battery -->
    <div class="card"><h3>&#128267; Battery</h3>
      <div class="gwrap">
        <svg class="gring" viewBox="0 0 120 120">
          <circle class="bgc" cx="60" cy="60" r="52"/>
          <circle class="fgc" id="g_b" cx="60" cy="60" r="52" stroke="#6ee7b7"
            stroke-dasharray="326.7" stroke-dashoffset="326.7"
            transform="rotate(-90 60 60)"/>
        </svg>
        <div><div class="gval"><span id="bat">--</span><span class="unit"> %</span></div>
        <div class="gmeta"><b id="bv">--</b> V &middot; <span id="batType">--</span></div></div>
      </div>
      <div class="sml" style="margin-top:9px">safe shutdown &le; <b id="setCut">--</b> %</div>
    </div>
    <!-- weigh scale -->
    <div class="card"><h3>&#9878; Batch weight <span class="sp"></span><span id="wtBadge"
      class="badge b-idle" style="display:none">&#10004; SETTLED</span></h3>
      <div class="big"><span id="wt">--</span><span class="unit"> g</span></div>
      <div class="sml" id="wtTgt">no target set</div>
      <div class="sml">losing <b id="wtRate">--</b> g/min &middot; dry-to-weight
      <b id="wtMode">off</b></div>
      <div class="wtctl">
        <input type="number" id="calG" placeholder="known weight (g)" step="10" min="10">
        <button class="act" onclick="espFetch('/api/scale?tare=1',{method:'POST'}).then(r=>toast(r.ok?'Tared':'Error')).catch(()=>toast('Offline',1))">&#9878; Tare</button>
        <button class="act" onclick="scaleCal()">&#9878; Calibrate</button>
      </div>
      <div class="sml">put a known weight on the trays, type its grams, Calibrate</div>
    </div>
    <!-- heater -->
    <div class="card"><h3>&#128293; Heater <span class="sp"></span><span id="boostBadge"
      class="badge b-purge" style="display:none">&#11014; MAX HEAT-UP</span></h3>
      <div class="big"><span id="heat">--</span><span class="unit"> % duty</span></div>
      <div class="bar heat"><i id="heatBar" style="width:0%"></i></div>
      <div style="margin-top:8px;display:flex;align-items:center;gap:8px">
        <input type="range" id="heatKnob" min="0" max="100" step="5" value="0"
               style="flex:1;accent-color:var(--gold);height:34px"
               oninput="knobDrag()" onchange="knobSend()">
        <b id="knobPct" style="min-width:3.2em;text-align:right">--</b>
        <span class="badge b-run" id="manBadge">AUTO</span>
      </div>
      <div class="sml">Drag the knob to drive the BTS yourself (while RUNNING) &middot;
      the MCU takes back automatic control after <b>60 s</b> &middot;
      over-temperature cut stays active &middot; PID cap <b id="setCap">--</b> %</div>
    </div>
    <!-- fans -->
    <div class="card"><h3>&#127744; Fans <span class="sp"></span>L298N</h3>
      <div class="big"><span id="fan">--</span><span class="unit"> % demand</span></div>
      <div class="bar fan"><i id="fanBar" style="width:0%"></i></div>
      <div class="sml">outlet <b id="fanOut">--</b>% (burst venting)
      (gentle breeze)</div>
    </div>
    <!-- cycle -->
    <div class="card"><h3>&#9201; Cycle</h3>
      <div class="big"><span id="rem">--:--</span></div>
      <div class="sml">elapsed <b id="el">--</b> &middot; set <b id="setD">--</b> min</div>
      <div class="sml">purge <b id="setPurge">--</b> s before power cut</div>
    </div>
    <!-- weather -->
    <div class="card" style="grid-column:1/-1"><h3>&#127787; Outdoor
      <span class="sp"></span><span id="wxBadge" class="badge b-idle">--</span></h3>
      <div class="wxbox">
        <div class="wxicon" id="wxIcon"></div>
        <div style="flex:1">
          <div class="big" style="font-size:24px"><span id="wxT">--</span><span class="unit">
          &deg;C &middot; RH </span><span id="wxH" class="unit" style="font-size:18px">--</span><span class="unit">%</span></div>
          <div class="wxrows">rain <b id="wxR">--</b>% &middot; wind <b id="wxW">--</b> km/h
          &middot; <span id="wxWhen">no data yet</span></div>
          <div class="sml" id="wxOut" style="margin-top:4px"></div>
          <details class="note" style="margin-top:8px"><summary>location &amp; manual entry</summary>
            <div class="frow" style="margin-top:9px">
              <div><label>Town / city (for live fetch)</label><input id="wxCity" placeholder="e.g. Razam"></div>
              <div><label>&nbsp;</label><button class="act" onclick="saveCity()"
                style="width:100%;background:var(--card2);color:var(--tx);border:1px solid var(--line2)">Save</button></div>
            </div>
            <div class="frow">
              <div><label>Manual temp (&deg;C)</label><input type="number" id="wxMt" step="0.5"></div>
              <div><label>Manual RH (%)</label><input type="number" id="wxMh" step="1" min="0" max="100"></div>
              <div><label>&nbsp;</label><button class="act" onclick="sendManualWx()" style="width:100%">Send</button></div>
            </div>
            <div id="wxDays" style="margin-top:9px"></div>
            <div class="note">Next 5 days above: typical values estimated from the date &amp; time.
            Live weather (badge LIVE) is fetched by your phone's browser and pushed to the ESP;
            no phone online? Type values manually.</div>
          </details>
        </div>
      </div>
    </div>
  </div>

  <!-- live multi-series chart -->
  <div class="chartbox" style="position:relative">
    <b>&#128200; Live trend</b>
    <div class="legend" id="legend"></div>
    <canvas id="chart"></canvas>
    <div class="tip" id="chartTip"></div>
  </div>

  <!-- past cycle viewer -->
  <div class="chartbox" id="cycView" style="display:none;margin-top:12px;position:relative">
    <b id="cycTitle">&#128230; Cycle</b>
    <button class="vbtn" style="float:right" onclick="closeCycle()">&#10005; close</button>
    <div class="legend" id="cycLegend"></div>
    <canvas id="cycChart"></canvas>
  </div>

  <!-- past cycles -->
  <div class="chartbox">
    <b>&#128230; Past cycles</b> <span class="note">(saved in ESP flash &mdash; survive power loss)</span>
    <div style="overflow-x:auto;margin-top:8px"><p class="note">Long-term registry (AT24C256 EEPROM): <b id="eeN">--</b> cycles stored &middot; <a class="dl" href="/eelog.csv" download>&#11015; Download ALL cycle summaries (CSV)</a></p>
<table id="cycTable">
      <tr><th>Started</th><th>Duration</th><th>Status</th><th>Time left</th><th>CSV</th><th></th></tr>
    </table></div>
    <div class="controls" style="margin:10px 0 0">
      <button class="act" id="btnClearCyc" onclick="clearCycles()"
        style="background:linear-gradient(135deg,rgba(170,24,52,.85),rgba(110,12,32,.85));color:#ffe3e7;border-color:rgba(255,138,154,.6)">&#128465; Clear history</button></div>
    <div class="note">Download shows every reading of that run: time, temp, RH, heater %,
      fan %, battery. Status: <b style="color:var(--ok)">completed</b> &middot;
      <b style="color:var(--warn)">stopped</b> &middot; <b style="color:var(--hot)">fault</b>
      &middot; press <b>view</b> to graph any run</div>
  </div>

  <div class="note"><a class="dl" href="/api/log.csv" download>&#11015; Download this run (CSV)</a>
  &middot; <span id="logN">0</span> records &middot; free heap <span id="heap">--</span> B
  &middot; keypad last key <b id="kpLast">&#8211;</b>
  (A start &middot; B stop &middot; C power on &middot; D +15 min)
  &middot; <a class="dl" href="/update">&#11014; Firmware update</a> (OTA)
  &middot; <a class="dl" href="/online">&#127760; online version</a> (UI from
  GitHub Pages, needs internet on your phone - falls back to this page)</div>
</section>

<!-- ================= SLIDE 1: DEFAULTS ================= -->
<!-- ================= SLIDE: PARAMETERS (edit + one-click defaults) ====== -->
<section id="secSet">
  <h2 style="margin-bottom:4px;font-size:17px">&#9881; Parameters &mdash; all on one page</h2>
  <p class="note" style="margin-bottom:12px">Change any value, then <b>Save</b> (or <b>Save &amp; Start</b>).
  One click puts every parameter back to the factory defaults.</p>
  <div class="controls" style="margin-bottom:12px">
    <button class="act" id="btnDef" onclick="applyDefaults()">&#8635; Reset all to defaults</button>
  </div>
  <form onsubmit="return false" id="cfgForm">
    <div class="fsec"><h4>&#127777; Temperature</h4>
      <div class="frow" style="margin-bottom:9px">
        <div style="flex:2"><label>Mode preset (C key / BUTTON-2 on the unit)</label>
          <div class="frow" style="gap:8px">
            <button type="button" class="tabbtn" data-mdbtn="0" onclick="setMode(0)">AGARBATTI 60&deg;C</button>
            <button type="button" class="tabbtn" data-mdbtn="1" onclick="setMode(1)">USER DEFINED</button>
            <button type="button" class="tabbtn" data-mdbtn="2" onclick="setMode(2)">SILICAGEL 80&deg;C</button>
          </div></div>
      </div>
      <div class="frow">
        <div><label>Target temperature (&deg;C, max 80)</label><input type="number" id="f_setTemp" step="0.5" min="40" max="80"></div>
        <div><label>Control band &plusmn; (&deg;C)</label><input type="number" id="f_tempHyst" step="0.1" min="0.2" max="5"></div>
        <div><label>Safety cutoff (&deg;C)</label><input type="number" id="f_maxTemp" step="1" min="35" max="110"></div>
      </div>
      <label class="chk"><input type="checkbox" id="f_boostHeat"><span>Full-power heat-up &mdash; BTS at MAX output until the chamber reaches target, then PID holds it</span></label></div>
    <div class="fsec"><h4>&#128167; Humidity &amp; airflow</h4>
      <div class="frow">
        <div><label>Humidity &ndash; fans ramp above (%RH)</label><input type="number" id="f_humHigh" step="1" min="20" max="95"></div>
        <div><label>Humidity &ndash; fans stop below (%RH)</label><input type="number" id="f_humLow" step="1" min="10" max="80"></div>
        <div><label>Target RH for "dry" (%RH)</label><input type="number" id="f_humTarget" step="1" min="5" max="70"></div>
      </div>
      <label class="chk"><input type="checkbox" id="f_requireHum"><span>Cycle only finishes when target RH is also reached (else time alone decides)</span></label>
      <label class="chk"><input type="checkbox" id="f_smartVent"><span>Outdoor-aware venting &mdash; pause venting when outside air is wetter than the chamber (needs weather data)</span></label>
      <div class="frow" style="margin-top:9px">
        <div><label>Minimum fan speed (%)</label><input type="number" id="f_fanMin" step="1" min="0" max="60"></div>
        <div><label>Heater power cap (%)</label><input type="number" id="f_heaterMax" step="5" min="10" max="100"></div>
      </div>
      <div class="frow">
        <div><label>Target batch weight (g, 0 = off)</label><input type="number" id="f_targetG" step="50" min="0" max="9000"></div>
        <div><label>Fan fires when RH &ge; (%)</label><input type="number" id="f_fanTrigRH" step="5" min="30" max="90"></div>
        <div><label>RH high for (min) before fan</label><input type="number" id="f_fanTrigMin" step="1" min="1" max="10"></div>
        <div><label>Fan burst length (s)</label><input type="number" id="f_fanBurstS" step="10" min="10" max="300"></div>
        <div><label>Exhaust fan speed (%)</label><input type="number" id="f_fanOut" step="5" min="10" max="100"></div>
        <div><label>Ramp per RH point (%/RH)</label><input type="number" id="f_fanSlope" step="1" min="1" max="12"></div>
      </div>
      <div class="note" style="margin:2px 0 8px">&#127788; Gentle recipe: intake 40&ndash;60, exhaust 60&ndash;80,
      min 5&ndash;10, slope 2&ndash;3 &mdash; a soft breeze that takes the humidity out without cooling the sticks.</div></div>
    <div class="fsec"><h4>&#9201; Drying time &amp; end of cycle</h4>
      <div class="frow">
        <div><label>Drying time &ndash; hours</label><input type="number" id="f_hrs" step="1" min="0" max="24" value="2"></div>
        <div><label>Drying time &ndash; minutes</label><input type="number" id="f_min" step="5" min="0" max="59" value="0"></div>
        <div><label>Purge before power cut (s)</label><input type="number" id="f_cooldownSec" step="5" min="10" max="600"></div>
      </div></div>
    <div class="fsec"><h4>&#128267; Battery</h4>
      <div class="frow">
        <div><label>Battery type</label><select id="f_battType">
          <option value="0">3S Li-ion (12.6 V)</option><option value="1">4S Li-ion (16.8 V)</option>
          <option value="2">12 V SLA</option><option value="3">4S LiFePO4</option></select></div>
        <div><label>Safe shutdown below (%)</label><input type="number" id="f_cutoffPct" step="1" min="0" max="40"></div>
      </div></div>
    <div class="fsec"><h4>&#9878; Dry to weight (weigh scale)</h4>
      <div class="frow">
        <label class="chk"><input type="checkbox" id="f_requireWeight"><span>End the cycle when the batch stops losing weight (needs the HX711 scale fitted)</span></label>
      </div>
      <div class="frow">
        <div><label>Settled below (g/min)</label><input type="number" id="f_weightRateG" step="0.5" min="0.5" max="50"></div>
        <div><label>Stable for (min)</label><input type="number" id="f_weightMinY" step="1" min="2" max="120"></div>
      </div></div>
    <div class="fsec"><h4>&#9200; Clock &mdash; date &amp; time (cycle history stamps)</h4>
      <p class="note" style="margin:2px 0 10px">The dashboard automatically pushes your phone's
      clock to the ESP32 whenever it connects &mdash; usually nothing to do here. You can also set it
      manually below, tap the clock on the <b>/display</b> page, or use keypad menu rows <b>6/7</b>.</p>
      <div class="frow">
        <div><label>Set date &amp; time manually</label><input type="datetime-local" id="f_dt"></div>
        <div><label>&nbsp;</label><button class="act" onclick="setClockManual()"
          style="width:100%">&#9201; Set clock</button></div>
        <div><label>Timezone offset (min)</label><input type="number" id="f_tz" step="15" min="-720" max="840"></div>
      </div></div>
    <div class="fsec"><h4>&#127987; Batch calculator &mdash; sticks &amp; paste (fills the target weight)</h4>
      <p class="note" style="margin:2px 0 10px">Moisture is a property of the <b>paste</b>:
      raw mix is typically <b>30&ndash;40&nbsp;% water</b>, finished agarbatti wants
      <b>8&ndash;10&nbsp;%</b>. Enter the batch below and the target weight is computed:
      <code>target&nbsp;=&nbsp;sticks&nbsp;&times;&nbsp;wet&nbsp;g&nbsp;&times;&nbsp;(1&nbsp;&minus;&nbsp;(paste&nbsp;%&nbsp;&minus;&nbsp;final&nbsp;%)/100)</code>.
      Example: 2.5&nbsp;g stick, 35&nbsp;%&nbsp;&rarr;&nbsp;10&nbsp;% = 1.875&nbsp;g dry
      (0.625&nbsp;g water per stick). Leave sticks at 0 to type the target by hand.</p>
      <div class="frow">
        <div><label>Sticks in batch (0 = off)</label><input type="number" id="f_stickCount" step="10" min="0" max="3000" oninput="stickCalc()"></div>
        <div><label>Avg wet stick (g)</label><input type="number" id="f_stickWetG" step="0.1" min="0.5" max="20" oninput="stickCalc()"></div>
        <div><label>Paste water %</label><input type="number" id="f_pasteWater" step="1" min="5" max="60" oninput="stickCalc()"></div>
        <div><label>Final moisture %</label><input type="number" id="f_targetMoist" step="1" min="3" max="20" oninput="stickCalc()"></div>
      </div>
      <p class="note" id="stickPrev" style="margin:2px 0 0">&mdash;</p></div>
    <div class="fsec"><h4>&#9878; Advanced &mdash; heater PID</h4>
      <div class="frow">
        <div><label>Kp (%/&deg;C)</label><input type="number" id="f_kp" step="0.5" min="0" max="100"></div>
        <div><label>Ki (%/&deg;C&middot;s)</label><input type="number" id="f_ki" step="0.01" min="0" max="10"></div>
        <div><label>Kd (%/&deg;C&middot;s&#8315;&sup1;)</label><input type="number" id="f_kd" step="0.5" min="0" max="100"></div>
      </div></div>
  </form>
  <div class="controls">
    <button class="act" id="btnSave" onclick="saveCfg(false)">&#128190; Save</button>
    <button class="act" id="btnGo" onclick="saveCfg(true)">&#9654; Save &amp; Start drying</button>
  </div>
  <details class="card" style="margin-top:14px;padding:12px 14px">
    <summary style="cursor:pointer;font-weight:700;color:var(--dim)">Factory defaults (reference)</summary>
    <table id="defTable" style="margin-top:10px"></table>
  </details>
</section>

<!-- v2.0.19: virtual keypad drawer - same keys as the pillar keypad -->
<div id="vpDrawer">
  <div id="vpHead"><b style="font-size:14px">&#9000; Virtual keypad</b>
    <button class="pill" onclick="vpClose()">&#10005;</button></div>
  <div id="vpText"></div>
  <div class="vpPad">
    <button onclick="vpKey('1')">1</button><button onclick="vpKey('2')">2<small>&#9650;</small></button><button onclick="vpKey('3')">3</button><button onclick="vpKey('A')">A<small>OK</small></button>
    <button onclick="vpKey('4')">4<small>&#9664;</small></button><button onclick="vpKey('5')">5</button><button onclick="vpKey('6')">6<small>&#9654;</small></button><button onclick="vpKey('B')">B<small>MENU</small></button>
    <button onclick="vpKey('7')">7</button><button onclick="vpKey('8')">8<small>&#9660;</small></button><button onclick="vpKey('9')">9</button><button onclick="vpKey('C')">C<small>MODE</small></button>
    <button onclick="vpKey('*')">*</button><button onclick="vpKey('0')">0</button><button onclick="vpKey('#')">#<small>BACK</small></button><button onclick="vpKey('D')">D<small>RUN</small></button>
  </div>
  <div class="note">digits type &middot; 2/4/6/8 move &middot; <b>A</b> OK &middot; <b>B</b> menu &middot; <b>C</b> mode &middot; <b>D</b> run/stop &middot; <b>*</b> home &middot; <b>#</b> back</div>
</div>
<div id="toast"></div>
<script>
'use strict';
/* espFetch: the online layer (injected by tools/online/sync-online.js into the
   GitHub-Pages build) provides espFetchBridge; standalone we use plain fetch. */
var espFetch=(typeof espFetchBridge!=='undefined')?espFetchBridge:function(u,o){return fetch(u,o)};
let S=null,DEF=null,T=[];           // status, defaults, chart series
const $=id=>document.getElementById(id);
function toast(m,e){const t=$('toast');t.textContent=m;t.className=e?'err':'';t.style.opacity=1;
  t.style.transform='translateX(-50%) translateY(0)';
  setTimeout(()=>{t.style.opacity=0;t.style.transform='translateX(-50%) translateY(8px)'},2200)}
function showTab(n){for(const s of['Dash','Set']){$('tab'+s).classList.toggle('on',s===n);
  $('sec'+s).classList.toggle('on',s===n)}if(n==='Set'){if(S)fillForm(S.set);if(DEF)fillDef()}}
const f2=(v,d=1)=>v==null||isNaN(v)?'--':(+v).toFixed(d);
const mmss=s=>{s=Math.max(0,s|0);const h=(s/3600)|0,m=((s%3600)/60)|0,x=s%60;
  return h>0?h+'h '+String(m).padStart(2,'0')+'m':String(m).padStart(2,'0')+':'+String(x).padStart(2,'0')};

async function api(url){try{const r=await espFetch(url,{method:'POST'});
  toast(r.ok?'OK':'Error '+(await r.text()),!r.ok)}catch(e){toast('Offline',1)}}

// one click: apply factory defaults on the ESP, then refresh this page
async function applyDefaults(){
  try{
    const r=await espFetch('/api/defaults',{method:'POST'});
    if(!r.ok){toast('Error '+(await r.text()),1);return}
    const j=await(await espFetch('/api/data')).json();
    S=j;DEF=j.defs||DEF;fillForm(S.set);fillDef();toast('Defaults applied')
  }catch(e){toast('Offline',1)}}

// ---------- background: royal blue + golden brown themes (per phone) -----
// v2.0.23: never black. Stored under a NEW key - phones that saved the old
// near-black default under 'dryerBg' start on Royal & Gold instead.
var bgCur={t:'royal',c:'#1b45b5',lines:true};
const BGP=[{n:'Royal & Gold',t:'royal',s:'linear-gradient(180deg,#2352c9 0 45%,#c4914a 62%,#8a5a14)'},
 {n:'Royal Blue',t:'blue',s:'linear-gradient(160deg,#2654d0,#16389a 70%,#b8862b)'},
 {n:'Golden Brown',t:'brown',s:'linear-gradient(180deg,#1d48b8 0 18%,#c4914a 32%,#8a5a14)'},
 {n:'Royal / Gold diagonal',t:'diag',s:'linear-gradient(152deg,#1b45b5 0 46%,#e1b35a 50%,#996515 56%)'},
 {n:'Sapphire',t:'sapphire',s:'linear-gradient(160deg,#1a3fa8,#10297a 70%,#c4914a)'},
 {n:'Bronze',t:'bronze',s:'linear-gradient(160deg,#2a58d0 0 18%,#b8862b 38%,#6e4610)'}];
function bgPaint(){var b=document.body;
  BGP.forEach(function(x){b.classList.remove('th-'+x.t)});b.classList.remove('th-custom');
  b.classList.add('th-'+bgCur.t);b.classList.toggle('nolines',!bgCur.lines);
  if(bgCur.t==='custom')b.style.setProperty('--bg',bgCur.c);else b.style.removeProperty('--bg');
  document.querySelectorAll('.bgsw').forEach(function(e){e.classList.toggle('on',e.dataset.t===bgCur.t)});
  try{localStorage.setItem('dryerTheme',JSON.stringify(bgCur))}catch(e){}}
function bgTheme(t){bgCur={t:t,c:bgCur.c,lines:bgCur.lines};bgPaint()}
function bgApply(c,lines){bgCur={t:'custom',c:c,lines:lines};bgPaint()}   // custom colour
function bgSet(custom){if(custom)bgApply(custom,$('bgLines').checked);
  else{bgCur.lines=$('bgLines').checked;bgPaint()}}
function toggleBg(){var p=$('bgPanel');p.style.display=p.style.display==='none'?'block':'none'}
function initBg(){
  try{localStorage.removeItem('dryerBg')}catch(e){}          // pre-v2.0.23 dark default
  try{var p=JSON.parse(localStorage.getItem('dryerTheme'));if(p&&p.t)bgCur=p}catch(e){}
  $('bgSwatches').innerHTML=BGP.map(function(b){return '<span class="bgsw" data-t="'+b.t+
    '" style="background:'+b.s+'" title="'+b.n+'" onclick="bgTheme(\''+b.t+'\')"></span>'}).join('');
  $('bgLines').checked=bgCur.lines;$('bgColor').value=bgCur.c;bgPaint()}

// ---------- manual heat knob (override for 60 s, then auto again) --------
var knobTmr=0;
function knobDrag(){                      // while dragging: live readout,
  var v=+$('heatKnob').value;             // throttled send so the ESP is
  $('knobPct').textContent=v+'%';         // not flooded
  $('manBadge').textContent='MANUAL\u2026';
  $('manBadge').className='badge b-purge';
  if(knobTmr)clearTimeout(knobTmr);
  knobTmr=setTimeout(knobSend,400);
}
async function setMode(m){
  await api('/api/mode?m='+m);
  var names=['AGARBATTI DEFAULT','USER DEFINED','SILICAGEL DEFAULT'];
  toast('Mode: '+names[m]+(m!==1?' applied (60/80\u00B0C, 120 min)':''));
  loadCfg();
}
function scaleCal(){                       // calibrate with a known weight
  var g=parseFloat($('calG').value);
  if(!(g>0)){toast('type the known weight first',1);return}
  espFetch('/api/scale?cal='+g,{method:'POST'})
    .then(r=>toast(r.ok?'Calibrated':'Error '+(r.ok?'':r.status),!r.ok))
    .catch(function(){toast('Offline',1)});
}

function knobSend(){                      // release / pause -> commit now
  knobTmr=0;
  var v=+$('heatKnob').value;
  espFetch('/api/heat?d='+v,{method:'POST'}).catch(function(){});
}

// ---------- live date & time (browser pushes its clock automatically) ----
let baseSec=0,baseMs=0,lastSync=0,lastState='';
const p2=n=>String(n).padStart(2,'0');
function fmtClock(sec){const d=new Date(sec*1000);
  return d.getUTCFullYear()+'-'+p2(d.getUTCMonth()+1)+'-'+p2(d.getUTCDate())+' '
        +p2(d.getUTCHours())+':'+p2(d.getUTCMinutes())+':'+p2(d.getUTCSeconds())}
function tickClock(){if(!baseSec)return;
  $('clock').textContent='\u23F1 '+fmtClock(baseSec+(Date.now()-baseMs)/1000)}
function syncClock(){                       // "takes initiative" from the phone
  const d=new Date();if(d.getFullYear()<2021)return;
  espFetch('/api/settime?epoch='+Math.floor(d.getTime()/1000)
       +'&tz='+(-d.getTimezoneOffset()),{method:'POST'}).catch(()=>{});
  lastSync=Date.now()}
function stickCalc(){                     // sticks & paste -> target weight
  const n=+$('f_stickCount').value||0, w=+$('f_stickWetG').value||0,
        p=+$('f_pasteWater').value||35, f=+$('f_targetMoist').value||10;
  const el=$('stickPrev');
  if(!(n>0)||!(w>=0.5)){el.innerHTML='\u2014 calculator off: target weight is typed by hand';return}
  const loss=Math.min(Math.max((p-f)/100,0),0.8), dry=w*(1-loss),
        wet=n*w, tgt=wet*(1-loss), water=wet-tgt;
  $('f_targetG').value=Math.round(tgt);
  el.innerHTML='\u{1FA7A} per stick: <b>'+w.toFixed(1)+' g wet \u2192 '+dry.toFixed(2)+
   ' g dry</b> ('+(w-dry).toFixed(2)+' g water) &middot; batch: <b>'+wet.toFixed(0)+
   ' g wet \u2192 '+tgt.toFixed(0)+' g target</b> &middot; total water to remove: <b>'+
   water.toFixed(0)+' g</b> ('+(water/1000).toFixed(2)+' L)';}
function setClockManual(){const v=$('f_dt').value;if(!v){toast('Pick a date & time first',1);return}
  const ep=Math.floor(new Date(v).getTime()/1000);
  if(isNaN(ep)){toast('Bad date',1);return}
  espFetch('/api/settime?epoch='+ep+'&tz='+(parseInt($('f_tz').value||'330')),{method:'POST'})
    .then(()=>toast('Clock set')).catch(()=>toast('Offline',1))}

// ---------- chart engine (dual axis, toggles, hover tooltip) -------------
const SER={temp:{c:'#f2c14e',lab:'Temp \u00B0C',ax:0,get:p=>p.t},
           hum:{c:'#4cc3ff',lab:'RH %',ax:0,get:p=>p.h},
           heat:{c:'#ff9a76',lab:'Heater %',ax:1,get:p=>p.heat},
           fan:{c:'#c4b5fd',lab:'Fans %',ax:1,get:p=>p.fan},
           bat:{c:'#6ee7b7',lab:'Battery %',ax:1,get:p=>p.bat},
           wt:{c:'#f5f7ff',lab:'Weight g',ax:1,get:p=>p.wt}};
const VIS={temp:true,hum:true,heat:false,fan:false,bat:false};
const CIRC=2*Math.PI*52;
function setRing(id,frac){const e=$(id);if(!e)return;frac=frac<0?0:frac>1?1:frac;
  e.style.strokeDashoffset=(CIRC*(1-frac)).toFixed(1)}
function setMark(id,frac,show){const e=$(id);if(!e)return;
  if(!show){e.style.display='none';return}
  e.style.display='';e.setAttribute('transform','rotate('+((frac*360).toFixed(1))+' 60 60)')}
function buildLegend(box,vis,redraw){box.innerHTML='';
  for(const k in SER){const s=SER[k];
    const el=document.createElement('span');el.className='lg'+(vis[k]?' on':'');
    el.style.setProperty('--c',s.c);
    el.innerHTML='<i></i>'+s.lab;
    el.onclick=()=>{vis[k]=!vis[k];el.classList.toggle('on',vis[k]);redraw()};
    box.appendChild(el)}}
function paint(cnv,data,vis,xsec){
  const g=cnv.getContext('2d'),W=cnv.clientWidth,H=250,DPR=devicePixelRatio||1;
  cnv.width=W*DPR;cnv.height=H*DPR;g.scale(DPR,DPR);g.clearRect(0,0,W,H);
  const PL=42,PR=40,PT=12,PB=24,IW=W-PL-PR,IH=H-PT-PB;
  // scales
  let a0min=1e9,a0max=-1e9;const a1min=0,a1max=100;
  const act=Object.keys(SER).filter(k=>vis[k]&&data.some(p=>SER[k].get(p)!=null&&!isNaN(SER[k].get(p))));
  if(!act.length){g.fillStyle='#cdd6f2';g.font='12px system-ui';
    g.fillText('waiting for data\u2026',PL,PT+14);return}
  for(const k of act)for(const p of data){const v=SER[k].get(p);
    if(v!=null&&!isNaN(v)){if(v<a0min)a0min=v;if(v>a0max)a0max=v}}
  if(SER.temp.ax===0){}   // temp+hum share axis 0
  if(a0min>a0max){a0min=0;a0max=1}
  if(a0max-a0min<4){const m=(a0max+a0min)/2;a0min=m-2;a0max=m+2}
  const pad=(a0max-a0min)*.12;a0min-=pad;a0max+=pad;
  const n=data.length;
  const X=i=>n<2?PL+IW/2:PL+i/(n-1)*IW;
  const Y0=v=>PT+IH-(v-a0min)/(a0max-a0min)*IH;
  const Y1=v=>PT+IH-(v-a1min)/(a1max-a1min)*IH;
  // grid + axis labels
  g.font='10.5px system-ui';g.strokeStyle='rgba(255,255,255,.13)';g.lineWidth=1;
  for(let i=0;i<=4;i++){const y=PT+IH*i/4;
    g.beginPath();g.moveTo(PL,y);g.lineTo(W-PR,y);g.stroke();
    g.fillStyle='#cdd6f2';g.textAlign='right';
    g.fillText((a0max-(a0max-a0min)*i/4).toFixed(0),PL-6,y+3.5);
    g.textAlign='left';
    g.fillText((a1max-(a1max-a1min)*i/4).toFixed(0),W-PR+6,y+3.5)}
  // x labels (elapsed)
  g.textAlign='center';g.fillStyle='#cdd6f2';
  const lastSec=xsec?data[n-1].sec:(data[n-1].ts-data[0].ts)/1000;
  for(let i=0;i<=4;i++){const idx=Math.round((n-1)*i/4);
    const sec=xsec?data[idx].sec:(data[idx].ts-data[0].ts)/1000;
    g.fillText(mmss(sec),X(idx),H-8)}
  // target temp guide on axis0 (live chart only)
  if(S&&!xsec&&VIS.temp){const y=Y0(S.set.setTemp);
    if(y>PT&&y<PT+IH){g.setLineDash([5,5]);g.strokeStyle='#f6dd9c';g.lineWidth=1.2;
      g.beginPath();g.moveTo(PL,y);g.lineTo(W-PR,y);g.stroke();g.setLineDash([])}}
  // series (linears + soft fill for axis0 series)
  for(const k of act){const s=SER[k],Y=s.ax?Y1:Y0;
    g.strokeStyle=s.c;g.lineWidth=2;g.lineJoin='round';g.beginPath();
    let started=false,last=null;
    data.forEach((p,i)=>{const v=s.get(p);
      if(v==null||isNaN(v)){if(started){g.stroke();started=false}return}
      const x=X(i),y=Y(v);
      if(started)g.lineTo(x,y);else{g.moveTo(x,y);started=true}last={x,y,v}});
    g.stroke();
    if(!s.ax&&n>2){                     // gradient fill under temp/RH
      g.lineTo(X(n-1),PT+IH);g.lineTo(PL,PT+IH);g.closePath();
      const gr=g.createLinearGradient(0,PT,0,PT+IH);
      gr.addColorStop(0,s.c+'26');gr.addColorStop(1,s.c+'00');g.fillStyle=gr;g.fill()}
    if(last){g.fillStyle=s.c;g.beginPath();
      g.arc(last.x,last.y,3.2,0,7);g.fill();
      g.fillStyle='#173a98';g.strokeStyle=s.c;g.lineWidth=1.4;
      g.beginPath();g.arc(last.x,last.y,5.4,0,7);g.stroke()}}
}
let hoverI=-1;
function draw(){paint($('chart'),T,VIS,false);if(hoverI>=0)showTip(hoverI)}
function showTip(i){const tip=$('chartTip'),cnv=$('chart');
  if(!T[i]){tip.style.opacity=0;return}
  const W=cnv.clientWidth,PL=42,PR=40,IW=W-PL-PR;
  const x=PL+(T.length<2?IW/2:i/(T.length-1)*IW);
  const sec=(T[i].ts-T[0].ts)/1000;
  let rows='';
  for(const k in SER){if(!VIS[k])continue;const v=SER[k].get(T[i]);
    if(v==null||isNaN(v))continue;
    rows+='<span style="color:'+SER[k].c+'">\u25CF</span> '+SER[k].lab+
          ' <b>'+f2(v)+'</b><br>'}
  tip.innerHTML='<b>'+mmss(sec)+'</b><br>'+rows;
  tip.style.opacity=1;
  tip.style.left=Math.min(W-150,Math.max(4,x+14))+'px';
  tip.style.top=(cnv.offsetTop+18)+'px';
  const g=cnv.getContext('2d');g.save();
  g.strokeStyle='rgba(255,255,255,.35)';g.setLineDash([4,4]);
  g.beginPath();g.moveTo(x,12);g.lineTo(x,226);g.stroke();g.restore()}
function chartHover(ev){const cnv=$('chart'),W=cnv.clientWidth,PL=42,PR=40;
  if(T.length<2)return;
  const r=cnv.getBoundingClientRect();const x=ev.clientX-r.left;
  const i=Math.round((x-PL)/(W-PL-PR)*(T.length-1));
  hoverI=(i>=0&&i<T.length)?i:-1;
  if(hoverI>=0){draw();showTip(hoverI)}else{$('chartTip').style.opacity=0;draw()}}
function chartLeave(){hoverI=-1;$('chartTip').style.opacity=0;draw()}

// ---------- past-cycle graph viewer ---------------------------------------
let VT=null;const VVIS={temp:true,hum:true,heat:true,fan:true,bat:false};
function viewCycle(file,started){
  espFetch('/api/cycle?file='+encodeURIComponent(file))
   .then(r=>{if(!r.ok)throw 0;return r.text()})
   .then(csv=>{const pts=[];
     csv.split(/\r?\n/).forEach(l=>{if(!l||l[0]==='#')return;
       const c=l.split(',');if(c.length<8)return;
       pts.push({sec:+c[0],t:+c[1],h:+c[2],heat:+c[4],fan:+c[5],bat:+c[7],
                 wt:(c[8]!==''&&c[8]!=null)?+c[8]:null})});
     if(pts.length<2){toast('No data points',1);return}
     VT=pts;$('cycView').style.display='block';
     $('cycTitle').textContent='\u{1F4E6} '+started;
     buildLegend($('cycLegend'),VVIS,()=>paint($('cycChart'),VT,VVIS,true));
     paint($('cycChart'),VT,VVIS,true);
     $('cycView').scrollIntoView({behavior:'smooth'})})
   .catch(()=>toast('Could not load cycle',1))}
function closeCycle(){VT=null;$('cycView').style.display='none'}

// ---------- outdoor weather (phone relays internet -> ESP) ----------------
// typical-season dummy forecast: derived from the REAL date & time, so it
// tracks the seasons and the hour of the day without any internet at all
function dummyWx(){
  const now=Date.now(),d=Math.floor(now/864e5),days=[];
  for(let i=0;i<5;i++){
    const hi=32.5+6*Math.sin((d-30+i)/365*2*Math.PI);
    const rh=72-9*Math.sin((d-30+i)/365*2*Math.PI);
    days.push({lab:new Date(now+i*864e5).toLocaleDateString(undefined,{weekday:'short'}),
      icon:rh>78?'\u{1F327}':(rh>64?'\u26C5':'\u2600'),
      hi:Math.round(hi),lo:Math.round(hi-7.5)});
  }
  const dt=new Date(now),h=dt.getHours()+dt.getMinutes()/60;
  const f=Math.max(0,Math.min(1,0.5-0.5*Math.cos((h-9)/12*Math.PI)));
  const tNow=days[0].lo+(days[0].hi-days[0].lo)*(0.35+0.5*f);
  return {days,tNow,rhNow:Math.round(Math.max(35,88-days[0].hi-f*14))};
}
const WMO={0:['\u2600\uFE0F','clear'],1:['\uD83C\uDF24\uFE0F','mainly clear'],2:['\u26C5','partly cloudy'],
3:['\u2601\uFE0F','overcast'],45:['\uD83C\uDF2B\uFE0F','fog'],48:['\uD83C\uDF2B\uFE0F','rime fog'],
51:['\uD83C\uDF26\uFE0F','light drizzle'],53:['\uD83C\uDF26\uFE0F','drizzle'],55:['\uD83C\uDF26\uFE0F','heavy drizzle'],
61:['\uD83C\uDF27\uFE0F','light rain'],63:['\uD83C\uDF27\uFE0F','rain'],65:['\uD83C\uDF27\uFE0F','heavy rain'],
71:['\u2744\uFE0F','light snow'],73:['\u2744\uFE0F','snow'],75:['\u2744\uFE0F','heavy snow'],
80:['\uD83C\uDF26\uFE0F','showers'],81:['\uD83C\uDF27\uFE0F','showers'],82:['\u2614\uFE0F','heavy showers'],
95:['\u26C8\uFE0F','thunderstorm'],96:['\u26C8\uFE0F','storm + hail'],99:['\u26C8\uFE0F','severe storm']};
function wxName(c){return WMO[c]?WMO[c][1]:'--'}
async function pushWx(t,h,r,w,c,manual,loc){
  try{await espFetch('/api/weather',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({t:t,h:h,r:r||0,w:w||0,c:c||100,m:!!manual,
      loc:loc||'',ep:Math.floor(Date.now()/1000)})})}catch(e){}}
async function fetchWx(){                 // live: phone's mobile data -> ESP
  let loc=null;try{loc=JSON.parse(localStorage.getItem('dryerLoc')||'null')}catch(e){}
  if(!loc){$('wxWhen').textContent='set your town below for live weather';return}
  try{
    const u='https://api.open-meteo.com/v1/forecast?latitude='+loc.lat+'&longitude='+loc.lon+
      '&current=temperature_2m,relative_humidity_2m,precipitation_probability,weather_code,wind_speed_10m';
    const j=await(await fetch(u)).json();const c=j.current||{};
    await pushWx(c.temperature_2m,c.relative_humidity_2m,c.precipitation_probability,
                 c.wind_speed_10m,c.weather_code,false,loc.name);
    $('wxWhen').textContent='live from your phone \u2713';
  }catch(e){$('wxWhen').textContent='no internet on phone \u2014 use manual entry'}}
async function saveCity(){
  const v=$('wxCity').value.trim();if(!v){toast('Type a town name',1);return}
  try{
    const g=await(await fetch('https://geocoding-api.open-meteo.com/v1/search?count=1&name='
      +encodeURIComponent(v))).json();
    if(!g.results||!g.results.length){toast('Town not found',1);return}
    const r=g.results[0];
    localStorage.setItem('dryerLoc',JSON.stringify({lat:r.latitude,lon:r.longitude,name:r.name}));
    toast('Location: '+r.name);fetchWx();
  }catch(e){toast('No internet on phone',1)}}
function sendManualWx(){
  const t=parseFloat($('wxMt').value),h=parseFloat($('wxMh').value);
  if(isNaN(t)||isNaN(h)){toast('Enter temp and RH',1);return}
  pushWx(t,h,0,0,100,true,'manual').then(()=>toast('Sent'));}

// ---------- settings form / defaults table --------------------------------
function fillForm(c){
  $('f_setTemp').value=c.setTemp;$('f_tempHyst').value=c.tempHyst;$('f_maxTemp').value=c.maxTemp;
  $('f_humHigh').value=c.humHigh;$('f_humLow').value=c.humLow;$('f_humTarget').value=c.humTarget;
  $('f_requireHum').checked=!!c.requireHum;$('f_smartVent').checked=!!c.smartVent;
  $('f_boostHeat').checked=!!c.boostHeat;
  $('f_hrs').value=Math.floor(c.dryMinutes/60);$('f_min').value=c.dryMinutes%60;
  $('f_fanMin').value=c.fanMin;$('f_heaterMax').value=c.heaterMax;
  $('f_fanOut').value=c.fanOut;$('f_fanSlope').value=c.fanSlope;
  $('f_targetG').value=c.targetG||0;$('f_fanTrigRH').value=c.fanTrigRH||60;
  $('f_stickCount').value=c.stickCount||0;$('f_stickWetG').value=c.stickWetG||2.5;
  $('f_pasteWater').value=c.pasteWaterPct!=null?c.pasteWaterPct:35;
  $('f_targetMoist').value=c.targetMoistPct!=null?c.targetMoistPct:10;stickCalc();
  $('f_fanTrigMin').value=c.fanTrigMin||1;$('f_fanBurstS').value=c.fanBurstS||60;
  document.querySelectorAll('[data-mdbtn]').forEach(function(b){
    b.classList.toggle('on', +b.dataset.mdbtn===(c.mode||0));});
  $('f_battType').value=c.battType;$('f_cutoffPct').value=c.cutoffPct;
  $('f_requireWeight').checked=!!c.requireWeight;
  $('f_weightRateG').value=c.weightRateG;$('f_weightMinY').value=c.weightMinY;
  $('f_cooldownSec').value=c.cooldownSec;$('f_kp').value=c.kp;$('f_ki').value=c.ki;$('f_kd').value=c.kd;
  $('f_tz').value=c.tzMinutes;
}
function num(id,min,max){let v=parseFloat($(id).value);if(isNaN(v))v=min;
  return Math.min(max,Math.max(min,v))}
async function saveCfg(start){
  const o={setTemp:num('f_setTemp',25,90),tempHyst:num('f_tempHyst',0.2,5),
    maxTemp:num('f_maxTemp',35,110),humHigh:num('f_humHigh',20,95),humLow:num('f_humLow',10,80),
    humTarget:num('f_humTarget',5,70),requireHum:$('f_requireHum').checked,
    smartVent:$('f_smartVent').checked,boostHeat:$('f_boostHeat').checked,
    dryMinutes:Math.max(1,Math.round(num('f_hrs',0,24)*60+num('f_min',0,59))),
    fanMin:num('f_fanMin',0,60),heaterMax:num('f_heaterMax',10,100),
    fanOut:num('f_fanOut',10,100),fanSlope:num('f_fanSlope',1,12),
    targetG:num('f_targetG',0,9000),
    stickCount:num('f_stickCount',0,3000),stickWetG:num('f_stickWetG',0.5,20),
    pasteWaterPct:num('f_pasteWater',5,60),targetMoistPct:num('f_targetMoist',3,20),
    fanTrigRH:num('f_fanTrigRH',30,90),
    fanTrigMin:num('f_fanTrigMin',1,10),fanBurstS:num('f_fanBurstS',10,300),
    battType:num('f_battType',0,3),cutoffPct:num('f_cutoffPct',0,40),
    requireWeight:$('f_requireWeight').checked,weightRateG:num('f_weightRateG',0.5,50),
    weightMinY:num('f_weightMinY',2,120),
    cooldownSec:num('f_cooldownSec',10,600),kp:num('f_kp',0,100),ki:num('f_ki',0,10),kd:num('f_kd',0,100),
    tzMinutes:num('f_tz',-720,840)};
  if(o.maxTemp<o.setTemp+5){toast('Safety cutoff must be at least 5 \u00B0C above target',1);return}
  if(o.humLow>=o.humHigh-1){toast('Humidity band is upside down',1);return}
  try{const r=await espFetch('/api/settings',{method:'POST',
    headers:{'Content-Type':'application/json'},body:JSON.stringify(o)});
    if(!r.ok){toast('Rejected: '+await r.text(),1);return}
    if(start){await espFetch('/api/start',{method:'POST'});showTab('Dash')}
    toast('Settings saved')}
  catch(e){toast('Offline',1)}
}
function fillDef(){
  const rows=[['Target temperature',f2(DEF.setTemp)+' \u00B0C','PID holds the chamber here'],
  ['Control band \u00B1',f2(DEF.tempHyst)+' \u00B0C','temperature smoothing window'],
  ['Safety cutoff',f2(DEF.maxTemp,0)+' \u00B0C','heater + fans cut above this'],
  ['Full-power heat-up',DEF.boostHeat?'ON':'OFF','BTS at max until target, then PID'],
  ['Fans ramp above',f2(DEF.humHigh,0)+' %RH','moist air is vented out'],
  ['Fans stop below',f2(DEF.humLow,0)+' %RH','heat retained, no over-drying'],
  ['Target RH',f2(DEF.humTarget,0)+' %RH',DEF.requireHum?'must be reached to finish':'informational'],
  ['Drying time',DEF.dryMinutes+' min','set manually in the form above'],
  ['Minimum fan speed',DEF.fanMin+' %','circulation inside the RH band'],
  ['Outlet fan burst','RH \u2265 '+DEF.fanTrigRH+'% for '+DEF.fanTrigMin+' min \u2192 100% for '+DEF.fanBurstS+' s','single exhaust fan'],
  ['Ramp per RH point',DEF.fanSlope+' %/RH','how fast fans spin up above humHigh'],
  ['Heater power cap',DEF.heaterMax+' %','soft limit on coil current'],
  ['Purge before cut',DEF.cooldownSec+' s','fans flush hot air, then all off'],
  ['Safe shutdown below',DEF.cutoffPct+' %','battery % that stops everything'],
  ['Heater PID (Kp/Ki/Kd)',DEF.kp+' / '+DEF.ki+' / '+DEF.kd,'duty control of coil current'],
  ['Dry to weight',DEF.requireWeight?'ON':'off','cycle ends when weight settles'],
  ['Settled below',DEF.weightRateG+' g/min','rate under this = dry enough'],
  ['Stable for',DEF.weightMinY+' min','how long the rate must stay low']];
  $('defTable').innerHTML='<tr><th>Parameter</th><th>Value</th></tr>'+
    rows.map(r=>'<tr><td>'+r[0]+'<div class="note">'+r[2]+'</div></td><td>'+r[1]+'</td></tr>').join('');
}

// ---------- main render ----------------------------------------------------
function render(){
  $('eeN').textContent = (S.eelog && S.eelog.ok) ? (S.eelog.used + '/' + S.eelog.slots) : 'not fitted';
  $('eeN2').textContent = (S.eelog && S.eelog.ok) ? S.eelog.used + ' cycles' : '--';
  if(!S)return;
  const st=S.state;
  const b=$('stBadge');b.innerHTML='<span class="dot"></span>'+st;
  b.className='pill '+({IDLE:'p-idle',DRYING:'p-run',PURGING:'p-purge',
    DONE:'p-done',FAULT:'p-fault'})[st];
  const fb=$('faultBox');fb.style.display=st==='FAULT'?'block':'none';
  var fr=S.faultRec||{};
  fb.innerHTML='\u26A0 <b>E'+String(fr.code||0).padStart(2,'0')+' '+(fr.name||'FAULT')+
    '</b> \u2014 '+S.fault+' \u00B7 power is cut. Fix the issue, then press Power On.'+
    ((fr.code&&fr.limit)?' \u00B7 value '+(fr.val||0).toFixed(1)+' / limit '+
      (fr.limit||0).toFixed(1):'')+' \u00B7 reset: MANUAL';
  var dc=$('doneCard');
  if(st==='DONE'){dc.style.display='block';
    var mm=Math.floor((S.cycleSecs||0)/60);
    dc.innerHTML='\u2705 <b>DRYING COMPLETE</b> \u00B7 initial '
     +Math.round(S.wtStart||0)+' g \u00B7 final '+Math.round(S.finalG||0)
     +' g \u00B7 target '+Math.round(S.set?(+S.set.targetG||0):0)+' g \u00B7 <b>moisture removed '
     +Math.round(S.moistureG||0)+' g</b> \u00B7 duration '+mm+' min'
     +' \u2014 batch ready for packaging';}
  else dc.style.display='none';
  var wb=$('warnBox'),wr=S.warnRec||{};
  if(wr&&wr.active&&wr.code){wb.style.display='block';
    wb.innerHTML='\u26A0 <b>E'+String(wr.code).padStart(2,'0')+' '+wr.name+
    '</b> \u2014 warning: the cycle continues, but check the machine';}
  else wb.style.display='none';
  // gauges
  $('tAvg').textContent=f2(S.tAvg);$('hAvg').textContent=f2(S.hAvg);
  setRing('g_t',S.tAvg!=null&&!isNaN(S.tAvg)?(S.tAvg-15)/60:0);
  setRing('g_h',S.hAvg!=null&&!isNaN(S.hAvg)?S.hAvg/100:0);
  setRing('g_b',S.bat.valid?S.bat.pct/100:0);
  const gb=$('g_b');if(S.bat.valid)
    gb.setAttribute('stroke',S.bat.pct>50?'#6ee7b7':S.bat.pct>20?'#f2c14e':'#ff7b7b');
  setMark('g_tmark',S.set?((S.set.setTemp-15)/60):0,S.set&&S.set.setTemp>15&&S.set.setTemp<75);
  setMark('g_hlo',S.set?S.set.humLow/100:0,true);setMark('g_hhi',S.set?S.set.humHigh/100:0,true);
  $('t1').textContent=f2(S.s1.t);$('t2').textContent=f2(S.s2.t);
  if(S.fw)$('fw').textContent=S.fw;
  var dr=S.door;
  if(dr){
    var dp=$('doorPill');
    if(dr.phase==='CALIBRATE'){dp.style.display='inline-flex';dp.className='pill p-fault';
      dp.textContent='\u{1F512} CALIBRATE THE SCALE - START LOCKED'}
    else if(dr.phase==='RUNNING'){dp.style.display='inline-flex';dp.className='pill p-run';
      dp.textContent='\u{1F513} OPEN DOOR = FAULT'}
    else if(dr.phase==='READY'){dp.style.display='inline-flex';dp.className='pill p-done';
      dp.textContent='\u2696 READY '+Math.round(dr.batch||0)+' g'}
    else{dp.style.display=dr.locked?'inline-flex':'none';dp.className='pill p-purge';
      dp.textContent='\u{1F513} LOAD THE TRAYS'}
  }
  $('kpLast').textContent=(S.kp&&S.kp.last)?S.kp.last:'\u2013';
  var dh=S.dht;
  if(dh&&dh.ok){$('wxOut').innerHTML='\u2600 <b>OUT (measured by the dryer)</b>: '
    +f2(dh.t,1)+'\u00B0C \u00B7 '+f2(dh.h,0)+'% RH \u2014 smart venting uses THIS';}
  else{$('wxOut').innerHTML='\u2600 no DHT22 fitted \u2014 outdoor values come from your phone';}
  var sc=S.scale||{ok:false,g:0,rate:0};
  if(sc.ok){
    $('wt').textContent=Math.round(sc.g);
    $('wtRate').textContent=(sc.rate!=null?sc.rate.toFixed(1):'--');
    $('wtMode').textContent=S.set&&S.set.requireWeight?'ON':'off';
    $('wtBadge').style.display=(S.set&&S.set.requireWeight&&Math.abs(sc.rate||1)<S.set.weightRateG)?'inline-block':'none';
    var tg=S.set?(+S.set.targetG||0):0;
    $('wtTgt').textContent=tg>0?('\u2192 '+Math.round(tg)+' g (diff '+Math.round(sc.g-tg)+' g)'):'no target set';
  }else{$('wt').textContent='--';$('wtRate').textContent='--';$('wtMode').textContent=S.set&&S.set.requireWeight?'ON (no scale!)':'off';$('wtBadge').style.display='none'}
  $('h1').textContent=f2(S.s1.h);$('h2').textContent=f2(S.s2.h);$('hMax').textContent=f2(S.hMax);
  $('d1').className='dot'+(S.s1.ok?'':' off');$('d2').className='dot'+(S.s2.ok?'':' off');
  $('heat').textContent=S.heat;$('fan').textContent=S.fan;
  $('boostBadge').style.display=S.boost?'inline-block':'none';
  // manual heat knob: MANUAL (countdown) while the override window is open,
  // otherwise the knob just mirrors the automatic duty
  var m=S.man||{pct:0,left:0};
  if(m.left>0){
    $('manBadge').textContent='MANUAL '+m.left+'s';
    $('manBadge').className='badge b-purge';
    $('heatKnob').value=m.pct;
    $('knobPct').textContent=m.pct+'%';
  }else{
    $('manBadge').textContent='AUTO';
    $('manBadge').className='badge b-run';
    if(document.activeElement!==$('heatKnob')&&!knobTmr){
      $('heatKnob').value=S.heat||0;
      $('knobPct').textContent=(S.heat||0)+'%';
    }
  }
  var sup=S.supply;
  if(sup){
    var sp=$('supPill');
    sp.textContent=sup.solar?'\u2600 SOLAR MODE':'\u26A1 BYPASS MODE';
    sp.className='pill '+(sup.live?'p-run':'p-fault');
    document.querySelectorAll('[data-mdbtn]').forEach(function(b){
      b.classList.toggle('on', +b.dataset.mdbtn===(sup.mode||0));});
  }
  $('fanOut').textContent=(S.fanOut!=null?S.fanOut:'--');
  $('heatBar').style.width=S.heat+'%';$('fanBar').style.width=S.fan+'%';
  $('setT').textContent=f2(S.set.setTemp);$('setH').textContent=f2(S.set.humLow,0)+'\u2013'+f2(S.set.humHigh,0);
  $('setD').textContent=S.set.dryMinutes;$('setPurge').textContent=S.set.cooldownSec;
  $('setCut').textContent=S.set.cutoffPct;
  $('setCap').textContent=S.set.heaterMax;
  $('bat').textContent=S.bat.valid?S.bat.pct:'--';$('bv').textContent=f2(S.bat.v,2);
  $('batType').textContent=S.bat.name;
  $('rem').textContent=st==='DRYING'?mmss(S.remaining):st==='PURGING'?'purging':st==='DONE'?'power cut':'--';
  $('el').textContent=mmss(S.elapsed);
  $('btnStart').disabled=st==='DRYING'||st==='PURGING';
  $('btnStop').disabled=st!=='DRYING';
  $('btnAdd').disabled=st!=='DRYING';
  $('btnPower').disabled=st!=='DONE'&&st!=='FAULT';
  $('logN').textContent=S.logCount;$('heap').textContent=S.heap;
  // live clock from the ESP (browser syncs it automatically on connect)
  if(S.clockSet){baseSec=S.now+(S.tz||0)*60;baseMs=Date.now();tickClock()}
  else{$('clock').textContent='\u23F1 clock not set \u2014 syncing from your phone\u2026';baseSec=0}
  if(!S.clockSet&&Date.now()-lastSync>60000)syncClock();
  else if(Date.now()-lastSync>1800000)syncClock();
  if(lastState==='DRYING'&&S.state==='DONE')loadCycles();   // fresh entry
  lastState=S.state;
  // outdoor weather card (source: live forecast > manual > stale)
  const w=S.wx;
  if(w&&w.outT!=null){
    $('wxBadge').textContent=(w.src||'none').toUpperCase();
    $('wxBadge').className='badge '+((w.src==='live')?'b-run':
      (w.src==='manual'?'b-purge':'b-fault'));
    $('wxT').textContent=f2(w.outT);$('wxH').textContent=f2(w.outH,0);
    $('wxR').textContent=w.ok?f2(w.r,0):'--';$('wxW').textContent=w.ok?f2(w.w,0):'--';
    $('wxIcon').textContent=(w.ok&&WMO[w.code])?WMO[w.code][0]:'';
    let when='';
    if(w.ok) when=(w.loc?w.loc+' \u00B7 ':'')+wxName(w.code)+' \u00B7 '+w.age+' min ago';
    else when='no outdoor data';
    if(S.set.smartVent&&w.outH!=null&&S.hMax!=null&&!isNaN(S.hMax)&&w.outH>=S.hMax)
      when+=' \u00B7 venting paused (outside wetter)';
    $('wxWhen').textContent=when;
  }else{const dwx=dummyWx();
    $('wxBadge').textContent='TYP.';$('wxBadge').className='badge b-idle';
    $('wxT').textContent=f2(dwx.tNow);$('wxH').textContent=f2(dwx.rhNow,0);
    $('wxR').textContent='--';$('wxW').textContent='--';$('wxIcon').textContent='';
    $('wxWhen').textContent='typical estimate from date & time (no live data)';
  }
  try{const dw=dummyWx();$('wxDays').innerHTML=dw.days.map(x=>
    '<span style="display:inline-block;text-align:center;padding:6px 9px;margin:2px;'+
    'border:1px solid var(--line2);border-radius:10px;background:var(--card2);font-size:11.5px">'+
    '<b style="color:var(--dim)">'+x.lab+'</b><br>'+x.icon+' '+x.hi+'\u00B0/'+x.lo+'\u00B0</span>').join('')}catch(e){}
  if(st==='DRYING'&&S.tAvg!=null&&!isNaN(S.tAvg))
    T.push({t:S.tAvg,h:S.hAvg,heat:S.heat,fan:S.fan,bat:S.bat.pct,ts:Date.now()});
  if(T.length>600)T.shift();
  try{draw()}catch(e){}          // never let a canvas glitch kill the poll loop
}
async function poll(){try{const r=await espFetch('/api/data');S=await r.json();
  if(DEF===null){DEF=S.defs;if($('secSet').classList.contains('on'))fillForm(S.set)}render();vpRenderIfOpen()}
  catch(e){$('stBadge').innerHTML='<span class="dot"></span>OFFLINE';
    $('stBadge').className='pill p-fault'}}

// ---------- virtual keypad (v2.0.19: touch-display entry) ----------------
function vesc(s){return String(s==null?'':s).replace(/&/g,'&amp;').replace(/</g,'&lt;')}
function vpOpen(){$('vpDrawer').classList.add('on');vpRender()}
function vpClose(){$('vpDrawer').classList.remove('on')}
function vpKey(k){espFetch('/api/key?k='+encodeURIComponent(k),{method:'POST'})
  .then(function(){poll()}).catch(function(){toast('Offline',1)})}
function vpRender(){
  var t=$('vpText'),m=S&&S.menu;
  if(!m){t.innerHTML=
    '<div class="vpRow vpSel"><span>'+vesc(S?S.state:'--')+'</span></div>'+
    '<div class="vpRow"><span>Press <b>B</b> (MENU) to open the on-device menu</span></div>'+
    '<div class="vpRow"><span><b>D</b> run/stop &middot; <b>C</b> mode &middot; <b>*</b> home &middot; <b>#</b> back &middot; digits type values</span></div>';}
  else if(m.edit){t.innerHTML=
    '<div class="vpRow"><span>ENTER VALUE</span></div>'+
    '<div class="vpRow vpSel" style="font-size:22px"><span>'+(m.buf||'--')+'</span>'+
    '<span style="font-size:12px;color:var(--dim)">'+vesc(m.hint)+'</span></div>'+
    '<div class="vpRow"><span>type digits &middot; <b>A</b> save &middot; <b>#</b> cancel</span></div>';}
  else{var h='';(m.items||[]).forEach(function(it,i){h+=
    '<div class="vpRow'+(i===m.cur?' vpSel':'')+'"><span>'+(i===m.cur?'▶ ':'')+
    vesc(it.n)+'</span><b>'+vesc(it.v)+'</b></div>'});
    t.innerHTML=h||'<div class="vpRow">menu empty</div>';}}
function vpRenderIfOpen(){if($('vpDrawer').classList.contains('on'))vpRender()}

// ---------- past cycles (saved in ESP flash) ------------------------------
async function loadCycles(){try{
  const r=await espFetch('/api/cycles'),list=await r.json();
  const t=$('cycTable');
  while(t.rows.length>1)t.deleteRow(-1);
  for(const c of list){
    const row=t.insertRow(-1);
    const dur=(+c.elapsedMin||0);
    const left=(c.reason==='completed')?'\u2714 done':
      ((+c.remainMin>0)?f2(c.remainMin,0)+' min left':'\u2714 done');
    row.innerHTML='<td>'+c.started+'<div class="note">target '+f2(c.setTemp,1)+
      ' \u00B0C \u00B7 set '+c.dryMin+' min'+(c.note?' \u00B7 '+c.note:'')+'</div></td>'+
      '<td>'+(dur>=60?f2(dur/60,1)+' h':f2(dur,0)+' min')+'</td>'+
      '<td><span class="badge '+(c.reason==='completed'?'b-run':c.reason==='stopped'?'b-purge':'b-fault')+
      '">'+c.reason+'</span></td><td>'+left+'</td>'+
      '<td><a class="dl" href="/api/cycle?file='+encodeURIComponent(c.file)+'" download>\u2B07 CSV</a></td>'+
      '<td><button class="vbtn" onclick="viewCycle(\''+c.file+'\',\''+c.started+'\')">\u{1F4C8} view</button></td>';
  }
 }catch(e){}}
async function clearCycles(){if(!confirm('Delete all saved cycles?'))return;
  try{await espFetch('/api/clearcycles',{method:'POST'});loadCycles();toast('History cleared')}
  catch(e){toast('Offline',1)}}

// ---------- boot -----------------------------------------------------------
(async()=>{
  buildLegend($('legend'),VIS,draw);
  $('chart').addEventListener('mousemove',chartHover);
  $('chart').addEventListener('mouseleave',chartLeave);
  try{const r=await espFetch('/api/history'),j=await r.json();
    const now=Date.now();
    T=j.t.map((t,i)=>({t:j.temp[i],h:j.hum[i],heat:null,fan:null,bat:null,
      ts:now-(j.t.length-1-i)*2000})).slice(-600)}catch(e){}
  try{const l=JSON.parse(localStorage.getItem('dryerLoc')||'null');
    if(l&&l.name)$('wxCity').value=l.name}catch(e){}
  loadCycles();fetchWx();setInterval(fetchWx,20*60*1000);
  initBg();setInterval(tickClock,1000);poll();setInterval(poll,2000)})();
addEventListener('resize',draw);
</script></body></html>)HTML";

// The OPTIONAL online-UI bridge page (served at /online). It frames the
// GitHub Pages dashboard and relays every /api call for it, because a
// browser will not let an https:// page call http://192.168.4.1 directly
// (mixed content). If the frame never says hello (no internet), it falls
// back to the built-in page at /.
static const char ONLINE_LOADER_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Dehumidifier - online interface</title>
<style>body{margin:0;background:#1b45b5 linear-gradient(180deg,#2352c9,#1d48b8 55%,#c4914a 72%,#8a5a14);color:#fff8ea;font:15px system-ui;
height:100vh;display:flex;align-items:center;justify-content:center;text-align:center}
#m{opacity:.9;padding:20px}a{color:#f6dd9c}iframe{border:0;width:100vw;height:100vh}</style>
</head><body>
<div id="m">loading the online interface&hellip;<br><br>
If nothing appears your phone has no internet -<br>
<a href="/">use the built-in interface</a></div>
<iframe id="f" src="__ONLINE_URL__"></iframe>
<script>
'use strict';
var seen=false;
addEventListener('message',function(e){
  if(e.source!==document.getElementById('f').contentWindow)return;
  seen=true;                                   // any msg = frame is alive
  var d=e.data; if(!d||!d.bridgeId)return;
  fetch(d.url,{method:d.method||'GET',body:d.body||null,
    headers:d.body?{'Content-Type':'application/json'}:{}})
  .then(function(r){return r.text().then(function(t){return{ok:r.ok,status:r.status,body:t}})})
  .catch(function(){return{ok:false,status:0,body:''}})
  .then(function(m){e.source.postMessage({bridgeId:d.bridgeId,ok:m.ok,status:m.status,body:m.body},'*')});
});
setTimeout(function(){if(!seen)location.replace('/')},7000);  // no internet -> built-in
</script></body></html>)HTML";


// ----------------------------------------------------------------------
//  GET /display - the KIOSK DISPLAY PAGE (v2.0.7). The hardware TFT was
//  removed from this build; a phone/tablet mounted on the pillar opens
//  this page and BECOMES the display: read-only, huge type, 1 Hz refresh,
//  wake-lock kept, zero controls. Same /api/data as the dashboard.
//  (The dashboard at / stays the full-control page.)
static const char DISPLAY_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SMART DEHUMIDIFIER - display</title>
<style>
:root{--navy:#10307f;--navy2:rgba(12,34,104,.74);--card:rgba(19,48,142,.86);--gold:#f6d77e;
--blue:#a8cbff;--green:#5ee89a;--red:#ff8a8a;--dim:#cdd6f2;--white:#fff8ea;
--edge:rgba(233,196,106,.34);
/* v2.0.23: royal-blue sky + golden-brown horizon (same as the dashboard) */
--bgimg:radial-gradient(110% 40% at 50% 74%,rgba(247,201,111,.34),rgba(247,201,111,0) 70%),
 linear-gradient(180deg,#2352c9 0%,#1d48b8 36%,#3560c8 55%,#c4914a 68%,#a8741d 80%,#8a5a14 100%)}
*{box-sizing:border-box}html,body{height:100%}
html{background:#1b45b5}
body{margin:0;background:var(--bgimg);color:var(--white);
font-family:'Segoe UI',system-ui,Arial,sans-serif;display:flex;
flex-direction:column;padding:12px;overflow:hidden}
.bar{display:flex;justify-content:space-between;align-items:center;
background:var(--navy2);border:1px solid var(--edge);border-radius:12px;padding:10px 18px;font-size:2.6vmin;
box-shadow:0 6px 18px rgba(6,16,58,.3)}
.bar b{color:var(--gold)}#sup{margin-left:12px}#sup.bad{color:var(--red)}
#sup.ok{color:var(--green)}#pmd{color:var(--dim)}
#stateRow{display:flex;align-items:baseline;gap:3vmin;padding:2vmin 2vmin 1vmin}
#st{font-size:9vmin;font-weight:800;letter-spacing:1px;text-shadow:0 2px 12px rgba(6,16,58,.5)}
#st.DRYING{color:var(--green)}#st.FAULT{color:var(--red)}
#st.DONE,#st.PURGING{color:var(--blue)}#st.IDLE{color:var(--dim)}
#times{font-size:3.4vmin;color:var(--white);text-shadow:0 1px 6px rgba(6,16,58,.55)}
#grid{flex:1;display:grid;grid-template-columns:1fr 1fr 1fr 1fr;
grid-template-rows:1fr 1fr;gap:12px;min-height:0}
.tile{background:linear-gradient(180deg,rgba(30,64,170,.9),var(--card));border:1px solid var(--edge);
border-radius:14px;padding:2vmin 2.4vmin;box-shadow:0 8px 22px rgba(6,16,58,.35);
display:flex;flex-direction:column;justify-content:center;min-height:0}
.tile .lbl{font-size:2.2vmin;color:var(--dim);letter-spacing:1px}
.tile .val{font-size:6.4vmin;font-weight:700;color:var(--gold);line-height:1.1}
.tile .sub{font-size:2.4vmin;color:var(--dim)}
.hbar{height:2.2vmin;border-radius:6px;background:rgba(6,18,64,.55);margin-top:1.2vmin;
overflow:hidden}.hbar i{display:block;height:100%;border-radius:6px}
#foot{font-size:2.6vmin;color:var(--dim);padding:1vmin 2vmin;margin-top:1.2vmin;text-align:center;
background:var(--navy2);border:1px solid var(--edge);border-radius:10px}
#fault{color:var(--red);font-weight:700;font-size:3vmin}
#clk{cursor:pointer;white-space:nowrap;margin-right:2vmin}
#clkPane{display:none;position:fixed;left:12px;right:12px;bottom:12px;
background:linear-gradient(180deg,#1d48b8,#10307f);border:1px solid var(--edge);
border-radius:12px;padding:14px;z-index:9;font-size:3vmin}
#clkPane input{font-size:3vmin;padding:6px;border-radius:8px;border:1px solid var(--edge);background:rgba(8,24,80,.7);color:var(--white)}
#clkPane button{font-size:3vmin;padding:8px 14px;margin:8px 6px 0 0;border:0;
border-radius:8px;background:linear-gradient(135deg,#f6dd9c,#b8862b);color:#2b1a02;font-weight:700;cursor:pointer}
/* v2.0.19: virtual keypad - landscape text page + 4x4 pad */
#vpWrap{display:none;flex:1;gap:12px;min-height:0}
body.vp #stateRow,body.vp #grid,body.vp #foot{display:none}
body.vp #vpWrap{display:flex}
#vpText{flex:1;background:var(--card);border:1px solid var(--edge);border-radius:14px;padding:2vmin 3vmin;
overflow:auto;font-size:3.2vmin;line-height:1.9;min-height:0}
#vpText .sel{color:var(--gold);font-weight:800}
#vpPad{width:min(46vmin,340px);display:grid;grid-template-columns:repeat(4,1fr);
grid-auto-rows:1fr;gap:1.2vmin}
#vpPad button{font-size:4.4vmin;font-weight:700;border-radius:12px;border:1px solid var(--edge);
background:var(--navy2);color:var(--white);cursor:pointer;font-family:inherit}
#vpPad button:active{transform:scale(.93);background:rgba(233,196,106,.3)}
#vpPad button small{display:block;font-size:1.9vmin;font-weight:600;color:var(--dim)}
@media(max-width:720px){#vpWrap{flex-direction:column}#vpPad{width:100%}}
</style></head><body>
<div class="bar"><span><b>SMART DEHUMIDIFIER</b> <span id="fw"></span></span>
<span><a id="vpBtn" onclick="vpToggle()" title="Virtual keypad - landscape text + 4x4 pad"
  style="color:var(--dim);text-decoration:none;margin-right:2vmin;cursor:pointer">&#9000; keypad</a><a href="/eelog.csv" download title="Download all cycle data (CSV)"
  style="color:var(--dim);text-decoration:none;margin-right:2vmin">&#11015; data</a><span id="clk" onclick="clkPane()">&#9201; --</span><span id="sup" class="ok">&#9728; SOLAR MODE</span>
<span id="pmd">AGARBATTI</span></span></div>
<div id="stateRow"><span id="st">--</span><span id="times">--</span></div>
<div id="grid">
 <div class="tile"><div class="lbl">T1 TOP</div><div class="val" id="t1">--</div>
   <div class="sub">T2 <span id="t2">--</span> &middot; AVG <span id="ta">--</span></div></div>
 <div class="tile"><div class="lbl">HUMIDITY</div><div class="val" id="rh">--</div>
   <div class="sub">OUT <span id="out">--</span> (<span id="outsrc">--</span>)</div></div>
 <div class="tile"><div class="lbl">BATTERY</div><div class="val" id="bat">--</div>
   <div class="sub"><span id="batv">--</span> V &middot; <span id="batn">--</span></div>
   <div class="hbar"><i id="batBar" style="background:var(--green);width:0%"></i></div></div>
 <div class="tile"><div class="lbl">WEIGHT</div><div class="val" id="wt">--</div>
   <div class="sub" id="wt2">no target</div></div>
 <div class="tile"><div class="lbl">HEAT</div><div class="val" id="heat">--</div>
   <div class="hbar"><i id="heatBar" style="background:var(--gold);width:0%"></i></div></div>
 <div class="tile"><div class="lbl">FAN</div><div class="val" id="fan">--</div>
   <div class="hbar"><i id="fanBar" style="background:var(--blue);width:0%"></i></div></div>
 <div class="tile"><div class="lbl">DOOR</div><div class="val" id="door">--</div>
   <div class="sub" id="door2">--</div></div>
 <div class="tile"><div class="lbl">TARGET</div><div class="val" id="set">--</div>
   <div class="sub">rate <span id="rate">--</span> g/min</div></div>
</div>
<div id="vpWrap"><div id="vpText"></div><div id="vpPad">
 <button onclick="vpKey('1')">1</button><button onclick="vpKey('2')">2<small>&#9650;</small></button><button onclick="vpKey('3')">3</button><button onclick="vpKey('A')">A<small>OK</small></button>
 <button onclick="vpKey('4')">4<small>&#9664;</small></button><button onclick="vpKey('5')">5</button><button onclick="vpKey('6')">6<small>&#9654;</small></button><button onclick="vpKey('B')">B<small>MENU</small></button>
 <button onclick="vpKey('7')">7</button><button onclick="vpKey('8')">8<small>&#9660;</small></button><button onclick="vpKey('9')">9</button><button onclick="vpKey('C')">C<small>MODE</small></button>
 <button onclick="vpKey('*')">*</button><button onclick="vpKey('0')">0</button><button onclick="vpKey('#')">#<small>BACK</small></button><button onclick="vpKey('D')">D<small>RUN</small></button>
</div></div>
<div id="foot"><span id="fault"></span><span id="hint">AgarbattiDryer &middot; full control page: 192.168.4.1</span></div>
<div id="doneCard" style="display:none;position:fixed;left:12px;right:12px;bottom:12px;
background:var(--card);border:2px solid var(--green);border-radius:14px;padding:2vmin 3vmin;
z-index:8;text-align:center">
<div style="font-size:6vmin;font-weight:800;color:var(--green)">DRYING COMPLETE</div>
<div style="font-size:3vmin;margin-top:1vmin">INITIAL <b id="dWi">--</b> g &middot; FINAL <b id="dWf">--</b> g
&middot; TARGET <b id="dWt">--</b> g</div>
<div style="font-size:4vmin;margin-top:0.6vmin;color:var(--gold)">MOISTURE REMOVED <b id="dWm">--</b> g
&middot; DURATION <b id="dDu">--</b></div>
<div style="font-size:2.6vmin;color:var(--dim);margin-top:0.6vmin">batch ready for packaging &middot; the green light means GO</div></div>
<div id="clkPane"><b>&#9201; SET CLOCK</b><br>
<input type="datetime-local" id="kdt">
<button onclick="kSet()">Set</button>
<button onclick="kDev()">Use this device</button>
<button onclick="clkPane()" style="background:rgba(8,24,80,.7);color:#fff8ea;border:1px solid rgba(233,196,106,.45)">Close</button>
<div class="sml" style="color:var(--dim);margin-top:6px">full control: 192.168.4.1 &middot; keypad: menu 6/7 &middot; auto: open / on a phone</div></div>
<script>
var wake=null;
async function keepAwake(){try{wake=await navigator.wakeLock.request('screen')}catch(e){}}
keepAwake();document.addEventListener('visibilitychange',()=>{if(!document.hidden)keepAwake()});
function f1(v){return v==null?'--':(Math.round(v*10)/10)}
const p2=n=>String(n).padStart(2,'0');
var kb=0,kbm=0,kTz=330;
function kFmt(s){var d=new Date(s*1000);
var W=['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
var M=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
return W[d.getUTCDay()]+' '+d.getUTCDate()+' '+M[d.getUTCMonth()]+' '
+p2(d.getUTCHours())+':'+p2(d.getUTCMinutes())+':'+p2(d.getUTCSeconds())}
function clkPane(){var p=$('clkPane');
p.style.display=(p.style.display==='block')?'none':'block'}
function kSet(){var v=$('kdt').value;if(!v)return;
fetch('/api/settime?epoch='+Math.floor(new Date(v).getTime()/1000)+'&tz='+kTz,{method:'POST'}).catch(function(){});clkPane()}
function kDev(){var d=new Date();if(d.getFullYear()<2021)return;
fetch('/api/settime?epoch='+Math.floor(d.getTime()/1000)+'&tz='+(-d.getTimezoneOffset()),{method:'POST'}).catch(function(){});clkPane()}
function mmss(s){s=Math.max(0,s|0);var h=(s/3600)|0,m=((s%3600)/60)|0;
return h?h+'h '+String(m).padStart(2,'0')+'m':m+'m '+String(s%60).padStart(2,'0')+'s'}
// ---- v2.0.19: virtual keypad - same keys as the pillar keypad ----------
var vpS=null;
function vesc(s){return String(s==null?'':s).replace(/&/g,'&amp;').replace(/</g,'&lt;')}
function vpToggle(){document.body.classList.toggle('vp');vpDraw()}
function vpKey(k){fetch('/api/key?k='+encodeURIComponent(k),{method:'POST'})
  .then(function(){tick()}).catch(function(){})}
function vpDraw(){
  var t=$('vpText');if(!t||!vpS)return;var m=vpS.menu;
  if(!m){t.innerHTML=
   '<div class="sel" style="font-size:5vmin">'+vesc(vpS.state||'--')+'</div>'+
   '<div>'+f1(vpS.tAvg)+'\u00B0C \u00B7 '+Math.round(vpS.hAvg||0)+'%RH \u00B7 '+
   (vpS.bat&&vpS.bat.valid?vpS.bat.pct+'% battery':'--')+'</div>'+
   '<div style="margin-top:2vmin;color:var(--dim)">Press <b>B</b> (MENU) to open the on-device menu \u00B7 <b>D</b> run/stop \u00B7 <b>C</b> mode \u00B7 <b>*</b> home \u00B7 <b>#</b> back \u00B7 digits type \u00B7 2/4/6/8 move</div>';}
  else if(m.edit){t.innerHTML=
   '<div class="sel">ENTER VALUE</div>'+
   '<div style="font-size:9vmin;font-weight:800;color:var(--gold)">'+vesc(m.buf||'--')+'</div>'+
   '<div style="color:var(--dim)">'+vesc(m.hint)+' \u00B7 A save \u00B7 # cancel</div>';}
  else{var h='';(m.items||[]).forEach(function(it,i){h+=
   '<div style="display:flex;justify-content:space-between;gap:2vmin'+(i===m.cur?';color:var(--gold);font-weight:800':'')+'">'+
   '<span>'+(i===m.cur?'▶ ':'')+vesc(it.n)+'</span><b>'+vesc(it.v)+'</b></div>'});
   t.innerHTML=h||'menu empty';}}
var miss=0;
async function tick(){
 try{
  const r=await fetch('/api/data',{cache:'no-store'});const S=await r.json();miss=0;
  vpS=S;vpDraw();
  $('fw').textContent='v'+(S.fw||'');
  var sup=S.supply||{};$('sup').textContent=sup.solar?'\u2600 SOLAR MODE':'\u26A1 BYPASS MODE';
  $('sup').className=sup.live?'ok':'bad';
  $('pmd').textContent=(sup.mode===2)?'SILICAGEL':(sup.mode===1)?'USER':'AGARBATTI';
  var st=S.state||'--';$('st').textContent=st;$('st').className=st;
  $('times').textContent=(st==='DRYING'||st==='PURGING')
   ?'ELAPSED '+mmss(S.elapsed)+' \u00B7 LEFT '+mmss(S.remaining)
   :'READY - open 192.168.4.1 to start';
  var s1=S.s1||{},s2=S.s2||{};
  $('t1').textContent=s1.ok?f1(s1.t)+'\u00B0':'--';
  $('t2').textContent=s2.ok?f1(s2.t)+'\u00B0':'--';
  $('ta').textContent=f1(S.tAvg)+'\u00B0';
  $('rh').textContent=f1(S.hAvg)+'%';
  var dh=S.dht||{};
  if(dh.ok){$('out').textContent=f1(dh.t)+'\u00B0 '+Math.round(dh.h)+'%';$('outsrc').textContent='measured'}
  else{$('out').textContent='--';$('outsrc').textContent='no sensor'}
  var b=S.bat||{};
  $('bat').textContent=b.valid?b.pct+'%':'--';
  $('batv').textContent=b.valid?f1(b.v):'--';$('batn').textContent=b.name||'';
  $('batBar').style.width=(b.pct||0)+'%';
  var sc=S.scale||{};
  $('wt').textContent=sc.ok?Math.round(sc.g)+'g':'--';
  var tg=S.set?(+S.set.targetG||0):0;
  $('wt2').textContent=tg>0?('\u2192 '+Math.round(tg)+'g \u00B7 diff '+Math.round((sc.g||0)-tg)+'g'):'no target';
  $('heat').textContent=(S.heat||0)+'%';$('heatBar').style.width=(S.heat||0)+'%';
  var fo=S.fanOut!=null?S.fanOut:0;
  $('fan').textContent=(fo?'ON ':'OFF ')+fo+'%';$('fanBar').style.width=fo+'%';
  var d=S.door||{};
  $('door').textContent=d.closed?'CLOSED':'OPEN';
  $('door2').textContent=d.phase||'--';
  $('set').textContent=S.set?f1(S.set.setTemp)+'\u00B0':'--';
  $('rate').textContent=sc.ok?f1(sc.rate):'--';
  if(S.clockSet){kb=S.now+(S.tz||0)*60;kbm=Date.now();kTz=S.tz||330}
  $('clk').textContent=kb?('⏱ '+kFmt(kb+(Date.now()-kbm)/1000)):'⏱ set clock';
  var dn=(st==='DONE');
  $('doneCard').style.display=dn?'block':'none';
  if(dn){$('dWi').textContent=Math.round(S.wtStart||0);
    $('dWf').textContent=Math.round(S.finalG||0);
    $('dWt').textContent=Math.round(S.set?(+S.set.targetG||0):0);
    $('dWm').textContent=Math.round(S.moistureG||0);
    $('dDu').textContent=mmss(S.cycleSecs||0);}
  $('fault').textContent=S.fault||'';
  $('hint').style.display=S.fault?'none':'inline';
 }catch(e){if(++miss>5){$('st').textContent='WAITING';$('st').className='';
  $('hint').textContent='waiting for the dryer - check WiFi AgarbattiDryer'}}
}
function $(i){return document.getElementById(i)}
setInterval(tick,1000);tick();
</script></body></html>)HTML";
