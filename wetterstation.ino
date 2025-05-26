/* Blynk */
#define BLYNK_TEMPLATE_ID "TMPL4piQgttv_"
#define BLYNK_TEMPLATE_NAME "Wetterstation"
#define BLYNK_AUTH_TOKEN "Qc6OIlH6FrwyRuH61Iqu0IVRfku_6v8P"

#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_NeoPixel.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp32.h>
#include <U8g2lib.h>
#include "LittleFS.h"
#include "time.h"
#include "DHT.h"

/* LED */
#define PIN 8
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

/* Sensor pins */
#define SENSOR_PIN 1
#define DHT11_PIN 2

DHT dht11(DHT11_PIN, DHT11);

/* Display */
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
  U8G2_R0,
  5,
  4,
  U8X8_PIN_NONE);

/* Web Server */
AsyncWebServer server(80);
HTTPClient http;

/* Discord Webhook */
const char* DISCORD_CERT = R"(
-----BEGIN CERTIFICATE-----
MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX
MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE
CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx
NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT
GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0
MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE83Rzp2iLYK5DuDXFgTB7S0md+8Fhzube
Rr1r1WEYNa5A3XP3iZEwWus87oV8okB2O6nGuEfYKueSkWpz6bFyOZ8pn6KY019e
WIZlD6GEZQbR3IvJx3PIjGov5cSr0R2Ko4H/MIH8MA4GA1UdDwEB/wQEAwIBhjAd
BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd
BgNVHQ4EFgQUgEzW63T/STaj1dj8tT7FavCUHYwwHwYDVR0jBBgwFoAUYHtmGkUN
l8qJUC99BM00qP/8/UswNgYIKwYBBQUHAQEEKjAoMCYGCCsGAQUFBzAChhpodHRw
Oi8vaS5wa2kuZ29vZy9nc3IxLmNydDAtBgNVHR8EJjAkMCKgIKAehhxodHRwOi8v
Yy5wa2kuZ29vZy9yL2dzcjEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqG
SIb3DQEBCwUAA4IBAQAYQrsPBtYDh5bjP2OBDwmkoWhIDDkic574y04tfzHpn+cJ
odI2D4SseesQ6bDrarZ7C30ddLibZatoKiws3UL9xnELz4ct92vID24FfVbiI1hY
+SW6FoVHkNeWIP0GCbaM4C6uVdF5dTUsMVs/ZbzNnIdCp5Gxmx5ejvEau8otR/Cs
kGN+hr/W5GvT1tMBjgWKZ1i4//emhA1JG1BbPzoLJQvyEotc03lXjTaCzv8mEbep
8RqZ7a2CPsgRbuvTPBwcOMBBmuFeU88+FSBX6+7iP0il8b4Z0QFqIwwMHfs/L6K1
vepuoxtGzi4CZ68zJpiq1UvSqTbFJjtbD4seiMHl
-----END CERTIFICATE-----
)";

#define DISCORD_WEBHOOK "https://discord.com/api/webhooks/1376593351079497728/L_QXPmTauqQif_z7Pxu181oMb4jqOgEQrZlkJlbtYc45d3_ri9H5JXj4PV8KyXPNoQng"
#define DISCORD_TTS "false"

/* Datenbank daten*/
const char* HOST_NAME = "http://192.168.0.213";
const char* PATH_NAME = "/insert_data.php";

/* NTP server */
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;
const int daylightOffset_sec = 0;

/* Konfiguration und State */
struct Config {
  bool led;
  long interval;
  long standby;
  long highHumidity;
  long highTemperature;
  long measurementInProcess;
  long noWlan;
} config;

struct State {
  float temperature;
  float humidity;
  float sensor;
  char timestamp[64];
} state;

/* LED status */
char ledStatus[32] = "standby";

void writeFile(fs::FS& fs, const char* path, const char* content) {
  File file = fs.open(path, FILE_WRITE);
  if (!file) return;

  file.print(content);
  file.close();
}

void deleteFile(fs::FS& fs, const char* path) {
  fs.remove(path);
}

String readFile(fs::FS& fs, const char* path) {
  File file = fs.open(path);
  if (!file || file.isDirectory()) return "";

  String content = file.readString();
  file.close();
  return content;
}

bool checkFile(fs::FS& fs, const char* path) {
  File file = fs.open(path);
  if (!file || file.isDirectory()) return false;

  file.close();
  return true;
}

String getValue(const String& str, const String& value) {
  int valueStart = str.indexOf(value + "=");
  if (valueStart == -1) return "";

  int endPos = str.indexOf(";", valueStart);
  if (endPos == -1) return "";

  return str.substring(valueStart + value.length() + 1, endPos);
}

String setValue(const String& str, const String& key, const String& value) {
  int valueStart = str.indexOf(key + "=");
  if (valueStart == -1) return str;

  int endPos = str.indexOf(";", valueStart);
  if (endPos == -1) return str;

  Serial.println("Setting Value - Key: " + key + " - " + " Value: " + value);

  return str.substring(0, valueStart + key.length() + 1) + value + str.substring(endPos);
}

String toJson(const String& str) {
  String json = "{";
  int start = 0;

  while (start < str.length()) {
    int eq = str.indexOf("=", start);
    int semi = str.indexOf(";", start);

    if (eq == -1 || semi == -1) return "";

    String key = str.substring(start, eq);
    String value = str.substring(eq + 1, semi);

    bool isNumeric = false;
    for (int i = 0; i < value.length(); i++) {
      char c = value.charAt(i);
      if (isDigit(c) || c == ',' || c == '.') {
        isNumeric = true;
      } else {
        isNumeric = false;
        break;
      }
    }

    bool isBool = false;
    if (value.equals("true") || value.equals("false")) isBool = true;

    if (isNumeric || isBool) {
      json += "\"" + key + "\"" + ":" + value + ",";
    } else {
      json += "\"" + key + "\"" + ":" + "\"" + value + "\"" + ",";
    }

    start = semi + 1;
  }

  if (json.endsWith(",")) json.remove(json.lastIndexOf(","));
  json += "}";

  return json;
}

long convertStringToHex(const String& str) {
  return strtoul(str.c_str() + 1, NULL, 16);
}

void loadConfig() {
  if (!checkFile(LittleFS, "/config.txt")) {
    Serial.println("No config file found: generating auto config");
    const char* confi = "led=true;interval=7200000;standby=#1E90FF;highHumidity=#708090;highTemperature=#FFA500;measurementInProcess=#FFD700;noWlan=#FF4C4C;";
    writeFile(LittleFS, "/config.txt", confi);
  }

  String conf = readFile(LittleFS, "/config.txt");
  Serial.println("Config file found!");
  Serial.println(conf);

  config.led = getValue(conf, "led") == "true";
  config.interval = strtol(getValue(conf, "interval").c_str(), NULL, 10);
  config.standby = convertStringToHex(getValue(conf, "standby"));
  config.highHumidity = convertStringToHex(getValue(conf, "highHumidity"));
  config.highTemperature = convertStringToHex(getValue(conf, "highTemperature"));
  config.measurementInProcess = convertStringToHex(getValue(conf, "measurementInProcess"));
  config.noWlan = convertStringToHex(getValue(conf, "noWlan"));
}

void updateConfig(const String& key, const String& value) {
  Serial.println(key);
  Serial.println(value);
  if (key == "led") {
    config.led = (value == "true");
  } else if (key == "interval") {
    config.interval = value.toInt();
  } else if (key == "standby") {
    config.standby = convertStringToHex(value);
  } else if (key == "highHumidity") {
    config.highHumidity = convertStringToHex(value);
  } else if (key == "highTemperature") {
    config.highTemperature = convertStringToHex(value);
  } else if (key == "measurementInProcess") {
    config.measurementInProcess = convertStringToHex(value);
  } else if (key == "noWlan") {
    config.noWlan = convertStringToHex(value);
  }

  String configFile = readFile(LittleFS, "/config.txt");
  String updatedConfig = setValue(configFile, key, value);
  Serial.println(updatedConfig);
  writeFile(LittleFS, "/config.txt", updatedConfig.c_str());
}

void setStatus(const char* status) {
  strcpy(ledStatus, status);
}

void updateLED() {
  if (!config.led) {
    pixels.clear();
  } else if (strcmp(ledStatus, "standby") == 0) {
    pixels.setPixelColor(0, config.standby);
  } else if (strcmp(ledStatus, "highHumidity") == 0) {
    pixels.setPixelColor(0, config.highHumidity);
  } else if (strcmp(ledStatus, "highTemperature") == 0) {
    pixels.setPixelColor(0, config.highTemperature);
  } else if (strcmp(ledStatus, "measurementInProcess") == 0) {
    pixels.setPixelColor(0, config.measurementInProcess);
  } else if (strcmp(ledStatus, "noWlan") == 0) {
    pixels.setPixelColor(0, config.noWlan);
  }

  pixels.show();
}

void insertData(float temp, float humi, float sensor, const char* timestamp) {
  String queryString = "?";
  queryString += "temperature=" + String(temp);
  queryString += "&humidity=" + String(humi);
  queryString += "&sensor=" + String(sensor);
  queryString += "&timestamp=" + String(timestamp);

  http.begin(String(HOST_NAME) + String(PATH_NAME) + queryString);
  int httpCode = http.GET();

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  }

  http.end();
}

void sendDiscord(String content, String embedJson) {
  WiFiClientSecure* client = new WiFiClientSecure;
  if (client) {
    client->setCACert(DISCORD_CERT);
    {
      HTTPClient https;
      if (https.begin(*client, DISCORD_WEBHOOK)) {
        https.addHeader("Content-Type", "application/json");

        String jsonPayload = "{\"content\":\"" + content + "\",\"tts\":" + DISCORD_TTS + ",\"embeds\": [" + embedJson + "]}";
        int httpCode = https.POST(jsonPayload);

        if (httpCode > 0) {
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = https.getString();
            Serial.print("HTTP Response: ");
            Serial.println(payload);
          }
        } else {
          Serial.print("HTTP Post failed: ");
          Serial.println(https.errorToString(httpCode).c_str());
        }

        https.end();
      }
    }

    delete client;
  }
}

void sendDiscordMessage(String content) {
  sendDiscord(content, "");
}

void updateDisplay(float temp, float humi, float sensor) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  String tempStr = "Temperature: " + String(temp, 1) + " C";
  String humStr = "Humidity: " + String(humi, 1) + " %";
  String senStr = "Sensor: " + String(sensor, 1);
  u8g2.drawStr(0, 20, tempStr.c_str());
  u8g2.drawStr(0, 40, humStr.c_str());
  u8g2.drawStr(0, 60, senStr.c_str());
  u8g2.sendBuffer();
}

void measure() {
  setStatus("measurementInProcess");
  updateLED();

  float humi = dht11.readHumidity();
  float tempC = dht11.readTemperature();
  float sensor = analogRead(SENSOR_PIN);

  Serial.println("Starting measuring process!");

  if (isnan(humi) || isnan(tempC)) {
    Serial.println("Measurement failed! Going into standby");
    setStatus("standby");
    return;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Fetching time failed! Going into standby");
    setStatus("standby");
    return;
  }

  char isoTime[32];
  strftime(isoTime, sizeof(isoTime), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

  String values = "Temperature " + String(tempC) + "C - Humidity " + String(humi) + "% - Sensor: " + String(sensor) + " - Timestamp " + isoTime;
  Serial.println(values);
  sendDiscordMessage(values);

  state.humidity = humi;
  state.temperature = tempC;
  state.sensor = sensor;
  strcpy(state.timestamp, isoTime);

  updateDisplay(tempC, humi, sensor);
  insertData(tempC, humi, sensor, isoTime);

  Blynk.virtualWrite(V0, tempC);
  Blynk.virtualWrite(V1, humi);
  Blynk.virtualWrite(V2, sensor);

  Serial.println("Measured successfully");

  setStatus("standby");

  if (tempC > 40) {
    Serial.println("Temperature too high! Going into high temperature status");
    setStatus("highTemperature");
  }

  if(humi > 80) {
    Serial.println("Humidity too high! Going into high humidity status");
    setStatus("highHumidity");
  }

}

void setup() {
  Serial.begin(115200);
  pixels.begin();
  pixels.clear();

  WiFiManager wm;
  if (!wm.autoConnect("Wetterstation")) {
    setStatus("noWlan");
    updateLED();
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  dht11.begin();
  pinMode(SENSOR_PIN, INPUT);
  u8g2.begin();

  if (!LittleFS.begin(true)) return;

  loadConfig();

  Blynk.config(BLYNK_AUTH_TOKEN);

  measure();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = toJson(readFile(LittleFS, "/config.txt"));
    request->send(200, "application/json", json);
  });

  server.on("/api/data/temperature", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = toJson("timestamp=" + String(state.timestamp) + ";" + "temperature=" + String(state.temperature) + ";");
    request->send(200, "application/json", json);
  });

  server.on("/api/data/humidity", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = toJson("timestamp=" + String(state.timestamp) + ";" + "humidity=" + String(state.humidity) + ";");
    request->send(200, "application/json", json);
  });

  server.on("/api/data/sensor", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = toJson("timestamp=" + String(state.timestamp) + ";" + "sensor=" + String(state.sensor) + ";");
    request->send(200, "application/json", json);
  });

  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = toJson("timestamp=" + String(state.timestamp) + ";" + "humidity=" + String(state.humidity) + ";" + "temperature=" + String(state.temperature) + ";" + "sensor=" + String(state.sensor) + ";");
    request->send(200, "application/json", json);
  });

  server.on(
    "/api/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      String str = "";
      for (size_t i = 0; i < len; i++) {
        str += (char)data[i];
      }

      int eq = str.indexOf("=");
      int semi = str.indexOf(";");
      String key = str.substring(0, eq);
      String value = str.substring(eq + 1, semi);

      Serial.println("Received POST request: " + str);

      updateConfig(key, value);

      request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

  server.serveStatic("/", LittleFS, "/");

  server.begin();
}

unsigned long previousMeasureTime = 0;

void loop() {
  unsigned long currentTime = millis();

  Blynk.run();
  updateLED();

  if (currentTime - previousMeasureTime >= config.interval) {
    previousMeasureTime = currentTime;
    measure();
  }
}
