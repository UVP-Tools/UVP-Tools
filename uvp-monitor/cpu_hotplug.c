/*
 * Enhances the cpu hot-plug
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


#include "include/xenstore_common.h"
#include "include/public_common.h"


/* Linux */
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <limits.h>
#include <errno.h>
#include "securec.h"

/* 执行脚本命令 超时时间 */
#define POPEN_TIMEOUT 	30

/* 超时错误 */
#define ERROR_TIMEOUT 	-2
/* 创建pipe失败 */
#define ERROR_PIPE 		-6
/* fork失败 */
#define ERROR_FORK 		-7
/* fdopen失败 */
#define ERROR_FDOPEN 	-8
/* select失败 */
#define ERROR_SELECT 	-9
/* waitpid失败 */
#define ERROR_WAITPID 	-10
/* waitpid失败 */
#define ERROR_PARAMETER -11

/*CPU 热插支持的最大cpu 个数*/
#define CPU_NR_MAX 64

/*******************************************************************************
  Function        : uvpPopen
  Description     : 通过系统调用执行shell脚本，并返回结果。
  @param          : const char* pszCmd   命令
  @param          : char* pszBuffer  结果
  @param          : int size  缓存大小
  Output          :
  Return          : 0-127 脚本返回值
                    <0 脚本执行失败
  Calls           :
  Called by       :

  History         :
  1 Date          : 2011-7-14
    Author        : z00179060
    Modification  : 添加函数
*******************************************************************************/
int uvpPopen(const char* pszCmd, char* pszBuffer, int size)
{
    int nRet = 0;
    FILE *fp = NULL;
    int pfd[2] = {0};
    pid_t pid = 0;
    int stat = 0;
    fd_set rfds;
    struct timeval tv;

    /* select返回值 */
    int retval;

    /*入参检查*/
    if((pszCmd == NULL)
        ||(pszBuffer == NULL)
        ||(0 == strlen(pszCmd)))
    {
        return ERROR_PARAMETER;
    }

    (void)memset_s(pszBuffer, size, 0, size);

    /* 创建pipe失败 */
    if (0 > pipe(pfd))
    {
        return ERROR_PIPE;
    }

    if (0 > (pid = fork()))
    {
        return ERROR_FORK;
    }
    else if (0 == pid)
    {
        /* 子进程 */
        (void)close(pfd[0]);
        if (pfd[1] != STDOUT_FILENO)
        {
            (void) dup2(pfd[1], STDOUT_FILENO);
            (void)close(pfd[1]);
        }

        (void) execl("/bin/sh", "sh", "-c", pszCmd, NULL);
        _exit(127);
    }

    /* 父进程 */
    (void)close(pfd[1]);
    if (NULL == (fp = fdopen(pfd[0], "r")))
    {
        (void) kill(pid, SIGKILL);
        (void) waitpid(pid, &stat, 0);
        return ERROR_FDOPEN;
    }

    /* Watch pfd[0] to see when it has input. */
    /*lint -save -e573 */
	/*lint -e530 */
    FD_ZERO(&rfds);
    FD_SET(pfd[0], &rfds);
	/*lint +e530 */
    /*lint -restore -e573 */

    /* Wait up to POPEN_TIMEOUT seconds. */
    tv.tv_sec = POPEN_TIMEOUT;
    tv.tv_usec = 0;
    retval = select(pfd[0] + 1, &rfds, NULL, NULL, &tv);

    if (-1 == retval)
    {
        /* select()函数执行失败 */
        (void) kill(pid, SIGKILL);
        nRet = ERROR_SELECT;
    }
    else if (retval)
    {
        /* 将子进程的标准输出写入字符数组,只读入一行 */
        (void) fgets(pszBuffer, size, fp);
    }
    else
    {
        /* No data within POPEN_TIMEOUT seconds */
        (void) kill(pid, SIGKILL);
        nRet = ERROR_TIMEOUT;
    }

    (void) fclose(fp);
    fp = NULL;

    while (0 > waitpid(pid, &stat, 0))
    {
        if (EINTR != errno)
        {
            return ERROR_WAITPID;
        }
    }

    /* 调用的脚本正常结束 处理返回值0-127 */
    if (WIFEXITED(stat))
    {
        nRet = WEXITSTATUS(stat);
    }

    return nRet;
}

/*****************************************************************************
 Function   : uvp_sleep
 Description: sleep 1 seconds
 Input      : None
 Output     : None
 Return     : None
 *****************************************************************************/
void uvp_sleep()
{
    (void)sleep(1);
    return;
}

/*****************************************************************************
 Function   : IsSupportCpuHotplug
 Description: get os info to detect if os support cpu hotplug
 Input      : None
 Output     : None
 Return     : TRUE or FALSE
 *****************************************************************************/
int IsSupportCpuHotplug(void)
{
    int ret = 0;
    char *pszHotplugFlagScript = "uname_str=`uname -r`;"
         "cpu_hotplug_conf=`grep 'CONFIG_HOTPLUG_CPU' /boot/config-$uname_str 2>/dev/null | awk -F= '{print $2}'`;"
         "printf $cpu_hotplug_conf 2>/dev/null ;";
    char pszHotplugFlag[1024] = {0};

    ret = uvpPopen(pszHotplugFlagScript, pszHotplugFlag, 1024);
    if (0 != ret)
    {
        ERR_LOG("Failed to call uvpPopen, ret=%d.", ret);
        return ret;
    }

    if (0 == strcmp("y", pszHotplugFlag))
    {
        return XEN_SUCC;
    }
    else
    {
        INFO_LOG("This OS is not supported cpu hotplug.");
        return XEN_FAIL;
    }
}

/*****************************************************************************
 Function   : GetSupportMaxnumCpu
 Description: cpu hotplug support the maxnum cpu
 Input      : None
 Output     : None
 Return     : return the maxnum
 *****************************************************************************/
int GetSupportMaxnumCpu(void)
{
    int ret = 0;
    int cpu_nr = 0;
    char *pszSysCpuNumScript = "uname_str=`uname -r`;"
         "syscpu_enable_num=`grep 'CONFIG_NR_CPUS' /boot/config-$uname_str 2>/dev/null | awk -F= '{print($2)}'`;"
         "printf $syscpu_enable_num 2>/dev/null ;";
    char pszSysCpuNum[1024] = {0};

    ret = uvpPopen(pszSysCpuNumScript, pszSysCpuNum, 1024);
    if (0 != ret)
    {
        ERR_LOG("Failed to call uvpPopen, ret=%d.", ret);
        return ret;
    }

    cpu_nr = strtoul(pszSysCpuNum, NULL, 10);

    /*lint -e648 */
    if (ULONG_MAX == cpu_nr)
    {
        return -ERANGE;
    }
    /*lint +e648 */
    else if (cpu_nr <= CPU_NR_MAX)
    {
        return cpu_nr;
    }
    else
    {
        return CPU_NR_MAX;
    }
}

/*****************************************************************************
 Function   : SetCpuHotplugFeature
 Description: write Cpu hotplug token
 Input      : handle -- xenbus file handle
 Output     : None
 Return     : return OS  support or not support cpu hotplug
 *****************************************************************************/
int SetCpuHotplugFeature(void * phandle)
{
    int cpu_hotplug_status = XEN_FAIL;

    /* enter OS has been supported cpu hotplug */
    cpu_hotplug_status = IsSupportCpuHotplug();

    /* set CpuHotplug Feature flag */
    (void)write_to_xenstore(phandle, CPU_HOTPLUG_FEATURE, \
                            cpu_hotplug_status == XEN_SUCC ? "1" : "0");
    return cpu_hotplug_status;
}

/*****************************************************************************
 Function   : cpuhotplug_regwatch
 Description: Register Cpu hotplug watch
 Input      : handle -- xenbus file handle
 Output     : None
 Return     : None
 *****************************************************************************/
void cpuhotplug_regwatch(void * phandle)
{
    (void)regwatch(phandle, CPU_HOTPLUG_SIGNAL , "");
    return;
}

/*****************************************************************************
 Function   : do_cpu_online
 Description: update cpu to online and wrtie state into xenstore
 Input      : handle -- xenbus file handle
 Output     : None
 Return     : XEN_SUCC or XEN_ERROR
 *****************************************************************************/
int DoCpuHotplug(void * phandle)
{
    char pszBuff[1024] = {0};
    char pszCommand[1024] = {0};
    int  i = 0;
    int  cpu_enable_num = 0;
    int  cpu_nr = 0;
    char *cpu_online = NULL;
    int  idelay = 0;
    int  ret = -1;
    int  rc = -1;

    cpu_nr = GetSupportMaxnumCpu();
    if (cpu_nr <= 0)
    {
        INFO_LOG("This OS has unexpectable cpus: %d.", cpu_nr);
        goto out;
    }

    INFO_LOG("This OS has cpu hotplug and less than %d.", cpu_nr);

    /* obtain cpu numbers */
    for(i = 0; i <= cpu_nr - 1; i++)
    {
        (void)snprintf_s(pszCommand, 1024, 1024, "cpu/%d/availability", i);
        cpu_online = read_from_xenstore(phandle, pszCommand);
        if((NULL != cpu_online) && (0 == strcmp(cpu_online, "online")))
        {
            cpu_enable_num ++;
        }
        free(cpu_online);
        cpu_online = NULL;
    }

    i = 1;

    /* loop for upgrade cpu online */
    while (i < cpu_enable_num )
    {
        if (idelay >= 300)
        {
            i ++;
            idelay = 0;
            continue;
        }

        (void)memset_s(pszCommand, 1024, 0, 1024);
        (void)memset_s(pszBuff, 1024, 0, 1024);
        (void)snprintf_s(pszCommand, 1024, 1024,
            "if [ -d \"/sys/devices/system/cpu/cpu%d\" ]; then \n  printf \"online\" \nfi", i);

        ret = uvpPopen(pszCommand, pszBuff, 1024);

        if (0 != ret)
        {
            ERR_LOG("Failed to call uvpPopen, ret=%d.", ret);
            idelay++;
            uvp_sleep();
            continue;
        }

        if (0 != strcmp("online", pszBuff))
        {
            idelay++;
            uvp_sleep();
            continue;
        }

        /*  判断cpu当前是否online */
        (void)memset_s(pszCommand, 1024, 0, 1024);
        (void)memset_s(pszBuff, 1024, 0, 1024);
        (void)snprintf_s(pszCommand, 1024, 1024, "cat /sys/devices/system/cpu/cpu%d/online", i);
        ret = uvpPopen(pszCommand, pszBuff, 1024);

        if (0 != ret)
        {
            ERR_LOG("Failed to call uvpPopen, ret=%d.", ret);
            idelay++;
            uvp_sleep();
            continue;
        }

        if ('1' == pszBuff[0])
        {
            INFO_LOG("Cpu%d is always online.", i);
            i++;
            idelay = 0;
            continue;
        }
        (void)memset_s(pszCommand, 1024, 0, 1024);
        (void)memset_s(pszBuff, 1024, 0, 1024);
        (void)snprintf_s(pszCommand, 1024, 1024, "echo 1 > /sys/devices/system/cpu/cpu%d/online", i);

        ret = uvpPopen(pszCommand, pszBuff, 1024);
        if (0 != ret)
        {
            ERR_LOG("Failed to call uvpPopen, ret=%d.", ret);
            idelay++;
            uvp_sleep();
            continue;
        }

        idelay = 0;
        i++;
    }

    rc = XEN_SUCC;

out:
    (void)write_to_xenstore(phandle, CPU_HOTPLUG_STATE, "0");
    (void)write_to_xenstore(phandle, CPU_HOTPLUG_SIGNAL, "0");
    return rc;
}

