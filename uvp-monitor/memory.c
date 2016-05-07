/*
 * Obtains the memory capacity and usage, and writes these to xenstore.
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
#include "securec.h"
#include "uvpmon.h"

#define PROC_MEMINFO "/proc/meminfo"
#define MEM_DATA_PATH  "control/uvp/memory"
#define SWAP_MEM_DATA_PATH  "control/uvp/mem_swap"
#define TMP_BUFFER_SIZE 255
#define DECIMAL 10

/*****************************************************************************
Function   : GetMMUseRatio
Description: 获得 memory的利用率
Input       :meminfo文件的路径
Input       :char size
Output     :按顺序保存内存总量，内存空闲量，内存使用量，内存buffer量，
            内存cache量以及swap总量，swap使用量，swap空闲量，单位KB
Return     : memory的利用率
*****************************************************************************/
int GetMMUseRatio(const char *mem_file, char *meminfo_buf, int size, char *swap_meminfo_buf)
{
    FILE *file;
    char tmp_buffer[TMP_BUFFER_SIZE + 1];
    char *start = NULL;
    char *start_swap = NULL;
    unsigned long ulMemTotal = 0L;
    unsigned long ulMemFree = 0L;
    unsigned long ulMemBuffers = 0L;
    unsigned long ulMemCached = 0L;
    unsigned long ulMemSwapCached = 0L;
    unsigned long ulMemSReclaimable = 0L;
    unsigned long ulMemNFSUnstable = 0L;
    unsigned long ulSwapTotal = 0L;
    unsigned long ulSwapFree = 0L;
    
    int iRetLen = 0;

    file = fopen(mem_file, "r");
    if (NULL == file)
    {
        //	LogPrint("Unable to open %s, errno: %d\n", PROC_MEMINFO, errno);

        (void)strncpy_s(meminfo_buf, size+1, ERR_STR, size);
        (void)strncpy_s(swap_meminfo_buf, size+1, ERR_STR, size);
        meminfo_buf[size] = '\0';
        swap_meminfo_buf[size] = '\0';

        return ERROR;
        //lint -save -e438
    }

    while (NULL != fgets(tmp_buffer, TMP_BUFFER_SIZE, file))
    {
        /*get total memory*/
        start = strstr(tmp_buffer, "MemTotal:");

        if ( NULL != start )
        {
            start = start + strlen("MemTotal:");
            /*lMemTotal = atol(start);*/
            ulMemTotal = strtoul(start, NULL, DECIMAL);
        }
        /*get free memory*/
        start = strstr(tmp_buffer, "MemFree:");
        if ( NULL != start )
        {
            start = start + strlen("MemFree:");
            /*lMemFree = atol(start);*/
            ulMemFree = strtoul(start, NULL, DECIMAL);
        }
        /*get buffers memory*/
        start = strstr(tmp_buffer, "Buffers:");
        if ( NULL != start )
        {
            start = start + strlen("Buffers:");
            ulMemBuffers = strtoul(start, NULL, DECIMAL);
        }
        /*get cached memory*/
        start = strstr(tmp_buffer, "Cached:");
        if ( NULL != start )
        {
            if(0 == strncmp(tmp_buffer, "Cached:", 7))
            {
                start = start + strlen("Cached:");
                ulMemCached = strtoul(start, NULL, DECIMAL);
            }
        }
        /*get SwapCached memory*/
        start = strstr(tmp_buffer, "SwapCached:");
        if ( NULL != start )
        {
            start = start + strlen("SwapCached:");
            ulMemSwapCached = strtoul(start, NULL, DECIMAL);
        }
        /*get swap total*/
        start = strstr(tmp_buffer, "SwapTotal:");
        if ( NULL != start )
        {
            start = start + strlen("SwapTotal:");
            ulSwapTotal = strtoul(start, NULL, DECIMAL);
        }
        /*get swap free*/
        start = strstr(tmp_buffer, "SwapFree:");
        if ( NULL != start )
        {
            start = start + strlen("SwapFree:");
            ulSwapFree = strtoul(start, NULL, DECIMAL);
        }
        /*get SReclaiable memory*/
        start = strstr(tmp_buffer, "SReclaimable:");
        if ( NULL != start )
        {
            start = start + strlen("SReclaimable:");
            ulMemSReclaimable = strtoul(start, NULL, DECIMAL);
        }      
        /*get NFSUnstable memory*/
        start = strstr(tmp_buffer, "NFS_Unstable:");
        if ( NULL != start )
        {
            start = start + strlen("NFS_Unstable:");
            ulMemNFSUnstable = strtoul(start, NULL, DECIMAL);
            break;
        }

    }

    (void)fclose(file);
    if(is_suse())
    {
        iRetLen = snprintf_s(meminfo_buf, size - 1, size - 1, "%lu:%lu:%lu:%lu:%lu", 
                    ulMemFree + ulMemBuffers + ulMemCached + ulMemSwapCached + ulMemSReclaimable + ulMemNFSUnstable,
                    ulMemTotal,
                    ulMemTotal - ulMemFree,
                    ulMemBuffers,
                    ulMemCached);
    }
    else
    {
        iRetLen = snprintf_s(meminfo_buf, size - 1, size - 1, "%lu:%lu:%lu:%lu:%lu",
                    ulMemFree + ulMemBuffers + ulMemCached,
                    ulMemTotal,
                    ulMemTotal - ulMemFree,
                    ulMemBuffers,
                    ulMemCached);
    }
    meminfo_buf[iRetLen] = '\0';

    iRetLen = snprintf_s(swap_meminfo_buf, size - 1, size - 1, "%lu:%lu:%lu",
                    ulSwapTotal,
                    ulSwapTotal - ulSwapFree,
                    ulSwapFree);
    swap_meminfo_buf[iRetLen] = '\0';
    
    return SUCC;
}

/*****************************************************************************
Function   : memoryworkctlmon
Description: 处理get请求，把free字节 + ":"+ total字节写入到xenstore
Input       :handle : handle of xenstore
Output     : None
Return     : 失败:-1，成功:0
*****************************************************************************/
int memoryworkctlmon(struct xs_handle *handle)
{
    char tmp_buffer[TMP_BUFFER_SIZE + 1];
    char tmp_swap_buffer[TMP_BUFFER_SIZE + 1];

    if (NULL == handle)
    {
        return -1;
    }

    (void)GetMMUseRatio(PROC_MEMINFO, tmp_buffer, TMP_BUFFER_SIZE, tmp_swap_buffer);

    if(xb_write_first_flag == 0)
    {
        write_to_xenstore(handle, MEM_DATA_PATH, tmp_buffer);
        write_to_xenstore(handle, SWAP_MEM_DATA_PATH, tmp_swap_buffer);
    }
    else
    {
        write_weak_to_xenstore(handle, MEM_DATA_PATH, tmp_buffer);
        write_weak_to_xenstore(handle, SWAP_MEM_DATA_PATH, tmp_swap_buffer);
    }

    //write_to_xenstore(handle, MEM_DATA_PATH, tmp_buffer);

    return 0;
}

