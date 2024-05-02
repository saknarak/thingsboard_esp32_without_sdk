////////////////////////////////////////
// ESP32 TB NODE WITHOUT SDK
// -------------------------------------
// BY saknarak@gmail.com
// -------------------------------------
// Libraries
// - PubSubClient by Nick O'leary
////////////////////////////////////////

////////////////////////////////////////
// INCLUDES
////////////////////////////////////////
#include <WiFi.h>
#include <PubSubClient.h>

////////////////////////////////////////
// DEFINES
////////////////////////////////////////
#define WIFI_SSID "xenex-ap-5g"
#define WIFI_PASSWORD "0891560526"
#define TB_MQTT_SERVER ""
#define TB_MQTT_PORT 1883
#define DEVICE_ACCESS_TOKEN ""
// -------------------------------------
#define WIFI_DISCONNECTED 0
#define WIFI_CONNECTING 1
#define WIFI_CONNECTED 2
// -------------------------------------
#define MQTT_DISCONNECTED 0
#define MQTT_CONNECTED 2
#define MQTT_PACKET_SIZE 1024
// -------------------------------------
#define TB_TELEMETRY_TOPIC "v1/devices/me/telemetry"

////////////////////////////////////////
// GLOBAL VARIABLES
////////////////////////////////////////

// WiFi
bool wifiReady = false;
uint8_t wifiState = WIFI_DISCONNECTED;
unsigned long wifiConnectTimer = 0;
unsigned long wifiConnectTimeout = 10000;

// MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool mqttReady = false;
uint8_t mqttState = MQTT_DISCONNECTED;
unsigned long mqttDelayTimer = 0;
unsigned long mqttDelayTimeout = 5000;

// Sensor
unsigned long sensorReadTimer = 0;
unsigned long sensorReadInterval = 1000;
unsigned long sensorUploadTimer = 0;
unsigned long sensorUploadInterval = 5000;
int16_t sensorValue = 0;

////////////////////////////////////////
// FUNCTION HEADERS
////////////////////////////////////////

// WiFi
void wifiSetup();
void wifiLoop();

// MQTT
void mqttSetup();
void mqttLoop();
void mqttCallback(char *topic, byte *payload, unsigned int length);

////////////////////////////////////////
// MAIN
////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  delay(1000);
  wifiSetup();
  mqttSetup();
  sensorSetup();
}

void loop() {
  unsigned long t = millis();
  wifiLoop(t);
  mqttLoop(t);
  sensorLoop(t);
}

////////////////////////////////////////
// FUNCTION BODIES
////////////////////////////////////////

// WIFI
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
    if (wifiState != WIFI_CONNECTING || t - wifiConnectTimer >= wifiConnectTimeout) {
      Serial.println("WiFi Connecting...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      wifiState = WIFI_CONNECTING;
      wifiConnectTimer = t;
    }
  } else  {
    if (wifiState != WIFI_CONNECTED) {
      Serial.println("WiFi Connected");
      wifiState = WIFI_CONNECTED;
    }
  }
}

// MQTT
void mqttSetup() {
  mqttClient.setBufferSize(MQTT_PACKET_SIZE);
  mqttClient.setServer(TB_MQTT_SERVER, TB_MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void mqttLoop(unsigned long t) {
  mqttReady = mqttClient.connected();
  if (!mqttReady) {
    if (mqttState == MQTT_CONNECTED) {
      Serial.println("MQTT Disconnected");
      mqttState = MQTT_DISCONNECTED;
      mqttDelayTimer = t;
    }
    if (t - mqttDelayTimer >= mqttDelayTimeout) {
      mqttDelayTimer = t;
      Serial.println("MQTT Connecting...");
      if (mqttClient.connect(DEVICE_ACCESS_TOKEN)) {
        Serial.println("MQTT Connected");
        mqttReady = true;
        mqttState = MQTT_CONNECTED;
        // TODO: subscribe for shared attributes and rpc request
      }
    }
  } else {
    mqttClient.loop();
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  // TODO: process shared attributes changed
  // TODO: process rpc request
  // TODO: process rpc resonse
}

// SENSORS
void sensorSetup() {
  // do nothing
}

void sensorLoop(unsigned long t) {
  if (t - sensorReadTimer >= sensorReadInterval) {
    int prevValue = sensorValue;
    sensorValue = sensorValue + random(0, 3) - 1;
    if (prevValue != sensorValue) {
      sensorReadTimer = 0; // force upload
    }
  }

  if (t - sensorUploadTimer >= sensorUploadInterval) {
    sensorUploadTimer = t;
    if (mqttReady) {
      char payload[1024];
      sprintf("{\"temperature\":%d}", sensorValue);
      mqttClient.publish(TB_TELEMETRY_TOPIC, payload);
    }
  }
}

