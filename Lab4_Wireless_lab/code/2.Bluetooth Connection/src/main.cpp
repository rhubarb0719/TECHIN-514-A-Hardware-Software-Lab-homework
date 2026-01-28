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
static BLEClient* pClient = nullptr;
static String serverDeviceName = "";  // 存储服务器设备名称

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify
) {
  // ✅ 显示服务器设备名称和接收到的数据
  Serial.print("Server Name: ");
  Serial.print(serverDeviceName);
  Serial.print(" | Data: ");
  
  // 将接收到的数据转换为字符串显示
  String receivedData = "";
  for(size_t i = 0; i < length; i++) {
    receivedData += (char)pData[i];
  }
  Serial.println(receivedData);
}

class MyClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pClient) override {
    Serial.println("[CLIENT] Connected to server");
  }

  void onDisconnect(BLEClient* pClient) override {
    connected = false;
    pRemoteCharacteristic = nullptr;
    Serial.println("[CLIENT] Disconnected -> rescanning...");
    
    if (myDevice) {
      delete myDevice;
      myDevice = nullptr;
    }
    
    serverDeviceName = "";  // ✅ 清空设备名称
    doScan = true;
  }
};

bool connectToServer() {
  if (!myDevice) return false;

  // ✅ 获取并保存服务器设备名称
  serverDeviceName = String(myDevice->getName().c_str());
  
  Serial.print("[CLIENT] Connecting to: ");
  Serial.println(serverDeviceName);
  Serial.print("[CLIENT] Address: ");
  Serial.println(myDevice->getAddress().toString().c_str());

  if (pClient) {
    delete pClient;
    pClient = nullptr;
  }

  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallbacks());

  if (!pClient->connect(myDevice)) {
    Serial.println("[CLIENT] Connection failed");
    delete pClient;
    pClient = nullptr;
    return false;
  }

  pClient->setMTU(517);

  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (!pRemoteService) {
    Serial.println("[CLIENT] Service not found");
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
    return false;
  }

  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHAR_UUID);
  if (!pRemoteCharacteristic) {
    Serial.println("[CLIENT] Characteristic not found");
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
    return false;
  }

  if (pRemoteCharacteristic->canRead()) {
    std::string v = pRemoteCharacteristic->readValue();
    Serial.print("[CLIENT] Initial read: ");
    Serial.println(v.c_str());
  }

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("[CLIENT] Notifications enabled ✅");
  }

  connected = true;
  return true;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    std::string nm = advertisedDevice.getName();
    
    if (nm.find("Lukina") != std::string::npos) {
      Serial.print("[SCAN] Found target: ");
      Serial.println(nm.c_str());
      
      BLEDevice::getScan()->stop();
      
      if (myDevice) {
        delete myDevice;
      }
      
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = false;
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println("\n=== BLE Client Started ===");

  BLEDevice::init("Yilu_Client");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  Serial.println("[SCAN] Scanning for BLE devices...");
  pBLEScan->start(0, false);
}

void loop() {
  if (doConnect) {
    if (connectToServer()) {
      Serial.println("[STATUS] Connection successful ✅");
    } else {
      Serial.println("[STATUS] Connection failed ❌");
      doScan = true;
    }
    doConnect = false;
  }

  if (connected && pRemoteCharacteristic && pRemoteCharacteristic->canWrite()) {
    String msg = "Hi Server, t=" + String(millis() / 1000);
    pRemoteCharacteristic->writeValue((uint8_t*)msg.c_str(), msg.length());
    delay(1000);
  } else if (doScan && !connected) {
    Serial.println("[SCAN] Resuming scan...");
    BLEDevice::getScan()->start(0, false);
    doScan = false;
  }

  delay(1000);
}