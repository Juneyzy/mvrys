#!/bin/bash

#SEV Parameter
SEV_USER=`echo $2`
SEV_IP=`echo $3`
SEV_MOUDLE_DATA=PR_ORIG_DAT

#CLI Parsmeter
PWD_FILE=/etc/rsyncd.pwd
BACKUP_PATH=`echo $1`
RSYNC=/usr/bin/rsync

if [ -a "$PWD_FILE" ] && [ -d "$BACKUP_PATH" ]
then
	$RSYNC -vzrtopg --delete --progress --password-file=$PWD_FILE \
		$BACKUP_PATH $SEV_USER@$SEV_IP::$SEV_MOUDLE_DATA >> /dev/null;
else
	echo "ERROR: If your pwd_file:($PWD_FILE) or backup_path:($BACKUP_PATH) exist ?  "
fi
