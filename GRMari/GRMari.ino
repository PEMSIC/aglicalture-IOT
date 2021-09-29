
//210416 Firebeetole+DS180B20+deepsleep(Wifi off) for Isahaya

// Grab a count of devices on the wire
//  numberOfDevices = sensors.getDeviceCount();
// getDeviceCount()がうまく利用できないためにとりあえず整数値を与える
//210324 BOOTCOUNT_const中は，5分に一回起動，その後時刻00分と30分に起動

// 関数のプロトタイプ宣言と何に使う関数なのか明記したい(21/07/30)

/*
 VS CODE上での文字化けを直す方法
 https://qiita.com/nori-dev-akg/items/e0811eb26274910cdd0e
 https://lang-ship.com/blog/work/arduino-for-visual-studio-code/
 を参照
*/
uint8_t numberOfDevices = 4;
  
#include <esp_wifi.h>
#include <esp_sleep.h>
#include "soc/rtc_cntl_reg.h"
#include "rom/rtc.h"
#include "driver/rtc_io.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "esp_deep_sleep.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/dport_reg.h"
#include "soc/i2s_reg.h"
#include "soc/sens_reg.h"
#include "soc/syscon_reg.h"

const char MY_SSID[] = "PIX-MT100";
const char MY_PASSWORD[] = "mistmist";
uint8_t baseMac[6];

/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com  
*********/

#include <OneWire.h>
#include <DallasTemperature.h>

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

// Data wire is plugged TO GPIO 4
#define ONE_WIRE_BUS 4

//Field ID
#define FIELDID 1005      // 諫早試験場フィールド"Isahaya1"のフィールド番号

//Initial Condition Test Term 5 min wake 210324
#define BOOTCOUNT_const 5

// FET Gate signal pin 
#define GRM_GATE 25
#define DS18B20_GATE 26

// post_ADCresult()関数で使うADCピンの範囲
#define FIRST_CH_OF_ADC 1
#define LAST_CH_OF_ADC 3

// Function Prototype Declarations
void DS();
void activate_sensors();                          // FETのゲートをHIGHにし、センサーを起動
void stop_sensors();                              // FETのゲートをLOWにし、センサーを停止
void post_DS18B20data();                          // 電池電圧をAD変換してサーバーに送信
void printAddress(DeviceAddress deviceAddress);   // 64bitのアドレスを16進数表示にする
void time_adj();                                  // よくわからんから放置。多分なくてもいい。
void sendLineNotify(float tempC);                 // Line通知する関数っぽい
void post_PSVolServer();                          // Power Supply Voltageをサーバーに送信
void post_wifilevel();                            // ESP32のRSSIをサーバーに送信
void post_GRMresult();                            // 接地抵抗の測定値をサーバーに送信
void post_ADCresult();                            // AD変換結果をサーバーに送信


RTC_DATA_ATTR int bootCount = 0;

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// We'll use this variable to store a found device address
DeviceAddress tempDeviceAddress;

uint16_t time_span = 30;

//ADC chnnel
const int adcPin[5] = {36, 39, 34, 35, 15};

const int R1 = 10000;
const int R2 = 10000;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200, SERIAL_8N1);         //ビットレート115200 8bit/パリティなし/1ストップビット
  while(!Serial);                           // シリアル通信ができるまで待機　
  int countw = 0;
  Serial.print("Time Span =");
  Serial.println(time_span);
  if(bootCount <= BOOTCOUNT_const) bootCount ++;
  Serial.println("Start");  
  Serial.println("Boot Count:" + String(bootCount));

  esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
//  esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
//  esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
//  esp_deep_sleep_pd_config(ESP_PD_DOMAIN_MAX, ESP_PD_OPTION_OFF); 
    
  // put your main code here, to run repeatedly:
  delay(100);

  // while (!Serial);                    //  ここ以前でもシリアル使ってるのになんで待機？？　多分いらないので削除予定
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(MY_SSID, MY_PASSWORD);
  Serial.print("WiFi connecting");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
    if(countw == 150){                // いつまでもWi－Fiがスタートしてくれないようならディープスリープに入るプログラムっぽい
      countw = 0;
      esp_wifi_stop();
      WiFi.mode(WIFI_OFF);
      DS();
    }
    countw = countw + 1;
  }

  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  activate_sensors();
  Serial.println(" connected");
  delay(1000);
  configTime(0, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");

    // Start up the library
  sensors.begin();
  // Loop through each device, print out address
  for(int i=0;i<numberOfDevices; i++){
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i)){
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();
    }
    
    else {
      Serial.print("Found number of total devices are ");
      numberOfDevices = i;
      Serial.println(i, DEC);
    }
  }

  post_DS18B20data();
  // postPSVolServer(); // Power Supply Voltage Monitoring
  post_wifilevel();
  post_GRMresult();
  // time_adj();  
  esp_wifi_stop();
  WiFi.mode(WIFI_OFF);
  DS();
}

void loop() {
}

void DS(){
  int set_sleep_sec;
//  if(time_span ==0)time_span = 30;                        //なんでここ？　time_spanはdefineにする予定
//  if (bootCount <= BOOTCOUNT_const) set_sleep_sec = 5 * 60; //sec 起動回数が80回未満のとき、待機時間は５分
//  else   set_sleep_sec = time_span * 60;   // sec  起動回数が80回を超えたとき、待機時間(set_sleep_sec)を30分にする
  
  if (bootCount <= BOOTCOUNT_const)
  {
    set_sleep_sec = 5 * 60;
  }
  
  else
  {
    set_sleep_sec = time_span * 60;
  }
  
  stop_sensors();
  Serial.println("Sleep!");
  Serial.print("The next boot will be in ");
  Serial.print(set_sleep_sec / 60, DEC);
  Serial.println(" minutes.");


  SET_PERI_REG_BITS(RTC_CNTL_TEST_MUX_REG, RTC_CNTL_DTEST_RTC, 0, RTC_CNTL_DTEST_RTC_S);                //削除予定
  CLEAR_PERI_REG_MASK(RTC_CNTL_TEST_MUX_REG, RTC_CNTL_ENT_RTC);                                         //削除予定
  CLEAR_PERI_REG_MASK(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_FORCE_PU);                                   //削除予定
  esp_deep_sleep_enable_timer_wakeup(set_sleep_sec * 1000 * 1000);  // wakeup(restart) after set_sleep_sec
  esp_deep_sleep_start();
  Serial.println("zzz");  // ここは実行されない
}

void activate_sensors(){
  pinMode(GRM_GATE, OUTPUT);          // 接地抵抗計のロードスイッチのN-MOSFETゲート信号として使用
  pinMode(DS18B20_GATE, OPEN_DRAIN);  // 温度センサのスイッチ回路をオープンドレインで使用

  digitalWrite(GRM_GATE, HIGH);       // 接地抵抗計のロードスイッチをON
  digitalWrite(DS18B20_GATE, LOW);   // DS28B20のスイッチをオン(オープンドレインON LOW出力)
}

void stop_sensors(){
  digitalWrite(GRM_GATE, LOW);       // 接地抵抗計のロードスイッチをON
  digitalWrite(DS18B20_GATE, HIGH);   // DS28B20のスイッチをオフ(オープンドレイン開放 ハイインピーダンス)
}

void post_DS18B20data(){
   // Not used in this example
    sensors.requestTemperatures(); // Send the command to get temperatures
    float tempC;
  // Loop through each device, print out temperature data
    struct tm timeInfo;
    char detecting_time[20];                     //どこで使ってるのか検索しづらいので直す
    char address_str[16];
    getLocalTime(&timeInfo);
    
  for(int i=0;i<numberOfDevices; i++){
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i)){
      // Output the device ID
    //  if(sensors.getAddress(tempDeviceAddress, i)==0){
    //    return;
    //  }

      Serial.print(" Temperature for device: ");
      Serial.println(i,DEC);
      // Print the data
      tempC = sensors.getTempC(tempDeviceAddress);
//      if(timeInfo.tm_hour ==0 || timeInfo.tm_hour ==9 && timeInfo.tm_min>29){
//        "Sending Message to LINE nitify agri-notify",
//        sendLineNotify(tempC);
        delay(1000);
      } 
      Serial.print("Temp C: ");
      Serial.print(tempC,DEC);
    
    Serial.println("Posting JSON data to server...");

    // Block until we are able to connect to the WiFi access point
    HTTPClient http;        
    http.begin("http://mist-hospital.mydns.jp/kabayaki/db/DS18B20");  
    http.addHeader("Content-Type", "application/json");         
 
    StaticJsonDocument<200> doc;
      // Add values in the document
      //
      sprintf(address_str, "%02x%02x%02x%02x%02x%02x%02x%02x",baseMac[2], baseMac[3], baseMac[4], baseMac[5],tempDeviceAddress[4],tempDeviceAddress[5],tempDeviceAddress[6],tempDeviceAddress[7]);
      doc["addr"] =  address_str;
      doc["DS18B20"] = tempC;
      doc["field"] = FIELDID;
      sprintf(detecting_time, "%04d-%02d-%02dT%02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
      doc["detect"] = detecting_time;
     
      String requestBody;
      serializeJson(doc, requestBody);
     
      int httpResponseCode = http.POST(requestBody);

      if(httpResponseCode>0){
        String response = http.getString();                          
        Serial.println(httpResponseCode);   
        Serial.println(response);
      }

      else{
      //      Serial.printf("Error occurred while sending HTTP POST: %s\n", HTTPClient.errorToString(statusCode).c_str());  
      }

    delay(100);
    }

    digitalWrite(DS18B20_GATE, HIGH);
  }  

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++){
    if (deviceAddress[i] < 16) Serial.print("0");           // なんで16以上だったら0に？？ 自己解決　16以下だと0x9みたいになって上位4bitが表示されないから
      Serial.print(deviceAddress[i], HEX);
  }
}

//function to time adjustment and LINE notify
void time_adj(){
  struct tm timeInfo;
  getLocalTime(&timeInfo);
  if(timeInfo.tm_hour=11 && timeInfo.tm_min>29){
    if(timeInfo.tm_min <= 30){
//      time_span = 30 - timeInfo.tm_min;
        time_span = 30;
//    }else time_span = 30;
    }else time_span = 30;

  }
}

//Line通知
void sendLineNotify(float tempC) {
  const char* host = "notify-api.line.me";
  const char* token = "62A3Yxophwryuu954UCYhYMba35N8RD2W7dwK9pbbZr";
  const char* message_headder = "現在の";
  const char* message_headder2 = "の地温は，"; 
  const char* message = "°Cです。";
  WiFiClientSecure client;
  Serial.println("Try");
  //LineのAPIサーバに接続
  if (!client.connect(host, 443)) {
    Serial.println("Connection failed");
    return;
  }
  Serial.println("Connected");
  //リクエストを送信
  String query = String("message=") +String(message_headder)+ String(message_headder2)+ String(tempC) +String(message);
  String request = String("") +
               "POST /api/notify HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Authorization: Bearer " + token + "\r\n" +
               "Content-Length: " + String(query.length()) +  "\r\n" + 
               "Content-Type: application/x-www-form-urlencoded\r\n\r\n" +
                query + "\r\n";
  client.print(request);
 
  //受信終了まで待つ 
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  
  String line = client.readStringUntil('\n');
  Serial.println(line);
}

void post_PSVolServer(){
  float value = analogRead(adcPin[0]);                  //電池電圧を計ってるっぽい
  float voltage = value * (R1+R2) / R2 *(3.6/4095);   //電池電圧を分圧してる計算っぽい
  char baseMacChr[18] = {0};
  sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);

    // Block until we are able to connect to the WiFi access point
    HTTPClient http;        
    http.begin("http://mist-hospital.mydns.jp/kabayaki/db/voltage");  
    http.addHeader("Content-Type", "application/json");         
    struct tm timeInfo;
    char detecting_time[20];
    char address_str[16];
    getLocalTime(&timeInfo);
    delay(10);
 
    StaticJsonDocument<200> doc;
     // Add values in the document
     //
     doc["addr"] =  baseMacChr;
     doc["voltage"] = voltage;
     doc["field"] = FIELDID;
     sprintf(detecting_time, "%04d-%02d-%02dT%02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
     doc["detect"] = detecting_time;
     
     String requestBody;
     serializeJson(doc, requestBody);
     
     int httpResponseCode = http.POST(requestBody);
 
     if(httpResponseCode>0){
        String response = http.getString();                          
        Serial.println(httpResponseCode);   
        Serial.println(response);
      }
      else {
      //      Serial.printf("Error occurred while sending HTTP POST: %s\n", HTTPClient.errorToString(statusCode).c_str());  
      }

Serial.println("MAC:");
Serial.println(baseMacChr);
Serial.println("ADC:");
Serial.println(voltage);

}

void post_wifilevel(){                     // int型になってたのでvoid方に直した
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  long rssi = WiFi.RSSI();
  char baseMacChr[18] = {0};
  sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);

    // Block until we are able to connect to the WiFi access point
    HTTPClient http;        
    http.begin("http://mist-hospital.mydns.jp/kabayaki/db/rssi");  
    http.addHeader("Content-Type", "application/json");         
    struct tm timeInfo;
    char detecting_time[20];
    char address_str[16];
    getLocalTime(&timeInfo);
    delay(10);
 
    StaticJsonDocument<200> doc;
     // Add values in the document
     //
     doc["addr"] =  baseMacChr;
     doc["rssi"] = rssi;
     doc["field"] = FIELDID;
     sprintf(detecting_time, "%04d-%02d-%02dT%02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
     doc["detect"] = detecting_time;
     
     String requestBody;
     serializeJson(doc, requestBody);
     
     int httpResponseCode = http.POST(requestBody);
 
     if(httpResponseCode>0){
        String response = http.getString();                          
        Serial.println(httpResponseCode);
        Serial.println(response);
      }
      else {
      //      Serial.printf("Error occurred while sending HTTP POST: %s\n", HTTPClient.errorToString(statusCode).c_str());  
      }

  Serial.println("MAC:");
  Serial.println(baseMacChr);
  Serial.println("RSSi:");
  Serial.println(rssi);
}

void post_GRMresult(){
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  Serial2.begin(9600);     // add PIC16F1765との通信レート
  while(!Serial2);

  float GroundResistance = 0.0;
  uint32_t buf[4];

  while(3 >> Serial2.available());            // バッファにデータが4つ蓄積されるまで待機
  buf[0] = Serial2.read();  // 最下位レジスタ
  buf[1] = Serial2.read();  // 中位レジスタ
  buf[2] = Serial2.read();  // 最上位レジスタ
  buf[3] = Serial2.read();  // サンプル数

  Serial2.end();
  digitalWrite(GRM_GATE, LOW);    // ロードスイッチを一旦オフ

  buf[2] = (buf[2] * 65536) + (buf[1] * 256) + buf[0];
  GroundResistance = (float)buf[2] / (float)buf[3] * 0.612 - 0.4664;
  Serial.print("number of sumples = ");
  Serial.println(buf[3]);
  Serial.print("Rg = ");
  Serial.println(GroundResistance, 2);

  char baseMacChr[18] = {0};
  sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);

    // Block until we are able to connect to the WiFi access point
    HTTPClient http;        
    http.begin("http://mist-hospital.mydns.jp/kabayaki/db/gr");  
    http.addHeader("Content-Type", "application/json");         
    struct tm timeInfo;
    char detecting_time[20];
    char address_str[16];
    getLocalTime(&timeInfo);
    delay(10);
 
    StaticJsonDocument<200> doc;
     // Add values in the document
     //
     doc["addr"] =  baseMacChr;
     doc["gr"] = GroundResistance;
     doc["field"] = FIELDID;
     sprintf(detecting_time, "%04d-%02d-%02dT%02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
     doc["detect"] = detecting_time;
     
     String requestBody;
     serializeJson(doc, requestBody);
     
     int httpResponseCode = http.POST(requestBody);
 
     if(httpResponseCode>0){
        String response = http.getString();                          
        Serial.println(httpResponseCode);
        Serial.println(response);
      }
      else {
      //      Serial.printf("Error occurred while sending HTTP POST: %s\n", HTTPClient.errorToString(statusCode).c_str());  
      }

  Serial.println("MAC:");
  Serial.println(baseMacChr);
}

void post_ADCresult(){
  for(uint8_t i=FIRST_CH_OF_ADC; i<LAST_CH_OF_ADC; i++){
    float adc_result;
    adc_result = 3.3 * (float)analogRead(adcPin[i]) /4096.0;

    char baseMacChr[18] = {0};
    sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);

    // Block until we are able to connect to the WiFi access point
    HTTPClient http;        
    http.begin("http://mist-hospital.mydns.jp/kabayaki/db/voltage");  
    http.addHeader("Content-Type", "application/json");         
    struct tm timeInfo;
    char detecting_time[20];
    char address_str[16];
    getLocalTime(&timeInfo);
    delay(10);
 
    StaticJsonDocument<200> doc;
    // Add values in the document
    doc["addr"] =  baseMacChr;
    doc["voltage"] = adc_result;
    doc["field"] = FIELDID;
    sprintf(detecting_time, "%04d-%02d-%02dT%02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    doc["detect"] = detecting_time;

    String requestBody;
    serializeJson(doc, requestBody);

    int httpResponseCode = http.POST(requestBody);

    if(httpResponseCode>0){
      String response = http.getString();                          
      Serial.println(httpResponseCode);   
      Serial.println(response);
    }
    else {
    //      Serial.printf("Error occurred while sending HTTP POST: %s\n", HTTPClient.errorToString(statusCode).c_str());  
    }

  Serial.println("MAC:");
  Serial.println(baseMacChr);
  Serial.println("ADC:");
  Serial.println(adc_result);

  }
}


