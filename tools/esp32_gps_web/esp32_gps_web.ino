// ESP32 WiFi GPS monitor for the salvaged Hubsan GPS (u-blox M8030).
// Open http://gps.local (or the IP printed on USB serial) from any device on the same WiFi.
//
// Wiring (classic ESP32 DevKit):
//   GPS red 5V -> VIN, white GND -> GND, black TX -> RX2 (GPIO16), blue RX -> TX2 (GPIO17)
// Power: any USB wall charger. ESP32 WiFi is 2.4 GHz only.

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ---- edit these ----
const char *WIFI_SSID = "Diggydog_2GEXT";
const char *WIFI_PASS = "BallsNutz";
const char *HOSTNAME  = "gps";          // http://gps.local
const int  GPS_RX_PIN = 16;            // ESP32 receives here (GPS TX, black)
const int  GPS_TX_PIN = 17;            // ESP32 sends here (GPS RX, blue)
const long GPS_BAUD   = 9600;
// --------------------

WebServer server(80);

struct Sat { int prn, elev, az, snr; };
const int MAX_SATS = 40;

struct Constellation {
  Sat sats[MAX_SATS];
  int count = 0;          // satellites stored
  int inView = 0;         // satellites in view (from GSV header)
  Sat building[MAX_SATS];
  int buildCount = 0;
};
Constellation gpCons, glCons, gaCons, bdCons;

String utcTime = "", utcDate = "", rmcStatus = "V";
int fixQuality = 0, satsUsed = 0;
float hdop = 99.99, altitude = 0;
double lat = 0, lon = 0;
bool havePos = false;
uint32_t sentences = 0, badChecksum = 0;
uint32_t firstFixMs = 0;
uint32_t lastByteMs = 0;

const int RAW_LINES = 12;
String rawLines[RAW_LINES];
int rawHead = 0;

// ---------- NMEA helpers ----------
bool checksumOk(const char *s) {
  // s starts with '$'
  const char *star = strchr(s, '*');
  if (!star || strlen(star) < 3) return false;
  uint8_t cs = 0;
  for (const char *p = s + 1; p < star; p++) cs ^= (uint8_t)*p;
  char hex[3] = { star[1], star[2], 0 };
  return cs == (uint8_t)strtol(hex, nullptr, 16);
}

// Split in place on commas, keeping empty fields. Stops at '*'.
int splitFields(char *s, char **f, int maxF) {
  char *star = strchr(s, '*');
  if (star) *star = 0;
  int n = 0;
  f[n++] = s;
  for (char *p = s; *p && n < maxF; p++) {
    if (*p == ',') { *p = 0; f[n++] = p + 1; }
  }
  return n;
}

double nmeaToDeg(const char *v, const char *hemi) {
  if (!v || !*v) return 0;
  double raw = atof(v);
  int deg = (int)(raw / 100);
  double d = deg + (raw - deg * 100) / 60.0;
  if (hemi && (*hemi == 'S' || *hemi == 'W')) d = -d;
  return d;
}

Constellation *consFor(const char *talker) {
  if (!strncmp(talker, "GP", 2)) return &gpCons;
  if (!strncmp(talker, "GL", 2)) return &glCons;
  if (!strncmp(talker, "GA", 2)) return &gaCons;
  if (!strncmp(talker, "GB", 2) || !strncmp(talker, "BD", 2)) return &bdCons;
  return nullptr;
}

void handleSentence(char *line) {
  sentences++;
  rawLines[rawHead] = String(line);
  rawHead = (rawHead + 1) % RAW_LINES;

  if (!checksumOk(line)) { badChecksum++; return; }

  char *f[40];
  int n = splitFields(line, f, 40);
  const char *id = f[0] + 1;           // e.g. "GNGGA"
  if (strlen(id) < 5) return;
  const char *type = id + 2;           // "GGA"

  if (!strcmp(type, "GGA") && n >= 10) {
    if (*f[1]) utcTime = String(f[1]);
    fixQuality = atoi(f[6]);
    satsUsed = atoi(f[7]);
    hdop = *f[8] ? atof(f[8]) : 99.99;
    altitude = atof(f[9]);
    if (fixQuality > 0 && *f[2] && *f[4]) {
      lat = nmeaToDeg(f[2], f[3]);
      lon = nmeaToDeg(f[4], f[5]);
      havePos = true;
      if (!firstFixMs) firstFixMs = millis();
    }
  } else if (!strcmp(type, "RMC") && n >= 10) {
    if (*f[1]) utcTime = String(f[1]);
    rmcStatus = *f[2] ? String(f[2]) : "V";
    if (*f[9]) utcDate = String(f[9]);
  } else if (!strcmp(type, "GSV") && n >= 4) {
    Constellation *c = consFor(id);
    if (!c) return;
    int msgNum = atoi(f[2]);
    int totalMsgs = atoi(f[1]);
    c->inView = atoi(f[3]);
    if (msgNum == 1) c->buildCount = 0;
    for (int i = 4; i + 3 < n; i += 4) {
      if (!*f[i]) continue;
      if (c->buildCount >= MAX_SATS) break;
      Sat s;
      s.prn  = atoi(f[i]);
      s.elev = *f[i + 1] ? atoi(f[i + 1]) : -1;
      s.az   = *f[i + 2] ? atoi(f[i + 2]) : -1;
      s.snr  = *f[i + 3] ? atoi(f[i + 3]) : 0;
      c->building[c->buildCount++] = s;
    }
    if (msgNum == totalMsgs) {
      memcpy(c->sats, c->building, sizeof(Sat) * c->buildCount);
      c->count = c->buildCount;
    }
  }
}

void readGps() {
  static char buf[128];
  static uint8_t len = 0;
  while (Serial2.available()) {
    char ch = Serial2.read();
    lastByteMs = millis();
    Serial.write(ch);                    // still mirror to USB for debugging
    if (ch == '$') len = 0;
    if (ch == '\n' || ch == '\r') {
      if (len > 6 && buf[0] == '$') { buf[len] = 0; handleSentence(buf); }
      len = 0;
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = ch;
    }
  }
}

// ---------- web ----------
void addCons(String &j, const char *name, Constellation &c) {
  j += "\""; j += name; j += "\":{\"inView\":"; j += c.inView; j += ",\"sats\":[";
  for (int i = 0; i < c.count; i++) {
    if (i) j += ",";
    j += "["; j += c.sats[i].prn; j += ","; j += c.sats[i].elev; j += ",";
    j += c.sats[i].az; j += ","; j += c.sats[i].snr; j += "]";
  }
  j += "]}";
}

void handleData() {
  String j = "{";
  j += "\"time\":\"" + utcTime + "\",\"date\":\"" + utcDate + "\",";
  j += "\"status\":\"" + rmcStatus + "\",";
  j += "\"fix\":" + String(fixQuality) + ",\"used\":" + String(satsUsed) + ",";
  j += "\"hdop\":" + String(hdop, 2) + ",\"alt\":" + String(altitude, 1) + ",";
  j += "\"havePos\":" + String(havePos ? "true" : "false") + ",";
  j += "\"lat\":" + String(lat, 6) + ",\"lon\":" + String(lon, 6) + ",";
  j += "\"uptime\":" + String(millis() / 1000) + ",";
  j += "\"ttff\":" + String(firstFixMs ? firstFixMs / 1000 : 0) + ",";
  j += "\"sinceByte\":" + String((millis() - lastByteMs) / 1000) + ",";
  j += "\"sentences\":" + String(sentences) + ",\"bad\":" + String(badChecksum) + ",";
  j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  addCons(j, "GPS", gpCons); j += ",";
  addCons(j, "GLONASS", glCons); j += ",";
  addCons(j, "Galileo", gaCons); j += ",";
  addCons(j, "BeiDou", bdCons); j += ",";
  j += "\"raw\":[";
  for (int i = 0; i < RAW_LINES; i++) {
    int k = (rawHead + i) % RAW_LINES;
    if (i) j += ",";
    j += "\""; j += rawLines[k]; j += "\"";
  }
  j += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GPS Monitor</title>
<style>
:root{--bg:#0f1115;--card:#181b22;--text:#e6e8ee;--dim:#8b92a5;--ok:#3ecf8e;--warn:#f5a524;--bad:#f25f5c;--bar:#4c8dff}
@media (prefers-color-scheme: light){:root{--bg:#f4f5f8;--card:#fff;--text:#1b1e26;--dim:#5d6475}}
*{box-sizing:border-box}body{margin:0;padding:16px;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,sans-serif}
h1{font-size:20px;margin:0 0 12px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px;margin-bottom:12px}
.card{background:var(--card);border-radius:10px;padding:12px}.lbl{color:var(--dim);font-size:12px;text-transform:uppercase;letter-spacing:.04em}
.val{font-size:22px;font-weight:600;margin-top:2px;font-variant-numeric:tabular-nums}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}
table{width:100%;border-collapse:collapse;font-variant-numeric:tabular-nums}td,th{padding:4px 6px;text-align:left;font-size:13px}
th{color:var(--dim);font-weight:500}.barw{width:100%;background:rgba(127,127,127,.18);border-radius:4px;height:10px}
.bar{height:10px;border-radius:4px;background:var(--bar)}pre{white-space:pre-wrap;word-break:break-all;font-size:12px;margin:0;color:var(--dim)}
a{color:var(--bar)}
</style></head><body>
<h1>GPS Monitor</h1>
<div class="grid">
 <div class="card"><div class="lbl">Fix</div><div class="val" id="fix">-</div></div>
 <div class="card"><div class="lbl">Sats used / in view</div><div class="val" id="sats">-</div></div>
 <div class="card"><div class="lbl">HDOP</div><div class="val" id="hdop">-</div></div>
 <div class="card"><div class="lbl">UTC time</div><div class="val" id="time">-</div></div>
 <div class="card"><div class="lbl">Uptime / first fix</div><div class="val" id="up">-</div></div>
 <div class="card"><div class="lbl">GPS data</div><div class="val" id="link">-</div></div>
</div>
<div class="card" style="margin-bottom:12px"><div class="lbl">Position</div><div class="val" id="pos" style="font-size:17px">no fix yet</div></div>
<div class="card" style="margin-bottom:12px"><div class="lbl">Satellites (signal in dB-Hz, 0 = seen but not tracked)</div>
<table><thead><tr><th>System</th><th>PRN</th><th>Elev</th><th>Az</th><th>SNR</th><th style="width:40%"></th></tr></thead><tbody id="sl"></tbody></table></div>
<div class="card"><div class="lbl">Raw NMEA (latest)</div><pre id="raw"></pre></div>
<script>
const $=id=>document.getElementById(id);
function fmtT(t){return t&&t.length>=6?t.slice(0,2)+":"+t.slice(2,4)+":"+t.slice(4,6):"-"}
function fmtS(s){if(!s)return"-";const m=Math.floor(s/60),x=s%60;return m?m+"m "+x+"s":x+"s"}
async function tick(){
 try{
  const d=await (await fetch("/data",{cache:"no-store"})).json();
  const fixTxt=d.fix==0?"No fix":(d.fix==1?"GPS fix":(d.fix==2?"DGPS fix":"Fix "+d.fix));
  $("fix").textContent=fixTxt;$("fix").className="val "+(d.fix>0?"ok":"bad");
  let view=0,rows="";
  for(const k of ["GPS","GLONASS","Galileo","BeiDou"]){
   view+=d[k].inView;
   for(const s of d[k].sats){const p=Math.min(100,s[3]*2);
    rows+=`<tr><td>${k}</td><td>${s[0]}</td><td>${s[1]<0?"-":s[1]+"°"}</td><td>${s[2]<0?"-":s[2]+"°"}</td><td>${s[3]||"-"}</td><td><div class="barw"><div class="bar" style="width:${p}%"></div></div></td></tr>`}}
  $("sl").innerHTML=rows||'<tr><td colspan="6" style="color:var(--dim)">No satellites in view yet</td></tr>';
  $("sats").textContent=d.used+" / "+view;$("sats").className="val "+(d.used>=8?"ok":(d.used>=4?"warn":"bad"));
  $("hdop").textContent=d.hdop>=99?"-":d.hdop.toFixed(2);
  $("time").textContent=fmtT(d.time);
  $("up").textContent=fmtS(d.uptime)+" / "+(d.ttff?fmtS(d.ttff):"-");
  const alive=d.sinceByte<3;$("link").textContent=alive?"OK":"silent "+d.sinceByte+"s";$("link").className="val "+(alive?"ok":"bad");
  if(d.havePos){$("pos").innerHTML=`${d.lat.toFixed(6)}, ${d.lon.toFixed(6)} &nbsp; alt ${d.alt} m &nbsp; <a href="https://maps.google.com/?q=${d.lat},${d.lon}" target="_blank">map</a>`}
  $("raw").textContent=d.raw.filter(x=>x).join("\n")+`\n\nsentences ${d.sentences}, bad checksum ${d.bad}, WiFi RSSI ${d.rssi} dBm`;
 }catch(e){$("link").textContent="ESP32 offline";$("link").className="val bad"}
}
tick();setInterval(tick,2000);
</script></body></html>
)HTML";

void handleRoot() { server.send_P(200, "text/html", PAGE); }

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("\nConnecting to %s", WIFI_SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) { delay(300); Serial.print("."); }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi OK. Open http://%s.local or http://%s\n", HOSTNAME, WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi failed (check name/password, 2.4 GHz). Will keep retrying.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  connectWifi();
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  readGps();
  server.handleClient();
  static uint32_t lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 10000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) { WiFi.disconnect(); WiFi.begin(WIFI_SSID, WIFI_PASS); }
  }
}
