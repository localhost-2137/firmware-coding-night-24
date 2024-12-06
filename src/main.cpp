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

bool displayWarning = false;
int lastDoorState = -1;

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
  // scanCard(cardId);
  lastCardId = cardId;

  mfrc522.PICC_HaltA();
  lastCardReadTime = millis();
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

  configTime(3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  // initWs();
}

unsigned long lastBorderChange = 0;
bool lastBorder = false;

void loop() {
  if (displayWarning) {
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

  int currDoor = digitalRead(DOOR_PIN);
  if (currDoor != lastDoorState) {
    tft.setTextSize(4);
    tft.setCursor(10, 10);
    tft.setTextColor(BLACK);
    tft.printf("21:37");

    tft.setCursor(10, 10);
    tft.setTextColor(WHITE);
    tft.printf("21:37");


    tft.drawCircle(30, 80, 13, WHITE);
    tft.drawRoundRect(10, 100, 40, 20, 5, WHITE);
    tft.setTextSize(6);
    
    tft.setCursor(55, 75);
    tft.setTextColor(BLACK);
    tft.printf("%d", lastDoorState);

    tft.setCursor(55, 75);
    tft.setTextColor(WHITE);
    tft.printf("%d", currDoor);

    lastDoorState = currDoor;
    displayWarning = currDoor == 1;
  }

  rfidLoop();
}