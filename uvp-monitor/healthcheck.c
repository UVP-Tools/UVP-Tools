/*
 * Determines the current Linux distribution whether to upgrade UVP Tools
 * successfully.
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
#include "public_common.h"
#include "securec.h"
#include <sys/vfs.h>
#include "uvpmon.h"


#define MAX_PATH 1024
#define SHELL_BUFFER 256
#define MIN_SPACE 60
#define CHECKKERPATH "/etc/.uvp-monitor/CheckKernelUpdate.sh"


int do_command(char *path)
{
    int flag = 0, exit_value = 0;
    
    exit_value = system(path);
    flag = WEXITSTATUS(exit_value);
    return flag;
}
int isdebian()
{
    FILE *pF = NULL;
    char strIssue[SHELL_BUFFER] = {0};

    if(0 == access("/etc/debian_version", R_OK))
    {
        return 1;
    }
    pF = fopen("/etc/issue", "r");
    if (NULL == pF)
    {
        ERR_LOG("[Monitor-Upgrade]: open /etc/issue fail.");
        return 0;
    }

    while(fgets(strIssue, SHELL_BUFFER-1, pF) != NULL)
    {
        if (strstr(strIssue, "Debian"))
        {
            fclose(pF);
            return 1;
        }
    }

    fclose(pF);
    return 0;
}

int is_redhat()
{
    FILE *pF = NULL;
    char strIssue[SHELL_BUFFER] = {0};

    if(0 == access("/etc/redhat-release", R_OK))
    {
        return 1;
    }
    pF = fopen("/etc/issue", "r");
    if (NULL == pF)
    {
        ERR_LOG("[Monitor-Upgrade]: open /etc/issue fail.");
        return 0;
    }

    while(fgets(strIssue, SHELL_BUFFER-1, pF) != NULL)
    {
        if (strstr(strIssue, "Red Hat") || strstr(strIssue, "GreatTurbo"))
        {
            fclose(pF);
            return 1;
        }
    }

    fclose(pF);
    return 0;
}


int is_suse()
{
    FILE *pF = NULL;
    char strIssue[SHELL_BUFFER] = {0};

    if(0 == access("/etc/SuSE-release", R_OK))
    {
        return 1;
    }
    pF = fopen("/etc/issue", "r");
    if (NULL == pF)
    {
        ERR_LOG("[Monitor-Upgrade]: open /etc/issue fail.");
        return 0;
    }

    while(fgets(strIssue, SHELL_BUFFER-1, pF) != NULL)
    {
        if (strstr(strIssue, "SUSE"))
        {
            fclose(pF);
            return 1;
        }
    }

    fclose(pF);
    return 0;
}

int eject_command()
{
    int ret = -1;
    int sys = 0;

    sys = is_suse();
    if(sys)
    {
        ret = access("/bin/eject", X_OK);
        return ret;
    }
    sys = is_redhat();
    if(sys)
    {
        ret = access("/usr/sbin/eject", X_OK);
        return ret;
    }
    sys = isdebian();
    if(sys)
    {
        ret = access("/usr/bin/eject", X_OK);
        return ret;
    }
    return ret;
    
}

int umount_command()
{
    int ret = -1;
        
    ret = access("/bin/umount", X_OK);
    return ret;
}


int CheckDiskspace()
{
    long tmpspace = 0;//最大空间2T,long够用

    tmpspace = getfreedisk("/tmp");
    if(tmpspace < MIN_SPACE)
    {
        DEBUG_LOG("[Monitor-Upgrade]: no-space!");
        return 1;
    }
    return 0;
}

int CheckUpKernel()
{
    int ret = 0;
    
    ret = do_command(CHECKKERPATH);

    return ret;
}

int CheckCommand()
{
    int ret = -1;
    
    ret = eject_command();
    if(ret)
    {
        ret = umount_command();
        if(ret)
            return 1;
    }
    return 0;
}

/*****************************************************************************
 Function   : do_healthcheck
 Description: do update heatch check
 Input      : handle -- xenbus file handle
 Output     : None
 Return     : XEN_SUCC or XEN_ERROR
 *****************************************************************************/
int do_healthcheck(void * handle)
{
    int iSpaceRet = 0;           //disksize
    int iKernelRet = 0;          //UpKernel flag
    int iCommandRet = 0;     //command flag
    char resStr[MAX_PATH] = {0};

    if (NULL == handle)
    {
        return XEN_FAIL;
    }

    /*检查磁盘空间是否剩余*/
    iSpaceRet = CheckDiskspace();

    /*检查内核是否升级*/
    iKernelRet = CheckUpKernel();

    /*检查系统命令是否存在*/
    iCommandRet = CheckCommand();

    (void)snprintf_s(resStr, MAX_PATH, MAX_PATH, "%d:%d:%d", iSpaceRet, iKernelRet, iCommandRet);

    if(xb_write_first_flag == 0)
    {
        /*如果返回成功，写入磁盘利用率信息*/
        write_to_xenstore(handle, HEALTH_CHECK_RESULT_PATH, resStr);
    }
    else
    {
        write_weak_to_xenstore(handle, HEALTH_CHECK_RESULT_PATH, resStr);
    }
    return XEN_SUCC;
}

