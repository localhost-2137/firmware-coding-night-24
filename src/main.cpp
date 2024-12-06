#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <Wire.h>

#define DOOR_PIN 7
#define NAME_PREFIX "SRAM-DOR"

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

void setup() {
  Serial.begin(115200);
  pinMode(DOOR_PIN, INPUT_PULLUP);

  delay(5000);

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
  } else {
    // initBt(generatedDeviceName);
  }

  while (!res && !wm.process()) {
    delay(5);
  }

  // wifiConnected = true;
  // if (!res)
    // deinitBt(true);
  configTime(3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  // initWs();
}

void loop() {
  Serial.println(digitalRead(DOOR_PIN));
  delay(100);
}