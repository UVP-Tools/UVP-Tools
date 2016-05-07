#!/bin/bash

MEM_DEVTREE_PATH="/sys/devices/system/memory"

if [ ! -d ${MEM_DEVTREE_PATH} ]
then
	echo "cannot access ${MEM_DEVTREE_PATH}: No such file or directory"
	exit 1
fi

for mem_index in ${MEM_DEVTREE_PATH}/memory*
do 
	state=$(grep offline ${mem_index}/state)
	if [ -n "$state" ]
	then
		echo online > ${mem_index}/state 
		echo "${mem_index}/state change to online."
	fi
done

allonline=$(grep offline ${MEM_DEVTREE_PATH}/*/state)
if [ -z "$allonline" ]
then
	echo "All hotplug memories have online."
else
	echo "There are memories offline, please execute the script again."
fi
