#!/bin/bash

#SEV Parsmeter
SEV_USER=oracle
SEV_IP=192.168.40.210
SEV_MOUDLE_DATA=PR_ORIG_DAT

#CLI Parsmeter
PWD_FILE=/etc/rsyncd.pwd
RSYNC=/usr/bin/rsync

#Imput Parsmeter
BACKUP_PATH=`echo $1`

#push data from sev
if [ -a "$PWD_FILE" ] && [ -d "$BACKUP_PATH" ]
then
        $RSYNC -vzrtopg --delete --progress --password-file=$PWD_FILE \
		$BACKUP_PATH $SEV_USER@$SEV_IP::$SEV_MOUDLE_DATA >> /dev/null;
else
        echo "ERROR: If your pwd_file:($PWD_FILE) or backup_path:($BACKUP_PATH) exist ?  "
fi


#lookup path
for file in ` find $BACKUP_PATH -name "*.index"`;
do
	if [ -e "$file" ]
	then :
		./script/py/load.py "$file"
	else
		echo "That's $file not exist"
	fi
done
