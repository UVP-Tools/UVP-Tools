/*
 * Obtains the kernel version of the current Linux distribution, when it is 
 * different form items of kernel_list, records this into messages.
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

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include "check_kernel.h"
#include "xenstore_common.h"
#include "public_common.h"
#include "securec.h"

#define BUFFER_SIZE 128

#define F_OS_ISSUE  "/etc/issue"

/*
 * SuSE Distribution
 */
#define SUSE_RELEASE     "suse"
#define OPENSUSE_RELEASE "opensuse"
#define F_SUSE_RELEASE   "/etc/SuSE-release"

#define SUSE11SP3        1
#define OPENSUSE13       2

/*
 * Red Hat Distribution
 */
#define RHEL_RELEASE     "redhat"
#define F_RHEL_RELEASE   "/etc/redhat-release"

#define RHEL7            3

/*
 * Debian Distribution
 */
#define DEBIAN_RELEASE   "debian"

#define UBUNTU14         4
#define DEBIAN8          5

/*
 * SuSE Info
 */
static char *sles11sp3_kernel_list[4] = {
    "3.0.76",
    "3.0.76-0.11-default",
    "3.0.101-0.47.52-default",
    "3.0.101-0.47.67-default"
};

static char *sles_key_value[] = {
    "default",
    "pae",
};

static char *opensuse13_kernel_list[2] = {
    "3.16.6",
    "3.16.6-2-desktop"
};

static char *opensuse_key_value[] = {
    "desktop"
};

static char *rhel7_kernel_list[2] = {
    "3.10.0",
    "3.10.0-229.el7.x86_64"
};

static char *rhel_key_value[] = {
    "el6.x86_64",
    "el7.x86_64"
};

static char *fedora22_kernel_list[2] = {
    "4.0.4",
    "4.0.4-301.fc22.x86_64"
};

static char *fedora22_key_value[] = {
    "fc22.x86_64"
};

static char *ubuntu_kernel_list[] = {
    "3.19.0",
    "3.19.0-25-generic"
};

static char *ubuntu_key_value[] = {
    "generic"
};

static char *debian8_kernel_list[] = {
    "3.16.0",
    "3.16.0-4-amd64"
};

static char *debian8_key_value[] = {
    "amd64"
};

static struct suse_release {
    char version[8];
    char patchlevel[8];
} suse_release = {0};

static struct opensuse_release {
    char version[8];
} opensuse_release = {0};

static struct rhel_release {
    char version[8];
} rhel_release = {0};

static struct fedora_release {
    char version[8];
} fedora_release = {0};

static struct ubuntu_release {
    char version[8];
} ubuntu_release = {0};

static struct debian_release {
    char version[8];
} debian_release = {0};

/*
 * Get issue info
 */
static char * get_issue_info(void)
{
    char *os_rel = NULL;
    FILE *filp = NULL;
    char issue_fix[BUFFER_SIZE] = {0};

    if ( 0 != access(F_OS_ISSUE, R_OK) ) {
        return NULL;
    }

    if ( 0 == access(F_RHEL_RELEASE, R_OK) ) {
        if ( NULL == (filp = fopen(F_RHEL_RELEASE, "r")) ) {
            ERR_LOG("Open file %s failed.\n", F_RHEL_RELEASE);
            return NULL;
        }

        while( NULL != (fgets(issue_fix, BUFFER_SIZE - 1, filp)) ) {
            if (strlen(issue_fix) != 1) {
                break;
            }
        }
    } else {
        if ( NULL == (filp = fopen(F_OS_ISSUE, "r")) ) {
            ERR_LOG("Open file %s failed.\n", F_OS_ISSUE);
            return NULL;
        }
    
        while( NULL != (fgets(issue_fix, BUFFER_SIZE - 1, filp)) ) {
            if (strlen(issue_fix) != 1) {
                break;
            }
        }
    }

    if ( NULL != strcasestr(issue_fix, "opensuse") ) {
        os_rel = OPENSUSE_RELEASE;
    } else if ( NULL != strcasestr(issue_fix, "suse") ) {
        os_rel = SUSE_RELEASE;
    } else if ( (NULL != strcasestr(issue_fix, "red hat")) || (NULL != strcasestr(issue_fix, "fedora")) ) {
        os_rel = RHEL_RELEASE;
    } else if ( (NULL != strcasestr(issue_fix, "debian")) || (NULL != strcasestr(issue_fix, "ubuntu")) ) {
        os_rel = DEBIAN_RELEASE;
    } else {
        fclose(filp);
        filp = NULL;
        return NULL;
    }

    fclose(filp);
    filp = NULL;

    return os_rel;
}

static int get_sles_version(char * rel_info)
{
    char *suse_rel_delims = "=";
    char *buf = rel_info;
    char *ptr = NULL;
    char *outer_ptr = NULL;
    int i = 1;

    if ( NULL == buf) {
        return -1;
    }

    if (strstr(buf, "VERSION")) {
        while( NULL != (ptr = strtok_s(buf, suse_rel_delims, &outer_ptr)) ) {
            if ( 2 == i ) {
                ptr = ptr + 1;
                memcpy_s(suse_release.version, sizeof(suse_release.version), ptr, 2);
            }
            i++;
            buf = NULL;
        }
    } else if (strstr(buf, "PATCHLEVEL")) {
        while( NULL != (ptr = strtok_s(buf, suse_rel_delims, &outer_ptr)) ) {
            if ( 2 == i ) {
                ptr = ptr + 1;
                memcpy_s(suse_release.patchlevel, sizeof(suse_release.patchlevel), ptr, 1);
            }
            i++;
            buf = NULL;
        }
    } else {
        return -1;
    }

    return 0;
}

static int get_opensuse_version(char * rel_info)
{
    char *suse_rel_delims = "=";
    char *buf = rel_info;
    char *ptr = NULL;
    char *outer_ptr = NULL;
    int i = 1;

    if ( NULL == buf) {
        return -1;
    }

    if (strstr(buf, "VERSION")) {
        while( NULL != (ptr = strtok_s(buf, suse_rel_delims, &outer_ptr)) ) {
            if ( 2 == i ) {
                ptr = ptr + 1;
                memcpy_s(opensuse_release.version, sizeof(opensuse_release.version), ptr, 4);
            }
            i++;
            buf = NULL;
        }
    }

    return 0;
}

static int get_rhel_version(char * rel_info)
{
    char *rhel_key_flag = "7";
    char *fedora_key_flag = "22";
    char *buf = rel_info;

    if ( NULL == buf ) {
        return -1;
    }

    if ( NULL != strstr(buf, rhel_key_flag) ) {
        memcpy_s(rhel_release.version, sizeof(rhel_release.version), rhel_key_flag, strlen(rhel_key_flag));
        return 0;
    } else if ( NULL != strstr(buf, fedora_key_flag) ) {
        memcpy_s(fedora_release.version, sizeof(fedora_release.version), fedora_key_flag, strlen(fedora_key_flag));
        return 0;
    } else {
        return -1;
    }
}

static int get_debian_version(char * rel_info)
{
    char *ubn_key_flag = "14.04.3";
    char *deb_key_flag = "8";
    char *buf = rel_info;

    if ( NULL == buf ) {
        return -1;
    }

    if ( NULL != strstr(buf, ubn_key_flag) ) {
        memcpy_s(ubuntu_release.version, sizeof(ubuntu_release.version), ubn_key_flag, strlen(ubn_key_flag));
        return 0;
    } else if ( NULL != strstr(buf, deb_key_flag) ) {
        memcpy_s(debian_release.version, sizeof(debian_release.version), deb_key_flag, strlen(deb_key_flag));
        return 0;
    } else {
        return -1;
    }
}

static int get_os_dist(char *issue_info)
{
    FILE *filp = NULL;
    char dist_fix[BUFFER_SIZE] = {0};

    if ( !issue_info ) {
        ERR_LOG("Can not determine linux distribution.\n");
        goto fail;
    }

    if (0 == strcmp(OPENSUSE_RELEASE, issue_info)) {
        if ( NULL == (filp = fopen(F_SUSE_RELEASE, "r"))) {
            goto fail;
        }
        while( NULL != (fgets(dist_fix, BUFFER_SIZE - 1, filp)) ) {
            (void)get_opensuse_version(dist_fix);
        }
    } else if (0 == strcmp(SUSE_RELEASE, issue_info)) {
        if ( NULL == (filp = fopen(F_SUSE_RELEASE, "r"))) {
            goto fail;
        }
        while( NULL != (fgets(dist_fix, BUFFER_SIZE - 1, filp)) ) {
            (void)get_sles_version(dist_fix);
        }
    } else if(0 == strcmp(RHEL_RELEASE, issue_info)) {
        if ( NULL == (filp = fopen(F_RHEL_RELEASE, "r"))) {
            goto fail;
        }
        while( NULL != (fgets(dist_fix, BUFFER_SIZE - 1, filp)) ) {
            (void)get_rhel_version(dist_fix);
        }
    } else if(0 == strcmp(DEBIAN_RELEASE, issue_info)) {
        if ( NULL == (filp = fopen(F_OS_ISSUE, "r"))) {
            goto fail;
        }
        while( NULL != (fgets(dist_fix, BUFFER_SIZE - 1, filp)) ) {
            (void)get_debian_version(dist_fix);
        }
    }

    if (filp)
        fclose(filp);
    return 0;

fail:
    return -1;
}

static int make_sure_linux_ver(void)
{
    char *p_issue_info = get_issue_info();

    if ( 0 != get_os_dist(p_issue_info) )
        return -1;

    if ( (0 == strcmp(suse_release.patchlevel, "3") || (0 == strcmp(suse_release.version, "11"))) ) {
        return SUSE11SP3;
    } else if ( 0 == strcmp(opensuse_release.version, "13.2") ) {
        return OPENSUSE13;
    } else if ( (0 == strcmp(rhel_release.version, "7")) || (0 == strcmp(fedora_release.version, "22")) ) {
        return RHEL7;
    } else if ( 0 == strcmp(ubuntu_release.version, "14.04.3") ) {
        return UBUNTU14;
    } else if ( 0 == strcmp(debian_release.version, "8") ) {
        return DEBIAN8;
    }

    return 0;
}

static char * get_sys_kern_info(void)
{
    struct utsname u;
    // Need to free outside!
    char *release = (char *)malloc(BUFFER_SIZE * sizeof(char));
    if (!release)
        return NULL;
    memset_s(release, BUFFER_SIZE, 0, BUFFER_SIZE);

    if ( -1 == uname(&u) ) {
        free(release);
        return NULL;
    }
    memcpy_s(release, BUFFER_SIZE, u.release, BUFFER_SIZE);
    
    return release;
}

static int compare_kern_ver(char * sys_rel, 
                int rel_count, 
                char *target_rel[],
                int key_count, 
                char *target_key[])
{
    int i = 0;
    unsigned long offset_len = 0;

    /* 1. white list */
    for (i = 0; i < rel_count; i++) {
        if (0 == strcmp(sys_rel, target_rel[i])) {
            goto nolog;
        }
    }
    /* 2. release key */
    if (0 == strncmp(sys_rel, target_rel[0], strlen(target_rel[0]))) {
        for (i = 0; i < key_count; i++) {
            offset_len = strlen(sys_rel) - strlen(target_key[i]);
            if (0 == strcmp(sys_rel + offset_len, target_key[i]))
                goto nolog;
        }
        if (i >= key_count)
            goto log;
    } else {
        goto log;
    }

nolog:
    return 0;
log:
    return -1;
}

static int check_kabi(int linux_ver)
{
    char sys_rel[BUFFER_SIZE] = {0};
    char * p_sys_rel = get_sys_kern_info();
    int rel_num = 0;
    int key_num = 0;
    int ret = 0;

    if (!p_sys_rel) {
        ERR_LOG("get_sys_kern_info failed!\n");
        goto nolog;
    }
    memcpy_s(sys_rel, BUFFER_SIZE, p_sys_rel, BUFFER_SIZE);
    free(p_sys_rel);
    p_sys_rel = NULL;

    switch(linux_ver) {
    case SUSE11SP3:
        rel_num = sizeof(sles11sp3_kernel_list)/sizeof(char*);
        key_num = sizeof(sles_key_value)/sizeof(char*);
        ret = compare_kern_ver(sys_rel, rel_num, sles11sp3_kernel_list, key_num, sles_key_value);
        if (0 == ret)
            goto nolog;
        else
            goto log;
    case OPENSUSE13:
        rel_num = sizeof(opensuse13_kernel_list)/sizeof(char*);
        key_num = sizeof(opensuse_key_value)/sizeof(char*);
        ret = compare_kern_ver(sys_rel, rel_num, opensuse13_kernel_list, key_num, opensuse_key_value);
        if (0 == ret)
            goto nolog;
        else
            goto log;
    case RHEL7:
        rel_num = sizeof(rhel7_kernel_list)/sizeof(char*);
        key_num = sizeof(rhel_key_value)/sizeof(char*);
        ret = compare_kern_ver(sys_rel, rel_num, rhel7_kernel_list, key_num, rhel_key_value);
        if (0 == ret)
            goto nolog;

        rel_num = sizeof(fedora22_kernel_list)/sizeof(char*);
        key_num = sizeof(fedora22_key_value)/sizeof(char*);
        ret = compare_kern_ver(sys_rel, rel_num, fedora22_kernel_list, key_num, fedora22_key_value);
        if (0 == ret)
            goto nolog;
        else
            goto log;
    case UBUNTU14:
        rel_num = sizeof(ubuntu_kernel_list)/sizeof(char*);
        key_num = sizeof(ubuntu_key_value)/sizeof(char*);
        ret = compare_kern_ver(sys_rel, rel_num, ubuntu_kernel_list, key_num, ubuntu_key_value);
        if (0 == ret)
            goto nolog;
        else
            goto log;
    case DEBIAN8:
        rel_num = sizeof(debian8_kernel_list)/sizeof(char*);
        key_num = sizeof(debian8_key_value)/sizeof(char*);
        ret = compare_kern_ver(sys_rel, rel_num, debian8_kernel_list, key_num, debian8_key_value);
        if (0 == ret)
            goto nolog;
        else
            goto log;

    default:
        break;
    }

nolog:
    return 0;
log:
    return 1;
}

void check_compatible(void)
{
    if( check_kabi(make_sure_linux_ver()))
        INFO_LOG("The kernel version of the running Linux VM may be incompatible with UVP Tools. kernel ver.: %s.\n",
                        get_sys_kern_info());

    return; 
}

