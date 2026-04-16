// AG Tech - Environmental Monitor Dashboard
// D1 Mini (ESP8266)
// DHT11: D5 (GPIO14) - Temperature & Humidity
// RGB LED: Red=D6 (GPIO12), Green=D7 (GPIO13), Blue=D1 (GPIO5)
// LED = status indicator (green=ok, yellow=warn, red=danger)
// Common cathode (long leg to GND)

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>

#include "config.h"
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

#define RED_PIN   12  // D6
#define GREEN_PIN 13  // D7
#define BLUE_PIN   5  // D1
#define DHTPIN    14  // D5
#define DHTTYPE DHT11

// Thresholds
#define TEMP_WARN_HIGH  30.0
#define TEMP_DANGER_HIGH 35.0
#define TEMP_WARN_LOW   10.0
#define TEMP_DANGER_LOW  5.0
#define HUM_WARN_HIGH   70.0
#define HUM_DANGER_HIGH  80.0
#define HUM_WARN_LOW    30.0
#define HUM_DANGER_LOW   20.0

DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);

#define MAX_READINGS 120
float tempHistory[MAX_READINGS];
float humHistory[MAX_READINGS];
int readIndex = 0;
int totalReadings = 0;
unsigned long lastRead = 0;
float currentTemp = 0, currentHum = 0;
String currentStatus = "ok";

void setLED(int r, int g, int b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

// Returns: "ok", "warn", or "danger"
String evaluateStatus(float temp, float hum) {
  if (temp >= TEMP_DANGER_HIGH || temp <= TEMP_DANGER_LOW ||
      hum >= HUM_DANGER_HIGH || hum <= HUM_DANGER_LOW) {
    return "danger";
  }
  if (temp >= TEMP_WARN_HIGH || temp <= TEMP_WARN_LOW ||
      hum >= HUM_WARN_HIGH || hum <= HUM_WARN_LOW) {
    return "warn";
  }
  return "ok";
}

void updateLED(String status) {
  if (status == "danger") {
    setLED(1023, 0, 0);       // Red
  } else if (status == "warn") {
    setLED(1023, 512, 0);     // Yellow/Orange
  } else {
    setLED(0, 1023, 0);       // Green
  }
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AG Tech - Plant Monitor</title>
  <style>
    *{margin:0;padding:0;box-sizing:border-box}
    body{font-family:-apple-system,sans-serif;background:#0f172a;color:#e2e8f0;padding:16px;max-width:600px;margin:0 auto}
    h1{text-align:center;margin-bottom:4px;color:#38bdf8;font-size:1.4em}
    .subtitle{text-align:center;color:#64748b;font-size:.8em;margin-bottom:16px}
    .status-bar{border-radius:12px;padding:14px;margin-bottom:12px;text-align:center;font-weight:bold;font-size:1.1em;transition:background .5s}
    .status-ok{background:#166534;color:#4ade80}
    .status-warn{background:#854d0e;color:#fbbf24}
    .status-danger{background:#991b1b;color:#fca5a5;animation:pulse 1s infinite}
    @keyframes pulse{0%,100%{opacity:1}50%{opacity:.7}}
    .section{background:#1e293b;border-radius:12px;padding:16px;margin-bottom:12px}
    .section-title{font-size:.8em;color:#94a3b8;margin-bottom:10px;text-transform:uppercase;letter-spacing:1px}
    .cards{display:flex;gap:12px;margin-bottom:12px}
    .card{flex:1;background:#0f172a;border-radius:8px;padding:14px;text-align:center;position:relative}
    .card .value{font-size:2em;font-weight:bold}
    .card .label{font-size:.75em;color:#94a3b8;margin-top:2px}
    .card .range{font-size:.65em;color:#475569;margin-top:4px}
    .temp .value{color:#f97316}
    .hum .value{color:#22d3ee}
    .chart-box{background:#0f172a;border-radius:8px;padding:12px;margin-top:10px}
    .chart-label{font-size:.75em;color:#64748b;margin-bottom:4px}
    canvas{width:100%!important;height:130px!important}
    .thresholds{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:10px;font-size:.75em}
    .th-item{background:#0f172a;border-radius:6px;padding:8px;display:flex;justify-content:space-between}
    .th-label{color:#94a3b8}
    .th-ok{color:#4ade80}
    .th-warn{color:#fbbf24}
    .th-danger{color:#f87171}
    .log{background:#0f172a;border-radius:8px;padding:10px;max-height:150px;overflow-y:auto;font-family:monospace;font-size:.7em;margin-top:10px}
    .log div{padding:2px 0;border-bottom:1px solid #1e293b}
    .log .log-ok{color:#4ade80}
    .log .log-warn{color:#fbbf24}
    .log .log-danger{color:#f87171}
    .footer{text-align:center;font-size:.7em;color:#334155;margin-top:12px}
  </style>
</head>
<body>
  <h1>AG Tech Plant Monitor</h1>
  <div class="subtitle">Wemos D1 Mini + DHT11</div>
  <div class="status-bar status-ok" id="sb">Sensors OK</div>
  <div class="section">
    <div class="section-title">Live Readings</div>
    <div class="cards">
      <div class="card temp"><div class="value" id="t">--</div><div class="label">Temperature C</div><div class="range">Safe: 10-30 C</div></div>
      <div class="card hum"><div class="value" id="h">--</div><div class="label">Humidity %</div><div class="range">Safe: 30-70%</div></div>
    </div>
  </div>
  <div class="section">
    <div class="section-title">History (last 4 min)</div>
    <div class="chart-box"><div class="chart-label">Temperature (C)</div><canvas id="tc"></canvas></div>
    <div class="chart-box" style="margin-top:8px"><div class="chart-label">Humidity (%)</div><canvas id="hc"></canvas></div>
  </div>
  <div class="section">
    <div class="section-title">Thresholds</div>
    <div class="thresholds">
      <div class="th-item"><span class="th-label">Temp OK</span><span class="th-ok">10-30 C</span></div>
      <div class="th-item"><span class="th-label">Temp Warn</span><span class="th-warn">5-10 / 30-35 C</span></div>
      <div class="th-item"><span class="th-label">Hum OK</span><span class="th-ok">30-70%</span></div>
      <div class="th-item"><span class="th-label">Hum Warn</span><span class="th-warn">20-30 / 70-80%</span></div>
    </div>
  </div>
  <div class="section">
    <div class="section-title">Event Log</div>
    <div class="log" id="log"></div>
  </div>
  <div class="footer">LED: Green=OK | Yellow=Warning | Red=Danger</div>
<script>
function dc(cv,data,color,mn,mx,wl,wh,dl,dh){
  let ctx=cv.getContext('2d');let w=cv.width=cv.offsetWidth*2;let h=cv.height=cv.offsetHeight*2;
  ctx.clearRect(0,0,w,h);
  // danger zones
  if(dl!==undefined){
    ctx.fillStyle='rgba(239,68,68,0.08)';
    let y1=0,y2=h-((dl-mn)/(mx-mn))*h;
    ctx.fillRect(0,0,w,h-((dh-mn)/(mx-mn))*h);  // top danger
    ctx.fillRect(0,h-((dl-mn)/(mx-mn))*h,w,h);   // bottom danger
    ctx.fillStyle='rgba(251,191,36,0.06)';
    let wy1=h-((wh-mn)/(mx-mn))*h;
    let wy2=h-((dh-mn)/(mx-mn))*h;
    ctx.fillRect(0,wy2,w,wy1-wy2);  // top warn
    let wy3=h-((dl-mn)/(mx-mn))*h;
    let wy4=h-((wl-mn)/(mx-mn))*h;
    ctx.fillRect(0,wy3,w,wy4-wy3);  // bottom warn
  }
  ctx.strokeStyle='#1e293b';ctx.lineWidth=1;
  for(let i=0;i<=4;i++){let y=h*i/4;ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke();
  ctx.fillStyle='#475569';ctx.font='18px sans-serif';ctx.fillText((mx-(mx-mn)*i/4).toFixed(1),4,y+14);}
  if(data.length<2)return;ctx.strokeStyle=color;ctx.lineWidth=2.5;ctx.beginPath();
  for(let i=0;i<data.length;i++){let x=i/(120-1)*w;let y=h-((data[i]-mn)/(mx-mn))*h;
  if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();
}
let lastStatus='ok';
function fs(){
  fetch('/data').then(r=>r.json()).then(d=>{
    document.getElementById('t').textContent=d.temp.toFixed(1);
    document.getElementById('h').textContent=d.hum.toFixed(1);
    let sb=document.getElementById('sb');
    sb.className='status-bar status-'+d.status;
    if(d.status==='ok') sb.textContent='All Sensors OK';
    else if(d.status==='warn') sb.textContent='Warning - Check Conditions';
    else sb.textContent='DANGER - Immediate Attention Needed';
    if(d.tempHistory.length>1){
      let a=d.tempHistory,b=d.humHistory;
      dc(document.getElementById('tc'),a,'#f97316',
        Math.min(0,Math.floor(Math.min(...a)-5)),Math.max(40,Math.ceil(Math.max(...a)+5)),
        10,30,5,35);
      dc(document.getElementById('hc'),b,'#22d3ee',
        Math.min(0,Math.floor(Math.min(...b)-10)),Math.max(100,Math.ceil(Math.max(...b)+10)),
        30,70,20,80);
    }
    let log=document.getElementById('log');
    let now=new Date().toLocaleTimeString();
    let cls='log-'+d.status;
    let entry=document.createElement('div');
    entry.className=cls;
    let msg=now+'  T:'+d.temp.toFixed(1)+'C  H:'+d.hum.toFixed(1)+'%';
    if(d.status!=='ok') msg+='  ['+d.status.toUpperCase()+']';
    entry.textContent=msg;
    log.insertBefore(entry,log.firstChild);
    if(log.children.length>50)log.removeChild(log.lastChild);
    lastStatus=d.status;
  }).catch(()=>{});
}
fs();setInterval(fs,2000);
</script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{\"temp\":" + String(currentTemp, 1) +
                ",\"hum\":" + String(currentHum, 1) +
                ",\"status\":\"" + currentStatus + "\"" +
                ",\"tempHistory\":[";
  int count = min(totalReadings, MAX_READINGS);
  int start = (totalReadings >= MAX_READINGS) ? readIndex : 0;
  for (int i = 0; i < count; i++) {
    int idx = (start + i) % MAX_READINGS;
    if (i > 0) json += ",";
    json += String(tempHistory[idx], 1);
  }
  json += "],\"humHistory\":[";
  for (int i = 0; i < count; i++) {
    int idx = (start + i) % MAX_READINGS;
    if (i > 0) json += ",";
    json += String(humHistory[idx], 1);
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- AG Tech Plant Monitor ---");

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  setLED(0, 0, 0);

  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  // Blink blue while connecting
  while (WiFi.status() != WL_CONNECTED) {
    setLED(0, 0, 512);
    delay(250);
    setLED(0, 0, 0);
    delay(250);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  // Flash green on connect
  setLED(0, 1023, 0);
  delay(500);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Dashboard: http://" + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();

  if (millis() - lastRead >= 2000) {
    lastRead = millis();
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      currentTemp = t;
      currentHum = h;
      tempHistory[readIndex] = t;
      humHistory[readIndex] = h;
      readIndex = (readIndex + 1) % MAX_READINGS;
      totalReadings++;

      currentStatus = evaluateStatus(t, h);
      updateLED(currentStatus);

      Serial.printf("Temp: %.1fC  Hum: %.1f%%  Status: %s\n", t, h, currentStatus.c_str());
    } else {
      // Sensor error - flash red
      setLED(1023, 0, 0);
      Serial.println("DHT11 read error!");
    }
  }
}
