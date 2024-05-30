#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <HX711_ADC.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <QMC5883LCompass.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// WiFi credentials (choose the appropriate one and comment out the others)
// const char *ssid = "GloryHammer";
// const char *password = "ajfnmqbdbf89jka";
// const char *ssid = "Freebox-3ED006";
// const char *password = "putatote7-mulatus?-ammittebat-subarmalis!";
const char *ssid = "Raspberry";
const char *password = "ScNmHf!CbmmFTH!";

// MQTT Broker details
const char *mqtt_server = "82.66.182.144";
const char *mqtt_topic = "test/sensor/data";

// Hive and user IDs (replace with your own)
const char *hive_id = "ff34f9a1-3916-4cc4-be69-992cade503c1";
const char *user_id = "4ca69f33-04c1-4b00-81cc-706dc490a3cc";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;

// Initialize NTP client
// We use the NTP (Network Time Protocol) to obtain the current time from a remote time server.
// This allows our ESP32 to have an accurate time reference, even without an integrated real-time clock (RTC).
// 
// The NTP client is initialized with two parameters:
// 1. ntpUDP: a WiFiUDP object that handles the UDP communication for the NTP protocol.
// 2. "europe.pool.ntp.org": the address of the NTP server we are using. Here, we use a pool of European NTP servers to get the time.
//
// By using an NTP server, our ESP32 can synchronize with UTC (Coordinated Universal Time) and get an accurate timestamp.
// This is particularly useful for timestamping sensor data and ensuring that our measurements are associated with the correct time.
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org");

// Initialize sensors
#define DHTPIN1 2   // GPIO pin connected to the DHT sensor at bottom part, lower left
#define DHTPIN2 4   // GPIO pin connected to the DHT sensor at top part, upper right
#define DHTPIN3 27  // GPIO pin connected to the DHT sensor outside
#define DHTTYPE DHT22   // DHT sensor type
DHT dhtBottomLeft(DHTPIN1, DHTTYPE);  // Initialize bottom part, lower left DHT sensor
DHT dhtTopRight(DHTPIN2, DHTTYPE);    // Initialize top part, upper right DHT sensor
DHT dhtOutside(DHTPIN3, DHTTYPE);     // Initialize outside DHT sensor
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
HX711_ADC LoadCell(5, 17);  // HX711 load cell connected to GPIO pins 5 and 17
QMC5883LCompass compass;

void sendData(float tempBottomLeft, float tempTopRight, float tempOutside, float pressure, float humidityBottomLeft, float humidityTopRight, float humidityOutside, float weight, float x, float y, float z);

void connectToWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Test network connectivity
    Serial.println("Testing network connectivity...");
    IPAddress ip;
    if (WiFi.hostByName("82.66.182.144", ip)) {
        Serial.print("MQTT Broker reachable at ");
        Serial.println(ip);
    } else {
        Serial.println("MQTT Broker not reachable.");
    }
}

void connectToMQTT() {
    mqttClient.setServer(mqtt_server, 1883);
    mqttClient.setBufferSize(1024);  // Increase buffer size if needed

    while (!mqttClient.connected()) {
        Serial.println("Connecting to MQTT...");
        if (mqttClient.connect("ESP32Client")) {
            Serial.println("Connected to MQTT");
        } else {
            Serial.print("Failed to connect to MQTT, rc=");
            Serial.print(mqttClient.state());
            delay(5000);  // Retry after 5 seconds
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);  // Wait for serial port to connect

    connectToWiFi();
    connectToMQTT();

    compass.init();
    Serial.println("Compass initialized.");
    compass.setMode(0x01, 0x04, 0x10, 0x00);  // Configure compass mode

    if (!bmp.begin()) {
        Serial.println("No BMP180 sensor detected. Check wiring or I2C address!");
        while (1);  // Halt execution if sensor not found
    }

    dhtBottomLeft.begin();
    dhtTopRight.begin();
    dhtOutside.begin();

    LoadCell.begin();
    LoadCell.start(2000);  // Tare the load cell with a 2-second stabilization time

    // Wait for time synchronization
    // We need to wait for the NTP client to synchronize with the time server before we can use the timestamps.
    // The timeClient.update() method tries to synchronize with the NTP server.
    // If the synchronization fails (e.g., due to a poor network connection), we force an update with timeClient.forceUpdate().
    // We repeat this process until the synchronization succeeds.
    timeClient.begin();  // Initialize NTP client
    while (!timeClient.update()) {
        timeClient.forceUpdate();  // Wait for time synchronization
    }
    // Once the synchronization is successful, we can get the current timestamp using timeClient.getEpochTime() later in the code.
}

void loop() {
    if (!mqttClient.connected()) {
        connectToMQTT();  // Reconnect to MQTT if disconnected
    }
    mqttClient.loop();

    sensors_event_t event;
    bmp.getEvent(&event);

    float tempBMP = 0;
    if (event.pressure) {
        bmp.getTemperature(&tempBMP);
        Serial.print("BMP180 Temperature: ");
        Serial.print(tempBMP);
        Serial.print(" C\tPressure: ");
        Serial.print(event.pressure);
        Serial.println(" hPa");
    }
    // Test: Verify BMP180 sensor readings

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
    // Test: Verify compass readings

    float humidityBottomLeft = dhtBottomLeft.readHumidity();
    float tempBottomLeft = dhtBottomLeft.readTemperature();
    Serial.print("Bottom Left Humidity: ");
    Serial.print(humidityBottomLeft);
    Serial.print(" %\tTemperature: ");
    Serial.print(tempBottomLeft);
    Serial.println(" *C");
    // Test: Verify bottom left DHT sensor readings

    float humidityTopRight = dhtTopRight.readHumidity();
    float tempTopRight = dhtTopRight.readTemperature();
    Serial.print("Top Right Humidity: ");
    Serial.print(humidityTopRight);
    Serial.print(" %\tTemperature: ");
    Serial.print(tempTopRight);
    Serial.println(" *C");
    // Test: Verify top right DHT sensor readings

    float humidityOutside = dhtOutside.readHumidity();
    float tempOutside = dhtOutside.readTemperature();
    Serial.print("Outside Humidity: ");
    Serial.print(humidityOutside);
    Serial.print(" %\tTemperature: ");
    Serial.print(tempOutside);
    Serial.println(" *C");
    // Test: Verify outside DHT sensor readings

    float weight = 0;
    if (LoadCell.update()) {
        weight = LoadCell.getData();
        Serial.print("Weight: ");
        Serial.print(weight);
        Serial.println(" g");
    }
    // Test: Verify load cell readings

    timeClient.update();  // Update the time from the NTP server at each iteration of the loop

    sendData(tempBottomLeft, tempTopRight, tempOutside, event.pressure, humidityBottomLeft, humidityTopRight, humidityOutside, weight, x, y, z);
    delay(1000);  // Wait for 1 second between readings
}

void sendData(float tempBottomLeft, float tempTopRight, float tempOutside, float pressure, float humidityBottomLeft, float humidityTopRight, float humidityOutside, float weight, float x, float y, float z) {
    StaticJsonDocument<1536> jsonDocument;
    jsonDocument["hive_id"] = hive_id;
    jsonDocument["user_id"] = user_id;

    // Get the current timestamp in UTC
    // Now that the NTP client is synchronized, we can get the current timestamp by calling timeClient.getEpochTime().
    // This method returns the number of seconds elapsed since January 1, 1970 (known as the "Unix timestamp" or "epoch time").
    // We store this timestamp in the epochTime variable to include it in the JSON data we send to the MQTT server.
    unsigned long epochTime = timeClient.getEpochTime();  // Get current timestamp in UTC

    JsonObject data = jsonDocument.createNestedObject("data");
    data["tempBottomLeft"] = tempBottomLeft;
    data["tempTopRight"] = tempTopRight;
    data["tempOutside"] = tempOutside;
    data["pressure"] = pressure;
    data["humidityBottomLeft"] = humidityBottomLeft;
    data["humidityTopRight"] = humidityTopRight;
    data["humidityOutside"] = humidityOutside;
    data["weight"] = weight;
    data["magnetic_x"] = x;
    data["magnetic_y"] = y;
    data["magnetic_z"] = z;
    data["timestamp"] = epochTime;

    String requestBody;
    serializeJson(jsonDocument, requestBody);
    Serial.print("Sending message to topic ");
    Serial.print(mqtt_topic);
    Serial.print(": ");
    Serial.println(requestBody);

    bool published = false;
    for (int i = 0; i < 3; i++) {
        if (mqttClient.publish(mqtt_topic, requestBody.c_str())) {
            Serial.println("Data sent to MQTT successfully");
            published = true;
            break;
        } else {
            Serial.print("Failed to send data to MQTT, state: ");
            Serial.println(mqttClient.state());
            delay(1000);  // Retry after 1 second
        }
    }
    if (!published) {
        Serial.println("Failed to publish after 3 attempts");
    }
    // Test: Verify MQTT message publishing
}