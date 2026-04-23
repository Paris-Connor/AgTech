// AG Tech - Environmental Monitor Dashboard
// D1 Mini (ESP8266)
// DHT11: D5 (GPIO14) - Temperature & Humidity
// GY-30 (BH1750): D1 (SCL), D2 (SDA) - Light Intensity
// Soil Moisture: A0 - Soil Moisture %
// RGB LED: Red=D6 (GPIO12), Green=D7 (GPIO13), Blue=D8 (GPIO15)
// Common cathode (long leg to GND)

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>

#include "config.h"
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

#define RED_PIN   12
#define GREEN_PIN 13
#define BLUE_PIN  15
#define DHTPIN    14
#define DHTTYPE DHT11
#define SOIL_PIN  A0
#define SOIL_DRY  1023
#define SOIL_WET  300

// Default thresholds (Fittonia) for LED
#define TEMP_WARN_HIGH   78.0
#define TEMP_DANGER_HIGH 85.0
#define TEMP_WARN_LOW    60.0
#define TEMP_DANGER_LOW  55.0
#define HUM_WARN_HIGH    65.0
#define HUM_DANGER_HIGH  75.0
#define HUM_WARN_LOW     35.0
#define HUM_DANGER_LOW   25.0
#define LUX_WARN_LOW     200.0
#define LUX_DANGER_LOW   100.0
#define LUX_WARN_HIGH   1200.0
#define LUX_DANGER_HIGH 2000.0
#define SOIL_WARN_LOW    40.0
#define SOIL_DANGER_LOW  25.0
#define SOIL_WARN_HIGH   85.0
#define SOIL_DANGER_HIGH 95.0

DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
ESP8266WebServer server(80);

#define MAX_READINGS 120
float tempHistory[MAX_READINGS], humHistory[MAX_READINGS];
float luxHistory[MAX_READINGS], soilHistory[MAX_READINGS];
int readIndex=0, totalReadings=0;
unsigned long lastRead=0, lastThingSpeak=0;
float currentTemp=0,currentHum=0,currentLux=0,currentSoil=0;
String currentStatus="ok";
bool lightSensorOk=false, soilSensorOk=false;

void setLED(int r,int g,int b){analogWrite(RED_PIN,r);analogWrite(GREEN_PIN,g);analogWrite(BLUE_PIN,b);}

float readSoilPercent(){
  int raw=analogRead(SOIL_PIN);
  if(raw>=1020)return -1;
  float p=(float)(SOIL_DRY-raw)/(SOIL_DRY-SOIL_WET)*100.0;
  return p<0?0:(p>100?100:p);
}

String evaluateStatus(float t,float h,float l,float s){
  if(t>=TEMP_DANGER_HIGH||t<=TEMP_DANGER_LOW||h>=HUM_DANGER_HIGH||h<=HUM_DANGER_LOW)return"danger";
  if(lightSensorOk&&(l<=LUX_DANGER_LOW||l>=LUX_DANGER_HIGH))return"danger";
  if(soilSensorOk&&(s<=SOIL_DANGER_LOW||s>=SOIL_DANGER_HIGH))return"danger";
  if(t>=TEMP_WARN_HIGH||t<=TEMP_WARN_LOW||h>=HUM_WARN_HIGH||h<=HUM_WARN_LOW)return"warn";
  if(lightSensorOk&&(l<=LUX_WARN_LOW||l>=LUX_WARN_HIGH))return"warn";
  if(soilSensorOk&&(s<=SOIL_WARN_LOW||s>=SOIL_WARN_HIGH))return"warn";
  return"ok";
}

void updateLED(String s){
  if(s=="danger")setLED(1023,0,0);
  else if(s=="warn")setLED(1023,512,0);
  else setLED(0,1023,0);
}

void sendToThingSpeak(){
  if(strlen(THINGSPEAK_API_KEY)<5)return;
  WiFiClient c;HTTPClient h;
  String u="http://api.thingspeak.com/update?api_key=";
  u+=THINGSPEAK_API_KEY;
  u+="&field1="+String(currentTemp,1)+"&field2="+String(currentHum,1);
  u+="&field3="+String(currentLux,1)+"&field4="+String(currentSoil,1);
  h.begin(c,u);h.setTimeout(5000);
  int code=h.GET();
  if(code>0)Serial.printf("ThingSpeak: OK (%s)\n",h.getString().c_str());
  else Serial.printf("ThingSpeak: FAIL\n");
  h.end();
}

void handleRoot(){
  String html=R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AG Tech</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,sans-serif;background:#f0f7f0;color:#1a3a1a;min-height:100vh}
.wrap{display:flex;max-width:1100px;margin:0 auto;padding:16px;gap:16px}
.main{flex:1;min-width:0}
.side{width:300px;flex-shrink:0}
h1{color:#2d6a2d;font-size:1.6em;margin-bottom:2px}
h1 span{color:#5a9a5a}
.sub{color:#6b8f6b;font-size:.8em;margin-bottom:14px}
.plant-bar{background:#fff;border-radius:12px;padding:12px 16px;margin-bottom:14px;display:flex;align-items:center;gap:10px;border:1px solid #c8e0c8;box-shadow:0 1px 3px rgba(0,0,0,.06)}
.plant-bar label{font-size:.85em;color:#3d6b3d;font-weight:600}
.plant-bar select{flex:1;padding:8px 12px;border-radius:8px;border:1px solid #b0d0b0;background:#f8fcf8;color:#1a3a1a;font-size:.9em}
.status-bar{border-radius:12px;padding:12px 16px;margin-bottom:14px;text-align:center;font-weight:700;font-size:1em;box-shadow:0 1px 3px rgba(0,0,0,.08)}
.status-ok{background:#dcfce7;color:#166534;border:1px solid #86efac}
.status-warn{background:#fef9c3;color:#854d0e;border:1px solid #fde047}
.status-danger{background:#fee2e2;color:#991b1b;border:1px solid #fca5a5;animation:pulse 1s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.7}}
.section{background:#fff;border-radius:12px;padding:16px;margin-bottom:14px;border:1px solid #c8e0c8;box-shadow:0 1px 3px rgba(0,0,0,.06)}
.section-title{font-size:.75em;color:#5a9a5a;margin-bottom:10px;text-transform:uppercase;letter-spacing:1.5px;font-weight:600}
.cards{display:flex;gap:10px;flex-wrap:wrap}
.card{flex:1;min-width:90px;background:#f0f7f0;border-radius:10px;padding:14px 10px;text-align:center;border:1px solid #d4e8d4;transition:border-color .3s}
.card .value{font-size:1.7em;font-weight:700}
.card .label{font-size:.7em;color:#6b8f6b;margin-top:2px}
.card .range{font-size:.6em;color:#8aaa8a;margin-top:4px}
.temp .value{color:#ea580c}
.hum .value{color:#0891b2}
.lux .value{color:#ca8a04}
.soil .value{color:#16a34a}
.na{opacity:.3}
.chart-box{background:#f8fcf8;border-radius:8px;padding:10px;margin-top:8px;border:1px solid #d4e8d4}
.chart-label{font-size:.7em;color:#6b8f6b;margin-bottom:4px}
canvas{width:100%!important;height:85px!important}
.log{background:#f8fcf8;border-radius:8px;padding:10px;max-height:160px;overflow-y:auto;font-family:monospace;font-size:.65em;border:1px solid #d4e8d4;margin-top:8px}
.log div{padding:3px 0;border-bottom:1px solid #e8f0e8}
.log .log-ok{color:#16a34a}
.log .log-warn{color:#ca8a04}
.log .log-danger{color:#dc2626}
.side-section{background:#fff;border-radius:12px;padding:16px;margin-bottom:14px;border:1px solid #c8e0c8;box-shadow:0 1px 3px rgba(0,0,0,.06)}
.plant-name{font-size:1.3em;font-weight:700;color:#2d6a2d;margin-bottom:4px}
.plant-aka{font-size:.75em;color:#6b8f6b;margin-bottom:12px;font-style:italic}
.plant-desc{font-size:.8em;color:#3d6b3d;line-height:1.5;margin-bottom:14px}
.care-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.care-item{background:#f0f7f0;border-radius:8px;padding:10px;border:1px solid #d4e8d4}
.care-item .ci-label{font-size:.65em;color:#6b8f6b;text-transform:uppercase;letter-spacing:1px;margin-bottom:4px}
.care-item .ci-value{font-size:.85em;color:#2d6a2d;font-weight:600}
.care-item .ci-range{font-size:.7em;color:#5a9a5a;margin-top:2px}
.th-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:8px;font-size:.75em}
.th-item{background:#f0f7f0;border-radius:6px;padding:6px 8px;display:flex;justify-content:space-between;border:1px solid #d4e8d4}
.th-label{color:#6b8f6b}
.th-ok{color:#16a34a}
.th-warn{color:#ca8a04}
.led-info{margin-top:14px;font-size:.75em;color:#6b8f6b;text-align:center}
.led-dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin:0 2px;vertical-align:middle}
</style>
</head><body>
<div class="wrap">
<div class="main">
<h1>AG <span>Tech</span></h1>
<div class="sub">Smart Plant Monitor</div>
<div class="plant-bar">
<label>Plant Profile:</label>
<select id="pp" onchange="setPlant()">
<option value="fittonia">Fittonia (Nerve Plant)</option>
<option value="pothos">Pothos</option>
<option value="snake">Snake Plant</option>
<option value="succulent">Succulent / Cactus</option>
<option value="basil">Basil</option>
<option value="tomato">Tomato</option>
</select>
</div>
<div class="status-bar status-ok" id="sb">All Sensors OK</div>
<div class="section">
<div class="section-title">Live Readings</div>
<div class="cards">
<div class="card temp"><div class="value" id="t">--</div><div class="label">Temp (F)</div><div class="range" id="tr"></div></div>
<div class="card hum"><div class="value" id="h">--</div><div class="label">Humidity</div><div class="range" id="hr"></div></div>
<div class="card lux"><div class="value" id="l">--</div><div class="label">Light</div><div class="range" id="lr"></div></div>
<div class="card soil"><div class="value" id="s">--</div><div class="label">Soil</div><div class="range" id="sr"></div></div>
</div>
</div>
<div class="section">
<div class="section-title">History</div>
<div class="chart-box"><div class="chart-label">Temperature (F)</div><canvas id="tc"></canvas></div>
<div class="chart-box" style="margin-top:6px"><div class="chart-label">Humidity (%)</div><canvas id="hc"></canvas></div>
<div class="chart-box" style="margin-top:6px"><div class="chart-label">Light (lux)</div><canvas id="lc"></canvas></div>
<div class="chart-box" style="margin-top:6px"><div class="chart-label">Soil Moisture (%)</div><canvas id="sc"></canvas></div>
</div>
<div class="section">
<div class="section-title">Event Log</div>
<div class="log" id="log"></div>
</div>
</div>
<div class="side">
<div class="side-section">
<div class="plant-name" id="pn">Fittonia</div>
<div class="plant-aka" id="pa">Nerve Plant</div>
<div class="plant-desc" id="pd">A tropical plant known for its striking veined leaves. Thrives in warm, humid environments with indirect light. Prefers consistently moist soil but not waterlogged. Great for terrariums.</div>
</div>
<div class="side-section">
<div class="section-title">Ideal Conditions</div>
<div class="care-grid">
<div class="care-item"><div class="ci-label">Temperature</div><div class="ci-value" id="ct"></div><div class="ci-range" id="ctr"></div></div>
<div class="care-item"><div class="ci-label">Humidity</div><div class="ci-value" id="ch"></div><div class="ci-range" id="chr"></div></div>
<div class="care-item"><div class="ci-label">Light</div><div class="ci-value" id="cl"></div><div class="ci-range" id="clr"></div></div>
<div class="care-item"><div class="ci-label">Soil Moisture</div><div class="ci-value" id="cs"></div><div class="ci-range" id="csr"></div></div>
</div>
</div>
<div class="side-section">
<div class="section-title">Thresholds</div>
<div class="th-grid" id="thd"></div>
</div>
<div class="led-info">
<div class="led-dot" style="background:#22c55e"></div> OK
<div class="led-dot" style="background:#eab308"></div> Warning
<div class="led-dot" style="background:#ef4444"></div> Danger
</div>
</div>
</div>
<script>
const P={
fittonia:{name:'Fittonia',aka:'Nerve Plant',desc:'A tropical plant known for its striking veined leaves. Thrives in warm, humid environments with indirect light. Prefers consistently moist soil but not waterlogged. Great for terrariums.',
  t:[55,65,75,85],h:[25,45,65,75],l:[100,250,1000,1500],s:[25,40,75,85],
  care:{t:'65-75 F',h:'45-65%',l:'Low-Medium',s:'Moist'}},
pothos:{name:'Pothos',aka:'Devil\'s Ivy',desc:'An easy-care trailing vine that tolerates a wide range of conditions. Excellent air purifier. Prefers moderate indirect light but adapts to low light. Let soil dry slightly between waterings.',
  t:[55,60,85,90],h:[30,40,60,70],l:[200,500,10000,15000],s:[20,30,60,75],
  care:{t:'60-85 F',h:'40-60%',l:'Low-Bright',s:'Moderate'}},
snake:{name:'Snake Plant',aka:'Sansevieria',desc:'One of the hardiest houseplants. Tolerates neglect, low light, and infrequent watering. Excellent air purifier that also releases oxygen at night. Perfect for beginners.',
  t:[55,60,85,95],h:[20,30,50,65],l:[100,300,10000,20000],s:[10,15,35,50],
  care:{t:'60-85 F',h:'30-50%',l:'Any',s:'Dry'}},
succulent:{name:'Succulent',aka:'Cactus / Succulent',desc:'Desert-adapted plants that store water in thick leaves. Need bright direct light and very infrequent watering. Plant in well-draining soil. Let dry completely between waterings.',
  t:[45,50,90,95],h:[15,25,50,65],l:[2000,5000,50000,80000],s:[5,10,25,40],
  care:{t:'50-90 F',h:'25-50%',l:'Bright/Direct',s:'Very Dry'}},
basil:{name:'Basil',aka:'Sweet Basil',desc:'A popular culinary herb that loves warmth and sunshine. Needs at least 6 hours of direct light daily. Keep soil consistently damp. Pinch flowers to encourage leaf growth.',
  t:[55,60,85,90],h:[30,40,60,75],l:[3000,5000,40000,60000],s:[35,45,70,85],
  care:{t:'60-85 F',h:'40-60%',l:'Full Sun',s:'Damp'}},
tomato:{name:'Tomato',aka:'Solanum lycopersicum',desc:'A warm-season fruiting plant that demands full sun and consistent moisture. Needs support as it grows. Feed regularly during fruiting. Sensitive to cold temperatures.',
  t:[55,62,82,90],h:[40,50,75,85],l:[5000,10000,50000,80000],s:[40,50,70,80],
  care:{t:'62-82 F',h:'50-75%',l:'Full Sun',s:'Even Moist'}}
};
let cp=P.fittonia;
function setPlant(){
  let k=document.getElementById('pp').value;cp=P[k];
  document.getElementById('pn').textContent=cp.name;
  document.getElementById('pa').textContent=cp.aka;
  document.getElementById('pd').textContent=cp.desc;
  document.getElementById('tr').textContent=cp.t[1]+'-'+cp.t[2]+'F';
  document.getElementById('hr').textContent=cp.h[1]+'-'+cp.h[2]+'%';
  document.getElementById('lr').textContent=cp.l[1]+'-'+cp.l[2];
  document.getElementById('sr').textContent=cp.s[1]+'-'+cp.s[2]+'%';
  document.getElementById('ct').textContent=cp.care.t;
  document.getElementById('ctr').textContent='Warn: <'+cp.t[1]+' / >'+cp.t[2];
  document.getElementById('ch').textContent=cp.care.h;
  document.getElementById('chr').textContent='Warn: <'+cp.h[1]+' / >'+cp.h[2];
  document.getElementById('cl').textContent=cp.care.l;
  document.getElementById('clr').textContent='Warn: <'+cp.l[1]+' / >'+cp.l[2];
  document.getElementById('cs').textContent=cp.care.s;
  document.getElementById('csr').textContent='Warn: <'+cp.s[1]+' / >'+cp.s[2];
  let th=document.getElementById('thd');
  th.innerHTML='<div class="th-item"><span class="th-label">Temp OK</span><span class="th-ok">'+cp.t[1]+'-'+cp.t[2]+' F</span></div>'+
  '<div class="th-item"><span class="th-label">Temp Warn</span><span class="th-warn">'+cp.t[0]+' / '+cp.t[3]+'</span></div>'+
  '<div class="th-item"><span class="th-label">Hum OK</span><span class="th-ok">'+cp.h[1]+'-'+cp.h[2]+'%</span></div>'+
  '<div class="th-item"><span class="th-label">Hum Warn</span><span class="th-warn">'+cp.h[0]+' / '+cp.h[3]+'</span></div>'+
  '<div class="th-item"><span class="th-label">Light OK</span><span class="th-ok">'+cp.l[1]+'-'+cp.l[2]+'</span></div>'+
  '<div class="th-item"><span class="th-label">Light Warn</span><span class="th-warn">'+cp.l[0]+' / '+cp.l[3]+'</span></div>'+
  '<div class="th-item"><span class="th-label">Soil OK</span><span class="th-ok">'+cp.s[1]+'-'+cp.s[2]+'%</span></div>'+
  '<div class="th-item"><span class="th-label">Soil Warn</span><span class="th-warn">'+cp.s[0]+' / '+cp.s[3]+'</span></div>';
}
function evalSt(d){
  let p=cp;
  if(d.temp<=p.t[0]||d.temp>=p.t[3])return'danger';
  if(d.hum<=p.h[0]||d.hum>=p.h[3])return'danger';
  if(d.soil>=0&&(d.soil<=p.s[0]||d.soil>=p.s[3]))return'danger';
  if(d.lux>=0&&(d.lux<=p.l[0]||d.lux>=p.l[3]))return'danger';
  if(d.temp<=p.t[1]||d.temp>=p.t[2])return'warn';
  if(d.hum<=p.h[1]||d.hum>=p.h[2])return'warn';
  if(d.soil>=0&&(d.soil<=p.s[1]||d.soil>=p.s[2]))return'warn';
  if(d.lux>=0&&(d.lux<=p.l[1]||d.lux>=p.l[2]))return'warn';
  return'ok';
}
function dc(cv,data,color,mn,mx){
  let ctx=cv.getContext('2d');let w=cv.width=cv.offsetWidth*2;let h=cv.height=cv.offsetHeight*2;
  ctx.clearRect(0,0,w,h);ctx.strokeStyle='#d4e8d4';ctx.lineWidth=1;
  for(let i=0;i<=4;i++){let y=h*i/4;ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke();
  ctx.fillStyle='#8aaa8a';ctx.font='18px sans-serif';let v=mx-(mx-mn)*i/4;
  ctx.fillText(v>=1000?(v/1000).toFixed(1)+'k':v.toFixed(1),4,y+14);}
  if(data.length<2)return;ctx.strokeStyle=color;ctx.lineWidth=2.5;ctx.beginPath();
  for(let i=0;i<data.length;i++){let x=i/(120-1)*w;let y=h-((data[i]-mn)/(mx-mn))*h;
  if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();
}
function fs(){
  fetch('/data').then(r=>r.json()).then(d=>{
    document.getElementById('t').textContent=d.temp.toFixed(1);
    document.getElementById('h').textContent=d.hum.toFixed(1);
    let le=document.getElementById('l');
    if(d.lux>=0){le.textContent=d.lux>=1000?(d.lux/1000).toFixed(1)+'k':d.lux.toFixed(0);le.parentElement.classList.remove('na');}
    else{le.textContent='N/A';le.parentElement.classList.add('na');}
    let se=document.getElementById('s');
    if(d.soil>=0){se.textContent=d.soil.toFixed(0)+'%';se.parentElement.classList.remove('na');}
    else{se.textContent='N/A';se.parentElement.classList.add('na');}
    let st=evalSt(d);
    let sb=document.getElementById('sb');sb.className='status-bar status-'+st;
    if(st==='ok')sb.textContent='All Sensors OK - Your plant is happy!';
    else if(st==='warn')sb.textContent='Warning - Check conditions';
    else sb.textContent='DANGER - Immediate attention needed!';
    if(d.tempHistory&&d.tempHistory.length>1){
      dc(document.getElementById('tc'),d.tempHistory,'#ea580c',
        Math.min(32,Math.floor(Math.min(...d.tempHistory)-5)),Math.max(100,Math.ceil(Math.max(...d.tempHistory)+5)));
      dc(document.getElementById('hc'),d.humHistory,'#0891b2',
        Math.min(0,Math.floor(Math.min(...d.humHistory)-10)),Math.max(100,Math.ceil(Math.max(...d.humHistory)+10)));
      if(d.luxHistory&&d.luxHistory.length>1&&d.luxHistory[0]>=0)
        dc(document.getElementById('lc'),d.luxHistory,'#ca8a04',
          Math.max(0,Math.floor(Math.min(...d.luxHistory)*.8)),Math.ceil(Math.max(...d.luxHistory)*1.2+1));
      if(d.soilHistory&&d.soilHistory.length>1&&d.soilHistory[0]>=0)
        dc(document.getElementById('sc'),d.soilHistory,'#16a34a',0,100);
    }
    let log=document.getElementById('log');let now=new Date().toLocaleTimeString();
    let entry=document.createElement('div');entry.className='log-'+st;
    let lx=d.lux>=0?(d.lux>=1000?(d.lux/1000).toFixed(1)+'k':d.lux.toFixed(0))+'lux':'--';
    let sl=d.soil>=0?d.soil.toFixed(0)+'%':'--';
    let msg=now+' T:'+d.temp.toFixed(1)+'F H:'+d.hum.toFixed(1)+'% L:'+lx+' S:'+sl;
    if(st!=='ok')msg+=' ['+st.toUpperCase()+']';
    entry.textContent=msg;log.insertBefore(entry,log.firstChild);
    if(log.children.length>50)log.removeChild(log.lastChild);
  }).catch(()=>{});
}
setPlant();fs();setInterval(fs,2000);
</script>
</body></html>
)rawliteral";
  server.send(200,"text/html",html);
}

void handleData(){
  String j="{\"temp\":"+String(currentTemp,1)+",\"hum\":"+String(currentHum,1)+
    ",\"lux\":"+String(currentLux,1)+",\"soil\":"+String(currentSoil,1)+
    ",\"status\":\""+currentStatus+"\",\"tempHistory\":[";
  int c=min(totalReadings,MAX_READINGS);
  int s=(totalReadings>=MAX_READINGS)?readIndex:0;
  for(int i=0;i<c;i++){int x=(s+i)%MAX_READINGS;if(i)j+=",";j+=String(tempHistory[x],1);}
  j+="],\"humHistory\":[";
  for(int i=0;i<c;i++){int x=(s+i)%MAX_READINGS;if(i)j+=",";j+=String(humHistory[x],1);}
  j+="],\"luxHistory\":[";
  for(int i=0;i<c;i++){int x=(s+i)%MAX_READINGS;if(i)j+=",";j+=String(luxHistory[x],1);}
  j+="],\"soilHistory\":[";
  for(int i=0;i<c;i++){int x=(s+i)%MAX_READINGS;if(i)j+=",";j+=String(soilHistory[x],1);}
  j+="]}";
  server.send(200,"application/json",j);
}

void setup(){
  Serial.begin(115200);
  Serial.println("\n--- AG Tech Plant Monitor ---");
  pinMode(RED_PIN,OUTPUT);pinMode(GREEN_PIN,OUTPUT);pinMode(BLUE_PIN,OUTPUT);
  setLED(0,0,0);
  dht.begin();
  Wire.begin(4,5);
  if(lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)){lightSensorOk=true;Serial.println("GY-30 found!");}
  else Serial.println("GY-30 not found");
  float st=readSoilPercent();
  if(st>=0){soilSensorOk=true;Serial.println("Soil sensor found!");}
  else Serial.println("Soil sensor not found");
  WiFi.begin(ssid,password);
  Serial.print("Connecting to WiFi");
  while(WiFi.status()!=WL_CONNECTED){setLED(0,0,512);delay(250);setLED(0,0,0);delay(250);Serial.print(".");}
  Serial.println("\nConnected! IP: "+WiFi.localIP().toString());
  setLED(0,1023,0);delay(500);
  server.on("/",handleRoot);server.on("/data",handleData);
  server.begin();
  Serial.println("Dashboard: http://"+WiFi.localIP().toString());
  Serial.printf("Free heap: %d bytes\n",ESP.getFreeHeap());
}

void loop(){
  server.handleClient();
  if(millis()-lastRead>=2000){
    lastRead=millis();
    float h=dht.readHumidity();float t=dht.readTemperature(true);
    float lux=-1;float soil=-1;
    if(lightSensorOk){lux=lightMeter.readLightLevel();if(lux<0)lux=0;}
    soil=readSoilPercent();soilSensorOk=(soil>=0);
    if(!isnan(h)&&!isnan(t)){
      currentTemp=t;currentHum=h;currentLux=lux;currentSoil=soil;
      tempHistory[readIndex]=t;humHistory[readIndex]=h;
      luxHistory[readIndex]=lux;soilHistory[readIndex]=soil;
      readIndex=(readIndex+1)%MAX_READINGS;totalReadings++;
      currentStatus=evaluateStatus(t,h,lux,soil);updateLED(currentStatus);
      Serial.printf("T:%.1fF H:%.1f%% L:%.0flux S:%.0f%% [%s]\n",t,h,lux,soil,currentStatus.c_str());
    }else{setLED(1023,0,0);Serial.println("DHT11 error!");}
  }
  if(millis()-lastThingSpeak>=20000){lastThingSpeak=millis();sendToThingSpeak();}
}
