#!/bin/bash
devname=$1
flag=0
Info()
{
    echo "`date` $1" >>/var/log/unplug.log
}

Info "begin to umount ${devname}"

if [ -z ${devname} ]; then
    exit 1
fi

#TODO:xvdaa xvdab
parts=`mount |grep /dev/${devname} |awk '{print $1}'`
for part in ${parts}
do
    count=0
    while [ 1 ]
    do
        count=`expr ${count} + 1`
        fuser -k -m ${part}
        ret=$?
        Info "kill  ${part} process: ${ret}"
        umount -l ${part}
        ret=$?
        Info "umount -l ${part}: ${ret}"
        if [ $ret = 0 ]; then
            break
	fi
	
        if [ ${count} = 3 ]; then
            flag=1
            break
        fi
    done
done

#handle logic volume
vges=`pvs|grep /dev/${devname}|awk '{print $2}'`
#symbol name may start with 'hd' or 'sd'
if [ "${vges}" = "" ]; then
    devsymbol="hd${devname:3}"
    vges=`pvs|grep /dev/${devsymbol}|awk '{print $2}'`
fi
if [ "${vges}" = "" ]; then
    devsymbol="sd${devname:3}"
    vges=`pvs|grep /dev/${devsymbol}|awk '{print $2}'`
fi

for vg in ${vges}
do
    lves=`lvs |awk '$2=="'$vg'" {print $1}'`
    for lv in ${lves}
    do
        count=0
        lvname="/dev/mapper/${vg}-${lv}"
        echo "${lvname}"
        mountpoint=`mount | awk '$1=="'${lvname}'" {print $1}'`
        if [ "${mountpoint}" = "" ]; then
            break
        fi
        while [ 1 ]
        do
            count=`expr ${count} + 1`
            fuser -k -m /dev/mapper/${vg}-${lv}
            ret=$?
            Info "kill  /dev/mapper/${vg}-${lv} process: ${ret}"
            umount -l /dev/mapper/${vg}-${lv}
            ret=$?
            Info "umount -l /dev/mapper/${vg}-${lv}: ${ret}"
            if [ $ret = 0 ]; then
                break
            fi
            if [ ${count} = 3 ]; then
                flag=1
                break
            fi
        done
    done

    vgchange -an ${vg}
done
Info "end of umount ${devname}: ${flag}"
Info "================================================"
exit ${flag}
