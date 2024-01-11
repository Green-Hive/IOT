#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <ArduinoJson.h>  // Include the ArduinoJson library
#include <DHT.h>
#include <HX711_ADC.h>
#include <WiFi.h>
#include <HTTPClient.h>

// WiFi credentials
const char* ssid = "VRBOY";
const char* password = "deschiffresetdeslettreS";

// Initialize DHT sensor
#define DHTPIN 4     // GPIO pin connected to the DHT sensor
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Initialize BMP180
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

// Initialize HX711
HX711_ADC LoadCell(5, 17);

// Function prototype for sendData
void sendData(float temperature, float tempDHT, float pressure, float humidity, float weight);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); // Wait for serial port to connect

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Initialize sensors
  if (!bmp.begin()) {
    Serial.println("Ooops, no BMP180 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }
  dht.begin();
  LoadCell.begin();
  LoadCell.start(2000); // tare time ms
}

void loop() {
  // Read from BMP180
  sensors_event_t event;
  bmp.getEvent(&event);
  float temperature = 0;
  if (event.pressure) {
    bmp.getTemperature(&temperature);
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" C");
    Serial.print("\tPressure: ");
    Serial.print(event.pressure);
    Serial.println(" hPa");
  }

  // Read from DHT22
  float humidity = dht.readHumidity();
  float tempDHT = dht.readTemperature();
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(tempDHT);
  Serial.println(" *C");

  // Read from HX711
  float weight = 0;
  if (LoadCell.update()) {
    weight = LoadCell.getData();
    Serial.print("Weight: ");
    Serial.print(weight);
    Serial.println(" g");
  }

  // Send data to API
  sendData(temperature, tempDHT, event.pressure, humidity, weight);

  delay(2000); // Wait a few seconds between reads
}

void sendData(float temperature, float tempDHT, float pressure, float humidity, float weight) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.50.104:3000/sensor-data"); // Update with your server's local IP and port
    http.addHeader("Content-Type", "application/json");

    // Create JSON object
    StaticJsonDocument<200> jsonDocument;
    jsonDocument["temperature"] = temperature;
    jsonDocument["tempDHT"] = tempDHT;
    jsonDocument["pressure"] = pressure;
    jsonDocument["humidity"] = humidity;
    jsonDocument["weight"] = weight;
    String requestBody;
    serializeJson(jsonDocument, requestBody);

    // Make POST request
    int httpResponseCode = http.POST(requestBody);
    
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String response = http.getString();
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}


