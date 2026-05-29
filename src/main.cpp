#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

// I2C引脚定义
#define SDA_PIN 8
#define SCL_PIN 9

// BMP280对象
Adafruit_BMP280 bmp; // I2C模式
bool bmp_ready = false;

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

// I2C扫描函数
void scanI2C() {
  byte error, address;
  int nDevices;
  
  Serial.println("Scanning I2C bus...");
  nDevices = 0;
  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) 
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) 
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found!");
  else
    Serial.println("done");
}

void setup() {
  Serial.begin(115200);
  delay(2000); // 增加等待时间确保串口连接
  
  Serial.println("\n========== System Start ==========");
  Serial.println("Serial initialized!");
  
  // 初始化I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); // 使用100kHz更稳定
  
  // 扫描I2C总线
  scanI2C();
  
  // 尝试初始化BMP280（尝试两个常见地址）
  Serial.println("\nTrying to initialize BMP280...");
  if (bmp.begin(0x76)) {
    Serial.println("BMP280 found at address 0x76");
    bmp_ready = true;
  } else if (bmp.begin(0x77)) {
    Serial.println("BMP280 found at address 0x77");
    bmp_ready = true;
  } else {
    Serial.println("BMP280 NOT FOUND! Please check wiring.");
    bmp_ready = false;
  }
  
  if (bmp_ready) {
    // 配置BMP280
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     // 模式: 正常模式
                    Adafruit_BMP280::SAMPLING_X2,     // 温度采样
                    Adafruit_BMP280::SAMPLING_X16,    // 气压采样
                    Adafruit_BMP280::FILTER_X16,      // 滤波器
                    Adafruit_BMP280::STANDBY_MS_500); // 待机时间
    
    Serial.println("BMP280 initialized successfully!");
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
  
  Serial.println("========== Setup Complete ==========\n");
}

void loop() {
  Serial.println("===============");
  
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
    } else {
      Serial.println("[AHT20] Read failed");
    }
  } else {
    Serial.println("[AHT20] Not ready");
  }
  
  delay(5000); // 每5秒采集一次
}