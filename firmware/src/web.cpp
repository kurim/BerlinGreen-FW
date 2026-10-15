// WebUI: alles, was die App kann (Licht, Zeitplan, Wasserstand, Blinken, WLAN, OTA) plus MQTT/System.
#include "gb.h"
#include <WebServer.h>

static WebServer server(80);

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>GreenBox</title>
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
 <button id="bdsave" class="s">Schwelle speichern</button>
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
</main><script>
const $=id=>document.getElementById(id);let S={},busy=false;
async function post(p,o){const b=new URLSearchParams(o||{});const r=await fetch(p,{method:'POST',body:b});return r.ok?r.json():null}
function fillOnce(){const f=['duration','duration_weekend','start','start_weekend','ota_url','mqtt_host','mqtt_port','mqtt_user','mqtt_base','tz','hostname'];
 for(const k of f)$(k).value=S[k]??(S.ota&&S.ota.url&&k=='ota_url'?S.ota.url:'');
 $('ota_url').value=S.ota.url;$('mqtt_host').value=S.mqtt.host;$('mqtt_port').value=S.mqtt.port;$('mqtt_user').value=S.mqtt.user;$('mqtt_base').value=S.mqtt.base;
 $('mqtt_enabled').checked=S.mqtt.enabled;$('mqtt_tls').checked=S.mqtt.tls;$('ota_auto').checked=S.ota.auto;$('ota_insecure').checked=S.ota.insecure;
 $('fwbyte').value=S.fw_byte;$('blink').checked=S.blink_on_low_water;$('button').checked=S.button_enabled;$('button_delta').value=S.button_delta}
function render(){
 $('ver').textContent='v'+S.fw+' - '+S.id;
 $('lt').textContent=(S.light_on?'an':'aus')+' ('+S.override+')';
 $('wl').textContent=S.water==255?'unbekannt':S.water+' %'+(S.low_water?' - niedrig':'');
 $('wf').textContent=S.wifi.connected?S.wifi.ssid+' ('+S.wifi.rssi+' dBm) '+S.wifi.ip:(S.wifi.ap?'Fallback-AP aktiv':'getrennt');
 $('mq').textContent=!S.mqtt.enabled?'aus':(S.mqtt.connected?'verbunden':'getrennt');
 $('tm').textContent=S.time_valid?new Date(S.time*1000).toLocaleString():'nicht gesetzt';
 $('tb').textContent=S.touch[3]+' (Baseline '+S.touch_base+')'+(S.button_pressed?' - BERUEHRT':'');$('ta').textContent=S.touch[0];$('tb2').textContent=S.touch[1];$('tc').textContent=S.touch[2];
 for(let i=1;i<=3;i++){if(document.activeElement!==$('stripe'+i))$('stripe'+i).value=S['stripe'+i];$('v'+i).textContent=S['stripe'+i]+' %'}
 document.querySelectorAll('[data-o]').forEach(b=>b.className=b.dataset.o==S.override?'':'s');
 $('otabar').style.width=S.ota.progress+'%';$('otamsg').textContent=S.ota.msg+(S.ota.latest?' (neueste: '+S.ota.latest+')':'');
}
async function poll(){try{const r=await fetch('/api/state');if(r.ok){const first=!S.fw;S=await r.json();if(first)fillOnce();render()}}catch(e){}setTimeout(poll,1000)}
document.querySelectorAll('[data-o]').forEach(b=>b.onclick=async()=>{S=await post('/api/set',{override:b.dataset.o})||S;render()});
for(let i=1;i<=3;i++)$('stripe'+i).onchange=async e=>{S=await post('/api/set',{['stripe'+i]:e.target.value})||S;render()};
$('blink').onchange=async e=>{S=await post('/api/set',{blink:e.target.checked?1:0})||S};
$('button').onchange=async e=>{S=await post('/api/set',{button:e.target.checked?1:0})||S};
$('bdsave').onclick=async()=>{S=await post('/api/set',{button_delta:$('button_delta').value})||S};
$('sched').onclick=async()=>{S=await post('/api/set',{start:$('start').value,start_weekend:$('start_weekend').value,duration:$('duration').value,duration_weekend:$('duration_weekend').value})||S;render()};
$('sync').onclick=async()=>{S=await post('/api/set',{time:Math.floor(Date.now()/1000)})||S;render()};
$('scan').onclick=async()=>{$('wmsg').textContent='suche...';await post('/api/scan');setTimeout(async()=>{const l=await (await fetch('/api/scan')).json();
 $('aps').innerHTML='<option value="">-</option>'+l.map(a=>`<option value="${a.ssid.replace(/"/g,'&quot;')}">${a.ssid} (${a.rssi} dBm)</option>`).join('');$('wmsg').textContent=l.length+' gefunden'},4500)};
$('aps').onchange=e=>{if(e.target.value)$('ssid').value=e.target.value};
$('wconn').onclick=async()=>{await post('/api/wifi',{ssid:$('ssid').value,pass:$('wpass').value});$('wmsg').textContent='verbinde (bis 30 s)...'};
$('otasave').onclick=async()=>{S=await post('/api/set',{ota_url:$('ota_url').value,ota_auto:$('ota_auto').checked?1:0,ota_insecure:$('ota_insecure').checked?1:0})||S};
$('otacheck').onclick=()=>post('/api/ota',{action:'check'});
$('otastart').onclick=()=>{if(confirm('Firmware jetzt von der URL installieren?'))post('/api/ota',{action:'start'})};
$('mqsave').onclick=async()=>{const o={mqtt_enabled:$('mqtt_enabled').checked?1:0,mqtt_host:$('mqtt_host').value,mqtt_port:$('mqtt_port').value,mqtt_tls:$('mqtt_tls').checked?1:0,mqtt_user:$('mqtt_user').value,mqtt_base:$('mqtt_base').value};
 if($('mqtt_password').value)o.mqtt_password=$('mqtt_password').value;S=await post('/api/set',o)||S;$('mqtt_password').value=''};
$('syssave').onclick=async()=>{const o={tz:$('tz').value,fwbyte:$('fwbyte').value,hostname:$('hostname').value};
 if($('web_password').value)o.web_password=$('web_password').value;if($('ap_password').value)o.ap_password=$('ap_password').value;
 S=await post('/api/set',o)||S;$('web_password').value='';$('ap_password').value=''};
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
  server.onNotFound([] {
    if (netApActive()) {                         // Captive Portal
      server.sendHeader("Location", "http://192.168.4.1/", true);
      server.send(302, "text/plain", "");
    } else server.send(404, "text/plain", "not found");
  });
  server.begin();
}

void webLoop() { server.handleClient(); }
