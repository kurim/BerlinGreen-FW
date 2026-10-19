// WebUI: alles, was die App kann (Licht, Zeitplan, Wasserstand, Blinken, WLAN, OTA) plus MQTT/System.
#include "gb.h"
#include <WebServer.h>

static WebServer server(80);

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>GreenBox</title>
<link rel="icon" type="image/png" href="/favicon.ico">
<style>
:root{--bg:#f4f6f4;--fg:#1d2a1d;--card:#fff;--ac:#2e7d32;--mu:#6b7a6b;--bd:#dde4dd}
@media(prefers-color-scheme:dark){:root{--bg:#121612;--fg:#e6ede6;--card:#1b221b;--ac:#66bb6a;--mu:#93a393;--bd:#2c372c}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px system-ui,sans-serif}
main{max-width:640px;margin:0 auto;padding:12px 16px 40px}h1{font-size:20px;margin:12px 0}
section{background:var(--card);border:1px solid var(--bd);border-radius:12px;padding:12px 14px;margin:12px 0}
h2{font-size:14px;margin:0 0 8px;color:var(--mu);text-transform:uppercase;letter-spacing:.05em}
label{display:block;margin:8px 0 2px;color:var(--mu);font-size:13px}
input[type=text],input[type=password],input[type=number],input[type=time],select{width:100%;padding:8px;border:1px solid var(--bd);border-radius:8px;background:transparent;color:var(--fg);font:inherit}
input[type=range]{width:100%;accent-color:var(--ac)}
button{padding:8px 12px;border:1px solid var(--ac);background:var(--ac);color:#fff;border-radius:8px;font:inherit;cursor:pointer;margin:6px 6px 0 0}
button.s{background:transparent;color:var(--ac)}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.row>*{flex:1}.kv{display:flex;justify-content:space-between;padding:3px 0;border-bottom:1px dashed var(--bd)}
.kv span:first-child{color:var(--mu)}.bar{height:10px;background:var(--bd);border-radius:6px;overflow:hidden}.bar i{display:block;height:100%;background:var(--ac)}
small{color:var(--mu)}
#toast{position:fixed;left:50%;bottom:18px;transform:translate(-50%,0);background:var(--ac);color:#fff;padding:8px 16px;border-radius:8px;
 opacity:0;pointer-events:none;transition:opacity .2s;font-size:14px;z-index:9}
#toast.err{background:#c62828}#toast.show{opacity:1}
</style></head><body><main>
<h1>GreenBox <small id="ver"></small></h1>
<section><h2>Status</h2>
 <div class="kv"><span>Licht</span><b id="lt"></b></div>
 <div class="kv"><span>Wasserstand</span><b id="wl"></b></div>
 <div class="kv"><span>WLAN</span><b id="wf"></b></div>
 <div class="kv"><span>MQTT</span><b id="mq"></b></div>
 <div class="kv"><span>Uhrzeit</span><b id="tm"></b></div>
</section>
<section><h2>Licht</h2>
 <div class="row"><button data-o="auto">Automatik</button><button data-o="on" class="s">An</button><button data-o="off" class="s">Aus</button></div>
 <label>Streifen 1 (warm) <b id="v1"></b></label><input type="range" min="0" max="100" id="stripe1">
 <label>Streifen 2 (neutral) <b id="v2"></b></label><input type="range" min="0" max="100" id="stripe2">
 <label>Streifen 3 (kalt) <b id="v3"></b></label><input type="range" min="0" max="100" id="stripe3">
 <label><input type="checkbox" id="blink"> Bei wenig Wasser blinken</label>
 <label><input type="checkbox" id="button"> Touch-Taster (Pad unter dem Gehaeuse, IO12) auswerten</label>
</section>
<section><h2>Touch-Sensoren (live)</h2>
 <small>Niedriger Wert = beruehrt/benetzt. Taster loest aus, wenn Wert &lt; Baseline &minus; Schwelle.</small>
 <div class="kv"><span>Taster IO12</span><b id="tb"></b></div>
 <div class="kv"><span>Wasser A (IO14)</span><b id="ta"></b></div>
 <div class="kv"><span>Wasser B (IO27)</span><b id="tb2"></b></div>
 <div class="kv"><span>Wasser C (IO13)</span><b id="tc"></b></div>
 <label>Taster-Schwelle (Original: 5)</label><input type="number" min="1" max="50" id="button_delta">
 <label>Abtastungen in Folge (je 50 ms): <b id="bsv"></b></label><input type="range" min="1" max="10" id="button_samples">
 <button id="bdsave" class="s">Taster speichern</button>
</section>
<section><h2>Zeitplan</h2>
 <small>Zeiten in der Zeitzone der Box (Firmware-Byte &lt; 4: UTC wie Original, sonst lokale Zeit).</small>
 <div class="row"><div><label>Start Werktag</label><input type="time" id="start"></div><div><label>Dauer (h)</label><input type="number" min="0" max="24" id="duration"></div></div>
 <div class="row"><div><label>Start Wochenende</label><input type="time" id="start_weekend"></div><div><label>Dauer WE (h, 0 = wie Werktag)</label><input type="number" min="0" max="24" id="duration_weekend"></div></div>
 <button id="sched">Speichern</button> <button class="s" id="sync">Uhrzeit vom Browser</button>
</section>
<section><h2>WLAN</h2>
 <div class="row"><button class="s" id="scan">Netzwerke suchen</button></div>
 <label>Netzwerk</label><select id="aps"></select>
 <label>oder SSID</label><input type="text" id="ssid" autocapitalize="off">
 <label>Passwort</label><input type="password" id="wpass">
 <button id="wconn">Verbinden</button> <small id="wmsg"></small>
</section>
<section><h2>Firmware-Update (OTA)</h2>
 <label>Firmware-URL</label><input type="text" id="ota_url">
 <label><input type="checkbox" id="ota_auto"> Taeglich automatisch pruefen und installieren</label>
 <label><input type="checkbox" id="ota_insecure"> TLS-Zertifikat nicht pruefen (unsicher)</label>
 <div class="bar"><i id="otabar" style="width:0"></i></div><small id="otamsg"></small>
 <button id="otasave">URL speichern</button><button class="s" id="otacheck">Pruefen</button><button class="s" id="otastart">Jetzt installieren</button>
</section>
<section><h2>MQTT</h2>
 <label><input type="checkbox" id="mqtt_enabled"> aktiviert</label>
 <div class="row"><div><label>Host</label><input type="text" id="mqtt_host"></div><div><label>Port</label><input type="number" id="mqtt_port"></div></div>
 <label><input type="checkbox" id="mqtt_tls"> TLS (ohne Zertifikatspruefung)</label>
 <div class="row"><div><label>Benutzer</label><input type="text" id="mqtt_user"></div><div><label>Passwort</label><input type="password" id="mqtt_password" placeholder="(unveraendert)"></div></div>
 <label>Basis-Topic (leer = greenbox/&lt;id&gt;)</label><input type="text" id="mqtt_base">
 <button id="mqsave">Speichern</button>
</section>
<section><h2>System</h2>
 <label>Zeitzone (POSIX, nur bei Firmware-Byte &ge; 4)</label><input type="text" id="tz">
 <label>Firmware-Byte (3 = wie Original 2.0.3 / UTC, 4 = lokale Zeit)</label><select id="fwbyte"><option>3</option><option>4</option></select>
 <label>Hostname</label><input type="text" id="hostname">
 <label>Web-Passwort (Benutzer admin; leer = keins)</label><input type="password" id="web_password" placeholder="(unveraendert)">
 <label>Fallback-AP-Passwort (min. 8 Zeichen)</label><input type="password" id="ap_password" placeholder="(unveraendert)">
 <button id="syssave">Speichern</button><button class="s" id="reboot">Neustart</button>
</section>
</main><div id="toast"></div><script>
const $=id=>document.getElementById(id);let S={},busy=false,toastT;
async function post(p,o){const b=new URLSearchParams(o||{});const r=await fetch(p,{method:'POST',body:b});return r.ok?r.json():null}
function toast(msg,err){const t=$('toast');t.textContent=msg;t.className=err?'err show':'show';clearTimeout(toastT);toastT=setTimeout(()=>t.className='',2000)}
async function save(p,o){const r=await post(p,o);toast(r?'Gespeichert':'Fehler beim Speichern',!r);return r}
function fillOnce(){const f=['duration','duration_weekend','start','start_weekend','ota_url','mqtt_host','mqtt_port','mqtt_user','mqtt_base','tz','hostname'];
 for(const k of f)$(k).value=S[k]??(S.ota&&S.ota.url&&k=='ota_url'?S.ota.url:'');
 $('ota_url').value=S.ota.url;$('mqtt_host').value=S.mqtt.host;$('mqtt_port').value=S.mqtt.port;$('mqtt_user').value=S.mqtt.user;$('mqtt_base').value=S.mqtt.base;
 $('mqtt_enabled').checked=S.mqtt.enabled;$('mqtt_tls').checked=S.mqtt.tls;$('ota_auto').checked=S.ota.auto;$('ota_insecure').checked=S.ota.insecure;
 $('fwbyte').value=S.fw_byte;$('blink').checked=S.blink_on_low_water;$('button').checked=S.button_enabled;$('button_delta').value=S.button_delta;$('button_samples').value=S.button_samples}
function render(){
 $('ver').textContent='v'+S.fw+' - '+S.id;
 $('lt').textContent=(S.light_on?'an':'aus')+' ('+S.override+')';
 $('wl').textContent=S.water==255?'unbekannt':S.water+' %'+(S.low_water?' - niedrig':'');
 $('wf').textContent=S.wifi.connected?S.wifi.ssid+' ('+S.wifi.rssi+' dBm) '+S.wifi.ip:(S.wifi.ap?'Fallback-AP aktiv':'getrennt');
 $('mq').textContent=!S.mqtt.enabled?'aus':(S.mqtt.connected?'verbunden':'getrennt');
 $('tm').textContent=S.time_valid?new Date(S.time*1000).toLocaleString():'nicht gesetzt';
 $('tb').textContent=S.touch[3]+' (Baseline '+S.touch_base+')'+(S.button_pressed?' - BERUEHRT':'');$('ta').textContent=S.touch[0];$('tb2').textContent=S.touch[1];$('tc').textContent=S.touch[2];
 $('bsv').textContent=document.activeElement===$('button_samples')?$('button_samples').value:S.button_samples;
 for(let i=1;i<=3;i++){if(document.activeElement!==$('stripe'+i))$('stripe'+i).value=S['stripe'+i];$('v'+i).textContent=S['stripe'+i]+' %'}
 document.querySelectorAll('[data-o]').forEach(b=>b.className=b.dataset.o==S.override?'':'s');
 $('otabar').style.width=S.ota.progress+'%';$('otamsg').textContent=S.ota.msg+(S.ota.latest?' (neueste: '+S.ota.latest+')':'');
}
async function poll(){try{const r=await fetch('/api/state');if(r.ok){const first=!S.fw;S=await r.json();if(first)fillOnce();render()}}catch(e){}setTimeout(poll,1000)}
document.querySelectorAll('[data-o]').forEach(b=>b.onclick=async()=>{S=await post('/api/set',{override:b.dataset.o})||S;render()});
for(let i=1;i<=3;i++)$('stripe'+i).onchange=async e=>{S=await post('/api/set',{['stripe'+i]:e.target.value})||S;render()};
$('blink').onchange=async e=>{S=await post('/api/set',{blink:e.target.checked?1:0})||S};
$('button').onchange=async e=>{S=await post('/api/set',{button:e.target.checked?1:0})||S};
$('button_samples').oninput=e=>$('bsv').textContent=e.target.value;
$('bdsave').onclick=async()=>{S=await save('/api/set',{button_delta:$('button_delta').value,button_samples:$('button_samples').value})||S};
$('sched').onclick=async()=>{S=await save('/api/set',{start:$('start').value,start_weekend:$('start_weekend').value,duration:$('duration').value,duration_weekend:$('duration_weekend').value})||S;render()};
$('sync').onclick=async()=>{S=await post('/api/set',{time:Math.floor(Date.now()/1000)})||S;render()};
$('scan').onclick=async()=>{$('wmsg').textContent='suche...';await post('/api/scan');setTimeout(async()=>{const l=await (await fetch('/api/scan')).json();
 $('aps').innerHTML='<option value="">-</option>'+l.map(a=>`<option value="${a.ssid.replace(/"/g,'&quot;')}">${a.ssid} (${a.rssi} dBm)</option>`).join('');$('wmsg').textContent=l.length+' gefunden'},4500)};
$('aps').onchange=e=>{if(e.target.value)$('ssid').value=e.target.value};
$('wconn').onclick=async()=>{await post('/api/wifi',{ssid:$('ssid').value,pass:$('wpass').value});$('wmsg').textContent='verbinde (bis 30 s)...'};
$('otasave').onclick=async()=>{S=await save('/api/set',{ota_url:$('ota_url').value,ota_auto:$('ota_auto').checked?1:0,ota_insecure:$('ota_insecure').checked?1:0})||S};
$('otacheck').onclick=()=>post('/api/ota',{action:'check'});
$('otastart').onclick=()=>{if(confirm('Firmware jetzt von der URL installieren?'))post('/api/ota',{action:'start'})};
$('mqsave').onclick=async()=>{const o={mqtt_enabled:$('mqtt_enabled').checked?1:0,mqtt_host:$('mqtt_host').value,mqtt_port:$('mqtt_port').value,mqtt_tls:$('mqtt_tls').checked?1:0,mqtt_user:$('mqtt_user').value,mqtt_base:$('mqtt_base').value};
 if($('mqtt_password').value)o.mqtt_password=$('mqtt_password').value;S=await save('/api/set',o)||S;$('mqtt_password').value=''};
$('syssave').onclick=async()=>{const o={tz:$('tz').value,fwbyte:$('fwbyte').value,hostname:$('hostname').value};
 if($('web_password').value)o.web_password=$('web_password').value;if($('ap_password').value)o.ap_password=$('ap_password').value;
 S=await save('/api/set',o)||S;$('web_password').value='';$('ap_password').value=''};
$('reboot').onclick=()=>{if(confirm('Neustart?'))post('/api/reboot')};
poll();
</script></body></html>)HTML";

static bool auth() {
  if (cfg.webPass.length() && !server.authenticate("admin", cfg.webPass.c_str())) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

// Favicon (32x32 PNG, aus dark_icon.png generiert): 1393 Bytes
static const uint8_t FAVICON_PNG[] PROGMEM = {
137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,32,
0,0,0,32,8,6,0,0,0,115,122,122,244,0,0,5,56,73,68,65,
84,120,218,237,87,93,136,93,213,21,254,190,189,247,153,153,123,51,99,114,
147,153,36,19,33,169,198,90,141,193,162,82,80,95,146,177,15,149,62,245,
229,54,80,106,176,10,201,67,95,138,8,149,150,50,51,18,177,138,98,91,
68,76,164,80,208,7,153,139,32,182,133,132,86,26,245,205,162,15,69,242,
52,136,105,109,50,77,50,153,142,243,115,231,252,236,245,249,112,238,185,115,
147,76,34,132,4,34,184,225,112,126,216,123,253,124,107,173,111,173,3,124,
179,110,180,53,62,14,167,169,41,47,201,93,203,107,106,170,233,1,240,134,
115,152,189,158,79,78,194,246,63,56,178,115,207,253,223,251,205,230,209,209,
237,133,73,4,32,137,206,57,192,0,81,221,51,148,4,231,32,137,36,165,
114,59,28,157,204,140,32,64,82,142,192,103,211,159,125,252,122,235,221,167,
63,60,143,5,149,138,5,0,161,146,53,49,33,253,113,146,181,31,124,255,
161,183,155,251,247,239,158,253,34,5,100,32,9,73,157,123,143,217,18,192,
74,140,122,124,89,125,38,1,51,67,223,64,13,99,106,143,185,36,12,127,
248,242,177,71,91,83,77,143,31,183,98,215,128,241,113,144,164,141,13,97,
251,238,123,239,219,253,207,79,254,61,243,218,75,47,30,216,216,104,44,23,
89,219,153,81,95,5,101,133,194,5,240,210,92,44,44,102,145,183,62,117,
104,226,149,111,223,185,107,47,112,44,236,219,247,86,81,185,17,122,15,248,
4,14,46,104,241,220,185,217,63,189,63,253,231,107,24,234,143,126,54,51,
115,216,135,126,187,56,17,195,26,73,65,159,36,126,124,207,158,48,113,252,
184,38,246,238,229,93,155,55,235,106,180,206,53,62,117,141,185,95,218,51,
239,60,178,33,9,9,218,176,75,170,32,172,149,150,38,97,242,189,247,226,
211,222,75,102,236,4,246,106,74,90,7,143,236,179,157,80,148,212,35,102,
85,156,187,52,152,184,90,125,95,89,111,196,165,185,228,214,220,200,235,192,
23,210,154,60,212,27,2,246,215,19,47,19,32,225,240,129,251,194,129,195,
47,232,200,193,39,121,251,232,224,85,65,114,22,103,221,63,198,155,246,212,
239,126,235,171,16,60,252,240,109,238,216,177,79,187,40,247,26,160,191,126,
158,159,123,214,57,136,62,30,60,242,81,126,240,200,216,53,112,125,18,0,
102,233,3,204,100,71,143,78,167,189,8,179,67,39,120,242,135,187,182,236,
216,62,122,232,214,239,220,241,216,226,114,122,190,61,127,246,77,6,15,69,
35,232,122,16,48,176,39,114,170,190,117,152,178,27,88,3,36,144,142,182,
188,220,30,26,221,190,243,167,202,22,219,255,57,249,249,115,175,190,255,183,
231,79,156,64,14,64,161,213,108,58,180,90,49,154,125,107,203,214,225,199,
3,210,184,113,208,109,26,216,120,243,207,43,230,19,9,118,9,80,221,28,
209,133,108,126,209,91,25,115,117,24,113,101,37,141,110,93,223,96,125,176,
118,32,155,193,31,0,100,149,243,104,54,155,190,213,106,197,71,31,184,249,
161,177,189,15,190,187,161,209,40,110,187,125,167,249,16,86,21,118,9,156,
149,88,200,34,0,129,62,233,128,147,67,32,73,47,64,144,68,31,18,205,
207,47,224,191,39,79,38,167,78,157,62,115,232,247,127,185,235,20,48,187,
38,19,110,219,186,241,255,131,67,131,88,191,97,61,55,109,30,9,193,59,
168,20,213,105,54,198,170,148,232,92,9,49,80,238,184,136,150,157,115,146,
0,231,28,251,146,36,46,204,205,114,105,105,168,112,192,242,101,137,40,141,
76,242,44,199,74,187,141,60,75,161,16,96,166,42,10,29,191,217,1,46,
130,128,86,27,85,9,40,233,32,43,0,18,50,99,8,137,210,44,99,187,
221,70,158,229,188,9,240,151,53,192,34,149,244,245,161,86,175,3,244,16,
64,146,34,29,4,3,85,26,130,138,213,202,8,18,176,114,143,69,136,6,
178,242,222,11,36,188,3,234,245,58,188,247,56,123,37,34,74,0,100,233,
10,218,75,75,101,43,238,122,94,166,52,36,200,140,146,161,186,32,147,4,
40,230,128,243,148,132,88,172,64,86,208,98,1,150,167,177,180,184,136,24,
11,140,92,201,0,71,42,36,9,250,7,250,209,73,34,152,197,238,32,32,
8,102,17,66,217,231,37,208,98,78,73,93,131,232,124,167,90,162,4,176,
200,51,0,68,173,94,131,115,238,138,8,208,209,121,139,134,44,47,96,38,
72,70,51,67,180,200,34,22,140,249,10,36,201,98,1,179,200,88,100,138,
38,20,69,206,104,98,44,50,196,34,99,52,131,137,180,88,72,32,98,145,
35,75,83,68,51,124,183,209,184,108,14,232,228,233,153,185,187,117,39,72,
41,4,7,239,61,156,115,93,34,151,3,232,66,25,245,34,149,115,125,0,
162,149,132,147,148,101,72,15,209,224,124,144,44,71,72,2,2,37,201,96,
146,253,125,110,110,254,2,222,16,192,9,128,233,143,238,104,108,221,178,237,
87,195,35,195,79,212,234,3,182,105,120,152,93,202,148,186,131,86,247,73,
17,116,1,178,2,164,235,140,103,90,189,119,167,34,34,203,50,205,207,205,
115,97,97,113,105,230,204,236,175,143,127,112,234,240,209,233,233,172,203,132,
147,173,86,252,69,158,236,186,101,199,182,39,6,234,181,88,20,5,191,152,
155,21,47,170,239,82,160,211,106,103,171,216,78,236,144,147,170,175,130,177,
195,92,34,137,129,129,16,215,223,52,50,72,232,185,90,56,209,2,112,186,
195,35,171,235,39,247,108,221,49,180,174,111,157,60,173,175,191,255,210,190,
146,2,232,191,66,223,73,59,163,157,163,98,34,122,82,200,128,20,41,138,
156,114,209,220,242,210,202,249,55,254,245,191,51,55,222,127,65,245,111,112,
189,21,78,76,66,188,46,35,215,215,117,125,9,247,51,24,173,164,227,156,
111,0,0,0,0,73,69,78,68,174,66,96,130
};

static void sendState() { server.send(200, "application/json", stateJson()); }

void webBegin() {
  server.on("/", HTTP_GET, [] {
    if (!auth()) return;
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.on("/api/state", HTTP_GET, [] { if (auth()) sendState(); });
  server.on("/api/set", HTTP_POST, [] {
    if (!auth()) return;
    for (int i = 0; i < server.args(); i++) applySetting(server.argName(i), server.arg(i));
    if (server.hasArg("web_password") || server.hasArg("ap_password") || server.hasArg("hostname")) {}  // wirkt nach Neustart
    sendState();
    mqttPublishState();
    bleStatusChanged();
  });
  server.on("/api/scan", HTTP_POST, [] { if (!auth()) return; netRequestScan(); server.send(200, "application/json", "{}"); });
  server.on("/api/scan", HTTP_GET, [] { if (auth()) server.send(200, "application/json", netScanJson(false)); });
  server.on("/api/wifi", HTTP_POST, [] {
    if (!auth()) return;
    netRequestConnect(server.arg("ssid"), server.arg("pass"));
    server.send(200, "application/json", "{}");
  });
  server.on("/api/ota", HTTP_POST, [] {
    if (!auth()) return;
    if (server.arg("action") == "check") netRequestCheckUpdate();
    else if (server.arg("action") == "start") netRequestOta();
    server.send(200, "application/json", "{}");
  });
  server.on("/api/reboot", HTTP_POST, [] { if (!auth()) return; server.send(200, "application/json", "{}"); netReboot(); });
  server.on("/favicon.ico", HTTP_GET, [] {
    server.send_P(200, "image/png", (const char*)FAVICON_PNG, sizeof(FAVICON_PNG));
  });
  server.onNotFound([] {
    if (netApActive()) {                         // Captive Portal
      server.sendHeader("Location", "http://192.168.4.1/", true);
      server.send(302, "text/plain", "");
    } else server.send(404, "text/plain", "not found");
  });
  server.begin();
}

void webLoop() { server.handleClient(); }
