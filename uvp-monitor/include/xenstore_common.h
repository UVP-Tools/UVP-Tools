/*
 * xenstore head file
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

#ifndef _XENSTORE_COMMOM_H
#define _XENSTORE_COMMOM_H

    /* ANSC C */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include "xs.h"

#define XEN_SUCC 0
#define XEN_FAIL -1


#define SERVICE_FLAG_WATCH_PATH  "control/uvp/monitor-service-flag"
#define UVP_VM_STATE_PATH "control/uvp/vm_state"
#define FEATURE_FLAG_WATCH_PATH  "control/uvp/feature_flag"
#define UVP_PATH                 "control/uvp/performance"
    /* 向xenstore注册pv-driver版本的键值 */
#define PVDRIVER_VERSION  "control/uvp/pvdriver-version"
    /* 查询版本信息等不变信息的键值 */
    //#define PVDRIVER_STATIC_INFO_PATH "control/uvp/static-info-flag"

    /* define couhotplug watch path */
#define CPU_HOTPLUG_FEATURE "control/uvp/cpuhotplug_feature"
#define CPU_HOTPLUG_SIGNAL  "control/uvp/cpu_hotplug"
#define CPU_HOTPLUG_STATE   "control/uvp/cpuhotplug_flag"
#define CPU_HOTPLUG_ONLINE  "control/uvp/enable_cpuonline"

/* 该键值表明此pv driver版本支持pvscsi*/
#define SCSI_FEATURE_PATH "control/uvp/vscsi_feature"

#define GUSET_OS_FEATURE "control/uvp/guest_feature"


/*扩展信息开关路径和位域*/
#define EXINFO_FLAG_PATH "control/uvp/exinfo_flag"
/*禁用扩展信息上报的键值*/
#define DISABLE_EXINFO_PATH "control/uvp/disable_exinfo"


#define EXINFO_FLAG_DEFAULT         0       //默认，只输出基本的扩展信息
#define EXINFO_FLAG_CPU_USAGE       (1<<0)  //输出cpu时间比信息
#define EXINFO_FLAG_FILESYSTEM      (1<<1)  //输出文件系统信息
#define EXINFO_FLAG_GATEWAY         (1<<2)  //输出网关信息
#define EXINFO_FLAG_NET_LOSS        (1<<3)  //输出丢包信息

#define DISABLE_EXINFO 1
#define ENABLE_EXINFO 0
long g_exinfo_flag_value;

//定义禁用扩展信息的全局标志位
long g_disable_exinfo_value;

// for netinfo global value
long g_netinfo_value;

// for upgrade pv restart value
long g_monitor_restart_value;

char *read_from_xenstore (void *handle, char *path);
void write_to_xenstore (void *handle, char *path, char *buf);
void write_weak_to_xenstore (void *handle, char *path, char *buf);
char **readWatch(void *handle);
bool regwatch(void *handle, const char *path, const char *token);
void *openxenstore(void);
void closexenstore(void *handle);

int watch_listen(int xsfd);
int condition(void);
int getxsfileno(void *handle);

//monitor第一次向xenstore写虚拟机扩展信息的标示
int xb_write_first_flag;

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

