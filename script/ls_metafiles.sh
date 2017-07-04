
time=`date`
slips=`ls $1 -l |grep "^-"|wc -l`
echo $time:$slips >> $2
