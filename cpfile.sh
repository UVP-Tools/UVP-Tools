#!/bin/bash

#####################################################
# Date: 2011-6-7
# Description: cp upgrade file
#		support redhat5.3_32,suse11
# History: 
# Date: 2011-6-24
# 1、修改所有redhat系统名称为redhat$_centos$名称
# 2、新增系统
###################################################

OS_VERSION=`uname -r`
OS_RELEASE=""
MOUNT_DIR=`dirname $0`
tar_file=""
UPGRADE_LOG=/etc/.tools_upgrade/pv_upgrade.log
DEST_DIR=/tmp/uvptools_temp
PATH=$PATH:/bin:/sbin:/usr/sbin:/usr/bin
BINARY='/usr/bin/uvp-monitor'
TMP_FILE=/tmp/pv_temp_file

####add for optimize upgrade 
SYS_TYPE=""
####added end

v_min_size=40960
ALIAS_CP=0
ALIAS_MV=0
ALIAS_RM=0

PIDFILE=$(basename "$BINARY")
if [ -f /etc/redhat-release -o -n "$(grep -i 'GreatTurbo' /etc/issue)" ]
then
    PIDFILE="/var/lock/subsys/$PIDFILE"
elif [ -f /etc/SuSE-release ]
then
    PIDFILE="/var/run/$PIDFILE"
elif [ -f /etc/debian_version ]
then
    PIDFILE="/var/run/$PIDFILE"
else
    if [ -d /var/run -a -w /var/run ]
    then
        PIDFILE="/var/run/$PIDFILE"
    fi
fi

cpu_arch_width=""
CPU_ARCH="$(uname -m)"
case "$CPU_ARCH" in
i[3456789]86|x86)
    cpu_arch_width="32"
    ;;
x86_64|amd64)
    cpu_arch_width="64"
    ;;
*)
    echo "$(date +%x-%X) Can not determine processor type" >> ${UPGRADE_LOG}
    exit 1
esac


# check upgrade package
function check_exit()
{
	tar_file=uvp-tools-*.tar.bz2
    
    if [ ! -f ${MOUNT_DIR}/${tar_file} ]; then
    	echo "$(date +%x-%X) uvp-tools.tar.bz2 no exist" >> ${UPGRADE_LOG}
    	exit 1
    fi
}

# remove -i
function modify_alias()
{
    alias | grep "cp -i" >/dev/null 2>&1
    if [ $? -eq 0 ];then
  	alias cp='cp'
  	ALIAS_CP=1
    fi	
    
    alias | grep "mv -i" >/dev/null 2>&1
    if [ $? -eq 0 ];then
  	alias mv='mv'
  	ALIAS_MV=1
    fi
    
    alias | grep "rm -i" >/dev/null 2>&1
    if [ $? -eq 0 ];then
  	alias rm='rm'
  	ALIAS_RM=1
    fi
}

# restore alias
function restore_alias()
{
    if [ $ALIAS_CP -eq 1 ];then
  	alias cp='cp -i'
    fi
    
    if [ $ALIAS_MV -eq 1 ];then
  	alias mv='mv -i'
    fi
    
    if [ $ALIAS_RM -eq 1 ];then
  	alias rm='rm -i'
    fi
}
####added for check linux distribution to obtain SYS_TYPE
function check_distribution()
{
    local issue
    ###determine linux distribution
    issue='/etc/issue'
    if [ -e '/etc/debian_version' -o -n "$(grep -i 'debian' $issue)" ]
    then
        SYS_TYPE='debian'
    elif [ -e '/etc/redhat-release' -o -n "$(grep -i 'red *hat' $issue)" -o -n "$(grep -i 'GreatTurbo' $issue)" ]
    then
        SYS_TYPE='redhat'
    elif [ -e '/etc/SuSE-release' -o -n "$(grep -i 'suse\|s\.u\.s\.e' $issue)" ]
    then
        SYS_TYPE='suse'
    elif [ -e '/etc/gentoo-release' ]
    then
        SYS_TYPE='gentoo'
    else
        echo "$(date +%x-%X) cannot determine linux distribution." >> ${UPGRADE_LOG}
        exit 1
    fi
}
###added end
function main()
{
    modify_alias
    check_exit
    check_distribution 

    ###get install package full name such as uvp-tools-linux-1.3.0.13-164.tar.bz2
    tar_file_basename=`basename ${MOUNT_DIR}/${tar_file}`
    echo "cpfile:$(date +%x-%X) tar_file_basename ${tar_file_basename}" >> ${UPGRADE_LOG}

    ###get uvp-tools file name such as uvp-tools-linux-1.3.0.13-164
    tar_uvpversion=$(echo ${tar_file_basename} | sed "s/.tar.bz2//g")
    echo "cpfile:$(date +%x-%X) tar_uvpversion ${tar_uvpversion}" >> ${UPGRADE_LOG}

    ###get xvf_modules_path such as /mnt/pvmount/lib/modules/xvf-2.6.18-128.el5-i686-RHEL5
    xvf_modules_path=$(. ${MOUNT_DIR}/get_uvp_kernel_modules "$SYS_TYPE" "$(uname -r)" "$(uname -m)")
    echo "cpfile:$(date +%x-%X) xvf_modules_path ${xvf_modules_path}" >> ${UPGRADE_LOG}

    ###get xvf_modules_path basename such as xvf-2.6.18-128.el5-i686-RHEL5
    xvf_modules=`basename ${xvf_modules_path}`
    echo "cpfile:$(date +%x-%X) xvf_modules ${xvf_modules}" >> ${UPGRADE_LOG}

    echo "$(date +%x-%X) start run cpfile.sh." >> ${UPGRADE_LOG}
	
    v_avail=`df / | grep "[0-9]%" | sed "s/%.*//" | awk '{print $(NF-1)}'`
    if [ "${v_avail}" -lt "${v_min_size}" ];then
        echo "$(date +%x-%X) The disk space is not enough to install." >> ${UPGRADE_LOG}
        exit 1
    fi
    
    chattr -i ${DEST_DIR} 2>/dev/null
    find ${DEST_DIR}/pvdriver -type f | xargs chattr -i 2>/dev/null
    rm -rf ${DEST_DIR}/pvdriver 2>/dev/null
    rm -rf ${DEST_DIR}/uvp-tools-* 2>/dev/null
    mkdir -p ${DEST_DIR} 2>/dev/null

    ####modified for optimization : tar the system needed file
    tar -jxf ${MOUNT_DIR}/${tar_file_basename} -C ${DEST_DIR} ./${tar_uvpversion}/bin ./${tar_uvpversion}/config ./${tar_uvpversion}/install ./${tar_uvpversion}/usr ./${tar_uvpversion}/etc ./${tar_uvpversion}/lib/modules/$xvf_modules 1>/dev/null 2>&1
    ####modified end

    if [ $? -ne 0 ];then
      	echo "$(date +%x-%X) tar -jxf files failed!" >> ${UPGRADE_LOG}
      	restore_alias
      	exit 1
    fi
	
	mv ${DEST_DIR}/uvp-tools-* ${DEST_DIR}/pvdriver 1>/dev/null 2>&1
    if [ $? -ne 0 ];then
      	echo "$(date +%x-%X) mv ${DEST_DIR}/uvp-tools-* failed!" >> ${UPGRADE_LOG}
      	restore_alias
      	exit 1
    fi
	
	cp -f ${MOUNT_DIR}/upg.sh ${DEST_DIR}/pvdriver 1>/dev/null 2>&1
    if [ $? -ne 0 ];then
      	echo "$(date +%x-%X) mv ${DEST_DIR}/uvp-tools-* failed!" >> ${UPGRADE_LOG}
      	restore_alias
      	exit 1
    fi

    dos2unix /tmp/uvptools_temp/pvdriver/upg.sh 1> /dev/null 2>&1
    chmod +x /tmp/uvptools_temp/pvdriver/upg.sh 2> /dev/null
    chattr +i ${DEST_DIR} 2>/dev/null
    find ${DEST_DIR}/pvdriver -type f | xargs chattr +i 2>/dev/null
    if [ "$?" != "0" ]
    then
        touch ${TMP_FILE} 2>/dev/null
        if [ $? -ne 0 ];then
            echo "$(date +%x-%X) touch ${TMP_FILE} failed!" >> ${UPGRADE_LOG}
            restore_alias
            rm -f "${TMP_FILE}" 2>/dev/null
            exit 1
        fi
        echo "$(date +%x-%X) copy uvp-monitor begin" >> $UPGRADE_LOG

        cp -f ${DEST_DIR}/pvdriver/usr/bin${cpu_arch_width}/uvp-monitor ${BINARY} 1>/dev/null 2>&1
        if [ $? -ne 0 ];then
            echo "$(date +%x-%X) copy uvp-monitor failed!" >> ${UPGRADE_LOG}
            restore_alias
            rm -f "${TMP_FILE}" 2>/dev/null
            exit 1
        else
            echo "$(date +%x-%X) copy uvp-monitor ok!" >> $UPGRADE_LOG
        fi

        old_ppid=`ps -ef |grep ${BINARY} |grep -v grep | awk '{print $2}' | sort -nu | head -1`
        echo "$(date +%x-%X) old_ppid is ${old_ppid}, restart uvp-monitor begin" >> ${UPGRADE_LOG}
        #/etc/init.d/uvp-monitor restart
        kill -15 ${old_ppid}
        rm -f "${PIDFILE}" 2>/dev/null

        pid_num=`ps -ef |grep ${BINARY} |grep -v grep | awk '{print $2}' | sort -nu | wc -l`
        if [ "${pid_num}" != "0" ];then
            echo "$(date +%x-%X) kill -15 failed, try again." >> ${UPGRADE_LOG}
            ppid=`ps -ef |grep ${BINARY} |grep -v grep | awk '{print $2}' | sort -nu | head -1`
            kill -9 ${ppid}
            rm -f "${PIDFILE}" 2>/dev/null
        fi
        sleep 1
        #${BINARY} #exec uvp-monitor now
        /etc/init.d/uvp-monitor restart

        #service uvp-monitor restart
        new_ppid=`ps -ef |grep ${BINARY} |grep -v grep | awk '{print $2}' | sort -nu | head -1`
        pid_num=`ps -ef |grep ${BINARY} |grep -v grep | awk '{print $2}' | sort -nu | wc -l`
        if [ -z "${new_ppid}" ] || [ "${old_ppid}" = "${new_ppid}" ] || [ "${pid_num}" = "0" ];then
            echo "$(date +%x-%X) new_ppid is ${new_ppid}, restart uvp-monitor failed" >> ${UPGRADE_LOG}
            restore_alias
            rm -f "${TMP_FILE}" 2>/dev/null
            exit 1
        else
            echo $new_ppid > $PIDFILE
            echo "$(date +%x-%X) new_ppid is ${new_ppid}, restart uvp-monitor ok" >> ${UPGRADE_LOG}
        fi
    fi
    
    echo "$(date +%x-%X) copy file successful" >> $UPGRADE_LOG

    restore_alias
}

main
