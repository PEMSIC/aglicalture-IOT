/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

//#include:外部のあらかじめ用意された機能群(ライブラリ)を取り入れる
#include <WiFi.h>         
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

//const char*:定数のデータへのポインタで、中身の変更は不可能だが、アドレスは変更可
const char* ssid = "E213";  //REPLACE_WITH_YOUR_SSID
const char* password = "213wl213wl";  //REPLACE_WITH_YOUR_PASSWORD

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "133.45.129.190";

WiFiClient espClient;           //client.connect()で定義されている指定のインターネットIPアドレスおよびポートに接続できるクライアントを作成?
PubSubClient client(espClient);     //MQTTの通信を行うためのPubsubClientのクラスから実際に処理を行うオブジェクトclientを作成?
long lastMsg = 0;     //long:32ビットの数値を格納できる     lastMsg:最後に送信したメッセージ？
char msg[50];         //char:符号(+や-)付きの1バイトの数値を記憶する
int value = 0;        //wsp-wroom-02では4バイト、ArduinoUno等では2バイトで整数(...-2,-1,0,1,2...)を格納

//uncomment the following lines if you're using SPI(SPIを使用するなら以下が必要)
/*#include <SPI.h>
#define BME_SCK 18
#define BME_MISO 19
#define BME_MOSI 23
#define BME_CS 5*/

Adafruit_BME280 bme; // I2C
//Adafruit_BME280 bme(BME_CS); // hardware SPI
//Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI
float temperature = 0;      //float:小数を扱える  温度
float humidity = 0;         //湿度

// LED Pin
const int ledPin = 4;     //const:変数として扱うが、値が変更できない(計算には使えるが、途中で違う値を入れ込むことはできない)  LED用のピンを定義


//setup:最初に一度だけ実行される  void:呼び出した側に何も情報を返さない
void setup() {
  Serial.begin(115200);     //シリアル通信のデータ転送のレート
  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  //status = bme.begin();  
  if (!bme.begin(0x76)) {   //?
    Serial.println("Could not find a valid BME280 sensor, check wiring!");    //「有効なBME280センサーが見つからない、配線をチェックしてください」と送信
    while (1);    //無限ループ
  }
  setup_wifi();   //関数(WiFi接続)
  client.setServer(mqtt_server, 1883);    //インスタンス化(実体化)したオブジェクトclientの接続先のサーバを、アドレスとポート番号を設定して通信できるようにする
  client.setCallback(callback);   //callback関数の設定

  pinMode(ledPin, OUTPUT);    //LEDピンの動作を出力に設定
}

//setupで使用する関数
void setup_wifi() {   //WiFiに接続するための関数
  delay(10);      //0.01秒待機
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());   //WiFIシールドのIPアドレスを入手する    
}

//IPアドレス：データの送信元や送信先などを識別するのに使用されている番号

/*ブローカーからのメッセージを受信すると呼び出される関数
 * topic:デバイス間の通信データの識別を行うための階層構造を識別するデータ
 * message:ブローカーから送られたメッセージ・データ
 * length:メッセージ長
 * 以上を引数として引き渡す
 */
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");   //「Message arrived on topic:」と表示
  Serial.print(topic);      //データの識別コードを表示
  Serial.print(". Message: ");    //「. Message: 」と表示
  String messageTemp;   //文字列？
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "esp32/output") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }
}

//loopで使用する関数   再接続のための関数
void reconnect() {
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

//実行したいメインのプログラム(繰り返し実行される)
void loop() {
  if (!client.connected()) {
    reconnect();    //関数(再接続)
  }
  client.loop();

  long now = millis();    //long:32ビットの数値を格納できる
  if (now - lastMsg > 5000) {
    lastMsg = now;
    
    // Temperature in Celsius
    temperature = bme.readTemperature();   
    // Uncomment the next line to set temperature in Fahrenheit 
    // (and comment the previous temperature line)
    //temperature = 1.8 * bme.readTemperature() + 32; // Temperature in Fahrenheit    この行のコメントを有効化すると、摂氏ではなく華氏で表示できる
    
    // Convert the value to a char array
    char tempString[8];           //char:符号(+や-)付きの1バイトの数値を記憶する
    dtostrf(temperature, 1, 2, tempString);
    Serial.print("Temperature: ");
    Serial.println(tempString);
    client.publish("esp32/temperature", tempString);

    humidity = bme.readHumidity();
    
    // Convert the value to a char array
    char humString[8];            //char:符号(+や-)付きの1バイトの数値を記憶する
    dtostrf(humidity, 1, 2, humString);
    Serial.print("Humidity: ");
    Serial.println(humString);
    client.publish("esp32/humidity", humString);
  }
}
