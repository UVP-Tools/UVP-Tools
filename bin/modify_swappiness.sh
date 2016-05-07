#!/bin/bash

log() {
	local level="$1"
	shift
	logger -p "daemon.$level" -- "$0:" "$@" || echo "$0 $@" >&2 &
}

if [ "$1" = "modify" ]
then
	old=`cat /proc/sys/vm/swappiness`
	if [ "$old" = "1" ]
	then
		exit 0
	fi
	cat /proc/sys/vm/swappiness > /etc/.uvp-monitor/swappiness_old
	echo 1 > /proc/sys/vm/swappiness
	new=`cat /proc/sys/vm/swappiness`
	log info "modify in suspend,swappiness is $new"
else
	if [ ! -e "/etc/.uvp-monitor/swappiness_old" ]
	then
		exit 0
	fi
	old=`cat /etc/.uvp-monitor/swappiness_old`
	echo $old > /proc/sys/vm/swappiness
	rm -rf /etc/.uvp-monitor/swappiness_old
	old=`cat /proc/sys/vm/swappiness`
	log info "restore in resume,swappiness is $old"
fi
