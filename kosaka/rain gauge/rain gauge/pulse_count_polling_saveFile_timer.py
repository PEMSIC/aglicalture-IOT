#! /usr/bin/python  shebangというLinux環境でスクリプトの1行目に記述する文字列

# import module
import RPi.GPIO as GPIO #ラズパイのGPIOピンを制御
import time     #time:時間を扱う 現在の時刻が取得できる
import datetime #datetime:日付や時刻を扱う 日付と時刻を合わせて表現できる
import threading    #threading:並行処理を行う
import os   #OSに依存する機能を扱えるようにする


# pulse count   おそらく雨量計がたまった雨水に反応した回数	countは関数の外で宣言しているので、グローバル変数
count = 0

# prev status of GPIO   グローバル変数
prev_status = 0     #prev:前の

# prev hour for calculating rainfall per hour   グローバル変数
prev_hour = 0

# polling interval  グローバル変数
polling_interval = 0.01

# polling function
def pulse_polling():    #def:関数を表す pulse_count.pyでの関数と中身が同じ

    global count    #global:関数内でグローバル変数に値を代入する時に使用
    global prev_status

    if prev_status == 0 and GPIO.input(4) == 1:
        count = count + 1
        d = datetime.datetime.today()       #表示例 2017-11-08 23:58:55.230456
        fallTime = "fallTime" + str(d.year) + "_" + str(d.month) + "_" + str(d.day) + ".txt"    #str:引数に指定したオブジェクトを文字列にする 表示 fallTime2020_5_9.txt
        catchTime = str(d.hour) + ":" + str(d.minute) + ":" + str(d.second)
        f = open("/var/log/raingage/"+fallTime, "a")    #ファイル操作	open(ファイル名, オプション, 文字エンコード)	a:ファイルへの追加	w:ファイルへの書き込み(上書きする)	x:ファイルの新規作成
        #mesg = "pulse count catch at "+str(count)+", time:"+catchTime
        mesg = catchTime
        f.write(str(mesg))          #ファイルに対してテキストを書き込む
        os.system('./db_call.sh')   #unixコマンドをpython上で記述するためのモジュール	osをインポートする必要がある
        f.write("\n")   #改行
        f.close()       #ファイルを閉じる
    prev_status = GPIO.input(4)

# record file per hour
def record():   #pulse_count.pyでの関数と中身が同じ

    global count
    tStamp = datetime.datetime.today()
    sleepInterval = (59 - tStamp.minute) * 60 + (60 - tStamp.second)
    recordTime = tStamp.hour - 1

    if recordTime < 0:
        recordDay = tStamp.day - 1
        recordTime = 23
        fallCountPerHour = "fallCountPerHour" + str(tStamp.year) + "_" + str(tStamp.month) + "_" + str(recordDay) + ".txt"

    else:
        fallCountPerHour = "fallCountPerHour" + str(tStamp.year) + "_" + str(tStamp.month) + "_" + str(tStamp.day) + ".txt"

    recordFile = open("/var/log/raingage/"+fallCountPerHour, "a")
    mesg = str(recordTime) + ", " + str(count)
    recordFile.write(str(mesg)) #ファイルに対してテキストを書き込む
    recordFile.write("\n")      #改行
    recordFile.close()          #ファイルを閉じる閉じる
    count = 0                   #カウントをリセット？
    t=threading.Timer(sleepInterval,record) #sleepIntervalの設定時間後にrecord()が実行される
    t.start()   #タイマースタート

# main function
def main():
    global prev_status              #グローバル変数prev_status
    prev_status = GPIO.input(4)     #GPIO4のピンの状態をprev_statusに代入
    d = datetime.datetime.today()   #現在の日付と時間を取得
    sleepInterval = (59 - d.minute) * 60 + (60 - d.second)      #スリープの間隔	reccord()で定義されていたtStampと中身は同じ
    t=threading.Timer(sleepInterval,record)     #sleepIntervalの設定時間後にrecord()が実行される
    t.start()   #タイマースタート
    while True: #ループ		条件式がTrueなので、無限ループ
        pulse_polling()     #pulse_polling()関数の呼び出し
        time.sleep(polling_interval)    #polling_intervalの設定時間秒処理を停止     polling_interval=0.01

# GPIO is designated by GPIO number
GPIO.setmode(GPIO.BCM)          #GPIOの各ピンを指定	GPIOの数字で指定する	(例)ピン番号3番のGPIO02を使用する場合はBOARDなら3, BCMなら2と指定する

# GPIO initialize
GPIO.cleanup()                  #すべてのGPIOを解放して終了

# GPIO No.4 is input from raingage
GPIO.setup(4, GPIO.IN)          #GPIO04を入力として指定

# polling start
if __name__ == '__main__':      #インポートされて際にプログラムが勝手に動かないようにするために必要
    main()
