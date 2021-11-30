#!usr/bin/env python
# -*- coding: utf-8 -*- 

import paho.mqtt.client as mqtt     # MQTTのライブラリをインポート
from time import sleep              # 3秒間のウェイトのために使う

# ブローカーに接続できたときの処理
def on_connect(client, userdata, flag, rc):
  print("Connected with result code " + str(rc))

# ブローカーが切断したときの処理
def on_disconnect(client, userdata, flag, rc):
  if rc != 0:
     print("Unexpected disconnection.")

# publishが完了したときの処理
def on_publish(client, userdata, mid):
  print("publish: {0}".format(mid))

# メイン関数   この関数は末尾のif文から呼び出される
def main():
  client = mqtt.Client()                 # クラスのインスタンス(実体)の作成
  client.on_connect = on_connect         # 接続時のコールバック関数を登録
  client.on_disconnect = on_disconnect   # 切断時のコールバックを登録
  client.on_publish = on_publish         # メッセージ送信時のコールバック

  # adress = "mms.mist-hospital.mydns.jp"
  adress = "133.45.129.18"

  client.connect(adress, 11883, 60)  # 接続先は自分自身
  print("0")
  # 通信処理スタート
  client.loop_start()    # subはloop_forever()だが，pubはloop_start()で起動だけさせる
  print("1")
  # 永久に繰り返す
  # while True:
  client.publish("agriIoT/message",'{"id":2, "user_name": "testssss", "node_name": "testssss", "message":"tessting"}')    # トピック名とメッセージを決めて送信
  print("2")
  sleep(3)   # 3秒待つ

if __name__ == '__main__':          # importされないときだけmain()を呼ぶ
  main()    # メイン関数を呼び出す