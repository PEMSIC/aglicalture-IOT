#!/bin/sh

# define

#PROXIP=192.168.1.100
#DBPATH=shirayaki.mydns.jp/kabayaki/db/rain
DBlocalPATH=localhost:3326/db/rain
DBPATH=mist-hospital.mydns.jp/kabayaki/db/rain
FIELD=1001

#
#DBPATH=$(printf "http://%s/%s" $PROXIP $DBPATH)

DATESTR=`date -u +%FT%H:%M:%S`

# proxy
#CMD=$(printf "curl -X POST -H 'Content-type: application/json' -d '{ \"field\": %s, \"addr\": 1, \"detect\":\"%sZ\", \"rain\":1 }' --noproxy %s %s" $FIELD $DATESTR $PROXIP $DBPATH)

# 
CMDlocal=$(printf "curl -X POST -H 'Content-type: application/json' -d '{ \"field\": %s, \"addr\": 1, \"detect\":\"%sZ\", \"rain\":1 }' %s %s" $FIELD $DATESTR $DBlocalPATH)
CMD=$(printf "curl -X POST -H 'Content-type: application/json' -d '{ \"field\": %s, \"addr\": 1, \"detect\":\"%sZ\", \"rain\":1 }' %s %s" $FIELD $DATESTR $DBPATH)

# test
echo CMD : $CMDlocal 
echo CMD : $CMD 

# exec

eval $CMDlocal
eval $CMD
