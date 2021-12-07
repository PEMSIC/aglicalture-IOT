#! /usr/bin/python  shebangというLinux環境でスクリプトの1行目に記述する文字列


# import module
import RPi.GPIO as GPIO     #ラズパイのGPIOピンを制御
import time                 #time:時間を扱う 現在の時刻が取得できる
import datetime             #datetime:日付や時刻を扱う 日付と時刻を合わせて表現できる
import threading            #threading:並行処理を行う

# log file path     グローバル変数 
l_path = '/var/opt/kabayaki/data/'
l_path2 = '/var/opt/kabayaki/raingage/'

# ID for rain gauge (if need, please get ID of actual rain gauge device)    グローバル変数
id = 'raingauge001' 

# drop volume of the target rain gauge  対象となる雨量計の滴下量    0.1667で1回分？     グローバル変数
k = 0.1667

# pulse count       グローバル変数
count = 0

# prev status of GPIO       グローバル変数
prev_status = 0     #prev:前の

# prev hour for calculating rainfall per hour       1時間あたりの降水量を計算するために必要     グローバル変数
prev_hour = 0

# polling interval      グローバル変数
polling_interval = 0.01

# polling function
def pulse_polling():
    global count
    global prev_status
    if prev_status == 0 and GPIO.input(4) == 1:
        count = count + 1
        d = datetime.datetime.today()   #タイムゾーン情報を受け取れない     タイムゾーン:同じ標準時を利用する地域や区分
        now = datetime.datetime.now()   #タイムゾーン情報を受け取れる

# for KK compatible : log file name     compatible:互換
        fallTime = "fallTime" + str(d.year) + "_" + str(d.month) + "_" + str(d.day) + ".txt"
        fallTime2 = "rain" + now.strftime("%Y%m%d") + ".log"        #strftime():引数で指定された書式文字列に従い、日付を表示する    %Y:西暦4桁, %m:月2桁, %d日2桁, %A:曜日
# for kk compatible : log data text (1 rain drop)
        catchTime = str(d.hour) + ":" + str(d.minute) + ":" + str(d.second)
        catchTime2 = now.strftime("\"%Y/%m/%d\",\"%H:%M:%S\"")      #%H:0埋めした10進数で表記した時 00,01,...23 %M:0埋めした10進数で表記した分 00,01,...59 %S:同様に秒
# for KK compatible : log file open
        f = open("/var/log/raingage/"+fallTime, "a")
        try:
            f2 = open(l_path2 + fallTime2, "r")                     #ファイル操作	open(ファイル名, オプション, 文字エンコード)    r:読み込み
        except IOError:
            f2 = open(l_path2 + fallTime2, "a")
            f2.write("\"date\",\"time\",\"target\",\"drop\"\n")
        else:
            f2 = open(l_path2 + fallTime2, "a")

###
        #mesg = "pulse count catch at "+str(count)+", time:"+catchTime

        mesg = catchTime

# for KK compatible : log output (1 rain drop)
        f.write(str(mesg))
        f2.write(str(catchTime2)+",\""+id+"\",\"1\"\n")
        f.write("\n")
        f.close()
        f2.close()
###
    prev_status = GPIO.input(4)


# record file per hour
def record():
    global count
    tStamp = datetime.datetime.today()
    sleepInterval = (59 - tStamp.minute) * 60 + (60 - tStamp.second)
    recordTime = tStamp.hour - 1
    if recordTime < 0:
        recordDay = tStamp.day - 1
        recordTime = 23
# for KK compatible : log file name
        fallCountPerHour = "fallCountPerHour" + str(tStamp.year) + "_" + str(tStamp.month) + "_" + str(recordDay) + ".txt"
    else:
        fallCountPerHour = "fallCountPerHour" + str(tStamp.year) + "_" + str(tStamp.month) + "_" + str(tStamp.day) + ".txt"

# for KK compatible : log file open
	now = datetime.datetime.now()
	fallCountPerHour2 = "precipitation" + now.strftime("%Y%m%d") + ".log"
	print "test\n"
	print "test "+str(fallCountPerHour2)+"\n"
    recordFile = open("/var/log/raingage/"+fallCountPerHour, "a")
    try:
        recordFile2 = open(l_path + fallCountPerHour2, "r")
    except IOError:
        recordFile2 = open(l_path + fallCountPerHour2, "a")
        recordFile2.write("\"date\",\"time\",\"ID\",\"precipitation[mm/h]\"\n")
    else:
        recordFile2 = open(l_path +fallCountPerHour2, "a")

#for KK compatible
    mesg = str(recordTime) + ", " + str(count)
    pctt = 1.0 * count * k
    mesg2 = now.strftime("\"%Y/%m/%d\",\"%H:%M:%S\",\""+ str(id) +"\",\"" + str(pctt)+"\"\n")

# for KK compatible
    recordFile.write(str(mesg))
    recordFile2.write(str(mesg2))
    recordFile.write("\n")
    recordFile.close()
    recordFile2.close()
###
    count = 0
    t=threading.Timer(sleepInterval,record)
    t.start()


# main function
def main():
    global prev_status
    prev_status = GPIO.input(4)
    d = datetime.datetime.today()
    sleepInterval = (59 - d.minute) * 60 + (60 - d.second)
    t=threading.Timer(sleepInterval,record)
    t.start()
    while True:
        pulse_polling()
        time.sleep(polling_interval)

# GPIO is designated by GPIO number
GPIO.setmode(GPIO.BCM)

# GPIO initialize
GPIO.cleanup()

# GPIO No.4 is input from raingage
GPIO.setup(4, GPIO.IN)

# polling start
if __name__ == '__main__':
    main()
