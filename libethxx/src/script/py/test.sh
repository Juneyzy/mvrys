#!/bin/bash

#SEV Parameter
SEV_USER=oracle
SEV_IP=192.168.40.210
SEV_MOUDLE_DATA=PR_ORIG_DAT

#CLI Parsmeter
PWD_FILE=/etc/rsyncd.pwd
MONITOR_SRC_DATA=`echo $1`
LOG=~/rsync.log
RSYNC=/usr/bin/rsync

if [ -a "$PWD_FILE" ] && [ -d "$MONITOR_SRC_DATA" ]
then
	$RSYNC -vzrtopg --delete --progress --password-file=$PWD_FILE \
		$MONITOR_SRC_DATA $SEV_USER@$SEV_IP::$SEV_MOUDLE_DATA >> $LOG;
else
	echo "ERROR: If your ($PWD_FILE) or ($MONITOR_SRC_DATA) exist ?  "
fi
