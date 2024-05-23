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
const char* ssid = "Pixel_1828";
const char* password = "papillon";

// MQTT Broker details
const char* mqtt_server = "172.201.14.60";
const char* mqtt_topic = "sensor/data";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Initialize sensors
#define DHTPIN1 2     // GPIO pin connected to the DHT sensor at bottom part, lower left
#define DHTPIN2 4     // GPIO pin connected to the DHT sensor at top part, upper right
#define DHTPIN3 27    // GPIO pin connected to the DHT sensor outside
#define DHTTYPE DHT22 // DHT sensor type
DHT dht1(DHTPIN1, DHTTYPE); // Initialize bottom part, lower left DHT sensor
DHT dht2(DHTPIN2, DHTTYPE); // Initialize top part, upper right DHT sensor
DHT dht3(DHTPIN3, DHTTYPE); // Initialize outside DHT sensor
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
HX711_ADC LoadCell(5, 17);
QMC5883LCompass compass;

void sendData(float avgInsideTemp, float tempDHT3, float pressure, float humidity1, float humidity2, float humidity3, float weight, float x, float y, float z);

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

    compass.init();
    Serial.println("Compass initialized.");
    compass.setMode(0x01, 0x04, 0x10, 0x00);

    if (!bmp.begin()) {
        Serial.println("Ooops, no BMP180 detected ... Check your wiring or I2C ADDR!");
        while (1);
    }

    dht1.begin();
    dht2.begin();
    dht3.begin();
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
  
    float tempBMP = 0;
    if (event.pressure) {
        bmp.getTemperature(&tempBMP);
    }

    compass.read();
    float x = compass.getX();
    float y = compass.getY();
    float z = compass.getZ();

    float humidity1 = dht1.readHumidity();
    float tempDHT1 = dht1.readTemperature();
    float humidity2 = dht2.readHumidity();
    float tempDHT2 = dht2.readTemperature();
    float humidity3 = dht3.readHumidity();
    float tempDHT3 = dht3.readTemperature();
    
    // Calculate average temperature of DHT1, DHT2, and BMP
    float avgInsideTemp = (tempDHT1 + tempDHT2 + tempBMP) / 3.0;

    Serial.print("Inside Average Temperature: ");
    Serial.print(avgInsideTemp);
    Serial.println(" *C");

    Serial.print("Outside Temperature: ");
    Serial.print(tempDHT3);
    Serial.println(" *C");

    float weight = 0;
    if (LoadCell.update()) {
        weight = LoadCell.getData();
        Serial.print("Weight: ");
        Serial.print(weight);
        Serial.println(" g");
    }

    sendData(avgInsideTemp, tempDHT3, event.pressure, humidity1, humidity2, humidity3, weight, x, y, z);
    delay(2000); // Wait a few seconds between reads
}

void sendData(float avgInsideTemp, float tempDHT3, float pressure, float humidity1, float humidity2, float humidity3, float weight, float x, float y, float z) {
    StaticJsonDocument<500> jsonDocument; // Increase size to accommodate additional data
    jsonDocument["avgInsideTemp"] = avgInsideTemp;
    jsonDocument["outsideTemp"] = tempDHT3;
    jsonDocument["pressure"] = pressure;
    jsonDocument["humidity1"] = humidity1;
    jsonDocument["humidity2"] = humidity2;
    jsonDocument["humidity3"] = humidity3;
    jsonDocument["weight"] = weight;
    jsonDocument["magnetic_x"] = x;
    jsonDocument["magnetic_y"] = y;
    jsonDocument["magnetic_z"] = z;
    String requestBody;
    serializeJson(jsonDocument, requestBody);
     if (mqttClient.publish(mqtt_topic, requestBody.c_str())) {
         Serial.println("Data sent to MQTT");
     } else {
         Serial.println("Failed to send data to MQTT");
    }
}
