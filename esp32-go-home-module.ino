// Import required libraries
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "HTTPClient.h"
#include "ArduinoJson.h"

// GLobal config
const char *ssid;
const char *ssidpass;
const char *hubip;
const char *hubcode;
const char *serv;
const char *devicename = "ESP32";
const char *deviceid;
const char *apssid = "ESP32-Access-Point";
const char *appass = "123456789";

bool isOn = false;

AsyncWebServer server(80);

void onRequest(AsyncWebServerRequest *request)
{
    //Handle Unknown Request
    request->send(404);
}

void disconnect(){
  SPIFFS.remove("/hub.txt");
}

void saveConnect(){
  SPIFFS.remove("/hub.txt");
  String dat = "{\"ssid\":\"" + String(ssid) + "\",\"ssidpass\":\"" + String(ssidpass) + "\",\"hubip\":\"" + String(hubip) + "\",\"hubcode\":\"" + String(hubcode) + "\",\"name\":\"" + String(devicename);
  if (deviceid) {
    dat += "\",\"id\":\"" + String(deviceid);
  }
  dat += "\"}";
  File file = SPIFFS.open("/hub.txt", "w");
  int written = file.print(dat);
  if (written <= 0) {
    disconnect(); 
  }
  file.close();
}

void unconnectedSetup()
{
    WiFi.softAP(apssid, appass);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", String(), false);
    });

    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/style.css", "text/css");
    });

    server.on(
        "/connect", HTTP_POST, [](AsyncWebServerRequest *request) {
            if (!(request->hasParam("ssid", true) && request->hasParam("ssidpass", true) && request->hasParam("hubip", true) && request->hasParam("hubcode", true)))
            {
                request->send(400, "text/plain", "Wrong data");
            }
            else
            {           
                AsyncWebParameter *p = request->getParam("ssid", true);
                ssid = p->value().c_str();
                Serial.print("ssid: ");
                Serial.println(ssid);
                
                p = request->getParam("ssidpass", true);
                ssidpass = p->value().c_str();
                Serial.print("ssidpass: ");
                Serial.println(ssidpass);

                p = request->getParam("hubip", true);
                hubip = p->value().c_str();
                Serial.print("hubip: ");
                Serial.println(hubip);

                p = request->getParam("hubcode", true);
                hubcode = p->value().c_str();
                Serial.print("hubcode: ");
                Serial.println(hubcode);

                p = request->getParam("name", true);
                devicename = p->value().c_str();
                Serial.print("device name: ");
                Serial.println(devicename);

                saveConnect();
                delay(3000);
                ESP.restart();
            }
        });
    server.onNotFound(onRequest);
    server.begin();
}

void connectSetup()
{
    File file = SPIFFS.open("/service.txt", "r");
    if (!file)
    {
        Serial.println("Error opening file for reading");
        disconnect();
    }
    String servData;
    while (file.available())
    {
        servData += char(file.read());
    }
    file.close();

    Serial.println("=====================================");
    Serial.println(servData);
    Serial.println("=====================================");

    HTTPClient http;
    String conn = String(hubip) + "/connect";
    Serial.println(conn);
    http.begin(conn);
    // Specify content-type header
    http.addHeader("Content-Type", "application/json");
    // Data to send with HTTP POST
    String httpRequestData;
    DynamicJsonDocument doc(1024);
    if (deviceid) {
      doc["id"] = deviceid;
    }
    doc["hub-code"] = hubcode;
    doc["name"] = devicename;
    doc["serv"] = servData;
    doc["algo"] = "none";
    serializeJson(doc, httpRequestData);
    Serial.println(httpRequestData);
    // Send HTTP POST request
    int httpResponseCode = http.POST(httpRequestData);
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    if (httpResponseCode != 200)
    {
        disconnect();
        ESP.restart();
    }
    if(!deviceid){
      String payload = http.getString();
      Serial.println(payload);
      deviceid = payload.c_str();
      saveConnect();
    }
    // Free resources
    http.end();
}

void setup()
{
    // Serial port for debugging
    Serial.begin(115200);

    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    bool hasConnected = SPIFFS.exists("/hub.txt");

    if (hasConnected)
    {   
        File file = SPIFFS.open("/hub.txt", "r");
        if (!file)
        {
            Serial.println("Error opening file for reading");
            disconnect();
            ESP.restart();
        }
        String Data;
        while (file.available())
        {
            Data += char(file.read());
        }
        file.close();
        Serial.println(Data);
        DynamicJsonDocument root(1024);
        deserializeJson(root, Data);
        ssid = root["ssid"];
        ssidpass = root["ssidpass"];
        hubip = root["hubip"];
        hubcode = root["hubcode"];
        devicename = root["name"];
        if(root.containsKey("id")){
          deviceid = root["id"];
        }
        
        //connecting to wifi
        Serial.println("Trying to connect to hub");
        WiFi.begin(ssid, ssidpass);
        int tries = 0;
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(1000);
            Serial.println("Connecting to WiFi...");
            tries++;
            if (tries > 15)
            {
                Serial.println("Failed to connect to WiFi");
                disconnect();
                ESP.restart();
            }
        }
        Serial.println("");
        Serial.print("Connected to WiFi network with IP Address: ");
        Serial.println(WiFi.localIP());

        pinMode(LED_BUILTIN, OUTPUT);
        server.on("/click", HTTP_GET, [](AsyncWebServerRequest *request) {
          if (isOn)
          {
              digitalWrite(LED_BUILTIN, LOW);
              isOn = false;
          }
          else
          {
              digitalWrite(LED_BUILTIN, HIGH);
              isOn = true;
          }
          request->send(200);
        });
        
        connectSetup();
        server.onNotFound(onRequest);
        server.begin();
    }
    else
    {
        unconnectedSetup();
    }
}

void loop()
{
}
