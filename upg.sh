#!/bin/bash

#####################################################
# Date: 2011-6-7
# Description: cp upgrade file
#        support redhat5.3_32,suse11
# History: 
###################################################

UPGRADE_LOG=/etc/.tools_upgrade/pv_upgrade.log
resultFile=/tmp/uvptools_temp/tmp_result
DEST_DIR=/tmp/uvptools_temp

oldVfile=/var/run/tools_upgrade/pvdriver_version.ini
newVfile=/etc/.tools_upgrade/pvdriver_version.ini

dir=`dirname $0`
UPGRADE_FILE=install


v_min_size=40960
ALIAS_CP=0
ALIAS_MV=0
ALIAS_RM=0

chattr -i ${DEST_DIR} 2>/dev/null
find ${DEST_DIR}/pvdriver -type f | xargs chattr -i 2>/dev/null

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

function main()
{
    modify_alias
    
    echo "$(date +%x-%X) start run upg.sh" >> ${UPGRADE_LOG}
    v_avail=`df / | grep "[0-9]%" | sed "s/%.*//" | awk '{print $(NF-1)}'`
    if [ "${v_avail}" -lt "${v_min_size}" ];then
        echo "$(date +%x-%X) The disk space is not enough to install." >> ${UPGRADE_LOG}
        echo failed:pvdriver:no-space >> ${resultFile}
        exit 1
    fi
    
    
    if [ ! -f ${dir}/$UPGRADE_FILE ]; then
        echo "$(date +%x-%X) install files is not exist" >> $UPGRADE_LOG
        echo failed:pvdriver:no-install >> ${resultFile}
        restore_alias
        exit 1   
    fi
    
    dos2unix ${dir}/$UPGRADE_FILE 1>/dev/null 2>&1
    chmod +x ${dir}/$UPGRADE_FILE 1>/dev/null 2>&1
    
    
    cd ${dir} && ./${UPGRADE_FILE} -i 1>/dev/null 2>&1
    exit_value=$?    
    ## PD Driver upgrade successfully with return value 0 , monitor single upgrade successfully with return value 3
    if [ $exit_value -ne 0 -a $exit_value -ne 3 ];then
          echo "$(date +%x-%X) $UPGRADE_FILE -i  error" >> $UPGRADE_LOG
          restore_alias
          echo failed:pvdriver:error-install >> ${resultFile}
          exit 1
    fi
    echo "$(date +%x-%X) ${UPGRADE_FILE} -i  successful" >> $UPGRADE_LOG
    
    if diff ${oldVfile} ${newVfile} 1>/dev/null 2>&1
    then
        restore_alias
        echo success:pvdriver:uninstall >> ${resultFile}
        echo "$(date +%x-%X) success:pvdriver:uninstall" >> $UPGRADE_LOG
        exit 5
    else
        if [ $exit_value -eq 0 ];
        then
            echo success:pvdriver:ready-upgrade >> ${resultFile}
            echo "$(date +%x-%X) success:pvdriver:ready-upgrade" >> $UPGRADE_LOG
        fi
        if [ $exit_value -eq 3 ];
        then
           echo "$(date +%x-%X) success:pvdriver:monitor-upgrade-ok" >> $UPGRADE_LOG
           cat ${newVfile} > ${oldVfile}
           exit 3
        fi
    fi
    
    restore_alias
    
}

main
