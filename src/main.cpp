#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <variant>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>; // Not needed in C++20+

// TODO use soft wifi
const char* ssid = "house";
const char* password = "super duper password";
Adafruit_SSD1306 display(128, 64, &Wire, -1);

int lastRequestMs = -1;

void clearStatusBar() {
  display.drawRect(0, 54, 128, 1, SSD1306_WHITE);
  display.setCursor(0, 56);
}


void updateStatusBar(int t) {
  clearStatusBar();
  display.print(F("Connecting"));
  for (int i = 0; i < (t / 8 % 12); i++) {
    display.print(F("."));
  }
}
  
void updateStatusBar(IPAddress addr) {
  clearStatusBar();
  display.print(addr);
  if (lastRequestMs > 0) {
    display.setCursor(80, 56);
    Serial.printf("Request took %dms\n", lastRequestMs);
    display.printf("%6dms", lastRequestMs);
  }
}

void setup() {  
  Serial.begin(9600);

  pinMode(SDA, INPUT_PULLUP);
  pinMode(SCL, INPUT_PULLUP);

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

  // Connect to WiFi network
  Serial.printf("Connecting to %s...\n", ssid);
  WiFi.begin(ssid, password);
  int t = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    
    display.clearDisplay();
    updateStatusBar(++t);
    display.display();
  }
  Serial.println();
  digitalWrite(LED_BUILTIN, LOW);

  display.clearDisplay();
  updateStatusBar(WiFi.localIP());
  display.display();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
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
  updateStatusBar(WiFi.localIP());
  display.display();

  delay(500);
}
