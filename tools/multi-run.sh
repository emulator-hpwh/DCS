#!/bin/bash
#run lots of simulated dcs
num="$1"

if [ "$num" = "" ];
then
	echo You must enter the number of clients
else
	n=0
	while [ $n -lt $num ];
	do
		tmux new-window -d "./build-run.sh $n"
		sleep 10
		let n+=1
	done
fi