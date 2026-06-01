#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>


// I2C引脚定义
#define SDA_PIN 8
#define SCL_PIN 9

// BMP280对象
Adafruit_BMP280 bmp; // I2C模式
bool bmp_ready = false;

// BLE服务和特征UUID
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
unsigned long previousMillis = 0;
const long interval = 2000;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("BLE设备已连接");
    // 获取当前连接数
    int count = pServer->getConnectedCount();
    Serial.printf("当前连接设备数: %d\n", count);
    // ESP32-C3最多支持3个并发连接
    if (count >= 3) {
      Serial.println("已达到最大连接数(3个)");
    }
    // 关键：连接后继续广播，允许其他设备发现
    pServer->startAdvertising();
  }
  void onDisconnect(BLEServer* pServer) {
    Serial.println("BLE设备已断开");
    pServer->startAdvertising();
  }
};


// AHT20相关定义
#define AHT20_ADDR 0x38
#define AHT20_CMD_INIT 0xBE
#define AHT20_CMD_MEASURE 0xAC
bool aht20_ready = false;

// AHT20初始化
bool AHT20_Init() {
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(AHT20_CMD_INIT);
  Wire.write(0x08);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  delay(100);
  
  // 检查校准状态
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0x71);
  Wire.endTransmission(false);
  Wire.requestFrom(AHT20_ADDR, 1);
  if (Wire.available()) {
    byte status = Wire.read();
    if ((status & 0x68) == 0x08) {
      return true;
    }
  }
  return false;
}

// 读取AHT20数据
bool AHT20_Read(float &temperature, float &humidity) {
  // 发送测量命令
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(AHT20_CMD_MEASURE);
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  
  // 循环等待数据就绪（最多等待500ms）
  int timeout = 50;
  byte status = 0;
  while (timeout--) {
    Wire.requestFrom(AHT20_ADDR, 1);
    if (Wire.available()) {
      status = Wire.read();
      if (status & 0x80) {
        break; // 数据就绪
      }
    }
    delay(10);
  }
  
  if (!(status & 0x80)) {
    return false;
  }
  
  // 读取完整的7字节（状态+数据）
  Wire.requestFrom(AHT20_ADDR, 7);
  if (Wire.available() != 7) {
    return false;
  }
  
  byte data[7];
  for (int i = 0; i < 7; i++) {
    data[i] = Wire.read();
  }
  
  // 计算湿度 (data[1], data[2], data[3]的高4位)
  uint32_t hum_raw = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((data[3] >> 4) & 0x0F);
  humidity = (float)hum_raw / 1048576.0 * 100.0;
  
  // 计算温度 (data[3]的低4位, data[4], data[5])
  uint32_t temp_raw = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
  temperature = (float)temp_raw / 1048576.0 * 200.0 - 50.0;
  
  return true;
}


void setup() {
  Serial.begin(115200);
  Serial.println("初始化BLE服务...");

  BLEDevice::init("ESP32-C3-BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  // 设置广播参数：即使有设备连接也继续广播，允许其他设备发现
  pAdvertising->setMinPreferred(0x06);  // 最小广告间隔
  pAdvertising->setMaxPreferred(0x10);  // 最大广告间隔
  // 使用默认的可连接广播类型(ADV_TYPE_IND)
  BLEDevice::startAdvertising();
  Serial.println("BLE服务已启动，支持多设备连接...");


  // 初始化I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); // 使用100kHz更稳定
  if (bmp.begin(0x77)) {
    bmp_ready = true;
    // 配置BMP280
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     // 模式: 正常模式
                    Adafruit_BMP280::SAMPLING_X2,     // 温度采样
                    Adafruit_BMP280::SAMPLING_X16,    // 气压采样
                    Adafruit_BMP280::FILTER_X16,      // 滤波器
                    Adafruit_BMP280::STANDBY_MS_500); // 待机时间
    
    Serial.println("BMP280 initialized successfully!");
  } else {
    Serial.println("BMP280 initialization failed!");
  }
  // 初始化AHT20
  Serial.println("\nTrying to initialize AHT20...");
  if (AHT20_Init()) {
    Serial.println("AHT20 found at address 0x38");
    Serial.println("AHT20 initialized successfully!");
    aht20_ready = true;
  } else {
    Serial.println("AHT20 NOT FOUND! Please check wiring.");
    aht20_ready = false;
  }  
}

// 一对多广播函数 - 向所有已连接设备发送数据
void notifyAllClients(const char* data) {
  if (pCharacteristic == NULL) return;
  
  // 设置要发送的值
  pCharacteristic->setValue(data);
  
  // notify()不带参数时，会向所有已订阅的客户端广播
  pCharacteristic->notify();
  
  Serial.printf("已向所有设备广播: %s\n", data);
}

void loop() {
  // 检查是否有设备连接
  int connectedCount = pServer->getConnectedCount();
  if (connectedCount > 0) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {

  // 读取BMP280数据
  if (bmp_ready) {
    float bmp_temp = bmp.readTemperature();
    float pressure = bmp.readPressure() / 100.0F; // 转换为hPa
    float altitude = bmp.readAltitude(1013.25); // 海平面气压为1013.25 hPa
    
    Serial.print(F("[BMP280] 温度: "));
    Serial.print(bmp_temp);
    Serial.println(F(" *C"));
    
    Serial.print(F("[BMP280] 气压: "));
    Serial.print(pressure);
    Serial.println(F(" hPa"));
    
    Serial.print(F("[BMP280] 海拔: "));
    Serial.print(altitude);
    Serial.println(F(" m"));


      // 将传感器数据格式化为字符串（包含中文标签）
      String data = "[BMP280] 温度:" + String(bmp_temp, 1) + "C,气压:" + String(pressure, 1) + "hPa" ;
      notifyAllClients(data.c_str());
  } else {
    Serial.println("[BMP280] Not ready");
  }

  // 读取AHT20数据
  if (aht20_ready) {
    float aht_temp, humidity;
    if (AHT20_Read(aht_temp, humidity)) {
      Serial.print(F("[AHT20] 温度: "));
      Serial.print(aht_temp);
      Serial.println(F(" *C"));
      
      Serial.print(F("[AHT20] 湿度: "));
      Serial.print(humidity);
      Serial.println(F(" %"));

      String data = "[AHT20]温度:" + String(aht_temp, 1) + "C,湿度:" + String(humidity, 1) + "%" ;
      notifyAllClients(data.c_str());
    } else {
      Serial.println("[AHT20] Read failed");
    }
  } else {
    Serial.println("[AHT20] Not ready");
  }
  
      previousMillis = currentMillis;

    }
  }
  delay(10);
}
