#include <WiFi.h>

#ifdef ESP32
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_bt.h"
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP Information");

#ifdef ESP32
  Serial.println("Board: ESP32");

  // WiFi MAC Address
  Serial.print("WiFi MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Bluetooth MAC Address
  uint8_t btMac[6];
  esp_read_mac(btMac, ESP_MAC_BT);

  Serial.print("Bluetooth MAC Address: ");
  for (int i = 0; i < 6; i++) {
    if (btMac[i] < 16) Serial.print("0");
    Serial.print(btMac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  Serial.println("WiFi Supported: YES");
  Serial.println("Bluetooth Supported: YES");

#elif defined(ESP8266)

  Serial.println("Board: ESP8266");

  Serial.print("WiFi MAC Address: ");
  Serial.println(WiFi.macAddress());

  Serial.println("WiFi Supported: YES");
  Serial.println("Bluetooth Supported: NO");

#else

  Serial.println("Unknown ESP Board");

#endif
}

void loop() {
}