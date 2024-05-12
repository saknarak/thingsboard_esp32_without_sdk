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
#include <Update.h>
////////////////////////////////////////
// DEFINES
////////////////////////////////////////
#define FW_VERSION "0.1"
#define WIFI_SSID "wlan_2.4G"
#define WIFI_PASSWORD "0891560526"
#define TB_MQTT_SERVER "192.168.1.103"
#define TB_MQTT_PORT 1883
#define DEVICE_ACCESS_TOKEN "g8cnzpG7i5cPL2yBN0Za"
#define DHT22_PIN 23
#define ATTRIBUTE_KEYS "{\"clientKeys\":\"localIp\",\"sharedKeys\":\"uploadInterval,deviceMode\"}"
#define JSON_DOC_SIZE 1024
#define FAN_PIN 12
#define WATER_PIN 12
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
#define TB_RPC_REQUEST_TOPIC "v1/devices/me/rpc/request/+"
#define TB_RPC_RESPONSE_TOPIC "v1/devices/me/rpc/response/"
#define TB_OTA_SUBSCRIBE_TOPIC "v2/fw/response/+/chunk/+"
#define TB_OTA_REQUEST_TOPIC "v2/fw/request/%d/chunk/%d"
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
unsigned long sensorReadInterval = 2000;
unsigned long sensorUploadTimer = 0;
unsigned long sensorUploadInterval = 5000;
float temperature = 25;
float humidity = 50;
float soilMoisure = 70;

// Device Status
unsigned long deviceStatusTimer = 0;
unsigned long deviceStatusInterval = 10000;
// DHT22
DHT22 dht22(DHT22_PIN);

// Device Config
unsigned int reqSeq = 1;
uint8_t fan = 0;
uint8_t water = 0;
int resTopicLen = strlen(TB_ATTRIBUTE_RESPONSE_TOPIC) - 1; // v1/devices/me/attributes/response/+
int rpcTopicLen = strlen(TB_RPC_REQUEST_TOPIC) - 1;

// OTA
int otaId = 1;
int otaIndex = 0;

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

        char topic[128];
        sprintf(topic, "%s%c", TB_ATTRIBUTE_RESPONSE_TOPIC, '+');
        mqttClient.subscribe(topic);
        mqttClient.subscribe(TB_ATTRIBUTE_SUBSCRIBE_TOPIC);
        mqttClient.subscribe(TB_RPC_REQUEST_TOPIC);
        mqttClient.subscribe(TB_OTA_SUBSCRIBE_TOPIC);

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
  Serial.printf("GOT: %s %d\n", topic, length);
  // expect paylaod to be JSON
  if (strncmp(topic, "v2/fw/response/", 14) == 0) {
    if (length > 0) {
      Update.write(payload, length);
      char otaTopic[100];
      sprintf(otaTopic, TB_OTA_REQUEST_TOPIC, otaId, otaIndex);
      Serial.println(otaTopic);
      mqttClient.publish(otaTopic, "512");
      otaIndex++;
    } else {
      Update.end();
      if (Update.isFinished()) {
        Serial.println("OTA Success");
        ESP.restart();
      } else {
        Serial.println("OTA Failed");
      }
    }
    return;
  }


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
  if (strncmp(topic, TB_RPC_REQUEST_TOPIC, rpcTopicLen) == 0) {
    unsigned int rpcReqId = atoi(topic + rpcTopicLen); // atoi string => int
    processRpcRequest(rpcReqId, doc);
  }
  // TODO: process rpc resonse
}

// SENSORS
void sensorSetup() {
  // do nothing
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, fan);
  pinMode(WATER_PIN, OUTPUT);
  digitalWrite(WATER_PIN, water);
}

void sensorLoop(unsigned long t) {
  if (t - sensorReadTimer >= sensorReadInterval) {
    sensorReadTimer = t;
    
    if (fan == 0) {
      temperature -= 1.0 * random(5, 11) / 10.0;
      humidity += 1.0 * random(5, 11) / 10.0;
      if (temperature <= 20) {
        temperature = 20;
      }
      if (humidity >= 90) {
        humidity = 90;
      }
    } else {
      temperature += 1.0 * random(5, 11) / 10.0;
      humidity -= 1.0 * random(5, 11) / 10.0;
      if (temperature > 45) {
        temperature = 45;
      }
      if (humidity <= 20) {
        humidity = 20;
      }
    }
    if (water == 0) {
      soilMoisure -= 1.0 * random(10, 21) / 10.0;
      if (soilMoisure <= 20) {
        soilMoisure = 20;
      }
    } else {
      soilMoisure += 1.0 * random(30, 51) / 10.0;
      if (soilMoisure >= 95) {
        soilMoisure = 95;
      }
    }
    sensorUploadTimer = 0;
  }

  if (t - sensorUploadTimer >= sensorUploadInterval) {
    sensorUploadTimer = t;
    if (mqttReady) {
      char payload[1024];
      sprintf(payload, "{\"temperature\":%0.1f,\"humidity\":%0.1f,\"soilMoisure\":%0.1f}",
        temperature, humidity, soilMoisure);
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
  sprintf(payload, "{\"fw_ver\":\"%s\",\"rssi\":%d,\"channel\":%d,\"bssid\":\"%s\",\"localIp\":\"%s\",\"ssid\":\"%s\",\"totalFree\":%d,\"minFree\":%d,\"largeFree\":%d,\"fan\":%d,\"water\":%d}",
    FW_VERSION,
    WiFi.RSSI(),
    WiFi.channel(),
    WiFi.BSSIDstr().c_str(),
    WiFi.localIP().toString().c_str(),
    WiFi.SSID().c_str(),
    info.total_free_bytes,
    info.minimum_free_bytes,
    info.largest_free_block,
    fan,
    water
  );
  mqttClient.publish(TB_ATTRIBUTE_TOPIC, payload);
  Serial.print("Device Status: ");
  Serial.println(payload);
}

void deviceConfigRequest() {
  char topic[256];
  sprintf(topic, "%s%d", TB_ATTRIBUTE_REQUEST_TOPIC, reqSeq++);
  mqttClient.publish(topic, ATTRIBUTE_KEYS);
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
  Serial.println("GOT Shared");
  if (shared.containsKey("uploadInterval")) {
    unsigned long newInterval = shared["uploadInterval"].as<unsigned long>();
    if (newInterval >= 1000 && newInterval <= 60000) {
      sensorUploadInterval = newInterval;
      Serial.printf("Config: sensorUploadInterval=%d\n", sensorUploadInterval);
    }
  }
  if (shared.containsKey("fan")) {
    uint8_t newMode = shared["fan"].as<uint8_t>();
    if (newMode >= 0 && newMode <= 1) {
      fan = newMode;
      digitalWrite(FAN_PIN, fan);
      deviceStatusUpload();
      Serial.printf("Config: fan=%d\n", fan);
    }
  }
  if (shared.containsKey("water")) {
    uint8_t newMode = shared["water"].as<uint8_t>();
    if (newMode >= 0 && newMode <= 1) {
      water = newMode;
      digitalWrite(WATER_PIN, water);
      deviceStatusUpload();
      Serial.printf("Config: water=%d\n", water);
    }
  }
  if (shared.containsKey("fw_version")) {
    Serial.print("GET: fw_version size=");
    long fwSize = shared["fw_size"];
    Serial.println(fwSize); 
    Update.begin(fwSize);
    char otaTopic[100];
    sprintf(otaTopic, TB_OTA_REQUEST_TOPIC, otaId, otaIndex);
    Serial.println(otaTopic);
    mqttClient.publish(otaTopic, "512");
    otaIndex++;
  }
}

void processRpcRequest(unsigned int reqId, DynamicJsonDocument &doc) {
  if (!doc.containsKey("method")) {
    Serial.println("RPC: no method");
    rpcResponse(reqId, "{\"error\":\"no method\"}");
    return;
  }
  char method[100];
  strlcpy(method, doc["method"], sizeof(method));

  if (strcmp(method, "reset") == 0) {
    // param: 1000ms
    // TODO: reset with daly
    int delay = doc["params"];
    Serial.printf("RPC: reset delay=%d\n", delay);
    return rpcResponse(reqId, "{\"ok\":1}");
    // processReset(doc["param"] | 0);
  } else if (strcmp(method, "setFan") == 0) {
    int newMode = doc["params"];
    Serial.printf("RPC: setFan to %d\n", newMode);
    fan = newMode;
    digitalWrite(FAN_PIN, fan);
    deviceStatusUpload();
    return rpcResponse(reqId, "{\"ok\":1}");
  } else if (strcmp(method, "setWater") == 0) {
    int newMode = doc["params"];
    Serial.printf("RPC: setWater to %d\n", newMode);
    water = newMode;
    digitalWrite(WATER_PIN, water);
    deviceStatusUpload();
    return rpcResponse(reqId, "{\"ok\":1}");
  }
  
  return rpcResponse(reqId, "{\"error\":\"unknown method\"}");
}

void rpcResponse(unsigned int reqId, char *payload) {
  char topic[256];
  sprintf(topic, "%s%d", TB_RPC_RESPONSE_TOPIC, reqId);
  mqttClient.publish(topic, payload);
}
