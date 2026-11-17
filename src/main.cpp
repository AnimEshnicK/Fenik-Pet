
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <GxEPD2_BW.h>

#define EPD_CS   10
#define EPD_DC    9
#define EPD_RST   8
#define EPD_BUSY  7
#define EPD_MOSI 11
#define EPD_SCK  12

GxEPD2_BW<GxEPD2_213_GDEY0213B74,
          GxEPD2_213_GDEY0213B74::HEIGHT>
display(GxEPD2_213_GDEY0213B74(
  EPD_CS, EPD_DC, EPD_RST, EPD_BUSY
));

WebServer server(80);
String mood = "happy";
bool redraw = true;
unsigned long lastAction = 0;
unsigned long lastRefresh = 0;

void drawAbrams() {
  display.setRotation(1);
  display.setFullWindow();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Horns
    display.fillTriangle(55, 35, 40, 5, 84, 26, GxEPD_BLACK);
    display.fillTriangle(165, 26, 207, 5, 192, 38, GxEPD_BLACK);

    // Face
    display.fillRoundRect(51, 17, 148, 94, 36, GxEPD_BLACK);
    display.fillRoundRect(58, 23, 134, 82, 30, GxEPD_WHITE);

    // Glasses
    display.fillRoundRect(66, 37, 55, 27, 8, GxEPD_BLACK);
    display.fillRoundRect(129, 37, 55, 27, 8, GxEPD_BLACK);
    display.fillRoundRect(70, 41, 47, 19, 6, GxEPD_WHITE);
    display.fillRoundRect(133, 41, 47, 19, 6, GxEPD_WHITE);

    if (mood == "sleep") {
      display.drawLine(77, 51, 109, 51, GxEPD_BLACK);
      display.drawLine(141, 51, 173, 51, GxEPD_BLACK);
    } else {
      display.fillCircle(93, 48, 5, GxEPD_BLACK);
      display.fillCircle(157, 48, 5, GxEPD_BLACK);
    }

    if (mood == "angry") {
      display.drawLine(70, 32, 112, 43, GxEPD_BLACK);
      display.drawLine(138, 43, 180, 32, GxEPD_BLACK);
    }

    // Big lips
    display.fillRoundRect(97, 76, 56, 16, 8, GxEPD_BLACK);
    display.drawLine(100, 84, 150, 84, GxEPD_WHITE);

    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(1);
    display.setCursor(5, 115);
    display.print("ABRAMS PET");

  } while (display.nextPage());

  lastRefresh = millis();
  redraw = false;
}

void setup() {
  Serial.begin(115200);

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.init(115200, true, 2, false);

  lastAction = millis();
  drawAbrams();

  WiFi.mode(WIFI_AP);
  WiFi.softAP("Abrams-Pet", "AbramsPet2026");

  server.on("/", []() {
    server.send(200, "text/html",
      "<meta name='viewport' content='width=device-width'>"
      "<h2>Abrams Pet</h2>"
      "<p><a href='/mood?set=happy'>Happy</a></p>"
      "<p><a href='/mood?set=angry'>Angry</a></p>"
      "<p><a href='/mood?set=sleep'>Sleep</a></p>"
      "<p><a href='/mood?set=strong'>Strong</a></p>");
  });

  server.on("/mood", []() {
    String value = server.arg("set");

    if (value == "happy" || value == "angry" ||
        value == "sleep" || value == "strong") {
      mood = value;
      redraw = true;
      lastAction = millis();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid mood");
    }
  });

  server.begin();
}

void loop() {
  server.handleClient();

  if (millis() - lastAction > 900000UL && mood != "sleep") {
    mood = "sleep";
    redraw = true;
  }

  if (redraw && millis() - lastRefresh > 3000UL) {
    drawAbrams();
  }

  delay(10);
}
