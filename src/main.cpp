#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

EspClass esp;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
int lastRequestMs = -1;

int TERMINAL_ID = 99;
const char* TERMINAL_PASSWORD = "password";
String BASE_URI = "http://10.1.0.100:2321/v1";

char accessToken[64] = "";

void drawStatusBar() {
  display.drawRect(0, 54, 128, 1, SSD1306_WHITE);
  display.setCursor(0, 56);

  display.print(WiFi.localIP());
  if (lastRequestMs > 0) {
    display.setCursor(80, 56);
    display.printf("%6dms", lastRequestMs);
  }
}

void configModeCallback(WiFiManager* wm) {
  display.clearDisplay();
  display.drawRoundRect(5, 5, 128 - (5 * 2), 64 - (5 * 2), 11, SSD1306_WHITE);
  display.drawRoundRect(6, 6, 128 - (6 * 2), 64 - (6 * 2), 10, SSD1306_BLACK);
  
  display.setCursor(10, 10);
  display.print("WiFi Config Mode");

  display.setCursor(10, 20);
  display.print(F("SSID: "));

  display.setCursor(14, 28);
  display.print(wm->getConfigPortalSSID());
  
  display.display();
}

void setup() {  
  Serial.begin(9600);

  pinMode(SDA, INPUT_PULLUP);
  pinMode(SCL, INPUT_PULLUP);

  WiFiManager wm;

  delay(500);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.display();
  Serial.println("logo displayed :)");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.clearDisplay();
  display.print(F("Connecting to WiFi..."));
  display.display();

  wm.setAPCallback(configModeCallback);
  auto res = wm.autoConnect("EnablerSP8266"); // anonymous ap
  if (!res) {
    display.print(F("Failed to connect..."));
    delay(2000);
    esp.restart();
  }

  display.clearDisplay();
  drawStatusBar();
  display.display();
}

bool updateAccessToken() {
  // update display
  display.clearDisplay();
  display.print(F("requesting token..."));
  drawStatusBar();
  display.display();

  // get a new token
  WiFiClient client;
  HTTPClient http;
  if (http.begin(client, BASE_URI + "/login")) {
    http.useHTTP10(true); // https://arduinojson.org/v7/how-to/use-arduinojson-with-httpclient/
    char output[256];
    {
      JsonDocument doc;
      doc["terminalID"] = TERMINAL_ID;
      doc["password"] = TERMINAL_PASSWORD;
      doc["active"] = 0;
      serializeJson(doc, output);
    }
    
    int httpCode = http.POST(output);
    if (httpCode == 0) {
      Serial.printf("POST request failed, error: %s\n", http.errorToString(httpCode).c_str());
      return false;
    }
    
    // got a response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    } else {
      const char* token = doc["data"][0]["accessToken"];
      Serial.print("got token: ");
      Serial.println(token);
      strcpy(accessToken, token);
      return true;
    }
  }
}

bool getPumpStatus() {
  // get a new token if required
  if (accessToken[0] == '\0') {
    if (!updateAccessToken()) return false;
  }

  auto start = millis();
  WiFiClient client;
  HTTPClient http;
  http.addHeader("Authorization", String(F("Bearer ")) + accessToken);
  if (http.begin(client, BASE_URI + "/pumps")) {
    http.useHTTP10(true); // https://arduinojson.org/v7/how-to/use-arduinojson-with-httpclient/
    int httpCode = http.GET();
    if (httpCode == 0) {
      Serial.printf("GET request failed, error: %s\n", http.errorToString(httpCode).c_str());
    } else {
      // got a response
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, http.getStream());
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return false;
      } else {
        display.clearDisplay();
        JsonArray pumps = doc["data"];
        int i = 0;
        for (JsonVariant pump : pumps) {
          int number = pump["number"];
          const char* status = pump["status"];

          Serial.print(F("number: ")); Serial.println(number);
          Serial.print(F("status: ")); Serial.println(status);

          display.setCursor(0, 8 * i++);
          display.printf("Pump %02d: %s", number, status);
        }
        lastRequestMs = millis() - start;
        drawStatusBar();
        display.display();
      }
    }
  }
}

int todoToGet = 1;
void loop() {
  auto start = millis();

  digitalWrite(LED_BUILTIN, WiFi.status() == WL_CONNECTED);
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    if (http.begin(client, String(F("http://jsonplaceholder.typicode.com/todos/")) + todoToGet++)) {
      http.useHTTP10(true); // https://arduinojson.org/v7/how-to/use-arduinojson-with-httpclient/
      int httpCode = http.GET();
      display.clearDisplay();

      if (httpCode == 0) {
        Serial.printf("GET request failed, error: %s\n", http.errorToString(httpCode).c_str());
      } else {
        // got a response
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, http.getStream());
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
        } else {
          int id = doc["id"];
          int userId = doc["userId"];
          const char* title = doc["title"];

          // Serial.printf("user: %d, title: %s\n", userId, title);

          display.setCursor(0, 0);
          display.println(title);
          display.println();
          display.print(F("ID: "));
          display.println(id);
          display.print(F("UserID: "));
          display.println(userId);
        }
      }
      http.end();
    }
  }

  lastRequestMs = (int)(millis() - start);  
  drawStatusBar();
  display.display();

  delay(500);
}
