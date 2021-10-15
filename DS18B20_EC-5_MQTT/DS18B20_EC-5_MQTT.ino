
//210416 Firebeetole+DS180B20+deepsleep(Wifi off) for Isahaya

// Grab a count of devices on the wire
//  numberOfDevices = sensors.getDeviceCount();
// getDeviceCount()がうまく利用できないためにとりあえず整数値を与える
//210324 BOOTCOUNT_CONST中は，5分に一回起動，その後時刻00分と30分に起動

// 関数のプロトタイプ宣言と何に使う関数なのか明記したい(21/07/30)

/*
 VS CODE上での文字化けを直す方法
 https://qiita.com/nori-dev-akg/items/e0811eb26274910cdd0e
 https://lang-ship.com/blog/work/arduino-for-visual-studio-code/
 を参照
*/
uint8_t numberOfDevices = 8;

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
#include <PubSubClient.h>  //MQTT通信のために必要

const char MY_SSID[] = "PIX-MT100";
const char MY_PASSWORD[] = "mistmist";
const char* mqtt_server = "133.45.129.190"; //MQTTのIPアドレス　追加
WiFiClient espClient;           //client.connect()で定義されている指定のインターネットIPアドレスおよびポートに接続できるクライアントを作成?
PubSubClient client(espClient);     //MQTTの通信を行うためのPubsubClientのクラスから実際に処理を行うオブジェクトclientを作成?

uint8_t baseMac[6];

/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com  
*********/

#include <OneWire.h>
#include <DallasTemperature.h>

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */

// Data wire is plugged TO GPIO 4
#define ONE_WIRE_BUS 4

//Field ID
#define FIELDID 1005 // 諫早干拓"Isahaya1"のフィールド番号

//Initial Condition Test Term 5 min wake 210324
#define BOOTCOUNT_CONST 80

//FET Gate signal pin
#define OPEN_DRAIN_PIN 2
#define PSVOL_GATE 5

//EC-5
#define NUM_OF_SAMP 64 //number of samples

// Function Prototype Declarations
void esp_startup();                             // esp32のDeep Sleepやらデバッグのためのシリアルポートやらのための立ち上げ設定
void wifi_startup();                            // wi-fi機能を立ち上げる
void DS18B20_startup();                         // DS18B20の立ち上げ
void DS();                                      // Deep Sleep
void activate_sensors();                        // FETのゲートをHIGHにし、センサーを起動
void stop_sensors();                            // FETのゲートをLOWにし、センサーを停止
void post_DS18B20data();                        // 電池電圧をAD変換してサーバーに送信
void printAddress(DeviceAddress deviceAddress); // 64bitのアドレスを16進数表示にする
void time_adj();                                // よくわからんから放置。多分なくてもいい。
void post_PSVolServer();                        // Power Supply Voltageをサーバーに送信
void post_wifilevel();                          // ESP32のRSSIをサーバーに送信
void post_EC5data();                            // EC-5のアナログ電圧をサーバーに送信
void callback(char* topic, byte* message, unsigned int length);                                // 使うか不明　デバッグ用？　追加
void reconnect();                               // 再接続のための関数

RTC_DATA_ATTR int bootCount = 0;

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// We'll use this variable to store a found device address
DeviceAddress tempDeviceAddress;

uint16_t boot_time_span = 5;
uint16_t time_span = 30;

//ADC chnnel
const int adcPin[5] = {36, 39, 34, 35, 15};

const int R1 = 27000;
const int R2 = 27000;

//EC-5
uint16_t one_shot_voltage[NUM_OF_SAMP];

void setup()
{
  esp_startup(); 
  delay(100);
  wifi_startup();

  client.setServer(mqtt_server, 1883);    //インスタンス化(実体化)したオブジェクトclientの接続先のサーバを、アドレスとポート番号を設定して通信できるようにする　追加
  client.setCallback(callback);   //callback関数の設定　追加

  if (!client.connected()) {  //接続できていない場合
    reconnect();    //関数(再接続)
  }
  client.loop();

  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  activate_sensors();
  delay(1000);
  configTime(0, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");

  DS18B20_startup();

  post_DS18B20data();
  post_PSVolServer(); // Power Supply Voltage Monitoring
  post_wifilevel();
  post_EC5data();
  esp_wifi_stop();
  WiFi.mode(WIFI_OFF);
  stop_sensors();
  DS();
}

void loop()
{
}

void esp_startup()
{
  Serial.begin(115200, SERIAL_8N1); //ビットレート115200 8bit/パリティなし/1ストップビット
  while (!Serial); // シリアル通信ができるまで待機
  Serial.print("Time Span =");
  Serial.println(time_span);
  if (bootCount <= BOOTCOUNT_CONST)
    bootCount++;
  Serial.println("Start");
  Serial.println("Boot Count:" + String(bootCount));

  esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
  esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);

}

void wifi_startup()
{
  int countw = 0;
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(MY_SSID, MY_PASSWORD);
  Serial.print("WiFi connecting");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
    if (countw == 150)
    { // いつまでもWi－Fiがスタートしてくれないようならディープスリープに入るプログラムっぽい
      countw = 0;
      esp_wifi_stop();
      WiFi.mode(WIFI_OFF);
      DS();
    }
    countw = countw + 1;
  }

  Serial.println(" connected");
}

void DS18B20_startup()
{
  // Start up the library
  sensors.begin();
  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices; i++){
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i)){
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();
    }
    else{
      Serial.print("Found number of total devices are ");
      numberOfDevices = i;
      Serial.println(i, DEC);
    }
  }
}

void DS()
{
  int set_sleep_sec;

  if (bootCount <= BOOTCOUNT_CONST){
    set_sleep_sec = boot_time_span * 60;
  }
  else{
    set_sleep_sec = time_span * 60;
  }

  Serial.println("Sleep!");
  Serial.print("The next boot will be in ");
  Serial.print(set_sleep_sec / 60, DEC);
  Serial.println(" minutes.");

  SET_PERI_REG_BITS(RTC_CNTL_TEST_MUX_REG, RTC_CNTL_DTEST_RTC, 0, RTC_CNTL_DTEST_RTC_S); 
  CLEAR_PERI_REG_MASK(RTC_CNTL_TEST_MUX_REG, RTC_CNTL_ENT_RTC);                          
  CLEAR_PERI_REG_MASK(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_FORCE_PU);                    
  esp_deep_sleep_enable_timer_wakeup(set_sleep_sec * 1000 * 1000);  // wakeup(restart) after set_sleep_sec
  esp_deep_sleep_start();
  Serial.println("zzz"); // ここは実行されない
}

void activate_sensors()
{
  pinMode(OPEN_DRAIN_PIN, OUTPUT_OPEN_DRAIN); // 3V3ラインのスイッチとして使用
  pinMode(PSVOL_GATE, OUTPUT);               // 電源電圧測定回路のスイッチとして使用

  digitalWrite(OPEN_DRAIN_PIN, LOW); // 3V3ラインのスイッチをオン
  digitalWrite(PSVOL_GATE, HIGH);   // 電源電圧測定回路のスイッチをオン
}

void stop_sensors()
{
  digitalWrite(OPEN_DRAIN_PIN, HIGH); // 3V3ラインのスイッチをオフ
  digitalWrite(PSVOL_GATE, LOW);     // 電源電圧測定回路のスイッチをオフ
}

void post_DS18B20data()
{
  // Not used in this example
  sensors.requestTemperatures(); // Send the command to get temperatures
  float tempC;
  // Loop through each device, print out temperature data
  struct tm timeInfo;
  char detecting_time[20]; //どこで使ってるのか検索しづらいので直す
  char address_str[16];
  getLocalTime(&timeInfo);

  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i))
    {
      // Output the device ID


      Serial.print(" Temperature for device: ");
      Serial.println(i, DEC);
      // Print the data
      tempC = sensors.getTempC(tempDeviceAddress);
      delay(1000);
    }
    Serial.print("Temp C: ");
    Serial.print(tempC, DEC);

    Serial.println("Posting JSON data to server...");

    // Block until we are able to connect to the WiFi access point
    HTTPClient http;
    http.begin("http://mist-hospital.mydns.jp/kabayaki/db/DS18B20");
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    // Add values in the document
    //
    sprintf(address_str, "%02x%02x%02x%02x%02x%02x%02x%02x", baseMac[2], baseMac[3], baseMac[4], baseMac[5], tempDeviceAddress[4], tempDeviceAddress[5], tempDeviceAddress[6], tempDeviceAddress[7]);
    doc["addr"] = address_str;
    doc["DS18B20"] = tempC;
    doc["field"] = FIELDID;
    sprintf(detecting_time, "%04d-%02d-%02dT%02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    doc["detect"] = detecting_time;

    String requestBody;
    serializeJson(doc, requestBody);

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0)
    {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    }

    else
    {
      //Serial.printf("Error occurred while sending HTTP POST: %s\n", HTTPClient.errorToString(statusCode).c_str());
    }

    delay(100);
  }
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16)
      Serial.print("0"); // なんで16以上だったら0に？？ 自己解決　16以下だと0x9みたいになって上位4bitが表示されないから
    Serial.print(deviceAddress[i], HEX);
  }
}

void post_PSVolServer()
{
  float voltage = analogReadMilliVolts(adcPin[0]) * (R1 + R2) / R2 / 1000.0; //電池電圧を分圧してる計算っぽい

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
  doc["addr"] = baseMacChr;
  doc["voltage"] = voltage;
  doc["field"] = FIELDID;
  sprintf(detecting_time, "%04d-%02d-%02dT%02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
  doc["detect"] = detecting_time;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    //      Serial.printf("Error occurred while sending HTTP POST: %s\n", HTTPClient.errorToString(statusCode).c_str());
  }

  Serial.println("MAC:");
  Serial.println(baseMacChr);
  Serial.println("PSVol:");
  Serial.println(voltage);
}

void post_wifilevel()
{
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
  doc["addr"] = baseMacChr;
  doc["rssi"] = rssi;
  doc["field"] = FIELDID;
  sprintf(detecting_time, "%04d-%02d-%02dT%02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
  doc["detect"] = detecting_time;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    //      Serial.printf("Error occurred while sending HTTP POST: %s\n", HTTPClient.errorToString(statusCode).c_str());
  }

  Serial.println("MAC:");
  Serial.println(baseMacChr);
  Serial.println("RSSi:");
  Serial.println(rssi);
}

void post_EC5data()
{
  uint32_t sum = 0;
  for (int i = 0; i < NUM_OF_SAMP; i++)
  {
    one_shot_voltage[i] = analogReadMilliVolts(adcPin[1]);
  }
  for (int i = 0; i < NUM_OF_SAMP; i++)
  {
    sum += one_shot_voltage[i];
  }
  float EC5Voltage = sum / NUM_OF_SAMP / 1000.0;

  char baseMacChr[18] = {0};
  sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);

  // Block until we are able to connect to the WiFi access point
  HTTPClient http;
  http.begin("http://mist-hospital.mydns.jp/kabayaki/db/ec5");
  http.addHeader("Content-Type", "application/json");
  struct tm timeInfo;
  char detecting_time[20];
  char address_str[16];
  getLocalTime(&timeInfo);
  delay(10);

  StaticJsonDocument<200> doc;
  // Add values in the document
  doc["addr"] = baseMacChr;
  doc["ec5"] = EC5Voltage;
  doc["field"] = FIELDID;
  sprintf(detecting_time, "%04d-%02d-%02dT%02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
  doc["detect"] = detecting_time;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0)
  {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  }
  else
  {
    //      Serial.printf("Error occurred while sending HTTP POST: %s\n", HTTPClient.errorToString(statusCode).c_str());
  }

  Serial.println("MAC:");
  Serial.println(baseMacChr);
  Serial.println("EC5Vol:");
  Serial.println(EC5Voltage);
}

// 使うか不明　デバッグ用？　追加
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");   //「Message arrived on topic:」と表示
  Serial.print(topic);      //データの識別コードを表示
  Serial.print(". Message: ");    //「. Message: 」と表示
  String messageTemp;   //文字列？
  
  for (int i = 0; i < length; i++) {  //メッセージ長だけ繰り返す
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "esp32/output") {  //String()：Stringクラス(文字列)のインスタンスを生成？
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      
    }
  }
}

//   再接続のための関数　追加
void reconnect(){
  // Loop until we're reconnected
  while (!client.connected()) {   //非接続の間繰り返す
    Serial.print("Attempting MQTT connection...");    //「Attempting MQTT connection..."」と表示
    // Attempt to connect
    if (client.connect("ESP8266Client")) {    //接続できた場合
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    } else {                                  //接続に失敗した場合
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);    //5秒待機
    }
  }
}