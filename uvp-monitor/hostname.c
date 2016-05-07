/*
 * Obtains the hostname of the current Linux distr., and writes it to xenstore.
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

#define HOSTNAME_DATA_PATH  "control/uvp/hostname"
#define MAX_HOSTNAME_LENGTH 256

/*****************************************************************************
Function   : GetHostname
Description: 获得主机名
Input       :N/A
Output     :hostname:存储hostname的字符串
Return     : N/A
*****************************************************************************/

int GetHostname(char *hostname, int len )
{
    if( !gethostname( hostname, len ) )
    {
        return SUCC;
    }

    return ERROR;
}

/*****************************************************************************
Function   : hostnameworkctlmon
Description:处理gethostname请求，将hostname写入xenstore中
Input       :handle : handle of xenstore
Output     : None
Return     : 失败:-1，成功:0
*****************************************************************************/
int hostnameworkctlmon(struct xs_handle *handle)
{
    char hostname[MAX_HOSTNAME_LENGTH] = { 0 };
    int Ret = 0;

    if (NULL == handle)
    {
        return -1;
    }

    Ret = GetHostname( hostname, MAX_HOSTNAME_LENGTH );

    if(xb_write_first_flag == 0)
    {
        (void)write_to_xenstore(handle, HOSTNAME_DATA_PATH, !Ret ? hostname : "error" );
    }
    else
    {
        (void)write_weak_to_xenstore(handle, HOSTNAME_DATA_PATH, !Ret ? hostname : "error" );
    }

    //(void)write_to_xenstore(handle, HOSTNAME_DATA_PATH, !Ret ? hostname : "error" );

    return 0;
}

