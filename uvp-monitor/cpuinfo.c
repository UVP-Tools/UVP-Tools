/*
 * Obtains the CPU usage, and writes these to xenstore.
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
#include <ctype.h>
#include "securec.h"

#define NCPUSTATES  9
#define MAXCPUNUM   64
#define BUFFSIZE    2048
#define TIMEBUFSIZE  128
#define IDLE_USAGE   3
#define CPU_STATE_FILE          "/proc/stat"
#define CPU_INFO_SIZE		10   /*record the information of CPU in form of "%d:%.2f;",
since there max number of cpu is 64, the longest size of the information for each CPU is 8bytes*/
#define CPU_USAGE_SIZE 32
typedef unsigned long long TIC_t;
typedef          long long SIC_t;
#define TRIMz(x)  ((tz = (SIC_t)(x)) < 0 ? 0 : tz)
typedef struct CPU_t {
   TIC_t u_current, n_current, s_current, i_current, w_current, x_current, y_current, z_current; // as represented in /proc/stat
   TIC_t u_save, s_save, n_save, i_save, w_save, x_save, y_save, z_save;
} CPU_t;

static int CpuTimeFirstFlag=0;

struct cpu_data
{
    long    cp_new[NCPUSTATES];
    long    cp_old[NCPUSTATES];
    long    cp_diff[NCPUSTATES];
    int     cpu_usage[NCPUSTATES];
};


/*****************************************************************************
Function   : skip_token
Description: skip the string, delete the "cpu" head
Input       : pointer to character
Output     : None
Return     : pointer to character
*****************************************************************************/
char *cpu_skip_token(const char *pString)
{
    /*coverity warning：增加 *pString 否为'\0' 判断*/
    while (*pString && isspace((unsigned char)(*pString)))
    {
        pString++;
    }
    while (*pString && !isspace((unsigned char)(*pString)))
    {
        pString++;
    }
    return (char *)pString;
}


/*****************************************************************************
Function   : OpenStatFile
Description: Open the /proc/stat file
Input       : None
Output     : None
Return     : success : return pointer to file,  fail : return NULL
*****************************************************************************/
FILE *OpenStatFile()
{
    FILE *pstat = NULL;

    if(NULL == (pstat = fopen(CPU_STATE_FILE, "r")))
    {
        //   LogPrint("open file /proc/stat error\n");
        return NULL;
    }
    else
    {
        return pstat;
    }
}


/*****************************************************************************
Function   : GetCPUCount
Description: get the CPU Count
Input       : None
Output     : None
Return     : Count [1~32]
*****************************************************************************/
int GetCPUCount()
{
    char    buf[BUFFSIZE];
    int     check_char;
    int     count = 0;
    FILE    *pFile = NULL;

    if(NULL == (pFile = OpenStatFile()))
    {
        return 0;
    }
    while(EOF != (check_char = fgetc(pFile)))
    {
        if (('c' == check_char) && ('p' == fgetc(pFile)))
        {
            (void)fseek(pFile, -2, SEEK_CUR);
            if(NULL != fgets(buf, BUFFSIZE, pFile))
            {
                count++;
            }
        }
        else
        {
            break;
        }
    }

    if (1 >= count)
    {
        count = 1;
    }
    else if (MAXCPUNUM < count)
    {
        count = MAXCPUNUM + 1;
    }
    count--;

    fclose(pFile);
    //lint -save -e438
    pFile = NULL;
    return count;
    //lint -restore
}


/*****************************************************************************
Function   : percentages
Description: Calculates the CPU usage percentages
Input       : cpu states count, pointer to cpu_usage struct
Output     : None
Return     :
*****************************************************************************/
long percentages(int StateCount, struct cpu_data *pCpuTmp)
{
    int     i = 0;
    long    change = 0;
    long    total_change = 0;
    int     *usage = NULL;
    long    *new = NULL;
    long    *old = NULL;
    long    *diffs = NULL;


    /* initialization */
    usage = pCpuTmp->cpu_usage;
    new = pCpuTmp->cp_new;
    old = pCpuTmp->cp_old;
    diffs = pCpuTmp->cp_diff;

    /* calculate changes for each state and the overall change */
    for (i = 0; i < StateCount; i++)
    {
        if ((change = *new - *old) < 0)
        {
            /* this only happens when the counter wraps */
            change = *old - *new;
        }
        total_change += (*diffs++ = change);
        *old++ = *new++;
    }

    /* avoid divide by zero potential */
    if (0 == total_change)
    {
        total_change = 1;
    }

    /* calculate percentages based on overall change, rounding up */
    diffs = pCpuTmp->cp_diff;
    for (i = 0; i < StateCount; i++)
    {
        *usage++ = (int)((*diffs++ * 1000) / total_change);
    }

    /* return the total in case the caller wants to use it */
    return(total_change);
}


/*****************************************************************************
Function   : pGetCPUUsage
Description: get the CPU usage
Input       :None
Output     : CPU usage string
Return     : success : return CPU usage string,  fail : return ERR_STR
*****************************************************************************/
char *pGetCPUUsage(char *pResult)
{
    int     i = 0;
    int     j = 0;
    int     flg = 0;
    int     cpucount = 0;
    int     shiftsize = 0;
    char    *pTmpString = NULL;
    char    *pResultTmp = NULL;
    FILE    *pFileStat = NULL;
    char    CpuInfoTmp[CPU_INFO_SIZE*4];
    errno_t rc = 0;

    char    BufTmp[BUFFSIZE];
    float   fCpuUsage[MAXCPUNUM];
    struct  cpu_data CpuRecord[MAXCPUNUM];

    for(i = 0; i < MAXCPUNUM; i ++)
    {
        fCpuUsage[i] = 0.0;
    }
    (void)memset_s(BufTmp, BUFFSIZE, 0, BUFFSIZE);
    (void)memset_s(CpuRecord, MAXCPUNUM, 0, MAXCPUNUM * sizeof(struct cpu_data));

    cpucount = GetCPUCount();

    while(1)
    {
        pFileStat = OpenStatFile();
        if(NULL == pFileStat)
        {
            return ERR_STR;
        }

        if(NULL == fgets(BufTmp, BUFFSIZE, pFileStat))
        {
            fclose(pFileStat);
            return ERR_STR;
        }


        for(i = 0; i < cpucount; i++)
        {
            if(NULL == fgets(BufTmp, BUFFSIZE, pFileStat))
            {
                fclose(pFileStat);
                return ERR_STR;
            }
            pTmpString = cpu_skip_token(BufTmp);	 /* skip "cpu" */
            for(j = 0; j < NCPUSTATES; j ++)
            {
                CpuRecord[i].cp_new[j] = strtoul(pTmpString, &pTmpString, 0);
            }

            (void)percentages(NCPUSTATES, &CpuRecord[i]);
            fCpuUsage[i] = 0.0;
            for(j = 0; j < NCPUSTATES ; j ++)
            {
                fCpuUsage[i] = fCpuUsage[i] + CpuRecord[i].cpu_usage[j] / 10.0;
            }
            fCpuUsage[i] = fCpuUsage[i] - CpuRecord[i].cpu_usage[IDLE_USAGE] / 10.0;

            if( fCpuUsage[i] < 0)
            {
                fCpuUsage[i] = abs(fCpuUsage[i]);
            }

        }

        fclose(pFileStat);
        pFileStat = NULL;
        if (0 != flg)
        {
            break;
        }

        (void)usleep(500000);

        flg++;
    }/* End of while(1) */

    pResultTmp = pResult;

    for (i = 0; i < cpucount; i++)
    {
        memset_s(CpuInfoTmp,sizeof(CpuInfoTmp),0,sizeof(CpuInfoTmp));

        shiftsize = snprintf_s(CpuInfoTmp,sizeof(CpuInfoTmp),  sizeof(CpuInfoTmp), "%d:%.2f;", i, fCpuUsage[i]);
        if(-1 == shiftsize)
        {
            return ERR_STR;
        }

        rc = memcpy_s(pResultTmp, strlen(CpuInfoTmp), CpuInfoTmp, strlen(CpuInfoTmp));


        if(rc != 0 )
        {
            /* hand erro*/
            return ERR_STR;
        }

        pResultTmp = pResultTmp + shiftsize;
    }
    (void)snprintf_s(pResultTmp, 1, 1, '\0');

    return SUCC;
}

/*****************************************************************************
Function   : CpuTimeWaitPercentage
Description: get cpu wait time percentage on usr task, sys task, wait task
Input      : cputimevalue
Output     :
Return     : -1: fail  0: success
*****************************************************************************/
int CpuTimeWaitPercentage(char *cputimevalue)
{
    SIC_t u_frme, s_frme, n_frme, i_frme, w_frme, x_frme, y_frme, z_frme, tot_frme, tz;
    FILE *fp = NULL;
    char *CpuTimeValue = NULL;
    char buf[TIMEBUFSIZE] = {0};
    float scale;
    static CPU_t cpus;
    if (!(fp = OpenStatFile()))
    {
       DEBUG_LOG("Failed open /proc/stat.");
       return ERROR;
    }
    if (!fgets(buf, sizeof(buf), fp))
    {
       fclose(fp);
       DEBUG_LOG("/proc/stat content is NULL.");
       return ERROR;
    }
    cpus.x_current = 0;  // FIXME: can't tell by kernel version number
    cpus.y_current = 0;  // FIXME: can't tell by kernel version number
    cpus.z_current = 0;  // FIXME: can't tell by kernel version number
    /*将每列的数据赋值当前变量保存*/
    (void)sscanf(buf, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",&cpus.u_current,&cpus.n_current,&cpus.s_current,&cpus.i_current,&cpus.w_current,&cpus.x_current,&cpus.y_current,&cpus.z_current);
    /*如果第一次读取的话，将当前变量赋给前值*/
    if(0 == CpuTimeFirstFlag)
    {
       cpus.u_save = cpus.u_current;
       cpus.s_save = cpus.s_current;
       cpus.n_save = cpus.n_current;
       cpus.i_save = cpus.i_current;
       cpus.w_save = cpus.w_current;
       cpus.x_save = cpus.x_current;
       cpus.y_save = cpus.y_current;
       cpus.z_save = cpus.z_current;
       CpuTimeFirstFlag = 1;
    }
    /*当前变量与前值进行相减计算得变化值*/
    u_frme = cpus.u_current - cpus.u_save;
    s_frme = cpus.s_current - cpus.s_save;
    n_frme = cpus.n_current - cpus.n_save;
    i_frme = TRIMz(cpus.i_current - cpus.i_save);
    w_frme = cpus.w_current - cpus.w_save;
    x_frme = cpus.x_current - cpus.x_save;
    y_frme = cpus.y_current - cpus.y_save;
    z_frme = cpus.z_current - cpus.z_save;
    /*总的变化量*/
    tot_frme = u_frme + s_frme + n_frme + i_frme + w_frme + x_frme + y_frme + z_frme;
    if (tot_frme < 1)
	{
	   tot_frme = 1;
    }
    scale = 100.0 / (float)tot_frme;
    CpuTimeValue = cputimevalue;
    /*计算百分比，并格式化格式保存在字符数组中*/
    (void)snprintf_s(CpuTimeValue, CPU_USAGE_SIZE, CPU_USAGE_SIZE,
            "%.2f:%.2f:%.2f", (float)u_frme * scale, (float)s_frme * scale, (float)w_frme * scale);
    /*将当前值赋给前值*/
    cpus.u_save = cpus.u_current;
    cpus.s_save = cpus.s_current;
    cpus.n_save = cpus.n_current;
    cpus.i_save = cpus.i_current;
    cpus.w_save = cpus.w_current;
    cpus.x_save = cpus.x_current;
    cpus.y_save = cpus.y_current;
    cpus.z_save = cpus.z_current;
    fclose(fp);
    return SUCC;
}
/*****************************************************************************
Function   : cpuworkctlmon
Description:
Input       :handle : handle of xenstore
Output     : None
Return     : None
*****************************************************************************/
int cpuworkctlmon(struct xs_handle *handle)
{
    char *value = NULL;
    char *cputimevalue = NULL;
    int  CpuUsageFlag = 0;
    value = (char *)malloc(MAXCPUNUM * CPU_INFO_SIZE + 1);
    if(NULL == value)
    {
        return ERROR;
    }
    (void)memset_s(value, MAXCPUNUM * CPU_INFO_SIZE + 1, 0, MAXCPUNUM * CPU_INFO_SIZE + 1);

    cputimevalue = (char *)malloc(CPU_USAGE_SIZE);
    if(NULL == cputimevalue)
    {
        if(value)
        {
            free(value);
            value = NULL;
        }
        return ERROR;
    }
    (void)memset_s(cputimevalue, CPU_USAGE_SIZE, 0, CPU_USAGE_SIZE);

    (void)pGetCPUUsage(value);
    if(g_exinfo_flag_value & EXINFO_FLAG_CPU_USAGE)
    {
       CpuUsageFlag = CpuTimeWaitPercentage(cputimevalue);
    }
    if(xb_write_first_flag == 0)
    {
        write_to_xenstore(handle, CPU_DATA_PATH, value);
        if(g_exinfo_flag_value & EXINFO_FLAG_CPU_USAGE)
        {
            if(SUCC == CpuUsageFlag)
            {
                write_to_xenstore(handle, CPU_TIME_PATH, cputimevalue);
            }
            else
            {
                write_to_xenstore(handle, CPU_TIME_PATH, "error");
            }
        }
    }
    else
    {
        write_weak_to_xenstore(handle, CPU_DATA_PATH, value);
        if(g_exinfo_flag_value & EXINFO_FLAG_CPU_USAGE)
        {
            if(SUCC == CpuUsageFlag)
            {
                write_weak_to_xenstore(handle, CPU_TIME_PATH, cputimevalue);
            }
            else
            {
                write_weak_to_xenstore(handle, CPU_TIME_PATH, "error");
            }
        }
    }

    free(value);
    free(cputimevalue);
    //lint -save -e438
    value = NULL;
    cputimevalue = NULL;
    return SUCC;
    //lint -save -e438
}
