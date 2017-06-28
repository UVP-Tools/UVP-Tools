/*
 * Main process
 *
 * Copyright 2016, Huawei Tech. Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation; or, when distributed
 * separately from the Linux kernel or incorporated into other
 * software packages, subject to the following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#include "libxenctl.h"
#include "xenstore_common.h"
#include "public_common.h"
#include <dirent.h>
#include <unistd.h>
#include <mntent.h>
#include "qlist.h"
#include <linux/fs.h>
#include <sys/utsname.h>
#include "securec.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include "uvpmon.h"
#include <sys/time.h>
#include <time.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#ifdef FIFREEZE
#undef FIFREEZE
#endif
#define FIFREEZE        _IOWR('X', 119, int)    /* Freeze */

#ifdef FITHAW
#undef FITHAW
#endif
#define FITHAW          _IOWR('X', 120, int)    /* Thaw */

/*
 * PV OPS kernel
 */
#define KERNEL_PV_OPS "control/uvp/pvops"

#define UVP_UNPLUG_DISK "control/uvp/unplug-disk"

#define CUR_PVDRIVER_VERSION "control/uvp/pvdriver-version"
/*热迁移完成的标志位*/
#define COMPLETE_RESTORE  "control/uvp/completerestore-flag"
/*热迁移开始的标志位*/
#define MIGRATE_FLAG  "control/uvp/migrate_flag"
#define HIBERNATE_MIGRATE_PATH  "/etc/.uvp-monitor/hibernate_migrate_flag.ini"
/*驱动resume完成的标志位*/
#define DRIVER_RESUME_FLAG "control/uvp/driver-resume-flag"
#define SYNC_TIME_FLAG "control/uvp/clock/mode"
#define TIME_BUFFER 100
/*VSA*/
#define PYTHON_PATH  "/opt/galax/vsa/vsaApi/vsa/service/router/service/allintaprping.py"
#define EXEC_PYTHON_PATH  "python /opt/galax/vsa/vsaApi/vsa/service/router/service/allintaprping.py &"
/*Nanoseconds*/
#define NANOTOMICRO  1000000
#define MICROTOSEC   1000
#define MILLITOMICRO 1000
#define TIME_DIFF 1

bool   hibernate_migrate_flag = 0;

/* 查询版本信息等不变信息的键值 */
#define PVDRIVER_STATIC_INFO_PATH "control/uvp/static-info-flag"

//新libvirt 对应的 ipv6 特征键值
#define NETINFO_FEATURE_PATH  "control/uvp/netinfo_feature"

#define BUFFER_SIZE             1024
#define SHELL_BUFFER            256
#define MAX_COMMAND_LENGTH      128
#define VER_SIZE                16
#define LOG_LEVEL_SIZE          8
#define DEFAULT_VERSION         "error"
#define DEFAULT_PATH            "/etc/.uvp-monitor/version.ini"

/* 一致性快照 */
#define STORAGE_SNAPSHOT_FLAG   "control/uvp/storage_snapshot_flag"
#define IOMIRROR_SNAPSHOT_FLAG  "control/uvp/iomirror_snapshot_flag"
#define THAW "thaw"
#define FREEZE "freeze"
#define SHELL_PATH "/usr/bin/userFreeze.sh"
#define DEV_TYPE_NUM 5
#define SUSE_11_SP1 "2.6.32.12-0.7"
#define SUSE_11_SP2 "3.0.13-0.27"
const char *devtype[] = {"ext3", "ext4", "reiserfs", "jfs", "xfs"};
int gfreezeflag = 0;

/*执行OS内部固定路径下的可执行文件*/
#define OS_CMD_XS_PATH "control/uvp/command"
#define CMD_RESULT_XS_PATH "control/uvp/command_result"
#define GUEST_CMD_FILE_PATH  "/etc/.uvp-monitor"
#define CMD_RESULT_BUF_LEN 5
#define ARG_CHECK_OK 1
#define ARG_ERROR 2
#define FILENAME_ERROR 3
#define CHECKSUM_ERROR 4
/* fork失败 */
#define ERROR_FORK 		-1
/* waitpid失败 */
#define ERROR_WAITPID 	-2
/* waitpid失败 */
#define ERROR_PARAMETER -3
#define UNEXPECTED_ERROR 10
char chret[CMD_RESULT_BUF_LEN];
char feature_str[SHELL_BUFFER];

/* VRM is based on SLES */
#define VRM_VERSION "/etc/os_version"
#define SUSE_VERSION "/etc/SuSE-release"
#define VRM_FLAG "control/uvp/vrm_flag"


/* 虚拟机心跳*/
#define XS_HEART_BEAT_RATE "control/uvp/heartbeat_rate"
#define XS_HEART_BEAT "control/uvp/heartbeat"
#define HEART_BEAT_BUF_LEN 5
unsigned int heartbeatrate = 0;
int heartbeatnum = 0;
bool heartbeat_thread_exist_flag = 0;
pthread_mutex_t heartbeat_mutex = PTHREAD_MUTEX_INITIALIZER;

/* don't provide pv-upgrade ability to user-compiled-pv vm */
#define XS_NOT_USE_PV_UPGRADE "control/uvp/not_use_pv_upgrade"

typedef struct FsMount
{
    char* dirname;
    char* devtype;
    QTAILQ_ENTRY(FsMount) next;
} FsMount;
typedef QTAILQ_HEAD(FsMountList, FsMount) FsMountList;

struct Freezearg
{
    void *handle;
    FsMountList *mounts;
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOG_BUF_LEN 256

char fReboot = '0';

void sys_log(const char* process, int Level, const char *func, int line, const char *format, ...)
{
    va_list ap;
    char tmpBuf[SHELL_BUFFER] = {0};
    char Log[BUFFER_SIZE] = {0};
    char szLevel[LOG_LEVEL_SIZE] = {0};
    int ltz = 0;
    int utz = 0;
    int timezone = 0;
    struct timeval tv = {0};
    struct tm *tm = NULL;
    struct tm *tm1 = NULL;
    time_t tt = {0};

    gettimeofday(&tv, NULL);
    tt = tv.tv_sec;
    tm1 = gmtime(&tt);
    utz= tm1->tm_hour;
    tm = localtime(&tt);
    ltz= tm->tm_hour;
    timezone=ltz-utz;
    if (timezone < -12)
    {
        timezone += 24;
    }
    else if (timezone >= 12)
    {
        timezone -= 24;
    }

    switch(Level)
    {
        case LOG_DEBUG:
        {
            strcpy_s(szLevel, LOG_LEVEL_SIZE, "debug");
            break;
        }
        case LOG_INFO:
        {
            strcpy_s(szLevel, LOG_LEVEL_SIZE, "info");
            break;
        }
        case LOG_ERR:
        {
            strcpy_s(szLevel, LOG_LEVEL_SIZE, "err");
            break;
        }
        default:
        {
            strcpy_s(szLevel, LOG_LEVEL_SIZE, "debug");
            break;
        }
    }

    snprintf_s(tmpBuf, SHELL_BUFFER, SHELL_BUFFER,
        "%d-%02d-%02dT%02d:%02d:%02d.%ld%+03d:00|%s|%s[%d]|%s[%d]|:",
        1900+tm->tm_year, tm->tm_mon+1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec,
        timezone, szLevel, process, getpid(), func, line);

    va_start(ap, format);
    vsprintf_s(Log, BUFFER_SIZE, format, ap);
    syslog(Level, "%s %s", tmpBuf, Log);
    va_end(ap);
}

/*****************************************************************************
 Function   : write_vrm_flag
 Description:  note vm is vrm
 Input      : phandle -- xenstore file handle
 Output     : None
 Return     : None
*****************************************************************************/
void write_vrm_flag(void *phandle)
{
    if (NULL == phandle) {
        return;
    }

    if((0 == access(VRM_VERSION, R_OK)) && (0 == access(SUSE_VERSION, R_OK))) {
        INFO_LOG("This is VRM.");
        write_to_xenstore(phandle, VRM_FLAG, "true");
    }
}

void set_guest_feature(void *handle)
{
    int ret = 0;
    char *get_feature_cmd = "cat /etc/.uvp-monitor/GuestOSFeature |grep -w `cat /etc/.uvp-monitor/CurrentOS` | awk '{printf $2}'";
    char *get_cfg_cmd = "cat /etc/.uvp-monitor/CurrentOS";

    (void)memset_s(feature_str, SHELL_BUFFER, 0, SHELL_BUFFER);
    //Can't read cfg files
    if(0 != access("/etc/.uvp-monitor/GuestOSFeature", R_OK)
        || 0 != access("/etc/.uvp-monitor/CurrentOS", R_OK))
    {
        ERR_LOG("Get Guest Feature, Can't read cfg files, errno=%d", errno);
        return;
    }

    ret = uvpPopen(get_cfg_cmd, feature_str, SHELL_BUFFER);
    if(0 != ret)
    {
        ERR_LOG("Failed to call uvpPopen 1, output=%s ret=%d.", feature_str, ret);
        return;
    }
    //Get name form CurrentOS error,maybe "NULL"
    if(0 == strlen(feature_str))
    {
        INFO_LOG("Get name form cfg error.");
        return;
    }

    ret = uvpPopen(get_feature_cmd, feature_str, SHELL_BUFFER);
    if (0 != ret)
    {
        ERR_LOG("Failed to call uvpPopen 2, output=%s ret=%d.", feature_str, ret);
        return;
    }
    //grep from GuestOSFeature maybe NULL
    if (0 == strlen(feature_str))
    {
        INFO_LOG("Get guest feature failed.");
    }
    else
    {
		INFO_LOG("Guest feature is %s.", feature_str);
        write_to_xenstore(handle, GUSET_OS_FEATURE, trim(feature_str));
    }
    return;
}


void SetPvDriverVer(void *handle)
{
    FILE *pFileVer = NULL;
    char CurrentPath[SHELL_BUFFER] = {0};
    char c = '=';
    char *pstart;
    char tmp_version[20];
    int count = 0;

    pFileVer = fopen(DEFAULT_PATH, "r");
    if (NULL == pFileVer)
    {
        DEBUG_LOG("Open /etc/.uvp-monitor/version.ini failed, errno=%d.", errno);
        write_to_xenstore(handle, PVDRIVER_VERSION, DEFAULT_VERSION);
        return;
    }
    (void)fgets(CurrentPath, SHELL_BUFFER, pFileVer);
    fclose(pFileVer);

    pstart = strchr(CurrentPath, c);

    // change the PVDRIVER_VERSION in xenstore ---2010.11.19
    if (NULL != pstart)
    {
        pstart[strlen(pstart) - 1] = '\0';
        tmp_version[0] = '2';
        tmp_version[1] = '.';
        (void)strncpy_s(&tmp_version[2], sizeof(tmp_version) - 2, pstart + 1, sizeof(tmp_version) - 3);
        tmp_version[sizeof(tmp_version) - 1] = '\0';
        pstart = tmp_version;
        while('\0' != *pstart)
        {
            if('.' == *pstart)
            {
                count++;
                if(4 == count)
                {
                    *pstart = '\0';
                    break;
                }
            }
            pstart++;
        }
        pstart = tmp_version;
        write_to_xenstore(handle, PVDRIVER_VERSION, pstart);
    }
    else
    {
        write_to_xenstore(handle, PVDRIVER_VERSION, DEFAULT_VERSION);
    }
}

/*****************************************************************************
Function   : deal_hib_migrate_flag_file
Description:热迁移时向/etc/.uvp-monitor/hibernate_migrate_flag.ini写入标志信息。
Input       :标志信息值
Input       :
Return     : None
*****************************************************************************/
void deal_hib_migrate_flag_file(int hib_mig_flag)
{
    int iRet = 0;
    char pszCommand[MAX_COMMAND_LENGTH] = {0};
    char pszBuff[MAX_COMMAND_LENGTH] = {0};

    if (1 == hib_mig_flag) {
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH,
              "mkdir -p %s 2> /dev/null;echo %d > %s 2> /dev/null;", GUEST_CMD_FILE_PATH, hib_mig_flag, HIBERNATE_MIGRATE_PATH);
    } else {
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH,
              "rm -rf %s 2> /dev/null ", HIBERNATE_MIGRATE_PATH);
    }

    iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
    if (0 != iRet) {
        ERR_LOG("hibernate_migrate_start: write migrate_start flag fail. pszCommand=%s output=%s ret=%d.", pszCommand, pszBuff, iRet);
    } else {
        ERR_LOG("write migrate_start flag %d success.", hib_mig_flag);
    }

    return;
}

/*****************************************************************************
Function   : write_to_file
Description:热迁移后向文件/var/log/uvp_migrate/complete_migrate.config写入信息。
Input       :
Input       :
Return     : None
*****************************************************************************/
void write_to_file()
{

    FILE *fo;
    char *filename;
    char *str;

    filename = "/etc/.uvp-monitor/complete_migrate.config";
    str = "The restore has been completed";
    fo = fopen(filename, "w+");
    if (NULL == fo)
    {
        ERR_LOG("fopen failed, errno=%d.", errno);
        return;
    }
    fprintf(fo, "%s\n", str);
    fclose(fo);
}

/*****************************************************************************
Function   : write_unplugdisk_flag
 Description:
 Input      : handle -- xenstore file handle
 Output     : None
 Return     : None
*****************************************************************************/
void write_unplugdisk_flag(void* phandle)
{
    if (NULL == phandle)
    {
        return;
    }
    write_to_xenstore(phandle, UVP_UNPLUG_DISK, "uvptoken");

}

/*****************************************************************************
Function   : uvp_regwatch
Description:注册watch
Input       :phandle xenstore句柄
Input       : service_flag  true or false
Return     : None
*****************************************************************************/
void uvp_regwatch(void *phandle)
{
    int iIsHotplug = XEN_FAIL;

    if(NULL == phandle)
    {
        return;
    }
    (void)write_unplugdisk_flag(phandle);
    (void)regwatch(phandle, PVDRIVER_STATIC_INFO_PATH , "staticInfoToken");
    (void)regwatch(phandle, COMPLETE_RESTORE, "0");
    (void)regwatch(phandle, DRIVER_RESUME_FLAG, "uvptoken");
    /* 为了在迁移后触发一次性能获取 */
    (void)regwatch(phandle, MIGRATE_FLAG, "0");
    (void)regwatch(phandle, RELEASE_BOND, "0");
    (void)regwatch(phandle, REBOND_SRIOV, "0");
    (void)regwatch(phandle, EXINFO_FLAG_PATH, "exinfo_token");
    (void)regwatch(phandle, DISABLE_EXINFO_PATH, "exinfo_token");
    /* write cpu hotplug feature if cpu support hotplug */
    iIsHotplug = SetCpuHotplugFeature(phandle);
    if (iIsHotplug == XEN_SUCC)
    {
        /* if OS support cpu hotplug register a watch */
        cpuhotplug_regwatch(phandle);
    }
    (void)regwatch(phandle, STORAGE_SNAPSHOT_FLAG, "0");

    (void)regwatch(phandle, XS_HEART_BEAT_RATE, "0");
    (void)regwatch(phandle, UVP_UNPLUG_DISK , "uvptoken");
    (void)regwatch(phandle, OS_CMD_XS_PATH, "0");
}

/*****************************************************************************
Function   : uvp_unregwatch
Description:反注册 watch。
Input       :phandle xenstore句柄
Input       :
Return     : None
*****************************************************************************/
void uvp_unregwatch(void *phandle)
{
    int iIsHotplug = XEN_FAIL;

    if(NULL == phandle)
    {
        return;
    }

    (void)xs_unwatch(phandle, PVDRIVER_STATIC_INFO_PATH , "staticInfoToken");
    (void)xs_unwatch(phandle, COMPLETE_RESTORE, "0");
    (void)xs_unwatch(phandle, DRIVER_RESUME_FLAG, "uvptoken");
    /* 为了在迁移后触发一次性能获取 */
    (void)xs_unwatch(phandle, MIGRATE_FLAG, "0");
    (void)xs_unwatch(phandle, RELEASE_BOND, "0");
    (void)xs_unwatch(phandle, REBOND_SRIOV, "0");
    (void)xs_unwatch(phandle, EXINFO_FLAG_PATH, "exinfo_token");
    (void)xs_unwatch(phandle, DISABLE_EXINFO_PATH, "exinfo_token");
    /* write cpu hotplug feature if cpu support hotplug */
    iIsHotplug = SetCpuHotplugFeature(phandle);
    if (iIsHotplug == XEN_SUCC)
    {
        /* if OS support cpu hotplug register a watch */
        (void)xs_unwatch(phandle, CPU_HOTPLUG_SIGNAL , "");
    }
    (void)xs_unwatch(phandle, STORAGE_SNAPSHOT_FLAG, "0");

    (void)xs_unwatch(phandle, XS_HEART_BEAT_RATE, "0");
    (void)xs_unwatch(phandle, UVP_UNPLUG_DISK , "uvptoken");
}

/*****************************************************************************
Function   : write_feature_flag
Description:
Input      : phandle xenstore句柄
Input      : feature_flag true or false
Return     : None
*****************************************************************************/
static void write_feature_flag(void *phandle, char *feature_flag)
{
    if (NULL == phandle || NULL == feature_flag)
    {
        return;
    }
    write_to_xenstore(phandle, FEATURE_FLAG_WATCH_PATH, feature_flag);
}

/*****************************************************************************
Function   : write_service_flag
Description:向xenstore写入性能监控进程是否被kill掉的标志位，false表示进程已经被kill。
Input       :phandle xenstore句柄
Input       : service_flag  true or false
Return     : None
*****************************************************************************/
void write_service_flag(void *phandle, char *service_flag)
{
    if (NULL == phandle || NULL == service_flag)
    {
        return;
    }
    write_to_xenstore(phandle, SERVICE_FLAG_WATCH_PATH, service_flag);
}

/*****************************************************************************
 Function   : write_vmstate_flag
 Description:  note vm is running
 Input      : handle -- xenstore file handle
 Output     : None
 Return     : None
*****************************************************************************/
void write_vmstate_flag(void *phandle, char *vm_state)
{
    if (NULL == phandle || NULL == vm_state)
    {
        return;
    }
    write_to_xenstore(phandle, UVP_VM_STATE_PATH, vm_state);

}

/*****************************************************************************
Function   : do_unplugdisk
 Description:
 Input      : handle -- xenstore file handle
 Output     : None
 Return     : None
*****************************************************************************/
void *do_unplugdisk(void* handle)
{
    char  pszCommand[SHELL_BUFFER] = {0};
    char  chUnplugPath[SHELL_BUFFER] = {0};
    char  *pchUnplugDiskName = NULL;
    char pszBuff[SHELL_BUFFER] = {0};
    (void)memset(chUnplugPath, 0, SHELL_BUFFER);
    (void)snprintf(chUnplugPath, sizeof(chUnplugPath), "%s", UVP_UNPLUG_DISK);
    pchUnplugDiskName = read_from_xenstore(handle, chUnplugPath);
    if (NULL != pchUnplugDiskName && strstr(pchUnplugDiskName, "xvd") != NULL)
    {
        (void)memset(pszCommand, 0, SHELL_BUFFER);
        (void)memset(pszBuff, 0, SHELL_BUFFER);
        (void)snprintf(pszCommand, SHELL_BUFFER, "pvumount.sh %s", pchUnplugDiskName);
        (void)system(pszCommand);
        /*if (0 != iRet)
        {
            ERR_LOG("unplug disk: call uvpPopen pszCommand=%s Fail ret = %d \n", pszCommand, iRet);
        }*/
    }
    if (NULL != pchUnplugDiskName)
    {
        free(pchUnplugDiskName);
        pchUnplugDiskName = NULL;
    }
    return pchUnplugDiskName;
}

/*****************************************************************************
 Function   : SetScsiFeature
 Description:  note this pv support scsi function
 Input      : handle -- xenstore file handle
 Output     : None
 Return     : None
*****************************************************************************/
void SetScsiFeature(void *phandle)
{
    if (NULL == phandle)
    {
        return;
    }
    write_to_xenstore(phandle, SCSI_FEATURE_PATH, "1");
}

/*****************************************************************************
 Function   : write_pvops_flag
 Description:  note vm is pvops
 Input      : handle -- xenstore file handle
 Output     : None
 Return     : None
*****************************************************************************/
void write_pvops_flag(void *phandle, char *vm_state)
{
    if (NULL == phandle || NULL == vm_state)
    {
        return;
    }
    write_to_xenstore(phandle, KERNEL_PV_OPS, vm_state);

}

/*****************************************************************************
 Function   : update_netinfo_flag
 Description: if libvirt has netinfo,then g_netinfo_value = 1
 Input      : handle -- xenstore file handle
 Output     : None
 Return     : None
*****************************************************************************/
void set_netinfo_flag(void *handle)
{
	char *netinfoFlag = NULL;

    if (NULL == handle)
    {
        return;
    }

    g_netinfo_value = 0;

    netinfoFlag = read_from_xenstore(handle, NETINFO_FEATURE_PATH);
    if(NULL != netinfoFlag)
    {
        if(strcmp(netinfoFlag, "1") == 0)
        {
            g_netinfo_value = 1;
        }
        free(netinfoFlag);
        netinfoFlag = NULL;
    }
}

/*****************************************************************************
 Function   : do_wtach_functions
 Description:  DomU's extended-information watch function
 Input      : handle -- xenstore file handle
 Output     : None
 Return     : None
*****************************************************************************/
void do_watch_functions(void *handle)
{
    if(!g_disable_exinfo_value)
    {
    	if(1 == g_netinfo_value)
    		NetinfoNetworkctlmon( handle );
    	else
	    	networkctlmon( handle );
        (void)memoryworkctlmon( handle );
        (void)diskworkctlmon( handle );
        (void)hostnameworkctlmon( handle );
        if (ERROR == cpuworkctlmon( handle ))
        {
            write_to_xenstore(handle, CPU_DATA_PATH, "error");
        }
    }
    return;
}

void do_watch_functions_delay(void *handle)
{
    static unsigned int i = 0;

    (void)sleep(5);
    if( !g_disable_exinfo_value )
    {
      if(1 == g_netinfo_value)
      	  NetinfoNetworkctlmon(handle);
      else
      	  networkctlmon( handle );
      (void)memoryworkctlmon( handle );
      i++;
      if ( 6 == i)
      {
          (void)diskworkctlmon( handle );
          (void)hostnameworkctlmon( handle );
          if (ERROR == cpuworkctlmon( handle ))
          {
            write_to_xenstore(handle, CPU_DATA_PATH, "error");
          }
          i = 0;
      }
    }
    return;
}

/*****************************************************************************
Function   : watch_listen
Description: 监听设备的select事件
Input       :xsfd : 文件句柄
Output     : None
Return     : SUCC : 在5秒内读取到返回true
*****************************************************************************/
int watch_listen(int xsfd)
{
    fd_set   set;
    struct timeval tv = {.tv_sec = 5, .tv_usec = 0};

	/*lint -e530 */
    FD_ZERO(&set);
    FD_SET((unsigned int)xsfd, (fd_set *)&set);
	/*lint +e530 */

    /* select中最大值要加1 */
    if ((select((unsigned int)(xsfd + 1), &set, NULL, NULL, &tv) > 0 )
            && FD_ISSET((unsigned int)xsfd, &set))
    {
        return SUCC;
    }

    return ERROR;
}

/*****************************************************************************
Function   : condition
Description: 进入while循环的条件判断
Input       :None
Output     : None
Return     : SUCC : 处于一直监控watch的处理
*****************************************************************************/
int  condition(void)
{
    return SUCC;
}

/*****************************************************************************
Function   : IsSupportStorageSnapshotcheck
Description: 判断操作系统是否支持数据一致性，若支持将键值iomirror_snapshot_flag置为"0"
Input      : handle -- xenstore file handle
Output     : None
Return     : None
*****************************************************************************/
void IsSupportStorageSnapshotcheck (void *handle)
{
    struct utsname buf;
    (void)memset_s (&buf, sizeof(struct utsname), 0, sizeof(struct utsname));

    if (uname(&buf) < 0)
    {
        ERR_LOG("Get os release failed, errno=%d.", errno);
        return;
    }
    /* 判断是否操作系统是否为Novell SUSE Linux Enterprise Server 11 SP1或
       Novell SUSE Linux Enterprise Server 11 SP2 */
    if (strstr(buf.release, SUSE_11_SP1) ||
        strstr(buf.release, SUSE_11_SP2))
    {
        write_to_xenstore(handle, IOMIRROR_SNAPSHOT_FLAG, "0");
    }
    return;
}

/*****************************************************************************
 Function   : timing_monitor
 Description: watch domU's extended-information per 5 seconds
 Input      : handle -- xenstore file handle
 Output     : None
 Return     : None
*****************************************************************************/
void *timing_monitor(void *handle)
{
    char  *xenversionflag = NULL;
    char  UpgradeOldVerInfo[VER_SIZE]= {0};
    FILE  *UpgradeOldVerFile = NULL;

    (void)sleep(5);
    InitBond();
    (void)netbond();

    //第一次写xentore必须写成功
    xb_write_first_flag = 0;
    do_watch_functions(handle);

    /* 若支持一致性快照则写入vss 标志位 */
    IsSupportStorageSnapshotcheck(handle);

    /* 该标志位表示此pv driver版本支持pvscsi 功能*/
    SetScsiFeature(handle);
    /* 写入pvdriver标志 */
    write_service_flag(handle, "true");
    /* 正在运行状态标志位*/
    write_vmstate_flag(handle, "running");
    /* 写入 PV OPS 内核标志位 */
    if ( ! access("/proc/xen/version", R_OK) || ! access("/proc/xen_version", R_OK)
       || ! access("/dev/xen/xenbus", R_OK))
    {
        write_pvops_flag(handle, "1");
    }

    while(SUCC == condition())
    {
        do_watch_functions_delay(handle);
    }
    return NULL;
}

/*****************************************************************************
Function   : free_fs_mount_list
Description: 释放链表
Input      : mounts 链表
Output     : None
Return     : None
*****************************************************************************/
static void free_fs_mount_list(FsMountList *mounts)
{
    FsMount *mount = NULL, *temp = NULL;

    if (!mounts)
    {
        return;
    }

    /* 遍历链表 */
    /*lint -e506 */
    QTAILQ_FOREACH_SAFE(mount, mounts, next, temp)
    {
        /* 从链表中删除 */
        QTAILQ_REMOVE(mounts, mount, next);
        if (mount)
        {
            freePath(mount->dirname, mount->devtype, NULL);
            free(mount);
        }
    }
    /*lint +e506 */
}

/*****************************************************************************
Function   : build_fs_mount_list
Description: 查找mount点放入链表中
Input      : mounts : 链表
Output     : None
Return     : -1 查找mount点失败
Return     : 0  查找mount点成功,并将其放入链表中
*****************************************************************************/
static int build_fs_mount_list(FsMountList *mounts)
{
    struct mntent *ment = NULL;
    FsMount *mount = NULL;
    const char *mtab = "/proc/self/mounts";
    FILE *fp = NULL;
    char *devstate = NULL;
    char *psaveptr1 = NULL;
    int i = 0;

    fp = setmntent(mtab, "r");
    if (!fp)
    {
        ERR_LOG("Unable to read mtab, errno=%d.", errno);
        return ERROR;
    }

    while ((ment = getmntent(fp)))
    {
        devstate = strtok_r(ment->mnt_opts, ",", &psaveptr1);
        /* 将只读的文件系统过滤掉 */
        if ((NULL != devstate) && (0 == strcmp(devstate, "ro")))
        {
            continue;
        }

        /* 将文件系统类型不是 ext3、ext4、reiserfs、jfs、xfs其中之一的过滤掉 */
        for ( i = 0; i < DEV_TYPE_NUM; i++)
        {
            if (!strcmp(ment->mnt_type, devtype[i]))
            {
                mount = (FsMount *)malloc(sizeof(FsMount));
                if (NULL == mount)
                {
                    ERR_LOG("Malloc failed.");
                    (void)endmntent(fp);
                    return ERROR;
                }
                (void)memset_s(mount, sizeof(FsMount), 0, sizeof(FsMount));
                mount->dirname = strdup(ment->mnt_dir);
                mount->devtype = strdup(ment->mnt_type);
                QTAILQ_INSERT_TAIL(mounts, mount, next);
                break;
            }
        }
    }

    (void)endmntent(fp);
    return SUCC;
}

/*****************************************************************************
Function   : execute_fsfreeze_shell
Description: 执行用户脚本
Input      : state 表示状态冻结或解冻
Output     : None
Return     : 0  执行成功
Return     : -1 执行失败
*****************************************************************************/
static int execute_fsfreeze_shell(const char *state)
{
    char buf[SHELL_BUFFER] = {0};
    char cmd[SHELL_BUFFER] = {0};
    int ret = 0;

    INFO_LOG("Enter execute_fsfreeze_shell.");

    /* 判断用户脚本是否存在 */
    if (access(SHELL_PATH, F_OK) != 0)
    {
        INFO_LOG("No usr shell, errno=%d.", errno);
        return SUCC;
    }

    (void)snprintf_s(cmd, sizeof(cmd), sizeof(cmd), "sh %s %s >/dev/null 2>&1", SHELL_PATH, state);

    /* 执行用户脚本 */
    ret = uvpPopen(cmd, buf, SHELL_BUFFER);
    if (0 != ret)
    {
        (void)ERR_LOG("Execute fsfreeze shell failed, cmd=%s, output=%s ret = %d.", cmd, buf, ret);
        return ERROR;
    }

    INFO_LOG("Leave execute_fsfreeze_shell success.");
    return SUCC;
}

/*****************************************************************************
Function   : guest_cache_thaw
Description: 解冻文件系统及数据库
Input      : mounts 存放mount点数据结构的链表
Output     : None
Return     : 0  解冻成功
Return     : -1 解冻失败
*****************************************************************************/
static int guest_cache_thaw(FsMountList *mounts)
{
    FsMount *mount = NULL;
    int fd = 0, i = 0, ret = 0;

    QTAILQ_FOREACH(mount, mounts, next)
    {
        fd = open(mount->dirname, O_RDONLY);
        if (fd == -1)
        {
            ERR_LOG("Open file error, errno=%d", errno);
            continue;
        }

        /* 若解冻不成功则最多尝试10次，若10次仍不成功则返回失败 */
        do
        {
            ret = ioctl(fd, FITHAW);
            i++;
        }
        while (0 != ret && i < 10);

        close(fd);
        if (10 == i && 0 != ret)
        {
            ERR_LOG("Ioctl [FITHAW] error, mount name is %s, errno=%d", mount->dirname, errno);
            goto out;
        }
    }

out:
    if (execute_fsfreeze_shell(THAW) < 0)
    {
        ERR_LOG("guest_fsfreeze_shell thaw failed");
        return ERROR;
    }
    if (0 != ret)
    {
        return ERROR;
    }
    return SUCC;
}

/*****************************************************************************
Function   : guest_cache_freeze
Description: 刷数据库及文件系统缓存，冻结IO
Input      : mounts 存放mount点数据结构的链表
Output     : None
Return     : 0  冻结成功
Return     : -1 冻结失败
*****************************************************************************/
static int guest_cache_freeze(FsMountList *mounts)
{
    struct FsMount *mount = NULL;
    int fd = 0;

    /* 执行用户脚本(刷数据库缓存，冻结数据库IO)*/
    if (execute_fsfreeze_shell(FREEZE) < 0)
    {
        ERR_LOG("execute_fsfreeze_shell freeze failed.");
    }

    /* 遍历mount点,将符合条件的放进链表中 */
    if (build_fs_mount_list(mounts) < 0)
    {
        ERR_LOG("build_fs_mount_list failed.");
        goto error;
    }

     /* 遍历链表获取需冻结的文件系统 */
    QTAILQ_FOREACH(mount, mounts, next)
    {
        fd = open(mount->dirname, O_RDONLY);
        if (-1 == fd)
        {
            ERR_LOG("Open file error, dirname=%s, errno=%d.", mount->dirname, errno);
            goto error;
        }
        /* 冻结文件系统 */
        if (ioctl(fd, FIFREEZE) < 0)
        {
            close(fd);
            ERR_LOG("Ioctl [FIFREEZE] error, errno=%d.", errno);
            goto error;

        }
        close(fd);
    }

    return SUCC;

error:
    /* 若失败则解冻文件系统及数据库 */
    (void)guest_cache_thaw(mounts);
    return ERROR;
}

/*****************************************************************************
Function   : do_cache_thaw
Description: 解冻文件系统及数据库并根据返回值写xenstore键值
Input      : handle xenstore句柄
Input      : mounts 存放mount点数据结构的链表
Output     : None
Return     : 0  执行成功
Return     : -1 执行失败
*****************************************************************************/
int do_cache_thaw(void *handle, FsMountList *mounts)
{
    int ret = 0;
    (void)pthread_mutex_lock(&mutex);
    if(gfreezeflag == 1)
    {
        gfreezeflag = 0;
        if (guest_cache_thaw(mounts) < 0)
        {
            ret = -1;
            ERR_LOG("guest_cache_thaw failed!");
            write_to_xenstore(handle, IOMIRROR_SNAPSHOT_FLAG, "-1");
        }
        else
        {
            INFO_LOG("Guest cache thaw success!");
            write_to_xenstore(handle, IOMIRROR_SNAPSHOT_FLAG, "0");
            write_to_xenstore(handle, STORAGE_SNAPSHOT_FLAG, "0");
        }
        /* 解冻成功或失败都需要将链表中数据结构释放掉 */
        free_fs_mount_list(mounts);
    }
    (void)pthread_mutex_unlock(&mutex);
    return ret;
}

/*****************************************************************************
Function   : do_time_thaw
Description: 等待10s后向xenstore写键值触发解冻文件系统操作
Input      : arg
Output     : None
Return     : None
*****************************************************************************/
void *do_time_thaw(void *arg)
{
    void *handle = NULL;
    FsMountList *mounts = NULL;
    struct Freezearg *rev_arg;
    if(NULL == arg)
    {
        return NULL;
    }
    rev_arg = (struct Freezearg *)arg;

    mounts = rev_arg->mounts;
    handle = rev_arg->handle;
    (void)sleep(10);
    do_cache_thaw(handle, mounts);
    if(arg)
    {
        free(arg);
        arg = NULL;
    }
    return arg;
}

/*****************************************************************************
Function   : wait_for_thaw
Description: 创建线程，在新线程中主要实现等待10s后解冻
Input      : handle : xenstore的句柄
Input      : mounts : 存放mount点数据结构的链表
Output     : None
Return     : 0  创建成功
Return     : -1 创建失败
*****************************************************************************/
int wait_for_thaw(void *handle, FsMountList *mounts)
{
    int thread = 0;
    pthread_t thread_id = 0;
    int ret = 0;
    struct Freezearg *arg;
    pthread_attr_t attr;

    /* 申请的arg需要在do_time_thaw中释放 */
    arg = (struct Freezearg *)malloc(sizeof(struct Freezearg));
    if(arg == NULL)
    {
        ERR_LOG("Malloc failed.");
        return -1;
    }
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
    arg->handle = handle;
    arg->mounts = mounts;

    thread = pthread_create(&thread_id, &attr, do_time_thaw, (void *)arg);
    if (strcmp(strerror(thread), "Success") != 0)
    {
        ret = -1;
        DEBUG_LOG("Create do_time_thaw fail, thread=%d.", thread);
        free(arg);
    }

    pthread_attr_destroy (&attr);
    return ret;
}

/*****************************************************************************
Function   : write_heartbeat
Description: 写虚拟机心跳
Input       :handle : xenstore的句柄
Output     : None
Return     : None
*****************************************************************************/
void *write_heartbeat(void *handle)
{
    (void)pthread_mutex_lock(&heartbeat_mutex);
    //置标志位，表示写心跳线程已启动
    heartbeat_thread_exist_flag = 1;
    char heartbeat[HEART_BEAT_BUF_LEN] = {0};
    while( 0 < heartbeatrate)
    {
        if (9999 > heartbeatnum)
        {
            heartbeatnum = heartbeatnum + 1;
        }
        else
        {
            heartbeatnum = 1;
        }
        (void)memset_s(heartbeat, HEART_BEAT_BUF_LEN, 0, HEART_BEAT_BUF_LEN);
        (void)snprintf_s(heartbeat, HEART_BEAT_BUF_LEN, HEART_BEAT_BUF_LEN, "%d", heartbeatnum);
        if(xb_write_first_flag == 0)
        {
            write_to_xenstore(handle, XS_HEART_BEAT, heartbeat);
        }
        else
        {
            write_weak_to_xenstore(handle, XS_HEART_BEAT, heartbeat);
        }

        (void)sleep(heartbeatrate);
    }

    heartbeat_thread_exist_flag = 0;
    (void)pthread_mutex_unlock(&heartbeat_mutex);
    return NULL;
}

/*****************************************************************************
Function   : create_write_heartbeat_thread
Description: 创建线程，在线程中写虚拟机心跳
Input       :handle : xenstore的句柄
Output     : None
Return     : 0  创建成功
Return     : -1 创建失败
*****************************************************************************/
int create_write_heartbeat_thread(void *handle)
{
    char logbuf[LOG_BUF_LEN] = {0};
    int thread = 0;
    pthread_t thread_id = 0;
    int ret = 0;
    pthread_attr_t attr;

   (void)pthread_attr_init (&attr);
   (void)pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

    thread = pthread_create(&thread_id, &attr, write_heartbeat, handle);
    if (strcmp(strerror(thread), "Success") != 0)
    {
         ret = -1;
        (void)memset_s(logbuf, LOG_BUF_LEN, 0, LOG_BUF_LEN);
        (void)snprintf_s(logbuf, LOG_BUF_LEN, LOG_BUF_LEN, "Create write_heartbeat, errorno = %d !", thread);
        INFO_LOG("logbuf=%s", logbuf);
    }
    (void)pthread_attr_destroy (&attr);
    return ret;
}

void do_heartbeat_watch(void *handle)
{
    char  *heartbeat_rate = NULL;
    char  pszCommand_mig[100] = {0};
    
    (void)snprintf_s(pszCommand_mig, sizeof(pszCommand_mig), sizeof(pszCommand_mig), "%s", XS_HEART_BEAT_RATE);
    heartbeat_rate = read_from_xenstore(handle, pszCommand_mig);
    if (NULL != heartbeat_rate)
    {
        unsigned int rate = (unsigned int)atoi(heartbeat_rate);
        free(heartbeat_rate);
        heartbeat_rate = NULL;
    
        if (0 == rate)
        {
            /* 键值设置为零停止写心跳*/
            heartbeatrate = 0;
            while(heartbeat_thread_exist_flag)
            {
                (void)sleep(1);
            }
            heartbeatnum = 0;
            if(xb_write_first_flag == 0)
            {
                write_to_xenstore(handle, XS_HEART_BEAT, "0");
            }
            else
            {
                write_weak_to_xenstore(handle, XS_HEART_BEAT, "0");
            }
        }
        else if (0 == heartbeatrate)
        {
            heartbeatrate = rate;
            /* 键值从零变为大于零时创建写心跳线程*/
            (void)create_write_heartbeat_thread(handle);
         }
        else
        {
            /* 修改写心跳频率*/
            heartbeatrate = rate;
        }
    }
}

void do_complete_restore_watch(void *handle)
{
    int   ret = 0;
    char  pszCommand[SHELL_BUFFER] = {0};
    char  *migratestate = NULL;
    char  hib_migrate[SHELL_BUFFER] = {0};
    char  *hib_migrate_buffer = "cat /etc/.uvp-monitor/hibernate_migrate_flag.ini";

    /*set ipv6 info value*/
    set_netinfo_flag(handle);
    /* 若支持一致性快照则写入标志位 */
    IsSupportStorageSnapshotcheck(handle);
    (void)memset_s(pszCommand, SHELL_BUFFER, 0, SHELL_BUFFER);
    (void)snprintf_s(pszCommand, sizeof(pszCommand), sizeof(pszCommand), "%s", COMPLETE_RESTORE);
    migratestate = read_from_xenstore(handle, pszCommand);
    if((NULL != migratestate) && \
            ((0 == strcmp(migratestate, "1")) || (0 == strcmp(migratestate, "2"))))
    {
        if ( ! access("/proc/xen/version", R_OK) || ! access("/proc/xen_version", R_OK)
           || ! access("/dev/xen/xenbus", R_OK))
        {
            SetScsiFeature(handle);
            write_pvops_flag(handle, "1");
            write_service_flag(handle, "true");
            write_vmstate_flag(handle, "running");
            write_feature_flag(handle, "1");

            INFO_LOG("modify_swappiness_after_blkfront.sh restore in PVOPS GuestOS");
            (void)system("sh /etc/.uvp-monitor/modify_swappiness_after_blkfront.sh restore 2>/dev/null");
        }
        write_vrm_flag(handle);
        INFO_LOG("Complate restore, send ndp");
        (void)system("sh /etc/init.d/xenvnet-arp 2>/dev/null");
        write_to_file();
        (void)write_to_xenstore(handle, CMD_RESULT_XS_PATH, chret);
        write_to_xenstore(handle, XS_NOT_USE_PV_UPGRADE, "true");
        INFO_LOG("Do not provide UVP Tools upgrade ability.");

        if(strlen(feature_str) > 0)
        {
            write_to_xenstore(handle, GUSET_OS_FEATURE, feature_str);
        }

        if(0 == access("/etc/.uvp-monitor/hibernate_migrate_flag.ini", R_OK))
        {
            ret = uvpPopen(hib_migrate_buffer, hib_migrate, SHELL_BUFFER);
            if((0 == ret) && (! strcmp(trim(hib_migrate), "1")))
            {
                hibernate_migrate_flag = 1;
                (void)write_to_xenstore(handle, COMPLETE_RESTORE, "0");
            }
            else
            {
                INFO_LOG("Failed to call uvpPopen hibernate_migrate_flag, ret=%d.", ret);
            }
        }

        if(!hibernate_migrate_flag)
        {
            (void)system("hwclock --hctosys 2>/dev/null");
        }
        hibernate_migrate_flag = 0;
        (void)deal_hib_migrate_flag_file(hibernate_migrate_flag);

        /*if Linux OS is VSA, exec this shell after migrate*/
        if(( ! access(PYTHON_PATH, R_OK)) && (0 == strcmp(migratestate, "2")))
        {
            (void)system(EXEC_PYTHON_PATH);
        }
    }
    
    if((NULL != migratestate) && ((0 == strcmp(migratestate, "3")) ))
    {
        if ( ! access("/proc/xen/version", R_OK) || ! access("/proc/xen_version", R_OK)
           || ! access("/dev/xen/xenbus", R_OK) )
        {
            write_service_flag(handle, "true");
        }
    }
    
    if(NULL != migratestate)
    {
        free(migratestate);
    }
}

void do_cpu_hotplug_watch(void *handle)
{
    char  pszCommand[SHELL_BUFFER] = {0};
    char  *cpustate = NULL;
    char  *cpuhotplugstate = NULL;

    (void)memset_s(pszCommand, SHELL_BUFFER, 0, SHELL_BUFFER);
    (void)snprintf_s(pszCommand, sizeof(pszCommand), sizeof(pszCommand), "%s", CPU_HOTPLUG_STATE);
    /*是否正在热插的标志*/
    cpustate = read_from_xenstore(handle, pszCommand);
    if((NULL != cpustate) && (0 == strcmp(cpustate, "1")))
    {
        (void)memset_s(pszCommand, SHELL_BUFFER, 0, SHELL_BUFFER);
        (void)snprintf_s(pszCommand, sizeof(pszCommand), sizeof(pszCommand), "%s", CPU_HOTPLUG_ONLINE);
        /*自动online状态判断，键值状态为1时开启cpu自动online功能，其他状态为不开启自动online功能*/
        cpuhotplugstate = read_from_xenstore(handle, pszCommand);
        if((NULL != cpuhotplugstate) && (0 == strcmp(cpuhotplugstate, "1")))
        {
            (void)DoCpuHotplug(handle);
        }
        else
        {
            (void)write_to_xenstore(handle, CPU_HOTPLUG_STATE, "0");
            (void)write_to_xenstore(handle, CPU_HOTPLUG_SIGNAL, "0");
        }
    
        if( NULL != cpuhotplugstate)
        {
            free(cpuhotplugstate);
        }
    }
    if( NULL != cpustate )
    {
        free(cpustate);
    }

}

void do_exinfo_flag_watch(void *handle)
{
    char  pszCommand_mig[100] = {0};
    char  *exinfo_flag = NULL;
    
    (void)memset_s(pszCommand_mig, 100, 0, 100);
    (void)snprintf_s(pszCommand_mig, sizeof(pszCommand_mig), sizeof(pszCommand_mig), "%s", EXINFO_FLAG_PATH);
    exinfo_flag = read_from_xenstore(handle, pszCommand_mig);
    if ((NULL != exinfo_flag) && (exinfo_flag[0] >= '0' && exinfo_flag[0] <= '9'))
    {
        g_exinfo_flag_value = strtol(exinfo_flag, NULL, 10);
    }
    if(NULL != exinfo_flag)
    {
        free(exinfo_flag);
        exinfo_flag = NULL;
    }
}

void do_driver_resume_watch(void *handle)
{
    char  pszCommand[SHELL_BUFFER] = {0};
    char  *driver_resume = NULL;
    
    (void)memset_s(pszCommand, SHELL_BUFFER, 0, SHELL_BUFFER);
    (void)snprintf_s(pszCommand, sizeof(pszCommand), sizeof(pszCommand), "%s", DRIVER_RESUME_FLAG);
    driver_resume = read_from_xenstore(handle, pszCommand);
    if((NULL != driver_resume) && (0 == strcmp(driver_resume, "1")))
    {
        INFO_LOG("Driver resume, send ndp.");
        (void)system("sh /etc/init.d/xenvnet-arp 2>/dev/null");
    }
    if(NULL != driver_resume)
    {
        free(driver_resume);
    }
}

static int uvpexecl(const char* pszCmd)
{
    int nRet = 0;
    int stat = 0;
    pid_t cpid = -1;

    /*入参检查*/
    if((pszCmd == NULL) || (0 == strlen(pszCmd)))
    {
        return ERROR_PARAMETER;
    }
    
    cpid = fork();

    if (0 > cpid)
    {
        ERR_LOG("Failed to create subprocess, errno=%d.", errno);
        return ERROR_FORK;
    }
    else if (0 == cpid)
    {
        /* 子进程 */
        (void) execl("/bin/sh", "sh", "-c", pszCmd, NULL);
        _exit(127);
    }
    else
    {
        /* 父进程 */
        while (0 > waitpid(cpid, &stat, 0))
        {
            if (EINTR != errno)
            {
                return ERROR_WAITPID;
            }
        }
    }

    /* 调用的脚本正常结束 处理返回值0-127 */
    if (WIFEXITED(stat))
    {
        nRet = WEXITSTATUS(stat);
    }

    return nRet;
}

void *do_guestcmd_watch(void *handle)
{
    char *pchCmdType=NULL;
    char *pchFileName=NULL;
    char *pchPara=NULL;
    char *guest_cmd = NULL;
    char chCmdStr[BUFFER_SIZE]={0};
    char pszCommand[SHELL_BUFFER] = {0};
    int  iRet = 0;   
    
    guest_cmd = read_from_xenstore(handle, OS_CMD_XS_PATH);
    /* 键值不为空时执行 */
    if( NULL == guest_cmd )
    {
        ERR_LOG("read_from_xenstore error, guest_cmd is NULL.");
        return NULL;
    }
    if( !strcmp(guest_cmd, " "))
    {
        free(guest_cmd);
        guest_cmd = NULL;
        return NULL;
    }  
    /* 删除信息*/
    write_to_xenstore(handle, OS_CMD_XS_PATH, " ");   
    (void)strncpy_s(chCmdStr, BUFFER_SIZE, guest_cmd, BUFFER_SIZE);
    
    iRet = CheckArg(chCmdStr, &pchCmdType, &pchFileName, &pchPara);
    (void)memset_s(chret, CMD_RESULT_BUF_LEN, 0, CMD_RESULT_BUF_LEN);
    (void)snprintf_s(chret, CMD_RESULT_BUF_LEN, CMD_RESULT_BUF_LEN, "%d", iRet);
    write_to_xenstore(handle, CMD_RESULT_XS_PATH, chret);
    if(ARG_CHECK_OK == iRet)
    {
        (void)snprintf_s(pszCommand, BUFFER_SIZE, BUFFER_SIZE, "%s %s/%s %s", 
                        pchCmdType, GUEST_CMD_FILE_PATH, pchFileName, pchPara);
        ERR_LOG("chCmdStr is %s, pszCommand is %s.", chCmdStr, pszCommand);
        iRet = uvpexecl(pszCommand);
        if (0 != iRet)
        {
            ERR_LOG("Call uvpexecl pszCommand failed ret = %d.", iRet);
            if (1 == iRet)
            {
                iRet = UNEXPECTED_ERROR;
                ERR_LOG("Change error code 1 to 10.");
            }
        }
        (void)memset_s(chret, CMD_RESULT_BUF_LEN, 0, CMD_RESULT_BUF_LEN);
        (void)snprintf_s(chret, BUFFER_SIZE, BUFFER_SIZE, "%d", iRet);
        write_to_xenstore(handle, CMD_RESULT_XS_PATH, chret);
    }
    
    if(NULL != guest_cmd)
    {
        free(guest_cmd);
        guest_cmd = NULL;
    }
    return NULL;
}

int CheckArg(char* chCmdStr, char** pchCmdType, char** pchFileName, 
            char** pchPara)
{
    char *pCmd = NULL;
    char *outer_ptr = NULL;
    char *buf = chCmdStr;
    char *pchCheck = NULL;
    char pszCommand[BUFFER_SIZE] = {0};
    char pszBuff[BUFFER_SIZE] = {0};
    char FullFileName[BUFFER_SIZE] = {0};
    int  iRet = 0;
    int  num = 1;

    while((pCmd = strtok_s(buf, ">", &outer_ptr)) != NULL)
    {
        if(num == 1)
            *pchCmdType = pCmd + 1;
        else if(num == 2)
            *pchFileName = pCmd + 1;
        else if(num == 3)
            *pchPara = pCmd + 1;
        else
            pchCheck = pCmd + 1;
        num++;
        buf = NULL;
    }
    
    if (NULL == *pchCmdType || NULL == *pchFileName 
        || NULL == *pchPara || NULL == pchCheck)
    {
        ERR_LOG("pchCmdType=%p, pchFileName=%p, pchPara=%p, pchCheck=%p.", \
            *pchCmdType, *pchFileName, *pchPara, pchCheck);
        return ARG_ERROR;
    }

    if(strstr(*pchFileName, "<") || strstr(*pchFileName, ">") || strstr(*pchFileName, "\\") 
        || strstr(*pchFileName, "/") || strstr(*pchFileName, "..") || strstr(*pchFileName, "&") 
        || strstr(*pchFileName, "|") || strstr(*pchFileName, ";") || strstr(*pchFileName, "$") 
        || strstr(*pchFileName, " ") || strstr(*pchFileName, "`") || strstr(*pchFileName, "\n"))
    {
        ERR_LOG("Check the file name error, pchFileName=%s.", *pchFileName);
        return ARG_ERROR;
    }
    if(strstr(*pchPara, "<") ||strstr(*pchPara, ">") || strstr(*pchPara, "\\") || strstr(*pchPara, "/") 
        || strstr(*pchPara, "&") || strstr(*pchPara, "|") || strstr(*pchPara, ";") || strstr(*pchPara, "$") 
        || strstr(*pchPara, "?") || strstr(*pchPara, ",") || strstr(*pchPara, "*") || strstr(*pchPara, "{") 
        || strstr(*pchPara, "}") || strstr(*pchPara, "`") || strstr(*pchPara, "\n"))
    {
        ERR_LOG("Check the parameters error, pchPara=%s.", *pchPara);
        return ARG_ERROR;
    }

    if(!strcmp(*pchCmdType, "sh"))
    {
        if(!strstr(*pchFileName, ".sh"))
        {
            ERR_LOG("The cmd type sh and filename is not match, pchFileName=%s.", *pchFileName);
            return ARG_ERROR;
        }
    }
    else if (!strcmp(*pchCmdType, "python"))
    {
        if(!strstr(*pchFileName, ".py"))
        {
            ERR_LOG("The cmd type python and filename is not match, pchFileName=%s.", *pchFileName);
            return ARG_ERROR;
        }
    }
    else if (!strcmp(*pchCmdType, "perl"))
    {
        if(!strstr(*pchFileName, ".pl"))
        {
            ERR_LOG("The cmd type perl and filename is not match, pchFileName=%s.", *pchFileName);
            return ARG_ERROR;
        }
    }
    else
    {
        ERR_LOG("The cmd type is Invalid, pchCmdType=%s.", *pchCmdType);
        return ARG_ERROR;
    }

    (void)snprintf_s(FullFileName, BUFFER_SIZE, BUFFER_SIZE, "%s/%s", GUEST_CMD_FILE_PATH, *pchFileName);
    if(access(FullFileName, F_OK))
    {
        ERR_LOG("The file not exist, FullFileName=%s.", FullFileName);
        return FILENAME_ERROR;
    }
    
    (void)snprintf_s(pszCommand, BUFFER_SIZE, BUFFER_SIZE, "sha256sum %s", FullFileName);
    iRet = uvpPopen(pszCommand, pszBuff, BUFFER_SIZE);
    if (0 != iRet)
    {
        ERR_LOG("Call uvpPopen pszCommand failed, pszCommand=%s, pszBuff=%s, ret=%d.", pszCommand, pszBuff, iRet);
        return CHECKSUM_ERROR;
    }

    pCmd = strtok_s(pszBuff, " ", &outer_ptr);
    if(NULL == pCmd)
    {
        ERR_LOG("strtok_s error, pszBuff=%s.", pszBuff);
        return CHECKSUM_ERROR;
    }
    if(strcmp(pchCheck, pCmd))
    {
        ERR_LOG("Integrity check error, pchCheck=%s, pCmd=%s.", pchCheck, pCmd);
        return CHECKSUM_ERROR;
    }
    return ARG_CHECK_OK;
}

/*****************************************************************************
Function   : do_watch_proc
Description: 接收watch事件
Input       :handle : xenstore的句柄
Output     : None
Return     : None
*****************************************************************************/
void do_watch_proc(void *handle)
{
    char  **vec;
    int   xsfd = -1;
    char  pszCommand[SHELL_BUFFER] = {0};
    char  pszCommand_mig[100] = {0};
    char  *migrate_start = NULL;
    char  *release_bond = NULL;
    char  *rebond = NULL;
    char  *disableInfo_flag = NULL;
    char  *healthstate = NULL;
    char  *storage_snapshot_flag = NULL;
    FsMountList mounts;
    QTAILQ_INIT(&mounts);

    pthread_t th_watch = 0;
    int iThread = -1;
    pthread_attr_t attr;

    xsfd = getxsfileno(handle);
    if ((int) - 1 == xsfd)
    {
        return;
    }

    while (SUCC == condition())
    {
        if (SUCC == watch_listen(xsfd))
        {
            vec = readWatch(handle);
            if (!vec)
            {
                continue;
            }

            if (strstr(*vec, PVDRIVER_STATIC_INFO_PATH))
            {
                SetPvDriverVer(handle);
                //将原动态查询的虚拟机静态信息，只在开机时写一次
                (void)SetCpuHotplugFeature(handle);
                SetScsiFeature(handle);
            }
            else if (NULL != strstr(*vec, CPU_HOTPLUG_SIGNAL))
            {
                do_cpu_hotplug_watch(handle);
            }
            else if ( NULL != strstr(*vec, COMPLETE_RESTORE))
            {
                do_complete_restore_watch(handle);
            }
            else if ( NULL != strstr(*vec, DRIVER_RESUME_FLAG))
            {
                do_driver_resume_watch(handle);
            }
            else if ( NULL != strstr(*vec, MIGRATE_FLAG))
            {
                (void)memset_s(pszCommand_mig, 100, 0, 100);
                (void)snprintf_s(pszCommand_mig, sizeof(pszCommand_mig), sizeof(pszCommand_mig), "%s", MIGRATE_FLAG);
                migrate_start = read_from_xenstore(handle, pszCommand_mig);
                if((NULL != migrate_start) && (0 == strcmp(migrate_start, "1")))
                {
                    hibernate_migrate_flag = 1;

                     /* 该变量会在monitor服务重启后失效，因此写到文件中  */
                    (void)deal_hib_migrate_flag_file(hibernate_migrate_flag);
                }
                if(NULL != migrate_start)
                {
                    free(migrate_start);
                }
            }
            /*before hot-migrate*/
            else if ( NULL != strstr(*vec, RELEASE_BOND))
            {
                //因为注册wacth  时会进一次，所以为了防止重启 monitor 时执行动作，每次执行完之后需要
                //更新下 xenstore 的键值，防止重启时进入此动作。
                (void)memset_s(pszCommand_mig, 100, 0, 100);
                (void)snprintf_s(pszCommand_mig, sizeof(pszCommand_mig), sizeof(pszCommand_mig), "%s", RELEASE_BOND);
                release_bond = read_from_xenstore(handle, pszCommand_mig);
                if((NULL != release_bond) && (0 == strcmp(release_bond, "1")))
                {
                    (void)releasenetbond(handle);
                }
                if(NULL != release_bond)
                {
                    free(release_bond);
                }
            }
            /*after hot-migrate*/
            else if ( NULL != strstr(*vec, REBOND_SRIOV))
            {
                (void)memset_s(pszCommand_mig, 100, 0, 100);
                (void)snprintf_s(pszCommand_mig, sizeof(pszCommand_mig), sizeof(pszCommand_mig), "%s", REBOND_SRIOV);
                rebond = read_from_xenstore(handle, pszCommand_mig);
                if((NULL != rebond) && (0 == strcmp(rebond, "1")))
                {
                    (void)rebondnet(handle);
                }
                if(NULL != rebond)
                {
                    free(rebond);
                }
            }
            else if ( NULL != strstr(*vec, EXINFO_FLAG_PATH))
            {
                do_exinfo_flag_watch(handle);
            }
            else if (NULL != strstr(*vec, DISABLE_EXINFO_PATH))
            {
                (void)memset_s(pszCommand_mig, 100, 0, 100);
                (void)snprintf_s(pszCommand_mig, sizeof(pszCommand_mig), sizeof(pszCommand_mig), "%s", DISABLE_EXINFO_PATH);
                disableInfo_flag = read_from_xenstore(handle, pszCommand_mig);
                if ((NULL != disableInfo_flag) && (disableInfo_flag[0] == '0' || disableInfo_flag[0] == '1'))
                {
                    g_disable_exinfo_value = strtol(disableInfo_flag, NULL, 10);
                }
                if (NULL != disableInfo_flag)
                {
                    free(disableInfo_flag);
                    disableInfo_flag = NULL;
                }
            }
            else if (NULL != strstr(*vec, STORAGE_SNAPSHOT_FLAG))
            {
                (void)snprintf_s(pszCommand_mig, sizeof(pszCommand_mig), sizeof(pszCommand_mig), "%s", STORAGE_SNAPSHOT_FLAG);
                storage_snapshot_flag = read_from_xenstore(handle, pszCommand_mig);
                /* 键值状态为1时刷数据库及文件系统缓存并冻结数据库及文件系统 */
                if ((NULL != storage_snapshot_flag) && (0 == strcmp(storage_snapshot_flag, "1")))
                {
                    write_to_xenstore(handle, IOMIRROR_SNAPSHOT_FLAG, "1");
                    if (guest_cache_freeze(&mounts) < 0)
                    {
                        ERR_LOG("guest_cache_freeze failed!");
                        write_to_xenstore(handle, IOMIRROR_SNAPSHOT_FLAG, "-1");
                        free_fs_mount_list(&mounts);
                    }
                    else
                    {
                        INFO_LOG("guest_cache_freeze success!");
                        write_to_xenstore(handle, STORAGE_SNAPSHOT_FLAG, "2");
                        /* gfreezeflag=1 表示已经冻结 */
                        gfreezeflag = 1;
                        /* 创建线程等待10s后自发解冻 */
                        wait_for_thaw(handle, &mounts);
                    }
                }
                /* 键值状态为3时解冻文件系统 */
                if ((NULL != storage_snapshot_flag) && (0 == strcmp(storage_snapshot_flag, "3")))
                {
                    /* 当处于冻结时才去解冻 */
                    do_cache_thaw(handle, &mounts);
                }
                if ( NULL != storage_snapshot_flag )
                {
                    free(storage_snapshot_flag);
                }
            }
            else if (NULL != strstr(*vec, XS_HEART_BEAT_RATE))
            {
                do_heartbeat_watch(handle);
            }
            else if (NULL != strstr(*vec, UVP_UNPLUG_DISK))
            {
                 pthread_attr_init (&attr);
                 pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
                 iThread = pthread_create(&th_watch, &attr, do_unplugdisk, (void*)handle);
                 if (strcmp(strerror(iThread), "Success") != 0)
                 {
                    pthread_attr_destroy (&attr);
                    ERR_LOG("Unplug disk thread create failed!");
                 }
                 pthread_attr_destroy (&attr);
            }
            else if ( NULL != strstr(*vec, OS_CMD_XS_PATH))
            {
                (void)pthread_attr_init (&attr);
                (void)pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
                iThread = pthread_create(&th_watch, &attr, do_guestcmd_watch, (void*)handle);
                if (strcmp(strerror(iThread), "Success") != 0)
                {
                   (void)pthread_attr_destroy (&attr);
                   ERR_LOG("Create guestcmd thread failed!");
                }
                (void)pthread_attr_destroy (&attr);
            }
            free(vec);
            vec = NULL;
        }
    }
    close(xsfd);
    return ;
}

/*****************************************************************************
Function   : ReleaseEnvironment
Description: release system environment
Input       :None
Output     : None
Return     : None
*****************************************************************************/
void ReleaseEnvironment(void  *handle)
{
    if (NULL != handle)
    {
        write_service_flag(handle, "false");
        closexenstore(handle);
        //lint -save -e438
        handle = NULL;
    }//lint -restore
}

/*****************************************************************************
Function   : do_deamon_model
Description: 启动监控功能模块，监控watch事件
Input       :None
Output     : None
Return     : None
*****************************************************************************/
void do_deamon_model(void *handle)
{
    /* 注册性能监控的watch */
    uvp_regwatch(handle);
    /* 写入pvdriver标志 */
    //write_service_flag(handle, "true");
    /* 获取性能监控数据 */
    do_watch_proc(handle);
    /* 释放xenstore句柄 */
    ReleaseEnvironment(handle);
}

/*****************************************************************************
Function   : do_monitoring
Description:性能监控主程序
Input       :handle xenstore句柄
Return     : None
*****************************************************************************/
void *do_monitoring(void *handle)
{
    if (NULL == handle)
    {
        return NULL;
    }

    /* 监控模块处理 */
    do_deamon_model(handle);
    return NULL;
}

/*****************************************************************************
Function   : exe_command
Description: 使用system执行命令
Input       :命令内容或路径
Output     : None
Return     : int
*****************************************************************************/
#define UPGRADE_SUCCESS 0
#define MONITOR_UPGRADE_OK 3
#define UPGRADE_ROLLBACK 5

int exe_command(char *path)
{
    char logBuf[BUFFER_SIZE] = {0};
    int flag,exit_value;
    exit_value = system(path);
    flag = WEXITSTATUS(exit_value);
    //flag = 1,3,5的时候不需要提示重启
    if ( (UPGRADE_SUCCESS != flag) && (MONITOR_UPGRADE_OK != flag) && (UPGRADE_ROLLBACK != flag) )
    {
        (void)snprintf_s(logBuf, BUFFER_SIZE - 1, BUFFER_SIZE - 1, "exe error-command:%s", path);
        INFO_LOG("[Monitor-Upgrade]: logBuf %s",logBuf);
        return 1;
    }

    if(UPGRADE_ROLLBACK == flag)
    {
        return 5;
    }

    if(MONITOR_UPGRADE_OK == flag)
        {
                return 3;
        }

    return 0;
}

/*****************************************************************************
Function   : init_daemon
Description:子进程和父进程的交互，用于监控进程被kill时，向xenstore写入标志位
Return     : None
*****************************************************************************/
void init_daemon()
{
    int iRet;
    int iStatus, iThread, tThread, mThread, sThread;
    pid_t cpid, wpid;
    pid_t pipes[2];
    pthread_t pthread_id;
    pthread_t tthread_id;
    pthread_t mthread_id;
    pthread_attr_t attr;
    char  *timeFlag = NULL;
    pthread_t sthread_id;
    char buf;
    void *handle;
    g_disable_exinfo_value = 0;
    g_exinfo_flag_value = 0;
    g_netinfo_value = 0;
    g_monitor_restart_value = 0;

again:
    /*建立管道*/
    iRet = pipe(pipes);
    if(iRet < 0){
        ERR_LOG("Pipe failed, errno=%d, just exit.", errno);
        exit(1);
    }
    cpid = fork();
    DEBUG_LOG("Mlock uvp-monitor memory.");
    iRet = mlockall(MCL_CURRENT|MCL_FUTURE);
    if(iRet != 0){
		 ERR_LOG("Mlock uvp-monitor memory failed, errno=%d.", errno);
    }
    /* fork失败，退出 */
    if(cpid < 0)
    {
        DEBUG_LOG("Process fork cpid fail, cpid=%d.", cpid);
        exit(1);
    }
    /*做子进程的事*/
    else if( 0 == cpid )
    {
        /*write monitor-service-flag "false" after stop uvp-monitor service*/
        struct sigaction sig;
        memset_s(&sig, sizeof(sig), 0, sizeof(sig));
        sig.sa_handler= SIG_IGN;
        sig.sa_flags = SA_RESTART;
        sigaction(SIGTERM, &sig, NULL);
        /*end */

        handle = openxenstore();
        if (NULL == handle)
        {
            DEBUG_LOG("Open xenstore fail!");
            return;
        }
        /*set ipv6 info value*/
        set_netinfo_flag(handle);
        set_guest_feature(handle);

        /* is vrm */
        write_vrm_flag(handle);

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

        write_to_xenstore(handle, XS_NOT_USE_PV_UPGRADE, "true");
        INFO_LOG("Do not provide UVP Tools upgrade ability.");

        //创建do_monitoring的线程
        iThread = pthread_create(&pthread_id, &attr, do_monitoring, (void *)handle);
        if (strcmp(strerror(iThread), "Success") != 0)
        {
            DEBUG_LOG("Create do_monitoring fail, iThread=%d.", iThread);
            pthread_attr_destroy (&attr);
            exit(1);
        }

        //新开线程用于定时5秒向xenstore写系统扩展信息
        mThread = pthread_create(&mthread_id, &attr, timing_monitor, (void *)handle);
        if (strcmp(strerror(mThread), "Success") != 0)
        {
            DEBUG_LOG("Create timing_monitor fail, try again. mThread=%d.", mThread);
            mThread = pthread_create(&mthread_id, &attr, timing_monitor, (void *)handle);
            if (strcmp(strerror(mThread), "Success") != 0)
            {
                DEBUG_LOG("Create timing_monitor fail, process exit. mThread=%d.", mThread);
                pthread_attr_destroy (&attr);
                exit(1);
            }
        }
        pthread_attr_destroy (&attr);
        /*关闭管道写*/
        close(pipes[1]);
        iRet = read(pipes[0], &buf, 1);

        if(0 == iRet)
        {
            ERR_LOG("The parent has dead, the uvp-monitor exits.");
            ReleaseEnvironment(handle);
            (void)kill(getpid(), SIGKILL);
        }
        if(g_monitor_restart_value == 1)
        {
            ERR_LOG("Upgrade pv free son process handle.");
            ReleaseEnvironment(handle);
        }
    }
    /*做父进程的事*/
    else
    {
        handle = openxenstore();
        if (NULL == handle)
        {
            ERR_LOG("Open xenstore fd failed, just exit.");
            exit(1);
        }
            /*cpid为子进程PID*/
wait_again:
        wpid = waitpid(cpid, &iStatus, 0);                          //FIXME
        if (-1 == wpid)
        {
            if(errno == EINTR)                                      //FIXME
            {
                INFO_LOG("waitpid when caught a signal.");
                goto wait_again;
            }
            ERR_LOG("waitpid failed, errno=%d.", errno);
            ReleaseEnvironment(handle);
            exit(1);
        }

        closexenstore(handle);
        handle = NULL;
        close(pipes[1]);
        sleep(1);
        if (WIFEXITED(iStatus))
            ERR_LOG("The uvp-monitor %d exits with status %d.", wpid, WEXITSTATUS(iStatus));
        else
        {
            ERR_LOG("The uvp-monitor %d exits with unknown reason, iStatus is %d.", wpid, iStatus);
        }
        goto again;
    }

}

/*****************************************************************************
Function   : start_service
Description: start the service daemon
1、初始化监控线程
2、初始化运行环境
3、进入监控功能处理
Input       :None
Output     : None
Return     : None
*****************************************************************************/
void start_service(void)
{
    /* 初始化监控线程 */
    init_daemon();
}

