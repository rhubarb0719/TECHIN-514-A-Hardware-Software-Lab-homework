#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <FirebaseClient.h>

#define TRIG_PIN 4  // D4
#define ECHO_PIN 5  // D5

#define NORMAL_SLEEP_SEC 5
#define LONG_SLEEP_SEC 30
#define DISTANCE_THRESHOLD 15.0
#define NO_MOTION_COUNT 6

// ------------------------- RTC -------------------------
RTC_DATA_ATTR float lastDistance = -1.0;
RTC_DATA_ATTR int noMotionCounter = 0;
RTC_DATA_ATTR int wakeCount = 0;

// ------------------------- WiFi/Firebase -------------------------
const char* ssid = "UW MPSK";
const char* wifi_password = "YOUR_FIREBASE_API_KEY";
const char* firebase_db_url = "*";
UserAuth user_auth("*");

FirebaseApp app;
WiFiClientSecure ssl_client1;
using AsyncClient = AsyncClientClass;
AsyncClient async_client1(ssl_client1);
RealtimeDatabase Database;

void processData(AsyncResult &aResult) { }

// ------------------------- sensor -------------------------
float getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 40000); 
  
  if (duration == 0) {
    Serial.println("[Sensor Error] Timeout! Check wiring/Voltage.");
    return -1.0f;
  }
  return (duration * 0.034f) / 2.0f;
}

// ------------------------- WiFi only -------------------------
void uploadState(float dist, bool detected) {
  WiFi.begin(ssid, wifi_password);
  Serial.print("Connecting WiFi");
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) { 
    delay(200);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK!");
    ssl_client1.setInsecure();
    initializeApp(async_client1, app, getAuth(user_auth), processData, "auth");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(firebase_db_url);
    
    unsigned long t0 = millis();
    while(!app.ready() && (millis() - t0 < 5000)) app.loop();

    if(app.ready()){
      Database.set<float>(async_client1, "/motion_sensor/distance", dist, processData);
      Database.set<bool>(async_client1, "/motion_sensor/motion_detected", detected, processData);
      Database.set<String>(async_client1, "/motion_sensor/status", detected ? "MOVEMENT" : "STATIC", processData);
      // 强制运行一次 Loop 确保发送
      unsigned long t1 = millis();
      while(millis() - t1 < 1000) app.loop(); 
      Serial.println("Upload Done.");
    }
  } else {
    Serial.println(" WiFi Fail.");
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// ------------------------- Setup -------------------------
void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  wakeCount++;
  Serial.println("\n--- Wake Up ---");

  // 1. 获取平滑后的距离
  float currentDistance = -1;
  float sum = 0;
  int validCounts = 0;
  
  for(int i=0; i<3; i++) {
    float d = getDistanceCM();
    if(d > 2.0 && d < 400.0) { // 过滤掉 0cm 或超大值的噪音
      sum += d;
      validCounts++;
    }
    delay(30);
  }
  
  if(validCounts > 0) currentDistance = sum / validCounts;

  Serial.printf("Curr: %.1f cm | Last: %.1f cm\n", currentDistance, lastDistance);

  // 2. 核心逻辑判断
  bool shouldUpload = false;
  int sleepTime = NORMAL_SLEEP_SEC;

  if (currentDistance < 0) {
    Serial.println("Invalid Sensor Reading. Sleeping.");
    // 读数失败不更新 lastDistance
  } 
  else if (lastDistance < 0) {
    // === 第一次运行的情况 ===
    Serial.println("First Run (Calibration). No upload.");
    lastDistance = currentDistance; // 建立基准
  } 
  else {
    // === 正常对比 ===
    float diff = abs(currentDistance - lastDistance);
    Serial.printf("Diff: %.1f cm\n", diff);

    if (diff > DISTANCE_THRESHOLD) {
      Serial.println(">>> MOTION DETECTED! <<<");
      shouldUpload = true;
      noMotionCounter = 0;
      lastDistance = currentDistance; // 更新基准：只有在剧烈变化时才更新？
      // 策略选择：如果移动了，我们更新基准为当前位置，这样如果人不动了，下次就不触发
    } else {
      Serial.println("No significant motion.");
      noMotionCounter++;
      // 如果你希望在微小变化时也慢慢更新基准，取消下面注释
      // lastDistance = currentDistance; 
    }
  }

  // 3. 执行上传
  if (shouldUpload) {
    uploadState(currentDistance, true);
  }

  // 4. 决定睡眠策略
  if (noMotionCounter >= NO_MOTION_COUNT) {
    Serial.println("Long Idle -> 30s Sleep");
    sleepTime = LONG_SLEEP_SEC;
  } else {
    Serial.println("Monitoring -> 5s Sleep");
  }

  // 5. 进入深度睡眠
  Serial.flush();
  esp_sleep_enable_timer_wakeup(sleepTime * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() { }