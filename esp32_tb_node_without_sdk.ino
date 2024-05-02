////////////////////////////////////////
// ESP32 TB NODE WITHOUT SDK
// -------------------------------------
// BY saknarak@gmail.com
////////////////////////////////////////


////////////////////////////////////////
// INCLUDES
////////////////////////////////////////
#include <WiFi.h>

////////////////////////////////////////
// DEFINES
////////////////////////////////////////
#define WIFI_SSID "xenex-ap-5g"
#define WIFI_PASSWORD "0891560526"

// -------------------------------------
#define WIFI_DISCONNECTED 0
#define WIFI_CONNECTING 1
#define WIFI_CONNECTED 2
////////////////////////////////////////
// GLOBAL VARIABLES
////////////////////////////////////////
bool wifiReady = false;
uint8_t wifiState = WIFI_DISCONNECTED;
unsigned long wifiConnectTimer = 0;
unsigned long wifiConnectTimeout = 10000;

////////////////////////////////////////
// FUNCTION HEADERS
////////////////////////////////////////
void wifiSetup();
void wifiLoop();

////////////////////////////////////////
// MAIN
////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  delay(1000);
  wifiSetup();
}

void loop() {
  unsigned long t = millis();
  wifiLoop(t);
}

////////////////////////////////////////
// FUNCTION BODIES
////////////////////////////////////////

void wifiSetup() {
  // do nothing
}

void wifiLoop(unsigned long t) {
  wifiReady = WiFi.status() == WL_CONNECTED; 
  if (!wifiReady) {
    if (wifiState == WIFI_CONNECTED) {
      Serial.println("WiFi Disconnected");
      wifiState = WIFI_DISCONNECTED;
    }
    if (wifiState != WIFI_CONNECTING || t - wifiConnectTimer > wifiConnectTimeout) {
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      wifiState = WIFI_CONNECTING;
      wifiConnectTimer = t;
    }
  } else {
    if (wifiState != WIFI_CONNECTED) {
      Serial.println("WiFi Connected");
      wifiState = WIFI_CONNECTED;
    }
  }
}

