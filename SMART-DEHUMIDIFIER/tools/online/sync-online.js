#!/usr/bin/env node
/**
 * sync-online.js - generate docs/index.html (GitHub Pages version of the
 * dashboard) from src/webui.h, so the UI has ONE source of truth.
 *
 * What the online layer adds on top of the embedded page:
 *   1. Bridge mode: when this page runs inside the ESP loader frame
 *      (http://192.168.4.1/online), all /api calls travel via postMessage
 *      (a browser will not let an https page call http://192.168.4.1
 *      directly - mixed content).
 *   2. Standalone mode: opened from anywhere, calls go to a configurable
 *      ESP address (default http://192.168.4.1 - only works from non-https
 *      contexts; use the /online link on the phone instead).
 *   3. CSV / cycle downloads go through the same pipe (no direct links).
 *   4. "Back up cycles to GitHub" - pushes every cycle CSV from the ESP
 *      into a GitHub repo via the contents API (token kept in the phone's
 *      localStorage only).
 *
 * Run after editing src/webui.h:   node tools/online/sync-online.js
 */
'use strict';
const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '../..');
const src = fs.readFileSync(path.join(root, 'src/webui.h'), 'utf8');
const part = src.split('R"HTML(', 2);
if (part.length !== 2) throw new Error('raw HTML literal not found in webui.h');
let html = part[1].split(')HTML"', 2)[0];

// ---------------------------------------------------------------- banner
const banner = `
<div id="modeBanner" style="max-width:860px;margin:10px auto 0;padding:8px 14px;
border:1px dashed #2c3a32;border-radius:10px;font-size:12px;color:#8fa89a">
&#127760; online interface &middot; <span id="modeTxt">connecting&hellip;</span></div>
`;
html = html.replace('<nav>', banner + '\n<nav>');

// ------------------------------------------------- GitHub backup section
const ghSection = `
    <details style="margin-top:10px"><summary>&#128230; Back up cycles to GitHub (optional)</summary>
      <p class="note" style="margin:6px 0">Uploads every cycle CSV saved on the ESP to a GitHub
      repository using a personal access token (contents: read+write). The token is stored only in
      this browser (localStorage) and is sent only to api.github.com. The ESP itself never touches
      the internet.</p>
      <div class="frow">
        <div><label>Repository (user/repo)</label><input id="ghRepo" placeholder="PAVAN-NIKHIL-993/arena"></div>
        <div><label>Branch</label><input id="ghBranch" value="main"></div>
        <div><label>Folder</label><input id="ghDir" value="dryer-cycles"></div>
      </div>
      <div class="frow">
        <div><label>Token (fine-grained PAT, contents:write)</label><input type="password" id="ghToken" placeholder="github_pat_&hellip;"></div>
        <div><label>ESP address (standalone mode)</label><input id="espBase" value="http://192.168.4.1"></div>
        <div><label>&nbsp;</label><button class="act" onclick="ghBackup()"
          style="width:100%;background:#233029;color:var(--tx)">&#128228; Back up all cycles</button></div>
      </div>
      <div class="note" id="ghStat"></div>
    </details>
`;
html = html.replace(
  '    <div class="controls" style="margin:10px 0 0">\n      <button class="act" id="btnClearCyc"',
  ghSection + '    <div class="controls" style="margin:10px 0 0">\n      <button class="act" id="btnClearCyc"');

// ------------------------------------------------------------ JS surgery
const js = `
// ======================= online layer (auto-injected) =======================
const O$=id=>document.getElementById(id);
const BRIDGED=(function(){try{return window.parent!==window}catch(e){return false}})();
let ESP_BASE=(localStorage.getItem('espBase')||'http://192.168.4.1');
O$('espBase').value=ESP_BASE;
O$('espBase').onchange=function(){ESP_BASE=this.value.trim().replace(/\\/$/,'');
  localStorage.setItem('espBase',ESP_BASE)};
let bridgeSeq=1;const bridgePend={};
if(BRIDGED){addEventListener('message',function(e){
  const m=e.data;if(m&&m.bridgeId&&bridgePend[m.bridgeId]){
    bridgePend[m.bridgeId](m);delete bridgePend[m.bridgeId]}});}
function espFetchBridge(url,opts){opts=opts||{};
  if(BRIDGED){return new Promise(function(res){
    const id=bridgeSeq++;bridgePend[id]=function(m){
      res({ok:m.status>=200&&m.status<300,status:m.status,
           text:async()=>m.body,json:async()=>{try{return JSON.parse(m.body)}catch(e){return{}}}})};
    parent.postMessage({bridgeId:id,url:url,method:opts.method||'GET',body:opts.body||null},'*');
    setTimeout(function(){if(bridgePend[id]){bridgePend[id]({ok:false,status:0,body:''});
      delete bridgePend[id]}},6000)})}
  return fetch(ESP_BASE+url,opts)}
if(BRIDGED){try{parent.postMessage({hello:1},'*')}catch(e){}}
O$('modeTxt').innerHTML=BRIDGED
  ?'bridged to the dryer over its hotspot &mdash; live data, configs and backups all work'
  :('standalone &mdash; connect this device to the <b>AgarbattiDryer</b> hotspot and open '
   +'<a class="dl" href="http://192.168.4.1/online">http://192.168.4.1/online</a> for full function');
// downloads must ride the same pipe (direct <a> links would hit github.io)
document.addEventListener('click',function(e){
  const a=e.target.closest('a');if(!a)return;
  const href=a.getAttribute('href')||'';
  if(!href.startsWith('/api/'))return;
  e.preventDefault();
  espFetchBridge(href).then(r=>r.text()).then(t=>{
    const name=(a.getAttribute('download')||'download').replace(/[^\\w.-]/g,'_')
      ||'download';
    const blob=new Blob([t],{type:'text/csv'});
    const u=URL.createObjectURL(blob);const l=document.createElement('a');
    l.href=u;l.download=name.includes('.')?name:name+'.csv';
    document.body.appendChild(l);l.click();l.remove();URL.revokeObjectURL(u);
  }).catch(()=>toast('Download failed',1));
});
// ---- GitHub cycle backup (browser -> api.github.com, token in localStorage)
function ghCfg(){return{repo:$('ghRepo').value.trim(),br:$('ghBranch').value.trim()||'main',
  dir:$('ghDir').value.trim().replace(/^\\/|\\/$/g,'')||'dryer-cycles',
  tok:$('ghToken').value.trim()}}
['ghRepo','ghBranch','ghDir','ghToken'].forEach(id=>{const el=O$(id);
  el.value=localStorage.getItem(id)||el.value;
  el.onchange=()=>localStorage.setItem(id,el.value)});
function ghStat(m){$('ghStat').textContent=m}
async function ghPut(tok,repo,br,path,content,msg,sha){
  const body={message:msg,content:btoa(unescape(encodeURIComponent(content))),branch:br};
  if(sha)body.sha=sha;
  const r=await fetch('https://api.github.com/repos/'+repo+'/contents/'+path,{
    method:'PUT',headers:{'Authorization':'Bearer '+tok,'Accept':'application/vnd.github+json'},
    body:JSON.stringify(body)});
  return r}
async function ghBackup(){
  const c=ghCfg();
  if(!c.repo||!c.tok){ghStat('Repository and token are required');return}
  ghStat('reading cycle list from the dryer\u2026');
  let list;try{list=await(await espFetchBridge('/api/cycles')).json()}catch(e){
    ghStat('Cannot reach the dryer - open this page via http://192.168.4.1/online');return}
  if(!list.length){ghStat('No saved cycles on the dryer yet');return}
  let ok=0,fail=0,skip=0;
  for(const cyc of list){
    const path=c.dir+'/'+cyc.file;
    ghStat('uploading '+(ok+fail+skip+1)+' / '+list.length+'\u2026');
    try{
      const r=await espFetchBridge('/api/cycle?file='+encodeURIComponent(cyc.file));
      if(!r.ok)throw new Error('read');
      const csv=await r.text();
      let put=await ghPut(c.tok,c.repo,c.br,path,csv,'dryer cycle '+cyc.started);
      if(put.status===422){                       // file exists -> update via sha
        const meta=await(await fetch('https://api.github.com/repos/'+c.repo+
          '/contents/'+path+'?ref='+c.br,{headers:{'Authorization':'Bearer '+c.tok}})).json();
        if(meta.sha){put=await ghPut(c.tok,c.repo,c.br,path,csv,
          'dryer cycle '+cyc.started+' (update)',meta.sha)}
        else{skip++;continue}
      }
      put.ok?ok++:fail++;
    }catch(e){fail++}
  }
  ghStat('Done: '+ok+' uploaded, '+skip+' unchanged, '+fail+' failed'
    +(fail?' - check token/repo/branch':''))
}
// ==================== end online layer (auto-injected) =====================
`;

html = html.replace("'use strict';", "'use strict';" + js, 1);
// route every ESP API call through the pipe (weather/geocoding stay direct)
html = html.split("fetch('/api").join("espFetch('/api");

const out = '<!-- AUTO-GENERATED from src/webui.h by tools/online/sync-online.js' +
            ' - edit webui.h, then re-run the script. Online layer injected.\n' +
     '     Served by GitHub Pages; full function when opened through' +
     ' http://192.168.4.1/online -->\n' + html;

fs.mkdirSync(path.join(root, 'docs'), { recursive: true });
fs.writeFileSync(path.join(root, 'docs/index.html'), out);
console.log('docs/index.html written,', out.length, 'bytes');
