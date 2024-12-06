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
// #include <MFRC522Debug.h>

#define DOOR_PIN 7
#define RFID_SCK 8
#define RFID_MISO 9
#define RFID_MOSI 10
#define RFID_CS 11
#define NAME_PREFIX "SRAM-DOR"

MFRC522DriverPinSimple ss_pin(RFID_CS);
MFRC522DriverSPI driver{ss_pin};
MFRC522 mfrc522{driver};

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

void loop() {
  int currDoor = digitalRead(DOOR_PIN);
  if (currDoor != lastDoorState) { 
    lastDoorState = currDoor;
    Serial.println(currDoor);
  }
  
  rfidLoop();
}