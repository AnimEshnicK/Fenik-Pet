#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include "sprites.h"

#define EPD_CS 10
#define EPD_DC 9
#define EPD_RST 8
#define EPD_BUSY 7
#define EPD_MOSI 11
#define EPD_SCK 12

GxEPD2_BW<GxEPD2_213_GDEY0213B74, GxEPD2_213_GDEY0213B74::HEIGHT> display(
  GxEPD2_213_GDEY0213B74(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
WebServer server(80);
String mood="neutral";
int energy=80, fullness=70;
uint32_t lastStats=0;
uint32_t lastAction=0, lastRefresh=0;
bool redraw=true;
bool otaAllowed=false, otaSuccess=false;
const char* apName="Abrams-Pet";
const char* apPassword="AbramsPet2026";

void drawFace() {
  const uint8_t* sprite = sprite_neutral;
  if (mood=="strong") sprite=sprite_proud;
  else if (mood=="calm") sprite=sprite_calm;
  else if (mood=="annoyed") sprite=sprite_annoyed;
  else if (mood=="shy") sprite=sprite_shy;
  else if (mood=="proud") sprite=sprite_proud;
  else if (mood=="happy") sprite=sprite_happy;
  else if (mood=="angry") sprite=sprite_angry;
  else if (mood=="surprised") sprite=sprite_surprised;
  else if (mood=="sleep") sprite=sprite_sleep;
  else if (mood=="thinking") sprite=sprite_thinking;
  else if (mood=="tired") sprite=sprite_tired;
  else if (mood=="sad") sprite=sprite_sad;
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawBitmap(0, 0, sprite, 160, 122, GxEPD_BLACK);
    display.drawFastVLine(160, 0, 122, GxEPD_BLACK);
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(1);
    display.setCursor(166, 11); display.print("ABRAMS");
    display.drawFastHLine(165, 16, 80, GxEPD_BLACK);
    uint32_t minutes=millis()/60000UL;
    char clockText[8];
    snprintf(clockText,sizeof(clockText),"%02lu:%02lu",(unsigned long)(minutes/60)%100,(unsigned long)(minutes%60));
    display.setCursor(166, 29); display.print("UP "); display.print(clockText);
    display.setCursor(166, 45); display.print("MOOD");
    display.setCursor(166, 56);
    String label=mood;
    if(label.length()>12)label=label.substring(0,12);
    display.print(label);
    display.setCursor(166, 72); display.print("ENERGY");
    display.drawRect(166, 76, 77, 8, GxEPD_BLACK);
    display.fillRect(168, 78, (energy*73)/100, 4, GxEPD_BLACK);
    display.setCursor(166, 96); display.print("FOOD");
    display.drawRect(166, 100, 77, 8, GxEPD_BLACK);
    display.fillRect(168, 102, (fullness*73)/100, 4, GxEPD_BLACK);
    display.setCursor(166, 118); display.print("AP ONLINE");
  } while(display.nextPage());
  redraw=false;
  lastRefresh=millis();
}

const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Abrams Pet</title><style>:root{color-scheme:dark}*{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at 50% -20%,#293f6b,#0c101b 65%);font:16px system-ui,-apple-system,sans-serif;color:#eaf1ff}.wrap{max-width:510px;margin:auto;padding:25px 18px 50px}.tag{font-size:12px;letter-spacing:3px;color:#8caddf}.hero{border:1px solid #45658e;border-radius:26px;padding:24px;background:linear-gradient(140deg,#233b64,#151d30);box-shadow:0 14px 45px #0006;margin:16px 0 20px;text-align:center}.avatar{font-size:72px;filter:drop-shadow(0 5px 14px #60aaff55)}h1{margin:4px 0;font-size:29px}.sub{color:#a9c0e1;font-size:14px}.pill{display:inline-block;border:1px solid #5d7eab;border-radius:30px;padding:7px 13px;margin-top:14px;font-size:13px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:11px}button,.btn{display:block;width:100%;border:1px solid #5276a9;border-radius:15px;padding:16px 9px;background:#21365a;color:#f0f6ff;font:600 16px system-ui;cursor:pointer}button:active{background:#3c6194}.wide{grid-column:span 2}.section{margin:25px 0 13px;font-size:13px;color:#a6c3e7;letter-spacing:2px;text-transform:uppercase}.panel{background:#141e30;border:1px solid #344c70;border-radius:20px;padding:18px;margin-top:16px}.panel a{color:#a8d4ff}#status{font-size:13px;color:#8ce0c0;margin-top:13px}small{color:#9db3d4}</style></head><body><div class="wrap"><div class="tag">DEADLOCK • VIRTUAL COMPANION</div><div class="hero"><div class="avatar">😈</div><h1>ABRAMS PET</h1><div class="sub">Сильный характер. Большое сердце.</div><div class="pill">ESP32-S3 · E-INK · v1.4 Emotions Update</div></div><div class="section">Эмоции и действия</div><div class="grid"><button onclick="m('calm')">😌 Спокоен</button><button onclick="m('annoyed')">😒 Раздражён</button><button onclick="m('shy')">😳 Смущён</button><button onclick="m('proud')">😏 Самодоволен</button><button onclick="m('happy')">💙 Доволен</button><button onclick="m('angry')">😠 Злится</button><button onclick="m('surprised')">😲 Удивлён</button><button onclick="m('strong')">💪 Сила</button><button onclick="m('thinking')">💭 Задумался</button><button onclick="m('tired')">😮‍💨 Устал</button><button onclick="m('sad')">🌧️ Грустит</button><button onclick="m('sleep')">🌙 Спать</button></div><div id="status">Готов к взаимодействию</div><div class="panel"><div class="section" style="margin-top:0">Обновление с iPhone</div><p><small>Загрузи только совместимый firmware.bin, собранный для Abrams Pet ESP32-S3. Не выключай питание во время обновления.</small></p><a class="btn" href="/update" style="text-decoration:none;text-align:center">⚙️ OTA-прошивка</a></div></div><script>async function m(v){let r=await fetch('/mood?set='+encodeURIComponent(v));document.querySelector('#status').textContent=r.ok?'Команда отправлена: '+v:'Ошибка соединения';}</script></body></html>)HTML";
const char OTA_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><style>body{font:17px system-ui;background:#10192a;color:#ecf3ff;max-width:500px;margin:40px auto;padding:20px}input,button{padding:14px;margin:12px 0;font:inherit}button{background:#78b8ff;border:0;border-radius:12px}</style><h2>💙 Abrams Pet · OTA</h2><p>Выбери <b>firmware.bin</b> от новой версии Abrams Pet для ESP32-S3. Нельзя загружать полный flash-образ или ZIP.</p><form action="/update" method="POST" enctype="multipart/form-data"><input type="file" name="firmware" accept=".bin" required><button>Обновить</button></form><p>Держи устройство подключённым к питанию до перезагрузки.</p><a href="/">Назад</a></html>)HTML";

void setup() {
  Serial.begin(115200);
  SPI.begin(EPD_SCK,-1,EPD_MOSI,EPD_CS);
  display.init(115200,true,2,false);
  drawFace();
  lastAction=millis();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName,apPassword);
  server.on("/",HTTP_GET,[](){server.send_P(200,"text/html; charset=utf-8",PAGE);});
  server.on("/mood",HTTP_GET,[](){
    String v=server.arg("set");
    if(v!="calm"&&v!="annoyed"&&v!="shy"&&v!="proud"&&v!="happy"&&v!="angry"&&v!="surprised"&&v!="strong"&&v!="sleep"&&v!="thinking"&&v!="tired"&&v!="sad"){
      server.send(400,"text/plain","Invalid mood");return;
    }
    mood=v;lastAction=millis();redraw=true;
    server.send(200,"text/plain","OK");
  });
  server.on("/update",HTTP_GET,[](){server.send_P(200,"text/html; charset=utf-8",OTA_PAGE);});
  server.on("/update",HTTP_POST,[](){
    bool ok=otaAllowed && otaSuccess && !Update.hasError();
    server.send(ok?200:500,"text/plain; charset=utf-8",ok?"Update complete. Rebooting...":"Update failed; device unchanged if image was rejected.");
    if(ok){delay(750);ESP.restart();}
    otaAllowed=false;otaSuccess=false;
  },[](){
    HTTPUpload& upload=server.upload();
    if(upload.status==UPLOAD_FILE_START){
      otaAllowed=upload.filename.endsWith(".bin") && Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH);
      otaSuccess=false;
    } else if(upload.status==UPLOAD_FILE_WRITE){
      if(otaAllowed && Update.write(upload.buf,upload.currentSize)!=upload.currentSize){otaAllowed=false;Update.abort();}
    } else if(upload.status==UPLOAD_FILE_END){
      if(otaAllowed)otaSuccess=Update.end(true);
    } else if(upload.status==UPLOAD_FILE_ABORTED){Update.abort();otaAllowed=false;}
  });
  server.begin();
  Serial.println("Abrams Pet v1.4: http://192.168.4.1");
}
void loop(){
  server.handleClient();
  if(mood!="sleep" && millis()-lastAction>900000UL){mood="sleep";redraw=true;}
  if(millis()-lastStats>=60000UL){
    lastStats=millis();
    if(mood=="sleep") energy=min(100,energy+2);
    else energy=max(0,energy-1);
    fullness=max(0,fullness-1);
    redraw=true;
  }
  if(redraw && millis()-lastRefresh>=3500UL)drawFace();
  delay(5);
}
