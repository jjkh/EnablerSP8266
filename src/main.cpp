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
String BASE_URI = "http://10.1.0.100:2321/api/v1";

const char* PUMP_STATES[] = {
  "PumpError",
  "NotResponding",
  "NotInstalled",
  "Busy",
  "Locked",
  "NotAllowed",
  "Calling",
  "Authorising",
  "AuthFailed",
  "Delivering",
  "DelFinished",
  "DelPaused",
  "DelStopped",
  "ManualPump",
};

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

void writeErrorToDisplay(String message, String details = "") {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("======= ERROR ======="));
  display.println(message);
  if (details.length() != 0)
    display.println(details);
  drawStatusBar();
  display.display();
}

std::optional<JsonArray> handleResponse(HTTPClient& http, JsonDocument& doc) {
  DeserializationError error = deserializeJson(doc, http.getStream());
  if (error) {
    writeErrorToDisplay(String("deserializeJson() ") + error.f_str());
    return {};
  }

  if (!doc["result"].is<int>()) {
    writeErrorToDisplay(F("no 'result' in response"));
    return {};
  }

  if (doc["result"] != 0) {
    writeErrorToDisplay(doc["error"], doc["errorMessage"]);
    if (doc["result"] == 7) // not logged on
      accessToken[0] = '\0';
    return {};
  }
  
  if (!doc["data"].is<JsonArray>()) {
    writeErrorToDisplay(F("no 'data' in response"));
    return {};
  }

  return doc["data"];
}

bool updateAccessToken() {
  // update display
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(F("requesting token..."));
  drawStatusBar();
  display.display();
  delay(500);

  // get a new token
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, BASE_URI + "/server/login")) {
    writeErrorToDisplay(F("failed to open http client"));
    return false;
  }

  http.useHTTP10(true); // https://arduinojson.org/v7/how-to/use-arduinojson-with-httpclient/
  char output[256];
  {
    JsonDocument doc;
    doc["terminalId"] = TERMINAL_ID;
    doc["password"] = TERMINAL_PASSWORD;
    doc["active"] = 0;
    serializeJson(doc, output);
  }
  
  http.addHeader("Content-Type", "application/json");
  http.POST(output);
  
  JsonDocument doc;
  auto data = handleResponse(http, doc);
  http.end();
  
  if (data) {
    const char* token = (*data)[0]["accessToken"];
    Serial.print(F("got token: ")); Serial.println(token);
    strcpy(accessToken, token);
    return true;
  }
  
  return false;
}

bool getPumpStatus() {
  // get a new token if required
  if (accessToken[0] == '\0') {
    if (!updateAccessToken()) return false;
  }

  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, BASE_URI + "/pumps")) {
    writeErrorToDisplay(F("failed to open http client"));
    return false;
  }

  http.addHeader("Authorization", String(F("Bearer ")) + accessToken);
  http.useHTTP10(true); // https://arduinojson.org/v7/how-to/use-arduinojson-with-httpclient/
  http.GET();
  
  JsonDocument doc;
  auto pumps = handleResponse(http, doc);
  http.end();
  if (!pumps) {
    return false;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  int i = 0;
  for (JsonVariant pump : *pumps) {
    if (i++ > 6) break;
    
    const char* name = pump["name"];
    int state = pump["state"];

    display.printf("%s: %s\n", name, PUMP_STATES[state]);
  }

  if (i == 0) {
    display.print(F("~~~ no pumps ~~~"));
  }

  return true;
}

void loop() {
  lastRequestMs = -1;
  auto start = millis();

  if (WiFi.status() != WL_CONNECTED) {
    writeErrorToDisplay("WIFI not connected");
    delay(100);
    return;
  }

  if (getPumpStatus()) {
    lastRequestMs = (int)(millis() - start);  
    drawStatusBar();
    display.display();
    delay(100);
  } else {
    // error message logged to screen, wait for a bit
    delay(10'000);
  }
}
