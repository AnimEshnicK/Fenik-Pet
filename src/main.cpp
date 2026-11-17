#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Update.h>
#include <GxEPD2_BW.h>
#include "fenik_emotions.h"
#include "fenik_web_emotions.h"

// Existing, tested wiring: MOSI 11, SCK 12, CS 10, DC 9, RST 8, BUSY 7
GxEPD2_BW<GxEPD2_213_GDEY0213B74, GxEPD2_213_GDEY0213B74::HEIGHT> display(GxEPD2_213_GDEY0213B74(10,9,8,7));
WebServer server(80);
Preferences prefs;
const char* AP_NAME="Fenik-Pet";
const char* AP_PASS="fenik1234";
int food=75,mood=65,fun=45,health=85;
uint32_t actionsCount=0;
uint32_t xp=32;
uint32_t bond=20, gifts=0, eventsSeen=0, gamesWon=0;
uint32_t lastEvent=0, eventUntil=0;
uint8_t currentEvent=0;
String dialogue="Привет! Я Феник ♡";
uint32_t dialogueUntil=0;
uint32_t gameRound=0;
int8_t gameTarget=-1;
uint32_t gameUntil=0;
uint32_t lastInteraction=0;
uint32_t feedCount=0, playCount=0, petCount=0, careCount=0;
uint32_t totalMinutes=0, lastVisitMinutes=0;
bool greetedThisBoot=false;
uint8_t clockHour=12;uint32_t clockAnchor=0;
uint32_t syncedEpoch=0,syncedAt=0;int16_t tzOffsetMinutes=0;
bool hasRealTime=false;
bool quizActive=false;uint8_t quizFace=0;
uint32_t gamesLost=0,bestReactionMs=0;
uint8_t minuteNow(){
 if(hasRealTime){int64_t local=(int64_t)syncedEpoch+(uint32_t)(millis()-syncedAt)/1000+(int64_t)tzOffsetMinutes*60;return (uint8_t)((((local/60)%60)+60)%60);}
 return (uint8_t)(((millis()-clockAnchor)/60000UL)%60);
}
uint8_t hourNow(){
 if(hasRealTime){int64_t local=(int64_t)syncedEpoch+(uint32_t)(millis()-syncedAt)/1000+(int64_t)tzOffsetMinutes*60;return (uint8_t)((((local/3600)%24)+24)%24);}
 return (clockHour+(millis()-clockAnchor)/3600000UL)%24;
}

uint32_t inventory[4]={1,1,0,0},given=0,quickWins=0,guessWins=0;
uint8_t gameType=0;
const char* itemNames[]={"Печенье","Клубок","Бантик","Звёздочка"};
const char* dayPart(){uint8_t h=hourNow();return h<6?"Ночь":h<12?"Утро":h<18?"День":h<22?"Вечер":"Ночь";}
String inventoryText(){String t="";for(int i=0;i<4;i++){if(i)t+=" · ";t+=String(itemNames[i])+": "+String(inventory[i]);}return t;}
void rewardItem(){inventory[random(0,4)]++;}

const char* personality(){
 if(playCount>=5 && playCount>petCount*2)return "Игривый";
 if(petCount>=5 && petCount>playCount*2)return "Ласковый";
 if(careCount>=5 && careCount>playCount+petCount)return "Спокойный";
 return "Нежный";
}
String memoryLine(){
 String t="Мы вместе уже "+String(totalMinutes)+" мин. ";
 if(playCount>petCount && playCount>feedCount)t+="Больше всего люблю наши игры! ♡";
 else if(petCount>playCount && petCount>feedCount)t+="Твои поглаживания — моё счастье. ♡";
 else if(feedCount>0)t+="Спасибо, что заботишься обо мне! ♡";
 else t+="Давай узнаем друг друга поближе!";
 return t;
}
const char* EVENT_NAMES[]={"Нет", "Нашёл звёздочку", "Просит обнимашки", "Сонный час", "Вдохновение", "Проголодался"};
const char* EVENT_LINES[]={"Мур~ Всё спокойно!", "Смотри, звёздочка! ✨", "Можно обнимашки? ♡", "Хочется поспать...", "У меня идея для игры!", "Может, перекусим?"};
uint32_t level(){return 1+xp/100;}
void say(const String& t,uint32_t duration=18000UL){dialogue=t;dialogueUntil=millis()+duration;}
void addBond(int n){bond=constrain((int)bond+n,0,100);}
void addXp(uint32_t n){xp+=n;}
String achievements(){
 String a="[";bool first=true;
 auto put=[&](const char* x){if(!first)a+=",";first=false;a+='"';a+=x;a+='"';};
 if(actionsCount>=1)put("Первое знакомство");
 if(bond>=50)put("Настоящие друзья");
 if(bond>=90)put("Неразлучные");
 if(gamesWon>=1)put("Первая победа");
 if(gamesWon>=10)put("Мастер игр");
 if(eventsSeen>=5)put("Исследователь событий");
 if(level()>=5)put("Опытный опекун");
 if(bestReactionMs>0&&bestReactionMs<=300)put("Молниеносная лапка");
 if(given>=1)put("Первый подарок");
 if(given>=10)put("Щедрое сердце");
 if(quickWins>=3)put("Быстрая лапка");
 if(guessWins>=3)put("Знаток эмоций");
 a+="]";return a;
}

bool sleeping=false;
bool nightModeEnabled=true;
bool nightRest=false;
bool nightOverride=false;
bool dayNap=false;
uint32_t napUntil=0, awakeSince=0, lastWake=0;
uint32_t wakeTransitionUntil=0;
uint8_t sleepiness=15;
uint32_t nightPets=0, nightCare=0, nightWakes=0;
uint32_t napCount=0, dreamCount=0;
uint32_t lastAutoScreen=0;
uint32_t lastManualFull=0;
uint32_t minRefreshMs=60000UL;
uint8_t refreshMode=1; // 0 normal, 1 eco, 2 essential only

uint32_t lastTick=0;
uint32_t lastScreen=0;
uint32_t lastFull=0;
uint8_t partialCount=0;
int shownFood=-1,shownMood=-1,shownFun=-1,shownHealth=-1;
uint32_t shownXp=UINT32_MAX;
String shownEmotion="";
bool shownBlink=false;
bool blink=false;
uint32_t blinkStart=0;
uint32_t lastBlink=0;
bool dirty=false;
uint8_t shownFace=255;
uint32_t lastFaceRefresh=0;
bool isNightHour();
extern uint8_t actionFace;
bool isResting(){return sleeping||nightRest||dayNap;}
void wakePet(bool fromInteraction=true){
 bool wasResting=isResting();
 sleeping=false;dayNap=false;napUntil=0;
 if(hasRealTime&&isNightHour())nightOverride=true;
 nightRest=false;
 lastInteraction=millis();awakeSince=millis();
 if(wasResting){actionFace=255;lastWake=millis();wakeTransitionUntil=millis()+90000UL;sleepiness=constrain((int)sleepiness+8,0,100);say("Ммм?.. Ты здесь? ♡");}
 dirty=true;
}
void updateSleepState(){
 uint32_t now=millis();
 bool night=nightModeEnabled&&hasRealTime&&isNightHour();
 if(!night)nightOverride=false;
 if(sleeping)return; // manually requested sleep
 if(night&&!nightOverride){
  if(!nightRest){nightRest=true;dayNap=false;napUntil=0;blink=false;dirty=true;say("Спокойной ночи... zZ ♡");}
  return;
 }
 if(nightRest){nightRest=false;awakeSince=now;lastInteraction=now;dirty=true;say("Доброе утро! ♡");}
 if(dayNap){
  if((int32_t)(now-napUntil)>=0){dayNap=false;awakeSince=now;lastInteraction=now;lastWake=now;wakeTransitionUntil=now+90000UL;sleepiness=constrain((int)sleepiness-20,0,100);say("Мрр... я выспался! ♡");dirty=true;}
  return;
 }
 if((uint32_t)(now-lastInteraction)>=15UL*60000UL && (uint32_t)(now-awakeSince)>=15UL*60000UL){
  if(night){nightOverride=false;nightRest=true;say("Я ещё немножко посплю... zZ");}
  else{dayNap=true;napUntil=now+(uint32_t)random(20,41)*60000UL;napCount++;say("Пойду немного вздремну... zZ");}
  sleepiness=constrain((int)sleepiness+8,0,100);blink=false;dirty=true;
 }
}


bool isNightHour(){uint8_t h=hourNow();return h>=22||h<7;}

const uint32_t FACE_GAP_MS=0UL; // portrait changes are immediate
const uint32_t TICK_MS=60000UL; // 1 minute of uptime
const uint32_t SCREEN_GAP_MS=0UL; // no artificial delay after actions
const uint32_t BLINK_INTERVAL_MS=90000UL;
const uint32_t BLINK_DURATION_MS=1200UL; // display refresh time may extend the blink
const uint8_t MAX_PARTIALS=12;
const uint32_t FULL_INTERVAL_MS=120UL*60UL*1000UL;

int clampStat(int x){return constrain(x,0,100);}
const char* emotion(){
 if(isResting())return "SLEEP";
 if(health<25)return "SICK";
 if(food<25)return "HUNGRY";
 if(fun<25||mood<25)return "SAD";
 if(mood>=75&&fun>=65)return "HAPPY";
 return "CALM";
}
const char* FACE_NAMES[]={"CALM","HAPPY","BLINK","SLEEP","SLEEPY","SAD","CRY","HUNGRY","SICK","ANGRY","SURPRISE","LOVE","DROWSY","RUB_EYES"};
int8_t debugFace=-1;
uint32_t debugUntil=0;
uint32_t refreshCount=0;
uint32_t facePartialCount=0;
uint32_t panelPartialCount=0;
uint32_t fullRefreshCount=0;
String lastAction="boot";
uint8_t actionFace=255;
uint32_t actionFaceUntil=0;
uint8_t faceMode(){
 if(quizActive)return quizFace;
 if(debugFace>=0 && (int32_t)(millis()-debugUntil)<0)return (uint8_t)debugFace;
 if(isResting())return 3; // SLEEP portrait
 // A recent interaction temporarily takes priority over grogginess.
 if(actionFace!=255 && (int32_t)(millis()-actionFaceUntil)<0)return actionFace;
 if((int32_t)(millis()-wakeTransitionUntil)<0){
  uint32_t elapsed=90000UL-(wakeTransitionUntil-millis());
  return elapsed<20000UL?13:12; // eye rubbing, then drowsy
 }
 // Sleepiness remains a statistic; it must not lock the face in a yawn.
 if(health<25)return 8;
 if(food<25)return 7;
 if(mood<15)return 6;
 if(fun<25||mood<25)return 5;
 if(blink)return 2;
 if(mood>=75&&fun>=65)return 1;
 if(food<40)return 7;
 return 0;
}
const uint8_t* faceBitmap(){
 switch(faceMode()){
 case 12:return fenikDrowsyArt;
 case 13:return fenikRubEyesArt;
 case 1:return fenikHappyArt;
 case 2:return fenikBlinkArt;
 case 3:return fenikSleepArt;
 case 4:return fenikSleepyArt;
 case 5:return fenikSadArt;
 case 6:return fenikCryArt;
 case 7:return fenikHungryArt;
 case 8:return fenikSickArt;
 case 9:return fenikAngryArt;
 case 10:return fenikSurpriseArt;
 case 11:return fenikLoveArt;
 default:return fenikCalmArt;
 }
}
void saveState(){
 prefs.putInt("food",food);prefs.putInt("mood",mood);prefs.putInt("fun",fun);
 prefs.putBool("nightAuto",nightModeEnabled);
 prefs.putUInt("nPets",nightPets);prefs.putUInt("nCare",nightCare);prefs.putUInt("nWakes",nightWakes);
 prefs.putUInt("naps",napCount);prefs.putUInt("dreams",dreamCount);
 prefs.putUChar("refMode",refreshMode);prefs.putUInt("refMs",minRefreshMs);
 prefs.putInt("health",health);prefs.putUInt("xp",xp);prefs.putBool("sleep",sleeping);
 prefs.putUInt("bond",bond);prefs.putUInt("gifts",gifts);prefs.putUInt("events",eventsSeen);prefs.putUInt("wins",gamesWon);
 prefs.putUInt("acts",actionsCount);
 prefs.putUInt("fed",feedCount);prefs.putUInt("played",playCount);
 prefs.putUInt("petted",petCount);prefs.putUInt("cared",careCount);
 prefs.putUInt("minutes",totalMinutes);
 prefs.putUChar("hour",hourNow());prefs.putUInt("given",given);
 prefs.putUInt("quick",quickWins);prefs.putUInt("guess",guessWins);
 prefs.putUInt("lost",gamesLost);prefs.putUInt("bestms",bestReactionMs);
 for(int i=0;i<4;i++)prefs.putUInt((String("item")+i).c_str(),inventory[i]);
}
// Adafruit GFX built-in font: cursor Y is the TOP of the 8px text cell.
// All stat labels and bars fit within their 18px rows.
void bar(int y,const char* name,int value){
 display.setTextColor(GxEPD_BLACK);
 display.setTextSize(1);
 display.setCursor(169,y+1);display.print(name); // pixels y+1 .. y+7
 display.drawRect(169,y+10,73,7,GxEPD_BLACK); // pixels y+10 .. y+16
 int n=(71*clampStat(value))/100;
 if(n>0)display.fillRect(170,y+11,n,5,GxEPD_BLACK);
}
void drawStatus(){
 display.setTextColor(GxEPD_BLACK);display.setTextSize(1);
 display.setCursor(169,6);display.print("FENIK <3");
 display.setCursor(169,18);display.print("LV");display.print(1+xp/100);
 display.print(" XP");display.print(xp%100);
 display.setCursor(169,30);display.print(emotion());
 display.setCursor(226,30);display.print(blink?"--":"oo");
 display.drawFastHLine(169,39,73,GxEPD_BLACK);
}
void drawRightPanel(){
 // A single clipped redraw avoids stale glyphs and overlapping partial windows.
 display.fillRect(168,0,82,122,GxEPD_WHITE);
 drawStatus();
 bar(44,"FOOD",food);bar(63,"MOOD",mood);
 bar(82,"FUN",fun);bar(101,"HP",health);
}
void snapshot(){
 shownFace=faceMode();
 shownFood=food;shownMood=mood;shownFun=fun;shownHealth=health;
 shownXp=xp;shownEmotion=emotion();shownBlink=blink;
 lastScreen=millis();dirty=false;
}
void drawScreen(){
 display.setRotation(1);display.setFullWindow();display.firstPage();
 do{
  display.fillScreen(GxEPD_WHITE);
  display.drawBitmap(0,0,faceBitmap(),160,122,GxEPD_BLACK);
  display.drawFastVLine(163,3,116,GxEPD_BLACK);
  drawRightPanel();
 }while(display.nextPage());
 lastScreen=millis();lastFull=millis();lastFaceRefresh=millis();partialCount=0;refreshCount++;fullRefreshCount++;snapshot();
}
void partialRightPanel(){
 // X=168 is byte-aligned; no portrait pixels are inside this window.
 display.setRotation(1);
 display.setPartialWindow(168,0,82,122);
 display.firstPage();
 do{drawRightPanel();}while(display.nextPage());
}
void partialFace(){
 // Portrait region is byte-aligned (160px); do not touch right panel.
 display.setRotation(1);
 display.setPartialWindow(0,0,160,122);
 display.firstPage();
 do{
  display.fillRect(0,0,160,122,GxEPD_WHITE);
  display.drawBitmap(0,0,faceBitmap(),160,122,GxEPD_BLACK);
 }while(display.nextPage());
 lastScreen=millis();lastFaceRefresh=millis();
 facePartialCount++;partialCount++;refreshCount++;
}
void updateScreen(){
 uint32_t now=millis();
 bool faceChanged=shownFace!=faceMode();
 bool panelChanged=shownXp!=xp||shownEmotion!=String(emotion())||
  shownFood!=food||shownMood!=mood||shownFun!=fun||shownHealth!=health;
 if(!faceChanged && !panelChanged && !dirty)return;
 if(partialCount>=MAX_PARTIALS || (!isResting() && (uint32_t)(now-lastFull)>=FULL_INTERVAL_MS)){
  drawScreen();return;
 }
 if(faceChanged)partialFace();
 if(panelChanged){partialRightPanel();panelPartialCount++;partialCount++;refreshCount++;}
 snapshot();
}
String jsonEscape(const String& value){
 String out;out.reserve(value.length()+8);
 for(size_t i=0;i<value.length();i++){
  char c=value[i];
  if(c=='\\'||c=='"'){out+='\\';out+=c;}
  else if(c=='\n')out+="\\n";
  else if(c=='\r')out+="\\r";
  else if((unsigned char)c<32){out+=' ';}
  else out+=c;
 }
 return out;
}
String json(){
 String s="{\"food\":"+String(food)+",\"mood\":"+String(mood)+",\"fun\":"+String(fun)+",\"health\":"+String(health);
 s+=",\"xp\":"+String(xp)+",\"level\":"+String(1+xp/100)+",\"sleeping\":";
 s+=(sleeping?"true":"false");
 s+=",\"nightRest\":";s+=(nightRest?"true":"false");
 s+=",\"dayNap\":";s+=(dayNap?"true":"false");
 s+=",\"resting\":";s+=(isResting()?"true":"false");
 s+=",\"sleepiness\":"+String(sleepiness);
 s+=",\"idleSeconds\":"+String((millis()-lastInteraction)/1000UL);
 s+=",\"napRemaining\":"+String(dayNap&&((int32_t)(napUntil-millis())>0)?(napUntil-millis())/1000UL:0);
 s+=",\"wakeStage\":"+String((int32_t)(millis()-wakeTransitionUntil)<0?1:0);
 s+=",\"nightPets\":"+String(nightPets)+",\"nightCare\":"+String(nightCare)+",\"nightWakes\":"+String(nightWakes);
 s+=",\"napCount\":"+String(napCount)+",\"dreamCount\":"+String(dreamCount);
 s+=",\"refreshMode\":"+String(refreshMode)+",\"refreshInterval\":"+String(minRefreshMs/1000UL);
 s+=",\"nightModeEnabled\":";s+=(nightModeEnabled?"true":"false");
 s+=",\"emotion\":\""+String(emotion())+"\"";
 s+=",\"face\":\""+String(FACE_NAMES[faceMode()])+"\"";
 s+=",\"faceId\":"+String(faceMode());
 s+=",\"blink\":"+String(blink?"true":"false");
 s+=",\"debugFace\":"+String(debugFace);
 s+=",\"uptime\":"+String(millis()/1000UL);
 s+=",\"freeHeap\":"+String(ESP.getFreeHeap());
 s+=",\"clients\":"+String(WiFi.softAPgetStationNum());
 s+=",\"refreshes\":"+String(refreshCount);
 s+=",\"facePartials\":"+String(facePartialCount);
 s+=",\"panelPartials\":"+String(panelPartialCount);
 s+=",\"fullRefreshes\":"+String(fullRefreshCount);
 s+=",\"actions\":"+String(actionsCount);
 s+=",\"bond\":"+String(bond)+",\"gifts\":"+String(gifts)+",\"eventsSeen\":"+String(eventsSeen)+",\"gamesWon\":"+String(gamesWon);
 s+=",\"personality\":\""+String(personality())+"\"";
 s+=",\"memory\":\""+jsonEscape(memoryLine())+"\"";
 s+=",\"feedCount\":"+String(feedCount)+",\"playCount\":"+String(playCount);
 s+=",\"petCount\":"+String(petCount)+",\"careCount\":"+String(careCount);
 s+=",\"totalMinutes\":"+String(totalMinutes);
 s+=",\"hour\":"+String(hourNow())+",\"minute\":"+String(minuteNow())+",\"dayPart\":\""+String(dayPart())+"\"";
 s+=",\"inventory\":\""+inventoryText()+"\"";
 s+=",\"items\":[";for(int i=0;i<4;i++){if(i)s+=",";s+=String(inventory[i]);}s+="]";
 s+=",\"given\":"+String(given)+",\"quickWins\":"+String(quickWins)+",\"guessWins\":"+String(guessWins);
 s+=",\"gameType\":"+String(gameType)+",\"quizActive\":"+(quizActive?String("true"):String("false"));
 s+=",\"gamesLost\":"+String(gamesLost)+",\"bestReactionMs\":"+String(bestReactionMs);
 s+=",\"realTime\":";s+=(hasRealTime?"true":"false");
 if(hasRealTime){uint32_t e=syncedEpoch+(uint32_t)(millis()-syncedAt)/1000;s+=",\"epoch\":"+String(e);}

 s+=",\"event\":\""+String(EVENT_NAMES[currentEvent])+"\"";
 s+=",\"dialogue\":\""+jsonEscape(dialogue)+"\"";
 s+=",\"gameActive\":"+String(gameTarget>=0?"true":"false");
 s+=",\"gameRound\":"+String(gameRound)+",\"gameSeconds\":"+String(gameTarget>=0?(int32_t)(gameUntil-millis())>0?(gameUntil-millis())/1000:0:0);
 s+=",\"achievements\":"+achievements();
 s+=",\"lastAction\":\""+jsonEscape(lastAction)+"\"}";
 return s;
}
const char HTML[] PROGMEM=R"FENIK(<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Fenik Pet · WebUI</title><style>
:root{color-scheme:dark;font-family:system-ui}*{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at top,#493153,#171622 75%);color:#f9edf8;padding:18px}main{max-width:750px;margin:auto}h1{color:#ffb5df;margin:0}h2{font-size:17px;color:#9de9f3;margin:0 0 14px}.muted{color:#b8b2c8}.card{background:#ffffff0b;border:1px solid #ffffff22;border-radius:20px;padding:18px;margin:14px 0}.hero{display:flex;gap:20px;align-items:center}.avatar{width:112px;min-width:112px;height:112px;display:flex;align-items:center;justify-content:center;border-radius:18px;background:#fff;overflow:hidden}.avatar img{width:100%;height:100%;object-fit:contain;image-rendering:pixelated}.pill{display:inline-block;background:#ffb0dc25;border-radius:40px;padding:5px 12px;color:#ffb4dd}.stats{display:grid;grid-template-columns:1fr 1fr;gap:10px 18px}.bar{height:9px;border-radius:10px;background:#ffffff20;overflow:hidden;margin-top:6px}.fill{height:100%;background:linear-gradient(90deg,#ff8ed4,#8fe6f6);transition:width .3s}.row{display:flex;justify-content:space-between;font-size:13px}button,select{border:0;border-radius:12px;padding:12px;background:linear-gradient(110deg,#ffaddd,#a6eaf3);color:#292039;font-weight:750;cursor:pointer}button:disabled{opacity:.4}
.cups{display:flex;justify-content:center;gap:15px;margin:20px auto}.cup{font-size:42px;min-width:70px;transition:transform .28s}.cup.reveal{transform:translateY(-22px)}
#gameStage .big{font-size:46px;margin:14px}.emotionPic{width:160px;height:122px;image-rendering:pixelated;border-radius:10px;background:white}
#reactionPad{width:100%;min-height:115px;font-size:28px;border-radius:18px;background:#55435e;color:white}
#reactionPad.ready{background:#37ba8c;color:#102d26}button.secondary{background:#ffffff18;color:white;border:1px solid #ffffff28}.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:9px}.emotions{display:grid;grid-template-columns:repeat(4,1fr);gap:7px}.emotions button{font-size:12px;padding:10px 2px}.emotions button.active{outline:3px solid #fff}.debug{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}.mono{font:12px ui-monospace,monospace;white-space:pre-wrap;line-height:1.7;color:#c7eaf1}#toast{position:sticky;bottom:10px;background:#29213c;border:1px solid #ff9cd4;border-radius:12px;padding:10px;display:none}label{display:block;margin-bottom:8px}.switch{display:flex;gap:10px;align-items:center;margin:12px 0}input{accent-color:#ff9bd6}.debugfields{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.debugfields label{font-size:12px;color:#b8b2c8}.debugfields input{width:100%;background:#ffffff15;color:#fff;border:1px solid #ffffff35;border-radius:10px;padding:10px;font-size:16px}.debugfields button{width:100%;margin-top:5px}.inventoryItem{display:flex;flex-direction:column;gap:6px;align-items:stretch}@media(max-width:430px){.emotions{grid-template-columns:repeat(3,1fr)}.hero{gap:8px}.avatar{width:95px;min-width:95px;height:95px}}

/* Mobile-first navigation; all original controls and endpoint handlers retained */
.tabnav{position:sticky;top:0;z-index:10;display:flex;gap:6px;overflow-x:auto;padding:9px 0 12px;background:#201a2c;scrollbar-width:thin}.tabnav button{white-space:nowrap;flex:0 0 auto;background:#ffffff16;color:#f9edf8;border:1px solid #ffffff28;padding:10px 13px;font-size:13px}.tabnav button[aria-selected="true"]{background:linear-gradient(110deg,#ffaddd,#a6eaf3);color:#292039}.tabpane[hidden]{display:none!important}.tabpane{animation:tabIn .12s ease-out}@keyframes tabIn{from{opacity:.7}to{opacity:1}}.tabpane>.card:first-child{margin-top:12px}.tabhint{font-size:12px;color:#b8b2c8;margin:8px 0 0}a.otabtn{display:block;text-align:center;border-radius:12px;padding:13px;background:linear-gradient(110deg,#ffaddd,#a6eaf3);color:#292039;font-weight:750;text-decoration:none}
</style></head><body><main><h1>♡ Fenik Pet <small style="font-size:13px;color:#a6eaf3">v1.4.5</small></h1><p class="muted">Living Companion · сон, память и тихий e-ink</p><nav class="tabnav" aria-label="Разделы Fenik Pet"><button type="button" data-tab="home" aria-selected="true">♡ Главная</button><button type="button" data-tab="status" aria-selected="false">📊 Status</button><button type="button" data-tab="games" aria-selected="false">🎮 Игры</button><button type="button" data-tab="inventory" aria-selected="false">🎒 Инвентарь</button><button type="button" data-tab="debug" aria-selected="false">🧪 Debug</button><button type="button" data-tab="settings" aria-selected="false">⚙️ Настройки</button></nav>
<div class="tabpane" id="tab-home"><section class="card hero"><div class="avatar" id="avatar"><img id="fenikPortrait" src="/portrait?face=0" alt="Феник"></div><div><div class="pill" id="face">CALM</div><h2 id="state" style="margin:10px 0">Подключение…</h2><div id="level">LV 1 · XP 0</div><div class="muted" id="conn">● Подключение</div></div></section><section class="card"><h2>💬 Феник говорит</h2><p id="dialogue" style="font-size:18px">Привет! ♡</p><p class="muted" id="event">Событие: нет</p></section><section class="card"><h2>🎮 Забота и игры</h2><div class="grid"><button onclick="send('action','feed')">🍙 Покормить</button><button onclick="send('action','play')">🎮 Поиграть</button><button onclick="send('action','sleep')" id="sleepBtn">💤 Сон</button><button onclick="send('action','wash')">🫧 Искупать</button><button onclick="send('action','pet')">💕 Погладить</button><button onclick="send('action','surprise')">🎁 Сюрприз</button></div></section></div>
<div class="tabpane" id="tab-status" hidden><section class="card"><h2>🐱 Характер и память</h2><div class="pill" id="personality">Нежный</div><p id="memory">Мы только знакомимся ♡</p><p class="muted" id="habits">Ожидание истории...</p><button class="secondary" onclick="send('memory','recall')">💭 Вспомнить нас</button></section><section class="card"><h2>💞 Дружба и достижения</h2><div class="row"><span>Привязанность</span><b id="bondNum">0%</b></div><div class="bar"><div class="fill" id="bondBar" style="width:0%"></div></div><p id="progress" class="muted"></p><div id="achievements" class="pill">Пока нет достижений</div></section><section class="card"><h2>🌙 Режим дня</h2><p id="daycycle">День</p><p class="muted" id="clockInfo">Часы синхронизируются с телефоном или ПК при открытии страницы.</p><button class="secondary" onclick="syncTime()">🕒 Синхронизировать время</button></section><section class="card"><h2>🌙 Сон и пробуждение</h2><p id="nightState" class="muted">Ожидание времени…</p><div class="grid"><button class="secondary" id="nightToggle" onclick="send('night','toggle')">🌙 Автосон: включён</button><button class="secondary" onclick="send('night','wake')">☀️ Разбудить Феника</button></div><p class="muted">Ночью спит с 22:00 до 07:00. Днём дремлет после 15 минут бездействия на 20–40 минут. Забота во сне не будит, игры будят. Экран почти не обновляется. После отключения питания открой страницу для синхронизации часов.</p></section></div>
<div class="tabpane" id="tab-games" hidden><section class="card"><h2>🎮 Игровой уголок</h2>
<div class="grid"><button onclick="startShells()">♡ Напёрстки</button><button onclick="startEmotion()">🎭 Угадай эмоцию</button><button onclick="startReaction()">⚡ Реакция</button></div>
<div id="gameStage" style="margin-top:15px;min-height:160px;text-align:center"><p class="muted">Выбери мини-игру!</p></div>
<p id="gameInfo" class="muted">Игры проходят в браузере, результат сохраняется на Фенике.</p>
<p id="gameStats" class="muted">Раундов: 0 · Побед: 0 · Поражений: 0</p></section></div>
<div class="tabpane" id="tab-inventory" hidden><section class="card"><h2>🎒 Инвентарь</h2><p class="muted">Показаны только предметы, которые есть в наличии.</p><div class="grid" id="inventoryItems"></div><p id="inventoryEmpty" class="muted" hidden>Пока нет предметов. Получай их в играх и событиях ♡</p><p id="inventory" class="muted" hidden></p><p id="giftStats" class="muted"></p></section></div>
<div class="tabpane" id="tab-debug" hidden><section class="card"><h2>🧪 Debug · 12 эмоций</h2><p class="muted" style="font-size:13px">Нажми эмоцию — Феник покажет её на дисплее примерно 20 секунд. Изменения характеристик сохраняются только после обычных действий; тест эмоций не меняет прогресс.</p><div class="emotions" id="emotions"></div><div style="margin-top:10px" class="grid"><button class="secondary" onclick="send('debug','auto')">↩ Автоэмоции</button><button class="secondary" onclick="send('debug','blink')">◡ Тест моргания</button></div></section><section class="card"><h2>🎛 Ручные значения</h2><p class="muted">Введи число и нажми «Применить». Изменения сохраняются. Шкалы: 0–100, XP: 0–1000000, предметы: 0–99.</p><div class="debugfields" id="debugFields"></div></section><section class="card"><h2>⏰ Debug времени и событий</h2><div class="grid"><button class="secondary" onclick="send('world','hour',8)">☀️ Утро 08:00</button><button class="secondary" onclick="send('world','hour',14)">🌤 День 14:00</button><button class="secondary" onclick="send('world','hour',20)">🌆 Вечер 20:00</button><button class="secondary" onclick="send('world','hour',23)">🌙 Ночь 23:00</button><button class="secondary" onclick="send('world','event')">🎲 Событие</button><button class="secondary" onclick="send('world','gift')">🎁 Тестовый предмет</button></div></section><section class="card"><h2>🖥 E-ink Control</h2><p class="muted">Полное обновление работает даже во сне и не будит Феника. Частичное обновление принудительно рисует текущую эмоцию и показатели.</p><div class="grid"><button class="secondary" onclick="send('debug','full')">🔄 Полное обновление</button><button class="secondary" onclick="send('debug','partial')">↻ Частичное обновление</button></div><p class="muted">Автоматическое обновление:</p><div class="grid"><button class="secondary" onclick="send('screen','mode',0)">Обычный</button><button class="secondary" onclick="send('screen','mode',1)">Экономный</button><button class="secondary" onclick="send('screen','mode',2)">Минимальный</button></div><p class="muted">Минимальный интервал: <span id="screenInterval">60</span> с</p><div class="grid"><button class="secondary" onclick="send('screen','interval',30)">30 с</button><button class="secondary" onclick="send('screen','interval',60)">60 с</button><button class="secondary" onclick="send('screen','interval',120)">120 с</button><button class="secondary" onclick="send('screen','interval',180)">180 с</button></div><p class="muted" id="screenStats">Загрузка...</p></section><section class="card"><h2>🛠 Диагностика и тесты</h2><div id="stats"></div><div class="debug"><button class="secondary" onclick="send('debug','tick')">⏩ +1 минута жизни</button><button class="secondary" onclick="send('debug','refresh')">🔄 Обновить экран</button><button class="secondary" onclick="send('debug','boost')">✨ Все шкалы 100%</button><button class="secondary" onclick="confirm('Сбросить характеристики и XP?')&&send('debug','reset')">↺ Сброс прогресса</button></div><p class="mono" id="diagnostics">Подключение к ESP32…</p><div class="grid"><button class="secondary" onclick="copyDebug()">📋 Копировать диагностику</button><button class="secondary" onclick="refresh()">↻ Обновить данные</button></div></section></div>
<div class="tabpane" id="tab-settings" hidden><section class="card"><h2>⚙️ Настройки и обновление</h2><p class="muted">Обновление прошивки через iPhone. Используй только совместимый firmware.bin для ESP32-S3.</p><a class="otabtn" href="/update">⬆️ Открыть OTA Update</a><p class="tabhint">Не отключай питание во время обновления. Настройки экрана находятся во вкладке Debug.</p><button class="secondary" onclick="syncTime()">🕒 Синхронизировать часы</button></section></div><p class="muted" style="font-size:12px;text-align:center">Автообновление данных каждые 2 секунды. E-ink медленнее веб-интерфейса.</p><div id="toast"></div></main><script>

(function(){const tabs=[...document.querySelectorAll('[data-tab]')];function selectTab(name){if(!document.getElementById('tab-'+name))name='home';document.querySelectorAll('.tabpane').forEach(p=>p.hidden=p.id!=='tab-'+name);tabs.forEach(b=>b.setAttribute('aria-selected',String(b.dataset.tab===name)));history.replaceState(null,'','#'+name)}tabs.forEach(b=>b.addEventListener('click',()=>selectTab(b.dataset.tab)));selectTab(location.hash.slice(1)||'home')})();
const faces=['CALM','HAPPY','BLINK','SLEEP','SLEEPY','SAD','CRY','HUNGRY','SICK','ANGRY','SURPRISE','LOVE','DROWSY','RUB_EYES'];const icons=['ฅ^•ﻌ•^ฅ','(≧◡≦)','(－_－)','(－.－) zZ','(－ω－)','(｡•́︿•̀｡)','(╥﹏╥)','(｡•́︿•̀｡)','(×_×)','( `ω´ )','(⊙_⊙)','(♡ω♡)','(－ω－)','(つω⊂)'];const labels={food:'🍙 Еда',mood:'💗 Настроение',fun:'🎮 Веселье',health:'✚ Здоровье'};let last=null,busy=false;const $=id=>document.getElementById(id);$('emotions').innerHTML=faces.map((f,i)=>`<button class="secondary" id="emo${i}" onclick="send('debug','face',${i})">${icons[i]}<br>${f}</button>`).join('');
const debugKeys=[['food','Еда',100],['mood','Настроение',100],['fun','Веселье',100],['health','Здоровье',100],['sleepiness','Сонливость',100],['bond','Дружба',100],['xp','Опыт XP',1000000],['item0','Печенье',99],['item1','Клубок',99],['item2','Бантик',99],['item3','Звёздочка',99]];
$('debugFields').innerHTML=debugKeys.map(([key,label,max])=>`<label>${label}<input type="number" id="set_${key}" min="0" max="${max}" inputmode="numeric" placeholder="0–${max}"><button class="secondary" onclick="setDebug('${key}')">Применить</button></label>`).join('');
async function setDebug(key){if(busy)return;const el=$('set_'+key),value=el.value;if(!/^\d+$/.test(value)){toast('Введи целое число');return;}busy=true;try{const r=await fetch('/debug?do=set&key='+encodeURIComponent(key)+'&value='+encodeURIComponent(value),{method:'POST'});if(!r.ok)throw Error(await r.text());toast('✓ Значение сохранено');await refresh();}catch(e){toast('Ошибка: '+e.message)}finally{busy=false}}
function toast(t){$('toast').textContent=t;$('toast').style.display='block';setTimeout(()=>$('toast').style.display='none',2800)}
async function refresh(){try{const r=await fetch('/status',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);const s=await r.json();last=s;$('conn').textContent='● На связи';$('conn').style.color='#8de7bd';$('face').textContent=s.quizActive?'???':s.face;const portrait=$('fenikPortrait');const faceId=s.quizActive?0:(Number(s.faceId)||0);if(portrait.dataset.face!==String(faceId)){portrait.src='/portrait?face='+faceId;portrait.dataset.face=String(faceId)}$('state').textContent=s.quizActive?'🎭 Угадай эмоцию':(s.resting?'💤 Феник спит':s.blink?'◡ Моргает':s.emotion);$('level').textContent=`LV ${s.level} · XP ${s.xp}`;$('personality').textContent=s.personality;$('memory').textContent=s.memory;
$('daycycle').textContent=s.dayPart+' · '+String(s.hour).padStart(2,'0')+':'+String(s.minute).padStart(2,'0');
$('nightState').textContent=s.resting?(s.nightRest?'🌙 Ночной сон до 07:00':s.dayNap?'💤 Дневной сон · ещё '+Math.ceil(s.napRemaining/60)+' мин':'💤 Ручной сон'):'☀️ Бодрствует · сонливость '+s.sleepiness+'% · бездействие '+Math.floor(s.idleSeconds/60)+' мин';
$('screenInterval').textContent=s.refreshInterval;$('screenStats').textContent='Режим: '+['Обычный','Экономный','Минимальный'][s.refreshMode]+' · Обновлений: '+s.refreshes+' · Полных: '+s.fullRefreshes+' · Частичных: '+(s.facePartials+s.panelPartials)+' · Ночных поглаживаний: '+s.nightPets+' · Ночных пробуждений: '+s.nightWakes;
$('nightToggle').textContent=s.nightModeEnabled?'🌙 Выключить автосон':'🌙 Включить автосон';
$('clockInfo').textContent=s.realTime?'✓ Реальное время синхронизировано':'⚠ Время ещё не синхронизировано';
$('inventory').textContent=s.inventory;
const itemNames=['🍪 Печенье','🧶 Клубок','🎀 Бантик','⭐ Звёздочка'];
$('inventoryItems').innerHTML=(s.items||[]).map((count,i)=>count>0?`<button class="inventoryItem" onclick="send('gift','give',${i})"><span>${itemNames[i]}</span><small>В наличии: ${count}</small></button>`:'').join('');
$('inventoryEmpty').hidden=(s.items||[]).some(n=>n>0);
$('gameStats').textContent=`Раундов: ${s.gameRound} · Побед: ${s.gamesWon} · Поражений: ${s.gamesLost} · Рекорд реакции: ${s.bestReactionMs||'—'} мс`;
$('giftStats').textContent='Подарено: '+s.given+' · Быстрых побед: '+s.quickWins+' · Угадано: '+s.guessWins;
$('habits').textContent=`Кормлений: ${s.feedCount} · Игр: ${s.playCount} · Поглаживаний: ${s.petCount} · Заботы: ${s.careCount}`;
$('dialogue').textContent=s.dialogue;$('event').textContent='Событие: '+s.event;$('bondNum').textContent=s.bond+'%';$('bondBar').style.width=s.bond+'%';$('progress').textContent=`Побед: ${s.gamesWon} · Подарков: ${s.gifts} · Событий: ${s.eventsSeen}`;$('achievements').textContent=Array.isArray(s.achievements)&&s.achievements.length?s.achievements.join(' · '):'Пока нет достижений';$('sleepBtn').textContent=s.resting?'☀️ Разбудить':'💤 Спать';if($('stats'))$('stats').innerHTML=Object.entries(labels).map(([k,v])=>`<div><div class="row"><span>${v}</span><b>${s[k]}%</b></div><div class="bar"><div class="fill" style="width:${s[k]}%"></div></div></div>`).join('');faces.forEach((_,i)=>{const b=$('emo'+i);if(b)b.classList.toggle('active',!s.quizActive&&s.debugFace===i)});$('diagnostics').textContent=`Эмоция на экране: ${s.quizActive?'СКРЫТО В ИГРЕ':s.face} (#${s.quizActive?'?':s.faceId})
Режим: ${s.debugFace>=0?'DEBUG':'AUTO'} | Моргание: ${s.blink}
Аптайм: ${Math.floor(s.uptime/60)} мин ${s.uptime%60} с
Свободная RAM: ${s.freeHeap} байт
Подключено клиентов: ${s.clients}
Обновлений дисплея: ${s.refreshes}
Действий: ${s.actions}
Последнее действие: ${s.lastAction}`;}catch(e){
 const msg=(e&&e.message)?e.message:String(e);
 $('conn').textContent='● Ошибка данных';$('conn').style.color='#ff9ca8';
 $('diagnostics').textContent='Не удалось обновить диагностику: '+msg+'\nОткрой http://192.168.4.1/status и проверь ответ.';
 if($('inventory').textContent==='Загрузка...')$('inventory').textContent='Нет данных: '+msg;
 console.error('Fenik /status:',e);
}}
async function send(route,cmd,id){if(busy)return;busy=true;document.querySelectorAll('button').forEach(b=>b.disabled=true);try{const url='/'+route+'?do='+encodeURIComponent(cmd)+(id===undefined?'':'&id='+id);const r=await fetch(url,{method:'POST'});if(!r.ok)throw Error(await r.text());toast('✓ Команда отправлена: '+cmd);await refresh()}catch(e){toast('Ошибка: '+e.message)}finally{busy=false;document.querySelectorAll('button').forEach(b=>b.disabled=false)}}
function copyDebug(){if(!last)return;const s=JSON.stringify(last,null,2);navigator.clipboard?.writeText(s).then(()=>toast('Диагностика скопирована')).catch(()=>prompt('Скопируй JSON:',s))||prompt('Скопируй JSON:',s)}

let currentGame='',roundBusy=false,gameTimer=null,reactStart=0,reactTimeout=null,reactArmed=false;
const gameStage=$('gameStage');
function gameMessage(t){$('gameInfo').textContent=t;}
function stopGameTimers(){if(gameTimer)clearTimeout(gameTimer);if(reactTimeout)clearTimeout(reactTimeout);gameTimer=reactTimeout=null;}
async function finishGame(win,ms=0){
 if(!roundBusy)return;roundBusy=false;stopGameTimers();
 try{
 const r=await fetch('/game?do=finish&win='+(win?1:0)+'&type='+encodeURIComponent(currentGame)+'&ms='+ms,{method:'POST'});
 if(!r.ok)throw Error(await r.text());
 gameMessage(win?'♡ Победа! Феник счастлив!':'Не получилось. Попробуем ещё?');
 await refresh();
 }catch(e){gameMessage('Ошибка сохранения результата: '+e.message)}
 finally{roundBusy=false}
}
function startShells(){
 stopGameTimers();currentGame='shells';roundBusy=true;
 let heart=Math.floor(Math.random()*3),positions=[0,1,2],moves=0,ready=false,animating=false;
 gameStage.innerHTML='<p>Запомни, где сердечко!</p><div class="cups" id="cups" style="position:relative;height:100px;max-width:340px"></div><p id="shellHint">Смотри внимательно...</p><div class="grid" id="shellChoices" style="display:none"><button onclick="chooseShell(0)">Левый</button><button onclick="chooseShell(1)">Средний</button><button onclick="chooseShell(2)">Правый</button></div>';
 const cupNodes=[];
 for(let i=0;i<3;i++){
  const el=document.createElement('div');el.className='cup';el.style.cssText='position:absolute;top:14px;left:calc(50% - 35px);width:70px;text-align:center;transition:transform 240ms ease-in-out;will-change:transform;pointer-events:none';el.textContent='🥤';$('cups').appendChild(el);cupNodes.push(el);
 }
 const stepWidth=92;
 const render=()=>positions.forEach((id,slot)=>{cupNodes[id].style.transform='translateX('+((slot-1)*stepWidth)+'px)';});
 render();cupNodes[heart].textContent='💗';gameMessage('Запоминай сердечко!');
 gameTimer=setTimeout(()=>{
  cupNodes[heart].textContent='🥤';
  function swap(){
   if(!roundBusy)return;
   if(moves>=16){ready=true;$('shellHint').textContent='Где сердечко?';$('shellChoices').style.display='grid';gameMessage('Выбери позицию стаканчика');return;}
   let a=Math.floor(Math.random()*3),b=(a+1+Math.floor(Math.random()*2))%3;
   [positions[a],positions[b]]=[positions[b],positions[a]];render();moves++;
   gameTimer=setTimeout(swap,260);
  }
  swap();
 },1100);
 window.chooseShell=async i=>{
  if(!roundBusy||!ready)return;ready=false;
  $('shellChoices').style.display='none';
  cupNodes[heart].textContent='💗';
  await finishGame(positions[i]===heart);
 };
}
let emoAnswer=0;
async function startEmotion(){
 stopGameTimers();currentGame='emotion';roundBusy=true;
 const variants=[{name:'HAPPY',icon:'😊',id:1},{name:'SAD',icon:'😢',id:5},{name:'SURPRISE',icon:'😮',id:10},{name:'ANGRY',icon:'😠',id:9},{name:'LOVE',icon:'😍',id:11}];
 emoAnswer=Math.floor(Math.random()*variants.length);
 const v=variants[emoAnswer];
 gameStage.innerHTML='<p>Смотри на экран самого Феника 🐱</p><div class="big">📟</div><p id="quizHint">Отправляю эмоцию на e-ink...</p><div class="grid" id="emotionChoices" style="margin-top:12px"></div>';
 gameMessage('Подожди, пока портрет обновится на e-ink.');
 try{
  const r=await fetch('/quiz?do=start&id='+v.id,{method:'POST'});
  if(!r.ok)throw Error(await r.text());
  if(!roundBusy)return;
  const choices=[emoAnswer];while(choices.length<3){const x=Math.floor(Math.random()*variants.length);if(!choices.includes(x))choices.push(x)}
  for(let i=choices.length-1;i>0;i--){const j=Math.floor(Math.random()*(i+1));[choices[i],choices[j]]=[choices[j],choices[i]]}
  $('emotionChoices').innerHTML=choices.map(i=>`<button onclick="chooseEmotion(${i})">${variants[i].icon} ${variants[i].name}</button>`).join('');
  $('quizHint').textContent='Какая эмоция сейчас на e-ink?';gameMessage('Выбирай ответ по настоящему дисплею!');
  window.chooseEmotion=async i=>{if(!roundBusy)return;gameStage.querySelectorAll('button').forEach(b=>b.disabled=true);await finishGame(i===emoAnswer)};
 }catch(e){roundBusy=false;gameMessage('Не удалось вывести эмоцию: '+e.message)}
}
function startReaction(){
 stopGameTimers();currentGame='reaction';roundBusy=true;reactArmed=false;
 gameStage.innerHTML='<p>Жди зелёного сигнала. Не нажимай раньше!</p><button id="reactionPad" onclick="reactionClick()">ЖДИ...</button>';
 gameMessage('Приготовься!');
 reactTimeout=setTimeout(()=>{if(!roundBusy)return;reactArmed=true;reactStart=performance.now();$('reactionPad').classList.add('ready');$('reactionPad').textContent='ЖМИ!';},1500+Math.random()*3000);
}
window.reactionClick=async()=>{
 if(!roundBusy)return;
 if(!reactArmed){$('reactionPad').textContent='Слишком рано!';await finishGame(false);return;}
 const ms=Math.round(performance.now()-reactStart);
 $('reactionPad').textContent=ms+' мс';
 await finishGame(ms<=750,ms);gameMessage(ms+' мс — '+(ms<=750?'отлично!':'попробуй быстрее!'));
};
async function syncTime(){
 try{const epoch=Math.floor(Date.now()/1000),offset=-new Date().getTimezoneOffset();
 const r=await fetch('/clock?epoch='+epoch+'&offset='+offset,{method:'POST'});
 if(!r.ok)throw Error(await r.text());await refresh();toast('Часы синхронизированы ♡');
 }catch(e){toast('Ошибка часов: '+e.message)}
}
refresh();syncTime();setInterval(()=>{if(!busy)refresh()},2000);
</script></body></html>)FENIK";
void action(){
 String a=server.arg("do");
 if(a=="feed"){feedCount++;if(isResting()){nightCare++;say("Ммм... вкусно... zZ ♡");}else say("Ням! Спасибо за вкусняшку ♡");addBond(2);actionFace=10;actionFaceUntil=millis()+6500UL;food=clampStat(food+20);mood=clampStat(mood+3);xp+=5;}
 else if(a=="play"){if(isResting())wakePet();say("Ура, играем вместе! ✨");addBond(3);actionFace=11;actionFaceUntil=millis()+6500UL;playCount++;fun=clampStat(fun+20);food=clampStat(food-7);mood=clampStat(mood+8);xp+=8;}
 else if(a=="pet"){petCount++;if(isResting()){nightPets++;say("Мрр... ещё немножко... zZ ♡");}else say("Мурр~ Ещё погладь! ♡");addBond(4);actionFace=11;actionFaceUntil=millis()+6000UL;mood=clampStat(mood+6);xp+=3;}
 else if(a=="surprise"){say("Ого! Какой сюрприз! 🎁");gifts++;addBond(2);actionFace=10;actionFaceUntil=millis()+6000UL;fun=clampStat(fun+7);xp+=3;}
 else if(a=="sleep"){careCount++;if(isResting())wakePet();else{sleeping=true;dayNap=false;nightRest=false;say("Спокойной ночи... zZ");}xp+=2;addBond(1);}
 else if(a=="wash"){careCount++;if(isResting()){nightCare++;say("Мрр... спасибо за заботу... zZ");}else say("Теперь я чистенький! 🫧");addBond(2);actionFace=1;actionFaceUntil=millis()+6500UL;health=clampStat(health+10);mood=clampStat(mood+5);xp+=4;}
 else{server.send(400,"text/plain","Unknown action");return;}
 debugFace=-1;actionsCount++;if(!isResting())lastInteraction=millis();lastAction=a;saveState();dirty=true;blink=false;lastBlink=millis();
 server.send(200,"application/json",json());
 if(!isResting()||shownFace!=faceMode())updateScreen(); // no flicker from quiet care
}
void randomEvent();
void giftAction(){
 if(server.arg("do")!="give"||!server.hasArg("id")){server.send(400,"text/plain","Invalid gift");return;}
 int id=server.arg("id").toInt();
 if(id<0||id>3||inventory[id]==0){server.send(409,"text/plain","Предмет закончился");return;}
 inventory[id]--;given++;gifts++;addBond(id==3?8:5);addXp(5);
 if(id==0){food=clampStat(food+8);say("Ммм, печенье! Спасибо ♡");}
 if(id==1){fun=clampStat(fun+10);say("Клубочек! Давай играть! ♡");}
 if(id==2){mood=clampStat(mood+10);say("Бантик мне очень идёт? ♡");}
 if(id==3){mood=clampStat(mood+15);say("Это наша звёздочка дружбы! ✨");}
 actionFace=11;actionFaceUntil=millis()+6000UL;if(isResting()){nightCare++;say("Ой... это мне?.. ♡");}else lastInteraction=millis();lastAction="gift";
 saveState();dirty=true;server.send(200,"application/json",json());updateScreen();
}
void worldAction(){
 String cmd=server.arg("do");
 if(cmd=="hour"){int h=server.arg("id").toInt();if(h<0||h>23){server.send(400,"text/plain","Invalid hour");return;}clockHour=h;clockAnchor=millis();hasRealTime=false;nightRest=false;say(String("Теперь ")+dayPart()+" ♡");}
 else if(cmd=="gift"){rewardItem();say("В инвентаре появился подарок! ♡");}
 else if(cmd=="event"){randomEvent();}
 else{server.send(400,"text/plain","Unknown world command");return;}
 lastAction="world:"+cmd;saveState();dirty=true;server.send(200,"application/json",json());updateScreen();
}
void memoryAction(){
 if(server.arg("do")!="recall"){server.send(400,"text/plain","Unknown memory command");return;}
 say(memoryLine(),24000UL);
 lastAction="memory:recall";
 server.send(200,"application/json",json());
}
void debugAction(){
 String cmd=server.arg("do");
 if(cmd=="face"){
  if(!server.hasArg("id")){server.send(400,"text/plain","Missing id");return;}
  int id=server.arg("id").toInt();
  if(id<0||id>13){server.send(400,"text/plain","Invalid face id");return;}
  debugFace=id;debugUntil=millis()+20000UL;
 }else if(cmd=="auto"){debugFace=-1;actionFace=255;blink=false;}
 else if(cmd=="blink"){debugFace=2;debugUntil=millis()+4500UL;blink=false;lastBlink=millis();}
 else if(cmd=="tick"){
  if(isResting()){health=clampStat(health+1);food=clampStat(food-1);}
  else{food=clampStat(food-1);fun=clampStat(fun-1);if(food<30||fun<30)mood=clampStat(mood-1);}
  if(food<15)health=clampStat(health-1);
  saveState();
 }else if(cmd=="set"){
  String key=server.arg("key"),value=server.arg("value");
  if(value.length()==0||value.length()>8){server.send(400,"text/plain","Invalid value");return;}
  for(unsigned int i=0;i<value.length();i++)if(value[i]<'0'||value[i]>'9'){server.send(400,"text/plain","Numbers only");return;}
  unsigned long n=strtoul(value.c_str(),nullptr,10);
  if(key=="food"&&n<=100)food=n;
  else if(key=="mood"&&n<=100)mood=n;
  else if(key=="fun"&&n<=100)fun=n;
  else if(key=="health"&&n<=100)health=n;
  else if(key=="sleepiness"&&n<=100)sleepiness=n;
  else if(key=="bond"&&n<=100)bond=n;
  else if(key=="xp"&&n<=1000000UL)xp=n;
  else if(key=="item0"&&n<=99)inventory[0]=n;
  else if(key=="item1"&&n<=99)inventory[1]=n;
  else if(key=="item2"&&n<=99)inventory[2]=n;
  else if(key=="item3"&&n<=99)inventory[3]=n;
  else {server.send(400,"text/plain","Unknown key or out of range");return;}
  saveState();
 }else if(cmd=="boost"){food=mood=fun=health=100;saveState();}
 else if(cmd=="reset"){food=75;mood=65;fun=45;health=85;xp=32;bond=20;gifts=eventsSeen=gamesWon=0;gamesLost=bestReactionMs=quickWins=guessWins=given=0;gameRound=0;actionsCount=0;feedCount=playCount=petCount=careCount=totalMinutes=0;currentEvent=0;gameTarget=-1;sleeping=false;debugFace=-1;actionFace=255;say("Начнём сначала! ♡");saveState();}
 else if(cmd!="refresh"&&cmd!="full"&&cmd!="partial"){server.send(400,"text/plain","Unknown debug command");return;}
 if((cmd=="full"||cmd=="refresh")&&lastManualFull!=0&&(uint32_t)(millis()-lastManualFull)<12000UL){server.send(429,"text/plain","Подожди 12 секунд между полными обновлениями");return;}
 actionsCount++;lastAction="debug:"+cmd;dirty=true;
 server.send(200,"application/json",json());
 if(cmd=="refresh"||cmd=="full"){
  if((uint32_t)(millis()-lastManualFull)<12000UL && lastManualFull!=0){return;}
  lastManualFull=millis();drawScreen();
 }else if(cmd=="partial"){
  partialFace();partialRightPanel();panelPartialCount++;partialCount++;refreshCount++;snapshot();
 }else updateScreen();
}
void quizAction(){
 if(server.arg("do")!="start"){server.send(400,"text/plain","Invalid quiz request");return;}
 int id=server.arg("id").toInt();
 if(id!=1&&id!=5&&id!=9&&id!=10&&id!=11){server.send(400,"text/plain","Invalid emotion");return;}
 if(isResting())wakePet();
 quizFace=(uint8_t)id;quizActive=true;dirty=true;
 updateScreen(); // Wait for actual e-ink refresh before replying to browser
 server.send(200,"application/json","{\"ok\":true}");
}
void gameAction(){
 if(server.arg("do")!="finish"){server.send(400,"text/plain","Unknown game command");return;}
 String type=server.arg("type");
 if(type!="shells"&&type!="emotion"&&type!="reaction"){server.send(400,"text/plain","Invalid game type");return;}
 if(isResting())wakePet();
 bool win=server.arg("win")=="1";
 if(type=="emotion" && !quizActive){server.send(409,"text/plain","Quiz not started");return;}
 if(type=="emotion")quizActive=false;
 uint32_t ms=(uint32_t)server.arg("ms").toInt();
 gameRound++;playCount++;actionsCount++;
 if(win){
  gamesWon++;if(type=="emotion")guessWins++;if(type=="reaction")quickWins++;
  if(type=="reaction"&&ms>=80&&ms<=10000&&(bestReactionMs==0||ms<bestReactionMs))bestReactionMs=ms;
  addBond(5);addXp(type=="reaction"?18:12);
  fun=clampStat(fun+8);mood=clampStat(mood+5);
  if(gamesWon%2==0)rewardItem();
  actionFace=11;actionFaceUntil=millis()+6000UL;say("Ура! Мы победили! ♡");
 }else{
  gamesLost++;fun=clampStat(fun+2);
  actionFace=5;actionFaceUntil=millis()+4000UL;say("Не грусти, давай ещё раз! ♡");
 }
 lastAction="game:"+type;lastInteraction=millis();
 saveState();dirty=true;server.send(200,"application/json",json());updateScreen();
}
void emotionImage(){
 int id=server.arg("id").toInt();
 const uint8_t* ptr=nullptr;size_t n=0;
 switch(id){
 case 1:ptr=emotionPng1;n=emotionPng1Len;break;
 case 5:ptr=emotionPng5;n=emotionPng5Len;break;
 case 10:ptr=emotionPng10;n=emotionPng10Len;break;
 case 9:ptr=emotionPng9;n=emotionPng9Len;break;
 case 11:ptr=emotionPng11;n=emotionPng11Len;break;
 default:server.send(404,"text/plain","Emotion not found");return;
 }
 server.sendHeader("Cache-Control","public, max-age=86400");
 server.send_P(200,"image/png",(const char*)ptr,n);
}
void clockAction(){
 if(!server.hasArg("epoch")||!server.hasArg("offset")){server.send(400,"text/plain","Missing time");return;}
 uint32_t e=strtoul(server.arg("epoch").c_str(),nullptr,10);
 int off=server.arg("offset").toInt();
 if(e<1700000000UL||off< -840||off>840){server.send(400,"text/plain","Invalid time");return;}
 syncedEpoch=e;tzOffsetMinutes=off;syncedAt=millis();hasRealTime=true;
 updateSleepState();
 clockHour=hourNow();clockAnchor=millis();
 server.send(200,"application/json",json());
}
void randomEvent(){
 if(isResting())return;
 currentEvent=random(1,6);eventsSeen++;
 say(EVENT_LINES[currentEvent],25000UL);
 if(currentEvent==1){gifts++;rewardItem();addXp(4);actionFace=10;}
 else if(currentEvent==2){actionFace=11;}
 else if(currentEvent==3){actionFace=3;}
 else if(currentEvent==4){fun=clampStat(fun+3);actionFace=1;}
 else if(currentEvent==5){food=clampStat(food-3);actionFace=7;}
 actionFaceUntil=millis()+6500UL;
 lastAction="event";dirty=true;saveState();
}
void tick(){
 totalMinutes++;
 if(isResting())sleepiness=(uint8_t)max(0,(int)sleepiness-2);else sleepiness=(uint8_t)min(100,(int)sleepiness+1);
 uint8_t h=hourNow();
 if(!isResting() && (h>=22||h<6) && totalMinutes%5==0){mood=clampStat(mood-1);if(food>30)actionFace=3;}

 if(isResting()){health=clampStat(health+1);food=clampStat(food-1);}
 else{food=clampStat(food-1);fun=clampStat(fun-1);if(food<30||fun<30)mood=clampStat(mood-1);}
 if(food<15)health=clampStat(health-1);
 saveState();dirty=true;
}
void screenAction(){
 String cmd=server.arg("do");int n=server.arg("id").toInt();
 if(cmd=="mode"&&n>=0&&n<=2)refreshMode=n;
 else if(cmd=="interval"&&n>=15&&n<=180)minRefreshMs=(uint32_t)n*1000UL;
 else{server.send(400,"text/plain","Invalid screen option");return;}
 saveState();server.send(200,"application/json",json());
}
void nightAction(){
 String cmd=server.arg("do");
 if(cmd=="toggle"){nightModeEnabled=!nightModeEnabled;nightOverride=false;saveState();}
 else if(cmd=="wake"){if(hasRealTime&&isNightHour())nightWakes++;wakePet();saveState();}
 else{server.send(400,"text/plain","Unknown night command");return;}
 updateSleepState();dirty=true;
 server.send(200,"application/json",json());updateScreen();
}
void setup(){
 Serial.begin(115200);
 prefs.begin("fenik",false);
 food=clampStat(prefs.getInt("food",75));mood=clampStat(prefs.getInt("mood",65));
 fun=clampStat(prefs.getInt("fun",45));health=clampStat(prefs.getInt("health",85));
 xp=prefs.getUInt("xp",32);sleeping=prefs.getBool("sleep",false);
 nightModeEnabled=prefs.getBool("nightAuto",true);
 nightPets=prefs.getUInt("nPets",0);nightCare=prefs.getUInt("nCare",0);nightWakes=prefs.getUInt("nWakes",0);
 napCount=prefs.getUInt("naps",0);dreamCount=prefs.getUInt("dreams",0);
 refreshMode=prefs.getUChar("refMode",1);minRefreshMs=constrain(prefs.getUInt("refMs",60000UL),15000UL,180000UL);
 bond=prefs.getUInt("bond",20);gifts=prefs.getUInt("gifts",0);
 eventsSeen=prefs.getUInt("events",0);gamesWon=prefs.getUInt("wins",0);
 actionsCount=prefs.getUInt("acts",0);
 feedCount=prefs.getUInt("fed",0);playCount=prefs.getUInt("played",0);
 petCount=prefs.getUInt("petted",0);careCount=prefs.getUInt("cared",0);
 totalMinutes=prefs.getUInt("minutes",0);
 clockHour=prefs.getUChar("hour",12);clockAnchor=millis();
 given=prefs.getUInt("given",0);quickWins=prefs.getUInt("quick",0);guessWins=prefs.getUInt("guess",0);
 gamesLost=prefs.getUInt("lost",0);bestReactionMs=prefs.getUInt("bestms",0);
 for(int i=0;i<4;i++)inventory[i]=prefs.getUInt((String("item")+i).c_str(),i<2?1:0);
 if(actionsCount>0)say("С возвращением! Я помню нашу дружбу ♡",25000UL);
 randomSeed(esp_random());lastEvent=millis();lastInteraction=millis();
 WiFi.mode(WIFI_AP);WiFi.softAP(AP_NAME,AP_PASS);
 SPI.begin(12,-1,11,10);display.init(115200);drawScreen();
 server.on("/",HTTP_GET,[](){server.send_P(200,"text/html; charset=utf-8",HTML);});
 server.on("/update", HTTP_GET, [](){
  server.send(200,"text/html; charset=utf-8", R"OTA(<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Fenik Pet OTA</title><body style="font:18px system-ui;max-width:500px;margin:30px auto;padding:16px"><h2>Fenik Pet — OTA</h2><p>Выбери только firmware.bin. Не отключай питание.</p><form method="POST" action="/update" enctype="multipart/form-data"><input type="file" name="firmware" accept=".bin" required><button type="submit">Обновить</button></form><p><a href="/">Вернуться к Фенику</a></p></body></html>)OTA");
 });
 server.on("/update", HTTP_POST, [](){
  bool ok=!Update.hasError() && Update.isFinished();
  server.send(ok?200:500,"text/plain; charset=utf-8",ok?"Firmware accepted. Rebooting...":"OTA failed. Check firmware and partition layout.");
  if(ok){delay(800);ESP.restart();}
 }, [](){
  HTTPUpload &u=server.upload();
  if(u.status==UPLOAD_FILE_START){
   if(!u.filename.endsWith(".bin") || !Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH)) Update.abort();
  }else if(u.status==UPLOAD_FILE_WRITE){
   if(!Update.hasError() && Update.write(u.buf,u.currentSize)!=u.currentSize)Update.abort();
  }else if(u.status==UPLOAD_FILE_END){
   if(!Update.hasError() && !Update.end(true))Update.abort();
  }else if(u.status==UPLOAD_FILE_ABORTED)Update.abort();
 });

 server.on("/portrait",HTTP_GET,[](){
  int face=server.hasArg("face")?server.arg("face").toInt():0;
  const uint8_t* data=emotionPng1;
  size_t length=emotionPng1Len;
  switch(face){
    case 12: data=fenikDrowsyPng;length=fenikDrowsyPngLen;break;
    case 13: data=fenikRubEyesPng;length=fenikRubEyesPngLen;break;
    case 5: case 6: case 7: case 8: data=emotionPng5;length=emotionPng5Len;break;
    case 9: data=emotionPng9;length=emotionPng9Len;break;
    case 10: data=emotionPng10;length=emotionPng10Len;break;
    case 11: data=emotionPng11;length=emotionPng11Len;break;
    default: break;
  }
  server.sendHeader("Cache-Control","public, max-age=300");
  server.send_P(200,"image/png",reinterpret_cast<PGM_P>(data),length);
 });
 server.on("/status",HTTP_GET,[](){server.send(200,"application/json",json());});
 server.on("/action",HTTP_POST,action);
 server.on("/debug",HTTP_POST,debugAction);
 server.on("/game",HTTP_POST,gameAction);
 server.on("/quiz",HTTP_POST,quizAction);
 server.on("/clock",HTTP_POST,clockAction);
 server.on("/night",HTTP_POST,nightAction);
 server.on("/screen",HTTP_POST,screenAction);
 server.on("/emotion",HTTP_GET,emotionImage);
 server.on("/memory",HTTP_POST,memoryAction);
 server.on("/gift",HTTP_POST,giftAction);
 server.on("/world",HTTP_POST,worldAction);
 server.begin();lastTick=millis();
 Serial.println("Fenik Pet v1.4 Living Companion ready: http://192.168.4.1");
}
void loop(){
 server.handleClient();
 uint32_t now=millis();
 if((uint32_t)(now-lastTick)>=TICK_MS){lastTick+=TICK_MS;tick();}
 // Auto wake on morning, auto sleep at night; reset temporary wake at 07:00.
 updateSleepState();
 if(!isResting() && refreshMode==0 && !blink && (uint32_t)(now-lastBlink)>=BLINK_INTERVAL_MS){blink=true;blinkStart=now;lastBlink=now;dirty=true;}
 if(blink&&(uint32_t)(now-blinkStart)>=BLINK_DURATION_MS){blink=false;dirty=true;}
 if(debugFace>=0 && (int32_t)(now-debugUntil)>=0){debugFace=-1;dirty=true;}
 if(actionFace!=255 && (int32_t)(now-actionFaceUntil)>=0){actionFace=255;dirty=true;}
 
 if((uint32_t)(now-lastEvent)>180000UL){lastEvent=now;if(!isResting())randomEvent();}
 if((uint32_t)(now-dialogueUntil)<0 && dialogueUntil!=0){} // dialogue remains visible until replaced
 // During night, accumulate stats silently; update only on entry, exit or explicit action.
 if(isResting()){
  if(shownFace!=faceMode())updateScreen();
 }else if(shownFace!=faceMode())updateScreen();
 else if(refreshMode!=2 && dirty && (uint32_t)(now-lastScreen)>=minRefreshMs)updateScreen();
 delay(2);
}
