#include <WiFi.h>
#include <ArduinoJson.h>    //arduinoでjsonを使うためのライブラリ
#include <PubSubClient.h>   //MQTT通信のために必要
#include <Wire.h>

#define RAIN_GAUGE_PIN 25
#define POLLING_INTERVAL 1     //POLLING_INTERVAL=1が一番うまく動作した

unsigned long recent_time;
unsigned long prev_time = 0; 
int prev_status = 0;    //全ての関数で見える変数
int count = 0;
int prev_count = 0;
int recent_count;
int Rcount;

const char MY_SSID[] = "E213"; //PIX-MT100
const char MY_PASSWORD[] = "213wl213wl";  //mistmist
const char* mqtt_server = "133.45.129.18"; //MQTTのIPアドレスまたはホスト名　追加  133.45.129.190  mms.mist-hospital.mydns.jp
const uint16_t mqtt_port = 11883; //MQTTブローカのポート番号(TCP接続)


WiFiClient espClient;           //client.connect()で定義されている指定のインターネットIPアドレスおよびポートに接続できるクライアントを作成?
PubSubClient client(espClient);     //MQTTの通信を行うためのPubsubClientのクラスから実際に処理を行うオブジェクトclientを作成?

void pulse_polling();         //関数：雨量計が反応した際にカウントを進める
void record_file_per_hour();  //関数：1時間当たりの雨量計が反応した回数を計算する
void send_json_rain();        //関数：jsonに変換&送信する

void reconnect();             //関数：再接続
void callback(char* topic, byte* message, unsigned int length);
void wifi_startup();

void setup() {
  Serial.begin(115200, SERIAL_8N1); //ビットレート115200 8bit/パリティなし/1ストップビット
  pinMode(RAIN_GAUGE_PIN,INPUT);    //7番ピンを入力に設定

  setup_wifi();   //関数(WiFi接続)
  client.setServer(mqtt_server, 11883);    //インスタンス化(実体化)したオブジェクトclientの接続先のサーバを、アドレスとポート番号を設定して通信できるようにする　追加
  client.setCallback(callback);   //callback関数の設定　追加

  if (!client.connected()) {  //接続できていない場合
    reconnect();    //関数(再接続)
  }
  client.loop();    //通信できているかの確認

  }


void loop() {
  recent_time = millis();    //プログラムが動き始めた時間(ミリ秒)を取得　約50日でオーバーフローし、0に戻る

  if(recent_time == 0){ //オーバーフロー対策 recent_timeが0に戻った際にprev_timeも0に戻す
    prev_time = 0;
  }
  
  digitalRead(RAIN_GAUGE_PIN);    //7番ピンの状態を取得
  pulse_polling();  //関数：雨量計が反応した際にカウントを進める
  //Serial.println(prev_status,DEC);
  delay(POLLING_INTERVAL);  //0.001秒待機
  
  
  if(recent_time - prev_time == 10000){   //1時間(60*60*1000=3600000)経ったら1時間ごとの雨量計が反応した回数を計算し、送信する   テスト用：10秒(10000)
    //Serial.print("recent_count:");
    //Serial.println(count,DEC);    //1時間立った時の雨量計のカウント
    recent_count = count;         
    record_file_per_hour();       //関数：1時間当たりの雨量計が反応した回数を計算する
    send_json_rain();           //関数：jsonに変換&送信する
    prev_time = recent_time;
    Serial.println(recent_time,DEC);    //反応テスト用
    prev_count = count;
  }
  
}

void pulse_polling(){   //関数：雨量計が反応した際にカウントを進める
  if(digitalRead(RAIN_GAUGE_PIN)==LOW && prev_status == 0){   //ジャンパー線を繋げた瞬間
    count = count + 1;            //カウントを進める
    Serial.print("rain gauge count:");
    Serial.println(count,DEC);    //現在の雨量計のカウントを表示
    prev_status = 1;              //一度のパルスで連続してカウントが進まないようにするための変数
  } 
  else if(digitalRead(RAIN_GAUGE_PIN)==HIGH){   //ジャンパー線を外しているとき
    prev_status = 0;              //一度バルスがなくなったので、次にパルスが来たときにカウントを進められるようにする
  }
  else{   //ジャンパー線を繋げ続けているとき
      //特に処理はなし
  }
}

void record_file_per_hour(){    //関数：1時間当たりの雨量計が反応した回数を計算する
  Rcount = recent_count - prev_count;
  Serial.print("rain gauge per hour count:");
  Serial.println(Rcount,DEC);    //1時間当たりの雨量計のカウント
}

void send_json_rain(){    //関数：jsonに変換&送信する
  // JSON用の固定バッファを確保。
    //この例では300文字分。
    StaticJsonDocument<200> doc;
   
    doc["rain_gauge_per_hour"] = Rcount;

    String requestBody;
    serializeJson(doc, requestBody);

     const char* requestChar = requestBody.c_str(); //MQTTでpublishするためにはJSONドキュメントをstring型からconst char*型にする必要があるため
    
    if (client.publish("agriIoT/rain_gauge",requestChar) == true) {
      Serial.println("Success sending message");
    } else {
      Serial.println("Error sending message");
    }
    client.loop();
    delay(100);
    
}


//   関数：再接続
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

//setupで使用する関数
void setup_wifi() {   //WiFiに接続するための関数
  delay(10);      //0.01秒待機
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(MY_SSID);

  WiFi.begin(MY_SSID, MY_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());   //WiFIシールドのIPアドレスを入手する    
}
