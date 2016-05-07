/*
 * Common head file
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


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifndef _LIBXENCTL_H
#define _LIBXENCTL_H

    /* ANSC C */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <malloc.h>
    /* linux */
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <sys/types.h>
    /* socket */
#include <sys/socket.h>
#include <arpa/inet.h>
    /* time */
#include <sys/times.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <net/if.h>


#include<signal.h>


#include<sys/file.h>


#include "xenstore_common.h"


#include <sys/wait.h>
#include <pthread.h>
#include <netdb.h>

#define IP_DOWN "disconnected"
#define ERR_STR "error"

#define VIF_DATA_PATH            "control/uvp/vif"
#define VIFEXTRA_DATA_PATH       "control/uvp/vif_extra1"

#define IPV6_VIF_DATA_PATH        "control/uvp/netinfo"

#define VIF_DROP_PATH            "control/uvp/networkloss"

#define CPU_DATA_PATH            "control/uvp/cpu"
#define CPU_TIME_PATH            "control/uvp/cpuusage"

#define ERROR -1
#define UNKNOWN ERROR
#define SUCC   0


#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

