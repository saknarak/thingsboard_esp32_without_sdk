////////////////////////////////////////
// ESP32 TB NODE WITHOUT SDK
// -------------------------------------
// BY saknarak@gmail.com
// -------------------------------------
// Libraries
// - PubSubClient by Nick O'leary
// - DHT22 by dvarrel
// - ArduinoJson
////////////////////////////////////////

////////////////////////////////////////
// INCLUDES
////////////////////////////////////////
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT22.h>

////////////////////////////////////////
// DEFINES
////////////////////////////////////////
#define WIFI_SSID "wlan_2.4G"
#define WIFI_PASSWORD "0891560526"
#define TB_MQTT_SERVER "192.168.1.106"
#define TB_MQTT_PORT 1883
#define DEVICE_ACCESS_TOKEN "st5l8N7tbEYi95KCkQWZ"
#define DHT22_PIN 23
#define ATTRIBUTE_KEYS "{\"clientKeys\":\"localIp\",\"sharedKeys\":\"uploadInterval,deviceMode\"}"
#define JSON_DOC_SIZE 1024
// -------------------------------------
#define WIFI_DISCONNECTED 0
#define WIFI_CONNECTING 1
#define WIFI_CONNECTED 2
// -------------------------------------
#define MQTT_DISCONNECTED 0
#define MQTT_CONNECTED 2
#define MQTT_PACKET_SIZE 1024
// -------------------------------------
// https://thingsboard.io/docs/reference/mqtt-api/#telemetry-upload-api
#define TB_TELEMETRY_TOPIC "v1/devices/me/telemetry"
// https://thingsboard.io/docs/reference/mqtt-api/#publish-attribute-update-to-the-server
#define TB_ATTRIBUTE_TOPIC "v1/devices/me/attributes"
// https://thingsboard.io/docs/reference/mqtt-api/#request-attribute-values-from-the-server
#define TB_ATTRIBUTE_REQUEST_TOPIC "v1/devices/me/attributes/request/"
#define TB_ATTRIBUTE_RESPONSE_TOPIC "v1/devices/me/attributes/response/+"
#define TB_ATTRIBUTE_SUBSCRIBE_TOPIC "v1/devices/me/attributes"
////////////////////////////////////////
// GLOBAL VARIABLES
////////////////////////////////////////

// WiFi
bool wifiReady = false;
uint8_t wifiState = WIFI_DISCONNECTED;
unsigned long wifiConnectTimer = 0;
unsigned long wifiConnectTimeout = 20000;

// MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool mqttReady = false;
uint8_t mqttState = MQTT_DISCONNECTED;
unsigned long mqttDelayTimer = 0;
unsigned long mqttDelayTimeout = 5000;
uint64_t chipId = 0;
char clientId[16];

// Sensor
unsigned long sensorReadTimer = 0;
unsigned long sensorReadInterval = 1000;
unsigned long sensorUploadTimer = 0;
unsigned long sensorUploadInterval = 5000;
int16_t sensorValue = 25;

// Device Status
unsigned long deviceStatusTimer = 0;
unsigned long deviceStatusInterval = 10000;
// DHT22
DHT22 dht22(DHT22_PIN);
float temperature = 0;
float humidity = 0;

// Device Config
unsigned int reqSeq = 1;
uint8_t deviceMode = 1; // 1 = ON, 0 = OFF
int resTopicLen = strlen(TB_ATTRIBUTE_RESPONSE_TOPIC) - 1; // v1/devices/me/attributes/response/+

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
  deviceStatusLoop(t);
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
  chipId = ESP.getEfuseMac();
  sprintf(clientId, "%08x", chipId);
  Serial.print("MQTT clientId=");
  Serial.println(clientId);
}

void mqttLoop(unsigned long t) {
  if (!wifiReady) {
    return;
  }
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
      
      // clientId, username, password
      if (mqttClient.connect(clientId, DEVICE_ACCESS_TOKEN, "")) {
        Serial.println("MQTT Connected");
        mqttReady = true;
        mqttState = MQTT_CONNECTED;
        // TODO: subscribe for shared attributes and rpc request
        char topic[128];
        mqttClient.subscribe(TB_ATTRIBUTE_RESPONSE_TOPIC);
        mqttClient.subscribe(TB_ATTRIBUTE_SUBSCRIBE_TOPIC);
        deviceConfigRequest();

        deviceStatusUpload();
        deviceStatusTimer = t;
      }
    }
  } else {
    mqttClient.loop();
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.printf("GOT: %d\n", length);
  // expect paylaod to be JSON
  DynamicJsonDocument doc(JSON_DOC_SIZE);
  // char json[MQTT_PACKET_SIZE];
  // memcpy(json, payload, length);
  // json[length] = 0;
  // Serial.println(json); // {"ok":1}\0 === 8, length == 9
  DeserializationError err = deserializeJson(doc, (char *)payload, length);
  if (err) {
    Serial.println("Not a valid json");
    return;
  }

  // TODO: process shared attributes changed
  if (strncmp(topic, TB_ATTRIBUTE_RESPONSE_TOPIC, resTopicLen) == 0) {
    processAttributeResponse(doc);
  } else if (strcmp(topic, TB_ATTRIBUTE_SUBSCRIBE_TOPIC) == 0) {
    JsonObject obj = doc.as<JsonObject>();
    processSharedAttributes(obj);
  }
  // TODO: process rpc request
  // TODO: process rpc resonse
}

// SENSORS
void sensorSetup() {
  // do nothing
}

void sensorLoop(unsigned long t) {
  if (t - sensorReadTimer >= sensorReadInterval) {
    sensorReadTimer = t;
    
    int prevValue = sensorValue;
    int rnd = random(0, 10);
    sensorValue = sensorValue + (rnd == 0 ? -1 : ( rnd == 9 ? 1 : 0));
    if (prevValue != sensorValue) {
      sensorUploadTimer = 0; // force upload
    }
    Serial.printf("Sensor: new=%d prev=%d\n", sensorValue, prevValue);
    
    float temp = dht22.getTemperature();
    float humi = dht22.getHumidity();

    if (dht22.getLastError() == dht22.OK) {
      float prevTemp = temperature;
      float prevHumi = humidity;
      temperature = temp;
      humidity = humi;
      if (prevTemp != temp || prevHumi != humi) {
        sensorUploadTimer = 0; // force upload
      }
    } else {
      Serial.print("last error :");
      Serial.println(dht22.getLastError());
    }
  }

  // read every 1sec
  // 1: temp = 25, prev = 25
  // 2: temp = 26, prev = 25 => upload
  // 3: temp = 26, prev = 26
  // 4: temp = 26, prev = 26
  // 5: temp = 26, prev = 26
  // 6: temp = 26, prev = 26
  // 7: temp = 26, prev = 26 => upload
  // 8: temp = 26, prev = 26
  // 9: temp = 25, prev = 26 => upload
  // upload every 5sec

  if (t - sensorUploadTimer >= sensorUploadInterval) {
    sensorUploadTimer = t;
    if (mqttReady) {
      char payload[1024];
      sprintf(payload, "{\"sensor\":%d,\"temperature\":%0.2f,\"humidity\":%0.2f}", sensorValue, temperature, humidity);
      mqttClient.publish(TB_TELEMETRY_TOPIC, payload);
      Serial.print("Sensor Upload: ");
      Serial.println(payload);
    } else {
      // TODO: append to queue
    }
  }
}

// Device Status
void deviceStatusLoop(unsigned long t) {
  if (!mqttReady || t - deviceStatusTimer < deviceStatusInterval) {
    return;
  }
  deviceStatusTimer = t;

  deviceStatusUpload();
}

void deviceStatusUpload() {
  char payload[1024];
  multi_heap_info_t info;
  heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  sprintf(payload, "{\"rssi\":%d,\"channel\":%d,\"bssid\":\"%s\",\"localIp\":\"%s\",\"ssid\":\"%s\",\"totalFree\":%d,\"minFree\":%d,\"largeFree\":%d}",
    WiFi.RSSI(),
    WiFi.channel(),
    WiFi.BSSIDstr().c_str(),
    WiFi.localIP().toString().c_str(),
    WiFi.SSID().c_str(),
    info.total_free_bytes,
    info.minimum_free_bytes,
    info.largest_free_block
  );
  mqttClient.publish(TB_ATTRIBUTE_TOPIC, payload);
  Serial.print("Device Status: ");
  Serial.println(payload);
}

void deviceConfigRequest() {
  char topic[256];
  sprintf(topic, "%s%d", TB_ATTRIBUTE_REQUEST_TOPIC, reqSeq++);
  mqttClient.publish(topic, ATTRIBUTE_KEYS);
  Serial.printf("attribute request topic=%s payload=%s\n", topic, ATTRIBUTE_KEYS);
}

void processAttributeResponse(DynamicJsonDocument &doc) {
  if (doc.containsKey("client")) {
    // TODO: process client attributes
  }
  if (doc.containsKey("shared")) {
    // process shared attributes
    JsonObject shared = doc["shared"];
    processSharedAttributes(shared);
  }
}

void processSharedAttributes(JsonObject &shared) {
  if (shared.containsKey("uploadInterval")) {
    unsigned long newInterval = shared["uploadInterval"].as<unsigned long>();
    if (newInterval >= 1000 && newInterval <= 60000) {
      sensorUploadInterval = newInterval;
      Serial.printf("Config: sensorUploadInterval=%d\n", sensorUploadInterval);
    }
  }
  if (shared.containsKey("deviceMode")) {
    uint8_t newMode = shared["deviceMode"].as<uint8_t>();
    if (newMode >= 0 && newMode <= 1) {
      deviceMode = newMode;
      Serial.printf("Config: deviceMode=%d\n", deviceMode);
    }
  }
}

