
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>

// =====================================================
// WIFI
// =====================================================

const char* WIFI_SSID     = "Your Wi-fi Name";
const char* WIFI_PASSWORD = "Your Wi-fi Password";

// =====================================================
// THINGSPEAK
// =====================================================

// Telemetry channel
const char* TELEMETRY_CHANNEL_ID = "Enter Telemetry Channel ID";
const char* TELEMETRY_WRITE_KEY  = "Enter Telemetry Channel Write Key";

// Control channel
const char* CONTROL_CHANNEL_ID = "Enter Control Channel ID";
const char* CONTROL_READ_KEY   = "Enter control Channel Read Key";

// =====================================================
// PIN DEFINITIONS
// =====================================================

// Soil moisture sensor
#define SOIL_PIN 34

// Rain sensor
#define RAIN_PIN 27

// DHT11
#define DHT_PIN 4
#define DHT_TYPE DHT11

// LDR
#define LDR_PIN 35

// Water level sensor
#define WATER_PIN 32

// =====================================================
// L298N PUMP
// =====================================================

#define PUMP_IN1 25
#define PUMP_IN2 26

// =====================================================
// LED
// =====================================================

#define LED_PIN 33

// =====================================================
// DHT OBJECT
// =====================================================

DHT dht(DHT_PIN, DHT_TYPE);

// =====================================================
// SENSOR CALIBRATION
// =====================================================

// Soil sensor
// Dry soil = higher ADC
// Wet soil = lower ADC
const int SOIL_DRY_VALUE = 3200;
const int SOIL_WET_VALUE = 1300;

// LDR
const int LIGHT_DARK_VALUE   = 3500;
const int LIGHT_BRIGHT_VALUE = 500;

// Water level
const int WATER_EMPTY_VALUE = 500;
const int WATER_FULL_VALUE  = 3000;

// =====================================================
// THRESHOLDS
// =====================================================

const int SOIL_LOW_PERCENT = 30;

const int LIGHT_LOW_PERCENT = 30;

const int PUMP_MIN_WATER_PERCENT = 40;

const int WATER_CRITICAL_PERCENT = 20;

// =====================================================
// TIMERS
// =====================================================

const unsigned long SENSOR_INTERVAL =
  2000;

const unsigned long TELEMETRY_INTERVAL =
  15000;

const unsigned long CONTROL_INTERVAL =
  15000;

const unsigned long WIFI_RETRY_INTERVAL =
  10000;

// =====================================================
// TIMING VARIABLES
// =====================================================

unsigned long lastSensorTime = 0;

unsigned long lastTelemetryTime = 0;

unsigned long lastControlTime = 0;

unsigned long lastWiFiRetryTime = 0;

// =====================================================
// SENSOR VALUES
// =====================================================

int soilMoisture = 0;

int rainStatus = 0;

float temperature = 0;

float humidity = 0;

int lightIntensity = 0;

int waterLevel = 0;

// =====================================================
// DEVICE STATUS
// =====================================================

bool pumpOn = false;

bool ledOn = false;

int remotePumpCommand = -1;

// =====================================================
// ANALOG VALUE TO PERCENT
// =====================================================

int analogPercent(
  int reading,
  int lowValue,
  int highValue
) {

  if (lowValue == highValue) {
    return 0;
  }

  long percentage =
    ((long)reading - lowValue) *
    100L /
    ((long)highValue - lowValue);

  return constrain(
    (int)percentage,
    0,
    100
  );
}

// =====================================================
// L298N PUMP CONTROL
// =====================================================

void pumpON() {

  digitalWrite(PUMP_IN1, HIGH);

  digitalWrite(PUMP_IN2, LOW);

  pumpOn = true;

  Serial.println("L298N PUMP: ON");
}

// =====================================================

void pumpOFF() {

  digitalWrite(PUMP_IN1, LOW);

  digitalWrite(PUMP_IN2, LOW);

  pumpOn = false;

  Serial.println("L298N PUMP: OFF");
}

// =====================================================
// READ ALL SENSORS
// =====================================================

void readSensors() {

  int soilRaw =
    analogRead(SOIL_PIN);

  int lightRaw =
    analogRead(LDR_PIN);

  int waterRaw =
    analogRead(WATER_PIN);

  // ---------------------------------------------
  // SOIL
  // ---------------------------------------------

  soilMoisture =
    analogPercent(
      soilRaw,
      SOIL_DRY_VALUE,
      SOIL_WET_VALUE
    );

  // ---------------------------------------------
  // LIGHT
  // ---------------------------------------------

  lightIntensity =
    analogPercent(
      lightRaw,
      LIGHT_DARK_VALUE,
      LIGHT_BRIGHT_VALUE
    );

  // ---------------------------------------------
  // WATER TANK
  // ---------------------------------------------

  waterLevel =
    analogPercent(
      waterRaw,
      WATER_EMPTY_VALUE,
      WATER_FULL_VALUE
    );

  // ---------------------------------------------
  // RAIN
  // ---------------------------------------------

  int rainRaw =
    digitalRead(RAIN_PIN);

  /*
    Most rain modules:
    LOW = rain
    HIGH = no rain
  */

  rainStatus =
    (rainRaw == LOW) ? 1 : 0;

  // ---------------------------------------------
  // DHT11
  // ---------------------------------------------

  float newTemperature =
    dht.readTemperature();

  float newHumidity =
    dht.readHumidity();

  if (!isnan(newTemperature)) {

    temperature =
      newTemperature;
  }

  if (!isnan(newHumidity)) {

    humidity =
      newHumidity;
  }

  // ---------------------------------------------
  // SERIAL MONITOR
  // ---------------------------------------------

  Serial.println();
  Serial.println("========== SENSOR DATA ==========");

  Serial.print("Soil Moisture : ");
  Serial.print(soilMoisture);
  Serial.println("%");

  Serial.print("Rain          : ");

  if (rainStatus == 1) {
    Serial.println("RAIN");
  } else {
    Serial.println("NO RAIN");
  }

  Serial.print("Temperature   : ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity      : ");
  Serial.print(humidity);
  Serial.println("%");

  Serial.print("Light         : ");
  Serial.print(lightIntensity);
  Serial.println("%");

  Serial.print("Water Tank    : ");
  Serial.print(waterLevel);
  Serial.println("%");

  Serial.print("Pump          : ");

  if (pumpOn) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  Serial.print("LED           : ");

  if (ledOn) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  Serial.println("=================================");
}

// =====================================================
// AUTOMATIC LED
// =====================================================

void controlLED() {

  /*
    Light below 30%
    = LED ON
  */

  if (lightIntensity < LIGHT_LOW_PERCENT) {

    digitalWrite(LED_PIN, HIGH);

    ledOn = true;
  }

  else {

    digitalWrite(LED_PIN, LOW);

    ledOn = false;
  }
}

// =====================================================
// PUMP SAFETY
// =====================================================

void applyPumpSafety() {

  /*
    WATER BELOW 20%
    ----------------
    Critical condition.
    Pump MUST be OFF.
  */

  if (waterLevel < WATER_CRITICAL_PERCENT) {

    if (pumpOn) {

      pumpOFF();

      Serial.println(
        "SAFETY: Water below 20%. Pump OFF."
      );
    }

    return;
  }

  /*
    WATER 20% TO 40%
    ----------------
    Pump cannot remain ON.
  */

  if (waterLevel <= PUMP_MIN_WATER_PERCENT) {

    if (pumpOn) {

      pumpOFF();

      Serial.println(
        "SAFETY: Water level <= 40%. Pump OFF."
      );
    }

    return;
  }
}

// =====================================================
// READ CONTROL CHANNEL
// =====================================================

int readPumpCommand() {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println(
      "Control read skipped: WiFi disconnected."
    );

    return -1;
  }

  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;

  String url =
    "https://api.thingspeak.com/channels/" +
    String(CONTROL_CHANNEL_ID) +
    "/fields/1/last.json?api_key=" +
    String(CONTROL_READ_KEY);

  Serial.println();
  Serial.println("Reading pump command...");

  if (!http.begin(client, url)) {

    Serial.println(
      "Control HTTP begin failed."
    );

    return -1;
  }

  http.setTimeout(8000);

  int httpCode =
    http.GET();

  int command = -1;

  if (httpCode == 200) {

    String response =
      http.getString();

    Serial.print(
      "ThingSpeak Control: "
    );

    Serial.println(response);

    /*
      Example:

      {"created_at":"...",
       "entry_id":1,
       "field1":"1"}
    */

    int fieldPosition =
      response.indexOf("\"field1\"");

    if (fieldPosition >= 0) {

      int colonPosition =
        response.indexOf(
          ':',
          fieldPosition
        );

      if (colonPosition >= 0) {

        int valueStart =
          response.indexOf(
            '"',
            colonPosition
          );

        if (valueStart >= 0) {

          int valueEnd =
            response.indexOf(
              '"',
              valueStart + 1
            );

          if (valueEnd > valueStart) {

            String value =
              response.substring(
                valueStart + 1,
                valueEnd
              );

            value.trim();

            int receivedValue =
              value.toInt();

            if (
              receivedValue == 0 ||
              receivedValue == 1
            ) {

              command =
                receivedValue;
            }
          }
        }
      }
    }
  }

  else {

    Serial.print(
      "Control HTTP error: "
    );

    Serial.println(httpCode);
  }

  http.end();

  return command;
}

// =====================================================
// PROCESS PUMP COMMAND
// =====================================================

void processPumpCommand(
  int command
) {

  if (
    command != 0 &&
    command != 1
  ) {

    return;
  }

  remotePumpCommand =
    command;

  // ---------------------------------------------
  // FARMER COMMAND = OFF
  // ---------------------------------------------

  if (command == 0) {

    pumpOFF();

    Serial.println(
      "Mobile command: PUMP OFF"
    );

    return;
  }

  // ---------------------------------------------
  // FARMER COMMAND = ON
  // ---------------------------------------------

  Serial.println(
    "Mobile command: PUMP ON"
  );

  // Critical water condition
  if (
    waterLevel <
    WATER_CRITICAL_PERCENT
  ) {

    pumpOFF();

    Serial.println(
      "PUMP ON BLOCKED: Water below 20%."
    );

    return;
  }

  // Pump requires more than 40% water
  if (
    waterLevel <=
    PUMP_MIN_WATER_PERCENT
  ) {

    pumpOFF();

    Serial.println(
      "PUMP ON BLOCKED: Water must be above 40%."
    );

    return;
  }

  // Safe to turn pump ON
  pumpON();
}

// =====================================================
// SEND TELEMETRY
// =====================================================

void uploadTelemetry() {

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    Serial.println(
      "Telemetry skipped: WiFi disconnected."
    );

    return;
  }

  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;

  String url =
    "https://api.thingspeak.com/update?api_key=";

  url += TELEMETRY_WRITE_KEY;

  url += "&field1=";
  url += String(soilMoisture);

  url += "&field2=";
  url += String(rainStatus);

  url += "&field3=";
  url += String(
    temperature,
    1
  );

  url += "&field4=";
  url += String(
    humidity,
    1
  );

  url += "&field5=";
  url += String(lightIntensity);

  url += "&field6=";
  url += String(waterLevel);

  url += "&field7=";
  url += String(
    pumpOn ? 1 : 0
  );

  url += "&field8=";
  url += String(
    ledOn ? 1 : 0
  );

  Serial.println();
  Serial.println(
    "Uploading telemetry..."
  );

  if (!http.begin(client, url)) {

    Serial.println(
      "Telemetry HTTP begin failed."
    );

    return;
  }

  http.setTimeout(10000);

  int httpCode =
    http.GET();

  if (httpCode == 200) {

    String response =
      http.getString();

    response.trim();

    Serial.print(
      "ThingSpeak Entry ID: "
    );

    Serial.println(response);

  }

  else {

    Serial.print(
      "Telemetry HTTP error: "
    );

    Serial.println(httpCode);
  }

  http.end();
}

// =====================================================
// WIFI
// =====================================================

void connectWiFi() {

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  Serial.print(
    "Connecting to WiFi"
  );

  unsigned long startTime =
    millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startTime < 15000
  ) {

    delay(500);

    Serial.print(".");
  }

  Serial.println();

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {

    Serial.println(
      "WiFi connected."
    );

    Serial.print(
      "ESP32 IP: "
    );

    Serial.println(
      WiFi.localIP()
    );
  }

  else {

    Serial.println(
      "WiFi connection failed."
    );
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  // ---------------------------------------------
  // PIN MODES
  // ---------------------------------------------

  pinMode(
    RAIN_PIN,
    INPUT
  );

  pinMode(
    PUMP_IN1,
    OUTPUT
  );

  pinMode(
    PUMP_IN2,
    OUTPUT
  );

  pinMode(
    LED_PIN,
    OUTPUT
  );

  // ---------------------------------------------
  // ADC
  // ---------------------------------------------

  analogReadResolution(12);

  // ---------------------------------------------
  // SAFE START
  // ---------------------------------------------

  pumpOFF();

  digitalWrite(
    LED_PIN,
    LOW
  );

  // ---------------------------------------------
  // DHT
  // ---------------------------------------------

  dht.begin();

  Serial.println();
  Serial.println(
    "================================="
  );

  Serial.println(
    "AGRI IoT SMART FARM"
  );

  Serial.println(
    "ESP32 + ThingSpeak + L298N"
  );

  Serial.println(
    "================================="
  );

  // ---------------------------------------------
  // WIFI
  // ---------------------------------------------

  connectWiFi();

  // ---------------------------------------------
  // FIRST SENSOR READING
  // ---------------------------------------------

  readSensors();

  controlLED();

  applyPumpSafety();

  // ---------------------------------------------
  // FIRST UPLOAD / CONTROL
  // ---------------------------------------------

  lastTelemetryTime =
    millis() - TELEMETRY_INTERVAL;

  lastControlTime =
    millis() - CONTROL_INTERVAL;
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  unsigned long currentTime =
    millis();

  // =================================================
  // WIFI RECONNECT
  // =================================================

  if (
    WiFi.status() != WL_CONNECTED &&
    currentTime - lastWiFiRetryTime >=
    WIFI_RETRY_INTERVAL
  ) {

    lastWiFiRetryTime =
      currentTime;

    Serial.println(
      "Reconnecting WiFi..."
    );

    WiFi.disconnect();

    WiFi.begin(
      WIFI_SSID,
      WIFI_PASSWORD
    );
  }

  // =================================================
  // SENSOR UPDATE
  // =================================================

  if (
    currentTime - lastSensorTime >=
    SENSOR_INTERVAL
  ) {

    lastSensorTime =
      currentTime;

    readSensors();

    controlLED();

    /*
      Local safety is checked
      continuously.
    */

    applyPumpSafety();
  }

  // =================================================
  // READ MOBILE PUMP COMMAND
  // =================================================

  if (
    currentTime - lastControlTime >=
    CONTROL_INTERVAL
  ) {

    lastControlTime =
      currentTime;

    int command =
      readPumpCommand();

    if (
      command == 0 ||
      command == 1
    ) {

      processPumpCommand(
        command
      );
    }

    // Always apply safety after command.
    applyPumpSafety();
  }

  // =================================================
  // UPLOAD TELEMETRY
  // =================================================

  if (
    currentTime - lastTelemetryTime >=
    TELEMETRY_INTERVAL
  ) {

    lastTelemetryTime =
      currentTime;

    // Final safety check
    applyPumpSafety();

    uploadTelemetry();
  }

  delay(10);
}
