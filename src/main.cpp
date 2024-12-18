#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <Wire.h>
#include <SPI.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <HTTPClient.h>
#include "mars.hpp"

#define TIME_URL "http://10.42.0.1:3000/time"
#define WS_HOST "10.42.0.1"
#define WS_PORT 3000
#define WS_PATH "/ws"

#define DOOR_PIN 7
#define RFID_SCK 8
#define RFID_MISO 9
#define RFID_MOSI 10
#define RFID_CS 11
#define NAME_PREFIX "SRAM-DOR"

#define BLACK 0xFFFF       ///<   0,   0,   0
#define NAVY 0xFFF0        ///<   0,   0, 123
#define DARKGREEN 0xFC1F   ///<   0, 125,   0
#define DARKCYAN 0xFC10    ///< 255, 125, 123
#define MAROON 0x87FF      ///< 123,   0,   0
#define PURPLE 0x87F0      ///< 123,   0, 123
#define OLIVE 0x841F       ///< 123, 125,   0
#define LIGHTGREY 0x39E7   ///< 198, 195, 198
#define DARKGREY 0x8410    ///< 123, 125, 123
#define BLUE 0xFFE0        ///<   0,   0, 255
#define GREEN 0xF81F       ///<   0, 255,   0
#define CYAN 0xF800        ///<   0, 255, 255
#define RED 0x07FF         ///< 255,   0,   0
#define MAGENTA 0x07E0     ///< 255,   0, 255
#define YELLOW 0x001F      ///< 255, 255,   0
#define WHITE 0x0000       ///< 255, 255, 255
#define ORANGE 0x02DF      ///< 255, 165,   0
#define GREENYELLOW 0x501A ///< 173, 255,  41
#define PINK 0x03E7        ///< 255, 130, 198

MFRC522DriverPinSimple ss_pin(RFID_CS);
MFRC522DriverSPI driver{ss_pin};
MFRC522 mfrc522{driver};

Adafruit_ILI9341 tft = Adafruit_ILI9341(1, 3, 4, 5, 2, 6);

WebSocketsClient webSocket;

unsigned long long epochTimeBase = 1733519928;
bool systemAlert = false;
unsigned long alertShowTime = 0;
int lastDoorState = -1;
int lastIndoorCount = -1;
std::vector<unsigned long> insideBase;

void getTimeFromEpoch(unsigned long epochTime, int &hour, int &minute) {
  const unsigned long SECONDS_PER_DAY = 86400;
  epochTime += 1 * 3600;

  unsigned long timeOfDay = epochTime % SECONDS_PER_DAY;
  hour = timeOfDay / 3600;
  minute = (timeOfDay % 3600) / 60;
}

unsigned long getEspId() {
  uint64_t efuse = ESP.getEfuseMac();
  efuse = (~efuse) + (efuse << 18);
  efuse = efuse ^ (efuse >> 31);
  efuse = efuse * 21;
  efuse = efuse ^ (efuse >> 11);
  efuse = efuse + (efuse << 6);
  efuse = efuse ^ (efuse >> 22);

  return (unsigned long)(efuse & 0x000000007FFFFFFF);
}

unsigned long lastCardReadTime = 0;
unsigned long lastCardId = 0;
void rfidLoop() {
  if (millis() - lastCardReadTime < 500)
    return;
  if (!mfrc522.PICC_IsNewCardPresent())
    return;
  if (!mfrc522.PICC_ReadCardSerial())
    return;

  unsigned long cardId =
      mfrc522.uid.uidByte[0] + (mfrc522.uid.uidByte[1] << 8) +
      (mfrc522.uid.uidByte[2] << 16) + (mfrc522.uid.uidByte[3] << 24);
  if (lastCardId == cardId && millis() - lastCardReadTime < 2500)
    return; // if same as last card (in 2.5s)

  Serial.printf("Scanned card ID: %lu\n", cardId);

  if(insideBase.size() > 0) {
    if(std::find(insideBase.begin(), insideBase.end(), cardId) != insideBase.end()) {
      // found
      insideBase.erase(std::find(insideBase.begin(), insideBase.end(), cardId));
    } else {
      insideBase.push_back(cardId);
    }

    tft.drawRect(0, 0, 320, 240, GREENYELLOW);
    tft.drawRect(1, 1, 318, 238, GREENYELLOW);
    tft.drawRect(2, 2, 316, 236, GREENYELLOW);
  } else {
    tft.drawRect(0, 0, 320, 240, RED);
    tft.drawRect(1, 1, 318, 238, RED);
    tft.drawRect(2, 2, 316, 236, RED);
  }
  
  delay(250);

  tft.drawRect(0, 0, 320, 240, BLACK);
  tft.drawRect(1, 1, 318, 238, BLACK);
  tft.drawRect(2, 2, 316, 236, BLACK);

  // scanCard(cardId);
  lastCardId = cardId;

  mfrc522.PICC_HaltA();
  lastCardReadTime = millis();
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  Serial.printf("type: %d\n", type);
  if (type == WStype_TEXT) {
    JsonDocument doc;
    deserializeJson(doc, payload);

    String typ = doc["type"];
    Serial.printf("type: %s\n", typ.c_str());

    if (typ == "alert") {
      String msg = doc["value"];
      systemAlert = true;
      alertShowTime = millis();
    } else if (typ == "end_alert") {
      systemAlert = false;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(DOOR_PIN, INPUT_PULLUP);
  SPI.begin(RFID_SCK, RFID_MISO, RFID_MOSI);
  mfrc522.PCD_Init();

  tft.begin(40000000);
  tft.setRotation(3);
  tft.fillScreen(BLACK);

  Serial.println("dsadsa");
  WiFi.mode(WIFI_STA);
  WiFiManager wm;

  char generatedDeviceName[100];
  snprintf(generatedDeviceName, 100, "%s-%lx", NAME_PREFIX, getEspId());

  wm.setConfigPortalTimeout(300);
  wm.setConfigPortalBlocking(false);
  wm.setConnectRetries(3);
  wm.setConnectTimeout(3);

  bool res = wm.autoConnect(generatedDeviceName, "SRAM_NA_TO");
  if (res) {
    Serial.printf("Connected!\n");
  }

  while (!res && !wm.process()) {
    delay(5);
  }

  drawMars(tft);
  WiFiClient client;
  HTTPClient http;
    
  http.begin(client, TIME_URL);
  int statusCode = http.GET();
  if (statusCode > 0) {
    String epochStr = http.getString();
    Serial.printf("get_string: %s\n", epochStr.c_str());
    char* endPtr;
    unsigned long long epoch = strtoull(epochStr.c_str(), &endPtr, 10);
    epochTimeBase = epoch - millis();
    Serial.printf("%llu | %llu\n", epoch, epochTimeBase);    
  }

  for(int i = 0; i < 63; i++) {
    insideBase.push_back(i);
  }

  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(1500);
  // initWs();
}

unsigned long lastBorderChange = 0;
unsigned long lastTimeChange = 0;
unsigned long lastStatsDropTime = 0;
unsigned long lastDoorOpen = 0;
unsigned long lastUpdate = 0;
unsigned long lastDead = 0;
unsigned long lastDeadAlert = 0;
int lastO2 = -1;
int currO2 = 100;
bool lastBorder = false;

void sendWsUpdate() {
  JsonDocument doc;
  doc["type"] = "base_data";
  doc["value"]["o2"] = currO2;
  doc["value"]["inside"] = insideBase.size();
  doc["value"]["open"] = lastDoorState == 1;

  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(output);
}

void sendDoorOpenAlert() {
  JsonDocument doc;
  doc["type"] = "alert";
  doc["value"] = "Drzwi otwarte przez 10s.";

  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(output);
}

void loop() {
  webSocket.loop();
  int currDoor = digitalRead(DOOR_PIN);

  if (systemAlert) {
    if (millis() - lastBorderChange > 350) {
      lastBorder = !lastBorder;
      if (lastBorder) {
        tft.drawRect(0, 0, 320, 240, RED);
        tft.drawRect(1, 1, 318, 238, RED);
        tft.drawRect(2, 2, 316, 236, RED);
      } else {
        tft.drawRect(0, 0, 320, 240, BLACK);
        tft.drawRect(1, 1, 318, 238, BLACK);
        tft.drawRect(2, 2, 316, 236, BLACK);
      }

      lastBorderChange = millis();
    }
  } else if (lastBorder) {
    lastBorder = false;
    tft.drawRect(0, 0, 320, 240, BLACK);
    tft.drawRect(1, 1, 318, 238, BLACK);
    tft.drawRect(2, 2, 316, 236, BLACK);
  }

  if (lastTimeChange == 0 || millis() - lastTimeChange > 60000) {
    int hour, minute;

    getTimeFromEpoch((epochTimeBase + millis()) / 1000 - 60, hour, minute);
    tft.setTextSize(4);
    tft.setCursor(10, 10);
    tft.setTextColor(BLACK);
    tft.printf("%02d:%02d", hour, minute);

    getTimeFromEpoch((epochTimeBase + millis()) / 1000, hour, minute);
    tft.setCursor(10, 10);
    tft.setTextColor(WHITE);
    tft.printf("%02d:%02d", hour, minute);

    lastTimeChange = millis();
  }

  if (lastIndoorCount != insideBase.size()) {
    tft.drawCircle(247, 18, 6, WHITE);
    tft.drawRoundRect(235, 28, 24, 10, 5, WHITE);
    
    tft.setCursor(270, 10);
    tft.setTextColor(BLACK);
    tft.printf("%d", lastIndoorCount);

    tft.setCursor(270, 10);
    tft.setTextColor(WHITE);
    tft.printf("%d", insideBase.size());
    
    lastIndoorCount = insideBase.size();
    sendWsUpdate();
  }

  if (lastO2 != currO2) {
    tft.setCursor(10, 50);
    tft.setTextColor(BLACK);
    tft.printf("o2:%d%%", lastO2);

    tft.setCursor(10, 50);
    tft.setTextColor(WHITE);
    tft.printf("o2:%d%%", currO2);
    
    lastO2 = currO2;
    sendWsUpdate();
  }

  if (currDoor == 1) {
    if (millis() - lastStatsDropTime >= 500) {
      currO2 -= 1;
      if (currO2 < 0) currO2 = 0;

      lastStatsDropTime = millis();
    }
  } else {
    if (millis() - lastStatsDropTime >= 2000) {
      currO2 += 1;
      if (currO2 > 100) currO2 = 100;

      lastStatsDropTime = millis();
    }
  }

  if (currDoor != lastDoorState) {
    tft.drawRect(10, 90, 24, 32, WHITE);
    tft.drawCircle(30, 104, 4, WHITE);

    if (currDoor == 1) {
      tft.setCursor(40, 94);
      tft.setTextColor(BLACK);
      tft.printf("ZAMKNIETE", lastO2);

      tft.setCursor(40, 94);
      tft.setTextColor(RED);
      tft.printf("OTWARTE", currO2);
    } else {
      tft.setCursor(40, 94);
      tft.setTextColor(BLACK);
      tft.printf("OTWARTE", lastO2);

      tft.setCursor(40, 94);
      tft.setTextColor(LIGHTGREY);
      tft.printf("ZAMKNIETE", currO2);
    }

    lastDoorState = currDoor;
    if (currDoor == 1) {
      lastDoorOpen = millis();
    }
    sendWsUpdate();
  }

  if (lastDoorState == 1 && millis() - lastDoorOpen >= 10000) {
    sendDoorOpenAlert();
    lastDoorOpen = millis();
    systemAlert = true;
    alertShowTime = millis();
  }

  if (systemAlert && millis() - alertShowTime > 5000) {
    systemAlert = false;
  }

  if (millis() - lastUpdate > 1000) {
    sendWsUpdate();
    lastUpdate = millis();
  }

  if(currO2 <= 0 && millis() - lastDead >= 250) {
    if (insideBase.size() > 0) {
      int randomIndex = random(insideBase.size());
      insideBase.erase(insideBase.begin() + randomIndex);
    }
    
    lastDead = millis();

    if(millis() - lastDeadAlert >= 5000) {
      systemAlert = true;
      alertShowTime = millis();
      sendDoorOpenAlert();
    }
  }

  rfidLoop();
}