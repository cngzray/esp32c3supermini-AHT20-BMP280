# 基于 ESP32-C3 SuperMini + AHT20 + BMP280 的串口环境监测工程
纯原生 I2C 驱动，无需第三方 AHT20 专用库，串口输出温湿度、气压、海拔数据。

## 项目简介
本项目使用 ESP32-C3 SuperMini 开发板，通过 I2C 总线同时驱动：
- **AHT20**：高精度温湿度传感器（I2C 地址 `0x38`），代码内置底层读写驱动，不依赖 Adafruit_AHTX0 库
- **BMP280**：气压/温度/海拔传感器（支持 `0x76` / `0x77` 双地址自动识别），采用官方 Adafruit 库驱动


<img width="300"  alt="image" src="https://github.com/user-attachments/assets/1d01f702-4c24-4f61-af52-3fab7cf8aee3" />

<img width="674" height="137" alt="image" src="https://github.com/user-attachments/assets/1573e90a-5c7c-4793-a922-78479b6a8f54" />


### 核心功能
1. 上电自动扫描全部 I2C 设备，打印在线传感器地址
2. 分别初始化 AHT20、BMP280，失败串口提示接线故障
3. 每 5 秒采集一次环境数据，串口打印完整参数：
   - BMP280：温度、气压(hPa)、理论海拔（基准海平面气压1013.25hPa）
   - AHT20：环境温度、相对湿度(%)
4. 硬件异常容错：传感器离线时跳过读取并打印提示
5. I2C 总线默认 100kHz，低速稳定适配廉价传感器模块

## 硬件清单
| 器件 | 型号说明 |
|------|--------|
| 主控 | ESP32-C3 SuperMini |
| 温湿度传感器 | AHT20（独立模块 / AHT20+BMP280二合一模块） |
| 气压传感器 | BMP280 |
| 辅助 | 杜邦线、面包板、USB数据线 |

## 硬件接线（代码固定引脚）
代码中 I2C 引脚定义：
```cpp
#define SDA_PIN 8
#define SCL_PIN 9
```
接线对照表：
| ESP32-C3 SuperMini | AHT20 / BMP280 I2C模块 |
|--------------------|------------------------|
| 3.3V               | VCC（严禁接5V！）|
| GND                | GND                    |
| GPIO8 (SDA)        | SDA                    |
| GPIO9 (SCL)        | SCL                    |

> 注意：BMP280 SDO 引脚接 GND 地址为 `0x76`，接3.3V地址为 `0x77`，代码自动兼容两种地址；AHT20 固定地址 `0x38`。

## 串口输出示例
```
========== System Start ==========
Serial initialized!
Scanning I2C bus...
I2C device found at address 0x38 !
I2C device found at address 0x76 !
done

Trying to initialize BMP280...
BMP280 found at address 0x76
BMP280 initialized successfully!

Trying to initialize AHT20...
AHT20 found at address 0x38
AHT20 initialized successfully!
========== Setup Complete ==========

===============
[BMP280] 温度: 25.62 *C
[BMP280] 气压: 1008.32 hPa
[BMP280] 海拔: 41.26 m
[AHT20] 温度: 25.31 *C
[AHT20] 湿度: 48.72 %
```
