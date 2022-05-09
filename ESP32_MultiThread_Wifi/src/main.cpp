//  由于项目的设计需要，此程序运用了多线程运行；
//  多线程功能基于FreeRTOS，可以多个任务并行处理；
//  ESP32具有两个32位Tensilica Xtensa LX6微处理器；
//  一般我们用Arduino进行编程时只使用到了第一个核（大核），第0核并没有使用；
//  而在多线程中可以指定在那个核运行；

#define BLINKER_WIFI            //设置连接的方式，这里为WIFI连接
#include <Blinker.h>
#include <Arduino.h>
#include <Wire.h>
#include "SSD1306Wire.h"  
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN 27                  //定义温湿度传感器的引脚
#define DHTTYPE  DHT11             //温湿度传感器的型号 DHT 11
#define SDA 22                     //定义OLED引脚   SDA -> GPIO22
#define SCL 21                     //定义OLED引脚   SCL -> GPIO21
#define LED_lv_ON digitalWrite(25,HIGH);
#define LED_lv_OFF digitalWrite(25,LOW);
#define LED_huang_ON digitalWrite(33,HIGH);
#define LED_huang_OFF digitalWrite(33,LOW);
#define LED_hong_ON digitalWrite(32,HIGH);
#define LED_hong_OFF digitalWrite(32,LOW);

char auth[] = "14e81dbaa834";   //手机端注册的密钥
char ssid[] = "HUAWEI Mate X2";       //所连接wifi的名称，注意必须是2.4G的WiFi
char pswd[] = "1236547890";   //所连接WiFi的密码
uint8_t smg_duan[10] ={0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90};    //数码管字符0~9
uint8_t dat_hong = 10;      //初始化红灯时间
uint8_t dat_guang = 5;     //黄灯时间
uint8_t dat_lv= 20;         //绿灯时间
int8_t dat_count = -1;
uint32_t car_count;
uint32_t car_count_0;       //车辆计数时，前一分钟的累计数量
uint32_t car_count_fen;     //每分钟的车辆数
uint8_t state;              //红绿灯的状态
uint8_t hongwai_state;
uint8_t test_state=0;
uint16_t i=0;
uint16_t j=0;



// 新建组件对象，对应app上的一个对象
BlinkerButton Button1("btn-abc");   //按键
BlinkerNumber TEMP("num-ca1");
BlinkerNumber HUMI("num-efz");
BlinkerNumber LEIji("num-2eh");
BlinkerNumber MEIFEN("num-w2o");
BlinkerSlider Slider1("ran-kn1");   //控制红灯滑动组件
BlinkerSlider Slider2("ran-x10");   //控制绿灯滑动组件
int counter = 0;
float humi_read = 0, temp_read = 0;

DHT_Unified dht(DHTPIN, DHTTYPE);    //实例化一个DHT11 
SSD1306Wire display(0x3c, SDA, SCL);   //实例化一个SSD1306Wire对象
void SMG_Display_duan( int data);     //数码管字段显示
void DHT11_Init();                    //温湿度初始化
void DHT11_Read_dat();             //温湿度读取
void GPIO_Init();                 
void SMG_Display(uint8_t dat);
void OLED_Display();
void Car_test();            //车辆检测

// 按下按键即会执行该函数，也就是回调函数，
void button1_callback(const String & state)  
{
    BLINKER_LOG("get button state: ", state);
    if (state=="off") {
      Button1.color("#000000");
      digitalWrite(LED_BUILTIN, LOW); 
      Button1.print("off");   // 反馈开关状态
    } 
    else if (state=="on") {
      Button1.color("#FF0000");
      digitalWrite(LED_BUILTIN, HIGH);     
      Button1.print("on");    // 反馈开关状态
    }
}
 
void Slider1_callback(int32_t value){  //红灯滑块回调函数
  BLINKER_LOG("get slider value:", value);
  dat_hong=value;
  Slider1.print(dat_hong);
}

void Slider2_callback(int32_t value){  //绿灯滑块回调函数
  BLINKER_LOG("get slider value:", value);
  dat_lv=value;
  Slider2.print(dat_lv);  
}

// 如果未绑定的组件被触发，则会执行其中内容
void dataRead(const String & data)
{
  BLINKER_LOG("Blinker readString: ", data);
  counter++;
}

void heartbeat()     //回传数据给app组件
{
  HUMI.print(humi_read);
  TEMP.print(temp_read);
  LEIji.print(car_count);
  MEIFEN.print(car_count_fen);
  Slider1.print(dat_hong);
  Slider2.print(dat_lv);
}

void dataStorage()   //发送数据到app的串口监视
{
  Blinker.dataStorage("num-ca1", temp_read);
  Blinker.dataStorage("num-efz", humi_read);
  Blinker.dataStorage("num-2eh", car_count);
  Blinker.dataStorage("num-w2o", car_count_fen);
}

void xTaskOne(void *xTask1)        //多任务中的任务一
{
    while (1)
    {
      SMG_Display(dat_count);
      delay(1);
    }
}

void xTaskTwo(void *xTask2)       //多任务中的任务二
{
  while (1)
  {
    switch(state)
    {
      case 0 : 
        LED_lv_ON;
        if(dat_count==-1) {dat_count = dat_lv;} 
        delay(1000);dat_count--;
        if(dat_count<0){LED_lv_OFF;state=1;} 
      break;

      case 1 : 
        LED_huang_ON;
        if(dat_count==-1) {dat_count = dat_guang;}
        delay(1000); dat_count--;
        if(dat_count<0){LED_huang_OFF;state=2;}
      break;

      case 2 : 
        LED_hong_ON;
        if(dat_count==-1) {dat_count = dat_hong;}
        delay(1000); dat_count--;
        if(dat_count<0){LED_hong_OFF;state=0;}
      break;
    }
  }
}

void setup()
{ 
  Serial.begin(115200); //设置串口的波特率
  GPIO_Init();          //初始化引脚
  display.init();      //初始化OLED显示
  BLINKER_DEBUG.stream(Serial);   //定调试信息输出的串口，设备开发时调试使用
  BLINKER_DEBUG.debugAll();  //查看更多内部信息
   
  // 初始化有LED的IO
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  // 初始化blinker
  Blinker.begin(auth, ssid, pswd);
  Blinker.attachData(dataRead);
  Blinker.attachHeartbeat(heartbeat);   //一个心脏包的函数，用于数据的回传
  Blinker.attachDataStorage(dataStorage);
  Button1.attach(button1_callback);  //绑定回调函数
  Slider1.attach(Slider1_callback);  //绑定滑块回调函数
  Slider2.attach(Slider2_callback);  //绑定滑块回调函数
  dht.begin(); 
  delay(10);
}

void loop()
{
  //最后一个参数非常重要，决定这个任务创建在哪个核上.PRO_CPU 为 0, APP_CPU 为 1。
  xTaskCreatePinnedToCore(xTaskOne, "TaskOne", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(xTaskTwo, "TaskTwo", 4096, NULL, 2, NULL, 1);

  while (1)
  {
    Car_test();
    sensors_event_t event;
    Blinker.run();  //负责处理Blinker收到的数据，每次运行都会将设备收到的数据进行一次解析。在使用WiFi接入时，该语句也负责保持网络连接
    Serial.println(car_count);
    Serial.println(car_count_fen);
    DHT11_Read_dat();
    OLED_Display();
    i++;   
    j++;
  if(i==15)     //可以设置数据回传的间隔  其大小为 N*100 ms
  {
    heartbeat();
    i=0;
  }
  if(j==300)    //设置时间，回传车辆的计数周期   其大小为 N*100 ms
  {
    car_count_fen=2*(car_count-car_count_0);
    j=0;
    car_count_0=car_count;
    if(state==0){
      if(car_count_fen<20){
        dat_lv=20;
      }
      else if(car_count_fen<30){
        dat_lv=30;
      }
      else if(car_count_fen<40){
        dat_lv=40;
      }
      else{
        dat_lv=50;
      }
    }
    

  }
  Blinker.delay(100);
  }
}

void GPIO_Init()
{
  pinMode(2,OUTPUT);   //开发板上的LED灯
  pinMode(13,OUTPUT);  //数码管引脚A
  pinMode(14,OUTPUT);  //数码管引脚B
  pinMode(15,OUTPUT);  //数码管引脚C
  pinMode(16,OUTPUT);  //数码管引脚D
  pinMode(17,OUTPUT);  //数码管引脚E
  pinMode(18,OUTPUT);  //数码管引脚F
  pinMode(19,OUTPUT);  //数码管引脚G
  pinMode(12,OUTPUT);  //COM1
  pinMode(23,OUTPUT);  //COM2
  pinMode(25,OUTPUT);  //绿灯
  pinMode(33,OUTPUT);  //黄灯
  pinMode(32,OUTPUT);  //红灯
  pinMode(26,INPUT);   //红外模块
} 

void SMG_Display_duan( int data)  // 数字段码
{
  int value = smg_duan[data];
  for ( int i = 13 ; i <= 19 ; i++)
  {
    digitalWrite(i, value & 0x01);
    value >>= 1;
  }
   delayMicroseconds(500);
}

void SMG_Display(uint8_t dat)     //数字显示
{
  digitalWrite(12,HIGH);  //com1
  SMG_Display_duan(dat/10);
  digitalWrite(12,LOW); 
  delay(1);
  digitalWrite(23,HIGH);  //com2
  SMG_Display_duan(dat%10);
  digitalWrite(23,LOW);
  delay(1);
}

const uint8_t image_wen[] = { 
0x00,0x00,0xC4,0x1F,0x48,0x10,0x48,0x10,0xC1,0x1F,0x42,0x10,0x42,0x10,0xC8,0x1F,
0x08,0x00,0xE4,0x3F,0x27,0x25,0x24,0x25,0x24,0x25,0x24,0x25,0xF4,0x7F,0x00,0x00,/*"温"*/};
const uint8_t image_du[] = { 
0x80,0x00,0x00,0x01,0xFC,0x7F,0x44,0x04,0x44,0x04,0xFC,0x3F,0x44,0x04,0x44,0x04,
0xC4,0x07,0x04,0x00,0xF4,0x0F,0x24,0x08,0x42,0x04,0x82,0x03,0x61,0x0C,0x1C,0x70,/*"度"*/};
const uint8_t image_1[] = { 
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x00,0x30,0x00,
0x00,0x00,0x30,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,/*"："*/};
const uint8_t image_2[] = { 
0x00,0x00,0x0C,0x00,0x12,0x5F,0xD2,0x60,0x6C,0x40,0x20,0x40,0x30,0x00,0x30,0x00,
0x30,0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x20,0x00,0x60,0x40,0xC0,0x20,0x00,0x1F,/*"℃"*/};
const uint8_t image_shi[] = {
0x00,0x00,0xE4,0x1F,0x28,0x10,0x28,0x10,0xE1,0x1F,0x22,0x10,0x22,0x10,0xE8,0x1F,
0x88,0x04,0x84,0x04,0x97,0x24,0xA4,0x14,0xC4,0x0C,0x84,0x04,0xF4,0x7F,0x00,0x00,/*"湿"*/};
const uint8_t image_3[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x08,0x62,0x04,0x63,0x04,0x63,0x02,0x62,0x01,
0x9C,0x3C,0x40,0x62,0x20,0x42,0x20,0x42,0x10,0x62,0x08,0x3C,0x00,0x00,0x00,0x00,/*"%"*/};
const uint8_t image_lei[] = {
0x00,0x00,0xFC,0x1F,0x84,0x10,0xFC,0x1F,0x84,0x10,0xFC,0x1F,0x40,0x00,0x20,0x04,
0xF8,0x03,0x80,0x01,0x60,0x08,0xFC,0x1F,0x80,0x10,0x88,0x04,0xA4,0x08,0x42,0x10,/*"累"*/};
const uint8_t image_ji[] = {
0x00,0x02,0x04,0x02,0x08,0x02,0x08,0x02,0x00,0x02,0x00,0x02,0xEF,0x7F,0x08,0x02,
0x08,0x02,0x08,0x02,0x08,0x02,0x08,0x02,0x28,0x02,0x18,0x02,0x08,0x02,0x00,0x02,/*"计"*/};
const uint8_t image_shu[] = {
0x10,0x04,0x92,0x04,0x54,0x04,0x10,0x7C,0xFF,0x22,0x54,0x22,0x92,0x22,0x11,0x25,
0x08,0x14,0x7F,0x14,0x44,0x08,0x42,0x08,0x26,0x14,0x18,0x14,0x2C,0x22,0x43,0x41,/*"数"*/};  
const uint8_t image_mei[] = {
0x08,0x00,0xF8,0x3F,0x04,0x00,0x04,0x00,0xFA,0x0F,0x09,0x08,0x48,0x08,0x88,0x08,
0xFF,0x7F,0x04,0x08,0x44,0x08,0x84,0x08,0xFC,0x3F,0x00,0x08,0x00,0x05,0x00,0x02,/*"每"*/};
const uint8_t image_fen[] = {
0x00,0x02,0x20,0x02,0x20,0x04,0x10,0x04,0x08,0x08,0x04,0x10,0x02,0x20,0xF9,0x47,
0x20,0x04,0x20,0x04,0x20,0x04,0x10,0x04,0x10,0x04,0x08,0x04,0x84,0x02,0x02,0x01,/*"分"*/};
const uint8_t image_zhong[] = {
0x08,0x04,0x08,0x04,0x3C,0x04,0x04,0x04,0x82,0x3F,0xBD,0x24,0x88,0x24,0x88,0x24,
0xBF,0x24,0x88,0x3F,0x88,0x24,0x08,0x04,0x28,0x04,0x18,0x04,0x08,0x04,0x00,0x04,/*"钟"*/};

void OLED_Display()     //oled屏幕显示
{
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  
  display.clear();
  display.drawIco16x16(0, 0, image_wen, 0);
  display.drawIco16x16(16, 0, image_du, 0);
  display.drawIco16x16(32, 0, image_1, 0);
  display.drawString(44, 0, String(event.temperature));
  display.drawIco16x16(88, 0, image_2, 0);
  dht.humidity().getEvent(&event);
  display.drawIco16x16(0, 16, image_shi, 0);
  display.drawIco16x16(16, 16, image_du, 0);
  display.drawIco16x16(32, 16, image_1, 0);
  display.drawString(44, 16, String(event.relative_humidity));
  display.drawIco16x16(88, 16, image_3, 0);

  display.drawIco16x16(0, 32, image_lei, 0);
  display.drawIco16x16(16, 32, image_ji, 0);
  display.drawIco16x16(32, 32, image_ji, 0);
  display.drawIco16x16(48, 32, image_shu, 0);
  display.drawIco16x16(64, 32, image_1, 0);
  display.drawString(76, 32, String(car_count));

  display.drawIco16x16(0, 48, image_mei, 0);
  display.drawIco16x16(16, 48, image_fen, 0);
  display.drawIco16x16(32, 48, image_zhong, 0);
  display.drawIco16x16(48, 48, image_ji, 0);
  display.drawIco16x16(64, 48, image_shu, 0);
  display.drawIco16x16(80, 48, image_1, 0);
  display.drawString(92, 48, String(car_count_fen));

  display.flipScreenVertically();
  display.display();
}

void DHT11_Read_dat()
{
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {     //检测读取的温度数值是否正确
    Serial.println(F("Error reading temperature!"));
  }
  else {
    Serial.print(F("Temperature: "));      //串口输出温度值
    Serial.print(event.temperature);
    Serial.println(F("°C"));
    float t = event.temperature;
    temp_read = t;
  }

  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {   //检测读取的湿度数值是否正确
    Serial.println(F("Error reading humidity!"));
  }
  else {
    Serial.print(F("Humidity: "));        //串口输出湿度值
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
    float h = event.relative_humidity;
    humi_read = h;
  }
}

void Car_test()
{
  hongwai_state=digitalRead(26);
  if(hongwai_state==0){
    if(test_state==0){
      car_count++;
      test_state=1;
    }
  }
  if(hongwai_state==1){
    test_state=0;
  }
  Serial.println(hongwai_state);
}