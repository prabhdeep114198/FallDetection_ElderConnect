#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// WiFi credentials
const char* ssid = "elderconnect";
const char* password = "CommandPrompt";
commit fall
// API URL
const char* serverURL = "https://elderconnect-api-esfdawb8drara7ge.centralindia-01.azurewebsites.net/api/v1/public/devices/7db5c7a2-3706-47a7-8ae5-8dcf48d2d3fa/users/2d5c135f-c131-4913-a5a5-e019ae4e6ed4/sos";

// Sensor values
float accX, accY, accZ;
float totalAcc;

// Cooldown to avoid multiple requests
unsigned long lastTriggerTime = 0;
const int cooldownTime = 5000; // 5 seconds

void setup() {

  Serial.begin(115200);
  Wire.begin();

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("MPU6050 not detected");
    while (1);
  }

  Serial.println("MPU6050 Ready");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
}

void loop() {

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  accX = a.acceleration.x;
  accY = a.acceleration.y;
  accZ = a.acceleration.z;

  totalAcc = sqrt(accX * accX + accY * accY + accZ * accZ);

  Serial.print("Acceleration: ");
  Serial.println(totalAcc);

  // -------------------------
  // FREE FALL DETECTION
  // -------------------------
  if (totalAcc < 4.5 && millis() - lastTriggerTime > cooldownTime) {

    Serial.println("Free fall detected...");
    delay(300);

    // Re-check to confirm
    sensors_event_t a2, g2, t2;
    mpu.getEvent(&a2, &g2, &t2);

    float confirmAcc = sqrt(
      a2.acceleration.x * a2.acceleration.x +
      a2.acceleration.y * a2.acceleration.y +
      a2.acceleration.z * a2.acceleration.z
    );

    Serial.print("Confirm Acc: ");
    Serial.println(confirmAcc);

    // If still low → real free fall
    if (confirmAcc < 6) {

      Serial.println("CONFIRMED FREE FALL!");
      sendFallAlert();

      lastTriggerTime = millis(); // update cooldown
    } else {
      Serial.println("False trigger");
    }
  }

  delay(150);
}

// ==============================
// 🚀 SEND ALERT TO AZURE
// ==============================

void sendFallAlert() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();   // required for HTTPS

  HTTPClient http;

  Serial.println("\n--- Sending POST Request ---");

  // IMPORTANT: use HTTP/1.1 (default) → DO NOT use HTTP10
  if (!http.begin(client, serverURL)) {
    Serial.println("HTTP begin failed");
    return;
  }

  // Headers (exact match with curl)
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", "YOUR_API_KEY");
  http.addHeader("Accept", "application/json");

  // CLEAN JSON (NO formatting issues)
  String jsonData = "{\"type\":\"fall_detection\",\"description\":\"Hardware device detected a severe fall\",\"latitude\":30.7333,\"longitude\":76.7794,\"priority\":\"critical\"}";

  Serial.println("Sending JSON:");
  Serial.println(jsonData);

  int httpResponseCode = http.POST(jsonData);

  Serial.print("HTTP Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Response:");
    Serial.println(response);
  } else {
    Serial.print("POST failed: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();

  Serial.println("--- Request Done ---\n");
}