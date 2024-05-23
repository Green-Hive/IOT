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
DHT dhtBottomLeft(DHTPIN1, DHTTYPE);    // Initialize bottom part, lower left DHT sensor
DHT dhtTopRight(DHTPIN2, DHTTYPE);      // Initialize top part, upper right DHT sensor
DHT dhtOutside(DHTPIN3, DHTTYPE);       // Initialize outside DHT sensor
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
HX711_ADC LoadCell(5, 17);
QMC5883LCompass compass;

void sendData(float tempBottomLeft, float tempTopRight, float tempOutside, float pressure, float humidityBottomLeft, float humidityTopRight, float humidityOutside, float weight, float x, float y, float z);

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

    dhtBottomLeft.begin();
    dhtTopRight.begin();
    dhtOutside.begin();
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

    float humidityBottomLeft = dhtBottomLeft.readHumidity();
    float tempBottomLeft = dhtBottomLeft.readTemperature();
    float humidityTopRight = dhtTopRight.readHumidity();
    float tempTopRight = dhtTopRight.readTemperature();
    float humidityOutside = dhtOutside.readHumidity();
    float tempOutside = dhtOutside.readTemperature();

    float weight = 0;
    if (LoadCell.update()) {
        weight = LoadCell.getData();
        Serial.print("Weight: ");
        Serial.print(weight);
        Serial.println(" g");
    }

    sendData(tempBottomLeft, tempTopRight, tempOutside, event.pressure, humidityBottomLeft, humidityTopRight, humidityOutside, weight, x, y, z);
    delay(2000); // Wait a few seconds between reads
}

void sendData(float tempBottomLeft, float tempTopRight, float tempOutside, float pressure, float humidityBottomLeft, float humidityTopRight, float humidityOutside, float weight, float x, float y, float z) {
    StaticJsonDocument<500> jsonDocument;
    jsonDocument["tempBottomLeft"] = tempBottomLeft;
    jsonDocument["tempTopRight"] = tempTopRight;
    jsonDocument["tempOutside"] = tempOutside;
    jsonDocument["pressure"] = pressure;
    jsonDocument["humidityBottomLeft"] = humidityBottomLeft;
    jsonDocument["humidityTopRight"] = humidityTopRight;
    jsonDocument["humidityOutside"] = humidityOutside;
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