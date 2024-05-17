#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <ArduinoJson.h>  // Include the ArduinoJson library
#include <DHT.h>
#include <HX711_ADC.h>
#include <WiFi.h>
#include <PubSubClient.h> // Include the PubSubClient library for MQTT
#include <QMC5883LCompass.h>

// WiFi credentials
const char* ssid = "partagematte";
const char* password = "lazu1801";

// MQTT Broker details
const char* mqtt_server = "172.201.14.60";
const char* mqtt_topic = "sensor/data";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Initialize sensors
#define DHTPIN 4     // GPIO pin connected to the DHT sensor
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
HX711_ADC LoadCell(5, 17);
QMC5883LCompass compass;
void sendData(float temperature, float tempDHT, float pressure, float humidity, float weight, float x, float y, float z);
void connectToWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
}

void connectToMQTT() {
    mqttClient.setServer(mqtt_server, 1883);
    while (!mqttClient.connected()) {
        Serial.println("Connecting to MQTT...");
        if (mqttClient.connect("ESP32Client")) {
            Serial.println("Connected to MQTT");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(mqttClient.state());
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10); // Wait for serial port to connect
    connectToWiFi();
    connectToMQTT();

    compass.init(); // Initialize compass without checking return status
    Serial.println("Compass initialized."); // Inform about initialization
    // mode = 0x01 (continuous mode), odr = 0x04 (10 Hz), rng = 0x10 (8 Gauss), osr = 0x00 (512 samples)
    compass.setMode(0x01, 0x04, 0x10, 0x00);

    if (!bmp.begin()) {
        Serial.println("Ooops, no BMP180 detected ... Check your wiring or I2C ADDR!");
        while (1);
    }

    dht.begin();
    LoadCell.begin();
    LoadCell.start(2000); // tare time in ms
}


void loop() {
    if (!mqttClient.connected()) {
         connectToMQTT();
     }
    mqttClient.loop();
  
    sensors_event_t event;
    bmp.getEvent(&event);
  
    float temperature = 0;
    if (event.pressure) {
        bmp.getTemperature(&temperature);
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.print(" C\tPressure: ");
        Serial.print(event.pressure);
        Serial.println(" hPa");
    }

    compass.read();
    float x = compass.getX();
    float y = compass.getY();
    float z = compass.getZ();
    Serial.print("Magnetic X: ");
    Serial.print(x);
    Serial.print(" uT, Y: ");
    Serial.print(y);
    Serial.print(" uT, Z: ");
    Serial.print(z);
    Serial.println(" uT");
     int a = compass.getAzimuth();
  
  Serial.print("A: ");
  Serial.print(a);
  Serial.println();

    float humidity = dht.readHumidity();
    float tempDHT = dht.readTemperature();
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print(" %\tTemperature: ");
    Serial.print(tempDHT);
    Serial.println(" *C");

    float weight = 0;
    if (LoadCell.update()) {
        weight = LoadCell.getData();
        Serial.print("Weight: ");
        Serial.print(weight);
        Serial.println(" g");
    }

    sendData(temperature, tempDHT, event.pressure, humidity, weight, x, y, z);
    delay(2000); // Wait a few seconds between reads
}

void sendData(float temperature, float tempDHT, float pressure, float humidity, float weight, float x, float y, float z) {
    StaticJsonDocument<300> jsonDocument;
    jsonDocument["temperature"] = temperature;
    jsonDocument["tempDHT"] = tempDHT;
    jsonDocument["pressure"] = pressure;
    jsonDocument["humidity"] = humidity;
    jsonDocument["weight"] = weight;
    jsonDocument["magnetic_x"] = x;
    jsonDocument["magnetic_y"] = y;
    jsonDocument["magnetic_z"] = z;
    String requestBody;
    serializeJson(jsonDocument, requestBody);
    //Uncomment and update for MQTT or other uses
     if (mqttClient.publish(mqtt_topic, requestBody.c_str())) {
         Serial.println("Data sent to MQTT");
     } else {
         Serial.println("Failed to send data to MQTT");
    }
}
