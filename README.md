默认接线

// I2C引脚定义

#define SDA_PIN 8
#define SCL_PIN 9

但有个关键风险：GPIO9 是 BOOT 脚（strapping pin） GPIO9 = BOOT：上电 / 复位时电平决定启动模式 若 I2C 上拉把它拉死高 / 低，可能导致板子偶尔启动失败、串口不出、连不上

推荐的稳定接法（4、5，避开 BOOT） 接线： SDA→GPIO4 SCL→GPIO5

<img width="300"  alt="image" src="https://github.com/user-attachments/assets/1d01f702-4c24-4f61-af52-3fab7cf8aee3" />

<img width="674" height="137" alt="image" src="https://github.com/user-attachments/assets/1573e90a-5c7c-4793-a922-78479b6a8f54" />
