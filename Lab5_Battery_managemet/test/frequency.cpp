#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <FirebaseClient.h>

// ------------------------- Pins -------------------------
#define TRIG_PIN 4
#define ECHO_PIN 5

// ------------------------- WiFi -------------------------
const char* ssid = "UW MPSK";
const char* wifi_password = "9Wvk7TMigsjVESWU";

// ------------------------- Firebase -------------------------
UserAuth user_auth(
  "AIzaSyD2BMaQc5XNessW-FCuf562Vh_-FLB6CF4",
  "rhubarb.wong@gmail.com",
  "@Chee0719"
);

FirebaseApp app;
WiFiClientSecure ssl_client1, ssl_client2;
using AsyncClient = AsyncClientClass;
AsyncClient async_client1(ssl_client1), async_client2(ssl_client2);
RealtimeDatabase Database;

const char* firebase_db_url = "https://lab5-6d6de-default-rtdb.firebaseio.com";

void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;
}

// ------------------------- Ultrasonic -------------------------
float getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration <= 0) return -1.0f;
  return (duration * 0.034f) / 2.0f;
}

// ------------------------- WiFi -------------------------
bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifi_password);
  
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 10000) {
    delay(200);
  }
  return (WiFi.status() == WL_CONNECTED);
}

// ------------------------- Firebase -------------------------
bool setupFirebase() {
  ssl_client1.setInsecure();
  ssl_client2.setInsecure();

  initializeApp(async_client1, app, getAuth(user_auth), processData, "authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(firebase_db_url);

  uint32_t t0 = millis();
  while (!app.ready() && (millis() - t0) < 10000) {
    app.loop();
    delay(50);
  }
  return app.ready();
}

// ------------------------- Test Function -------------------------
void testTransmissionRate(float hz, uint32_t duration_ms) {
  uint32_t interval_ms = (uint32_t)(1000.0f / hz);
  
  Serial.print("\n=== Testing ");
  Serial.print(hz);
  Serial.print(" Hz (every ");
  Serial.print(interval_ms);
  Serial.println(" ms) ===");
  
  uint32_t t0 = millis();
  uint32_t lastSend = 0;
  int count = 0;
  
  while (millis() - t0 < duration_ms) {
    app.loop();
    
    if (millis() - lastSend >= interval_ms) {
      float d = getDistanceCM();
      if (d > 0 && app.ready()) {
        Database.set<float>(async_client1, "/rate_test/distance", d, processData, "distTask");
        count++;
      }
      lastSend = millis();
    }
    delay(10);
  }
  
  Serial.print("Sent ");
  Serial.print(count);
  Serial.println(" readings");
}

// ------------------------- Setup -------------------------
void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  delay(500);
  Serial.println("\n=== Transmission Rate Power Test ===");
  
  // Connect WiFi
  Serial.println("Connecting WiFi...");
  if (!connectWiFi()) {
    Serial.println("WiFi failed!");
    return;
  }
  Serial.println("WiFi connected");
  
  // Setup Firebase
  Serial.println("Setting up Firebase...");
  if (!setupFirebase()) {
    Serial.println("Firebase failed!");
    return;
  }
  Serial.println("Firebase ready");
  
  // 测试5种频率，每种30秒
  Serial.println("\nStarting 5 transmission rate tests (30s each)...");
  
  // 1. 2 Hz (500ms interval)
  testTransmissionRate(2.0, 30000);
  
  // 2. 1 Hz (1000ms interval)
  testTransmissionRate(1.0, 30000);
  
  // 3. 0.5 Hz (2000ms interval)
  testTransmissionRate(0.5, 30000);
  
  // 4. 0.333 Hz (3000ms interval)
  testTransmissionRate(0.333, 30000);
  
  // 5. 0.25 Hz (4000ms interval)
  testTransmissionRate(0.25, 30000);
  
  Serial.println("\n=== All tests completed ===");
}

void loop() {
}