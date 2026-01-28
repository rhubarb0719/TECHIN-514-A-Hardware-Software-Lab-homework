#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

static BLEUUID SERVICE_UUID("ec5d3da5-5fc1-4f03-9164-c586a51a4f02");
static BLEUUID CHAR_UUID   ("e09301b1-b541-408b-b93a-92ae95f5d767");

static const char* TARGET_NAME = "Lukina_BLE_Server";
static const bool  MATCH_BY_NAME = true;
static const bool  MATCH_BY_UUID = true;

static bool doConnect = false;
static bool connected = false;
static bool doScan = false;

static BLEAdvertisedDevice* myDevice = nullptr;
static BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;

// ========== 添加：数据统计变量 ==========
float currentDistance = 0.0;
float maxDistance = -999999.0;
float minDistance = 999999.0;
int receiveCount = 0;

// ========== 添加：数据聚合函数 ==========
void updateDistanceStats(float newValue) {
  currentDistance = newValue;

  if (newValue > maxDistance) {
    maxDistance = newValue;
  }

  if (newValue < minDistance) {
    minDistance = newValue;
  }

  receiveCount++;
}

// ========== 修改：notifyCallback函数 ==========
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify
) {
  // 将接收到的数据转换为字符串
  String dataStr = "";
  for (size_t i = 0; i < length; i++) {
    dataStr += (char)pData[i];
  }

  // 将字符串转换为浮点数（距离值）
  float distance = dataStr.toFloat();

  // 更新统计数据
  updateDistanceStats(distance);

  // 打印所需的三个数据
  Serial.println("========================================");
  Serial.println("----- New Data Received -----");
  Serial.print("Current Distance: ");
  Serial.print(currentDistance);
  Serial.println(" cm");
  
  Serial.print("Max Distance: ");
  Serial.print(maxDistance);
  Serial.println(" cm");
  
  Serial.print("Min Distance: ");
  Serial.print(minDistance);
  Serial.println(" cm");
  
  Serial.print("Receive Count: ");
  Serial.println(receiveCount);
  Serial.println("========================================");
}

class MyClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pClient) override {
    Serial.println("[CLIENT] onConnect");
  }

  void onDisconnect(BLEClient* pClient) override {
    connected = false;
    Serial.println("[CLIENT] onDisconnect -> will rescan");
    doScan = true;
  }
};

bool connectToServer() {
  if (!myDevice) return false;

  Serial.print("[CLIENT] Connecting to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallbacks());

  if (!pClient->connect(myDevice)) {
    Serial.println("[CLIENT] Connect failed");
    return false;
  }
  Serial.println("[CLIENT] Connected");

  pClient->setMTU(517);

  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (!pRemoteService) {
    Serial.println("[CLIENT] Service not found (UUID mismatch?)");
    pClient->disconnect();
    return false;
  }
  Serial.println("[CLIENT] Service found");

  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHAR_UUID);
  if (!pRemoteCharacteristic) {
    Serial.println("[CLIENT] Characteristic not found (UUID mismatch?)");
    pClient->disconnect();
    return false;
  }
  Serial.println("[CLIENT] Characteristic found");

  if (pRemoteCharacteristic->canRead()) {
    std::string v = pRemoteCharacteristic->readValue();
    Serial.print("[CLIENT] Read: ");
    Serial.println(v.c_str());
  }

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("[CLIENT] Notify registered");
  } else {
    Serial.println("[CLIENT] Not notify-capable");
  }

  connected = true;
  return true;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    std::string nm = advertisedDevice.getName();
    Serial.printf("[SCAN] name=%s addr=%s\n",
                  nm.c_str(),
                  advertisedDevice.getAddress().toString().c_str());

    if (nm.find("Lukina") != std::string::npos) {   // ✅ 不怕空格/下划线
      Serial.println("[SCAN] >>> Target found! Stop scan & connect.");
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("=== Yilu BLE Client Start ===");

  BLEDevice::init("Yilu_Client");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  Serial.println("[SCAN] Start scanning...");
  pBLEScan->start(0, false); // scan forever
}

void loop() {
  if (doConnect) {
    if (connectToServer()) {
      Serial.println("[MAIN] Connected ✅");
    } else {
      Serial.println("[MAIN] Connect failed ❌ (resume scan)");
      BLEDevice::getScan()->start(0);
    }
    doConnect = false;
  }

  if (connected && pRemoteCharacteristic) {
    if (pRemoteCharacteristic->canWrite()) {
      String msg = "Hi Server, t=" + String(millis() / 1000);
      Serial.print("[CLIENT][WRITE] ");
      Serial.println(msg);
      pRemoteCharacteristic->writeValue((uint8_t*)msg.c_str(), msg.length());
    } else {
      Serial.println("[CLIENT][WRITE] Not writable");
    }
  } else if (doScan) {
    Serial.println("[SCAN] Resuming scan...");
    BLEDevice::getScan()->start(0);
    doScan = false;
  }

  delay(1000);
}