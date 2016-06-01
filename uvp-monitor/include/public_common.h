/*
 * public_common.h
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

#ifndef _PUBLIC_COMMON_H
#define _PUBLIC_COMMON_H

#include <syslog.h>

#define DEBUG_LOG(fmt, args...)                 syslog(LOG_DEBUG, fmt, ##args)
#define INFO_LOG(fmt, args...)                  syslog(LOG_INFO, fmt, ##args)
#define ERR_LOG(fmt, args...)                   syslog(LOG_ERR, "%s:%d:%s: " fmt, __FILE__, __LINE__, __func__, ##args)

#define RELEASE_BOND "control/uvp/release_bond"
#define REBOND_SRIOV "control/uvp/rebond_sriov"

#define HEALTH_CHECK_PATH "control/uvp/upgrade/inspection"
#define HEALTH_CHECK_RESULT_PATH "control/uvp/upgrade/inspect-result"

int cpuworkctlmon(struct xs_handle *handle);
int memoryworkctlmon(struct xs_handle *handle);
int hostnameworkctlmon( struct xs_handle *handle );
int diskworkctlmon(struct xs_handle *handle);
void networkctlmon(void *handle);

void NetinfoNetworkctlmon(void *handle);

int netbond();
int releasenetbond(void *handle);
int rebondnet(void *handle);
void InitBond();

void start_service(void);
int SetCpuHotplugFeature(void *phandle);
void cpuhotplug_regwatch(void *phandle);
int DoCpuHotplug(void *phandle);
void doUpgrade(void *handle, char *path);
void doMountISOUpgrade(void *handle);
char *trim(char *str);
int exe_command(char *path);
void write_tools_result(void *handle);

int do_healthcheck(void * phandle);
int eject_command();
int CheckDiskspace();
int do_command(char *path);
int is_suse();

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

