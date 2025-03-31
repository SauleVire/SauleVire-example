#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <WebServer.h>

const char* ssid = "ssid";
const char* password = "password";
const char* firmwareUrl = "https://raw.githubusercontent.com/SauleVire/SauleVire-example/main/bin/firmware.bin";
const char* versionUrl = "https://raw.githubusercontent.com/SauleVire/SauleVire-example/main/bin/version.json";

int currentVersion = 1; // Dabartinė versija
bool autoUpdate = true; // Automatinis naujinimas
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 3600000; // Tikrinti kas valandą (milisekundėmis)

WebServer server(80);

void updateFirmware(); // Funkcijos deklaracija
void checkForUpdate(); // Funkcijos deklaracija
void handleUpdateRequest(); // Funkcijos deklaracija

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");

  server.on("/", HTTP_GET, []() {
    String html = "<html><body><h1>ESP32 Firmware Update</h1>";
    html += "<p>Current Version: " + String(currentVersion) + "</p>";
    html += "<p><a href=\"/update\">Check for Update</a></p>";
    html += "<p><a href=\"/toggle\">Toggle Auto Update: " + String(autoUpdate ? "ON" : "OFF") + "</a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/update", HTTP_GET, []() {
    handleUpdateRequest();
    server.send(200, "text/html", "<html><body><h1>Update Check</h1><p>Update process started.</p></body></html>");
  });

  server.on("/toggle", HTTP_GET, []() {
    autoUpdate = !autoUpdate;
    server.send(200, "text/html", "<html><body><h1>Auto Update</h1><p>Auto Update is now " + String(autoUpdate ? "ON" : "OFF") + ".</p></body></html>");
  });  

  server.begin();
}

void loop() {
  server.handleClient();
  if (autoUpdate && millis() - lastCheckTime > checkInterval) {
    lastCheckTime = millis();
    checkForUpdate();
  }
}

void handleUpdateRequest() {
  checkForUpdate();
}

void checkForUpdate() {
  HTTPClient http;
  http.begin(versionUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    int latestVersion = doc["version"];

    if (latestVersion > currentVersion) {
      Serial.println("New version available: " + String(latestVersion));
      updateFirmware();
    } else {
      Serial.println("No new version available.");
    }
  } else {
    Serial.println("Failed to check version. HTTP error: " + String(httpCode));
  }

  http.end();
}

void updateFirmware() {
  HTTPClient http;
  http.begin(firmwareUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      WiFiClient* client = http.getStreamPtr();
      size_t written = Update.writeStream(*client);

      if (written == contentLength) {
        Serial.println("Written : " + String(written) + " successfully");
      } else {
        Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
      }

      if (Update.end()) {
        Serial.println("OTA done!");
        if (Update.isFinished()) {
          Serial.println("Update successfully completed. Rebooting.");
          ESP.restart();
        } else {
          Serial.println("Update not finished? Something went wrong!");
        }
      } else {
        Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    } else {
      Serial.println("Not enough space to begin OTA");
    }
  } else {
    Serial.println("Failed to download firmware file. HTTP error: " + String(httpCode));
  }

  http.end();
}

