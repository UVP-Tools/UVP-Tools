/*
 * Accesses xenstore
 * writes these to xenstore.
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


#include <fcntl.h>
#include "xenstore_common.h"
#include "public_common.h"

static int get_fd_from_handle(struct xs_handle * handle)
{
    if(handle == NULL)
        return -2;
    else
        return *(int *)handle;
}
/*****************************************************************************
Function   : write_to_xenstore
Description: write into xenstore
Input      : handle  -- xenstore句柄
path   -- the xenstore path
buf  -- 要写入xenstore的值
Output     : None
Return     : None
*****************************************************************************/
void write_to_xenstore (void *handle, char *path, char *buf)
{
    bool err;
    struct xs_handle *head;
    int fd_pre = -1, fd_aft = -1;
    if(NULL == handle || NULL == path || NULL == buf || 0 == strlen(buf))
    {
        return ;
    }
    head = (struct xs_handle *)handle;
    fd_pre = get_fd_from_handle(head);
    err = xs_write(head, XBT_NULL, path, &buf[0], strlen(buf));
    fd_aft = get_fd_from_handle(head);
    if (!err)
    {
        ERR_LOG("write %s %s failed, errno is %d, fd_pre is %d, fd_aft is %d",
                path, buf, errno, fd_pre, fd_aft);
    }
    else
        return;
}

/*****************************************************************************
Function   : write_weak_to_xenstore
Description: write into xenstore
Input      : handle  -- xenstore句柄
path   -- the xenstore path
buf  -- 要写入xenstore的值
Output     : None
Return     : None
*****************************************************************************/
void write_weak_to_xenstore (void *handle, char *path, char *buf)
{
    bool ret = 0;
    struct xs_handle *head;
    int retry_times = 0;
    int fd_pre = -1, fd_aft = -1;
    if(NULL == handle || NULL == path || NULL == buf || 0 == strlen(buf))
    {
        return ;
    }
    head = (struct xs_handle *)handle;
    fd_pre = get_fd_from_handle(head);
    //对写xenstore失败进行重试
    do
    {
        ret = xs_write(head, XBT_NULL, path, &buf[0], strlen(buf));
        if(1 == ret)
        {
            break;
        }
        (void)usleep(300000);
        retry_times++;
    }
    while(retry_times < 3);
    if(ret != 1)
    {
        fd_aft = get_fd_from_handle(head);
        ERR_LOG("write %s %s failed, errno is %d, fd_pre is %d, fd_aft is %d",
                path, buf, errno, fd_pre, fd_aft);
        return;
    }
    else
        return;
}

/*****************************************************************************
Function   : read_from_xenstore
Description: read from xenstore
Input      : handle  -- xenstore句柄
path   -- the xenstore path
Output     : None
Return     : buf -- 从xenstore中指定路径下读取到内容
*****************************************************************************/
char *read_from_xenstore (void *handle, char *path)
{
    unsigned int len;
    char *buf = NULL;
    struct xs_handle *head;
    int fd_pre = -1, fd_aft = -1;
    if(NULL == handle || NULL == path)
    {
        return NULL;
    }
    head = (struct xs_handle *)handle;

    fd_pre = get_fd_from_handle(head);
    buf = (char *)xs_read(head, XBT_NULL, path, &len);
    fd_aft = get_fd_from_handle(head);
    if(buf == NULL && (errno != 2))
    {
        ERR_LOG("read %s failed, errno is %d, fd_pre is %d, fd_aft is %d",
                path, errno, fd_pre, fd_aft);
        if(fd_aft < 0)
        {
            exit(1);
        }
    }
//   else
    return buf;
}

/*****************************************************************************
Function   : regwatch
Description: 向xenstore注册watch
Input      : handle  -- xenstore句柄
path   -- the xenstore path
token -- watch的token
Output     : None
Return     : true or false
*****************************************************************************/
bool regwatch(void *handle, const char *path, const char *token)
{
    struct xs_handle *head;
    if (NULL == handle || NULL == path || NULL == token)
    {
        return false;
    }
    head = (struct xs_handle *)handle;
    return xs_watch(head, path, token);

}

/*****************************************************************************
Function   : openxenstore
Description: 打开xenstore
Input      : None
Output     : None
Return     : xenstore的句柄指针
*****************************************************************************/
void *openxenstore(void)
{
    struct xs_handle *h = xs_domain_open();
    int fd;
    int flag;

    if (h) {
        fd = get_fd_from_handle(h);
        flag = fcntl(fd, F_GETFD);
        if (fcntl(fd, F_SETFD, flag | FD_CLOEXEC) == -1) {
            ERR_LOG("set FD_CLOEXEC failed! errno[%d]\n", errno);
        }
    }
    return h;
}
/*****************************************************************************
Function   : closexenstore
Description: 关闭xenstore服务
Input      : handle  -- xenstore句柄

Output     : None
Return     : None
*****************************************************************************/
void closexenstore(void *handle)
{
    struct xs_handle *head;
    if (NULL == handle)
    {
        return;
    }
    head = (struct xs_handle *)handle;
    (void) xs_unwatch(head, UVP_PATH , "uvptoken");

    /* 为了在迁移后触发一次性能获取 */
    (void) xs_unwatch(head, SERVICE_FLAG_WATCH_PATH , "migtoken");
    xs_daemon_close(head);
    return;
}

/*****************************************************************************
Function   : getxsfileno
Description: 获取xenstore文件句柄
Input      : handle  -- xenstore句柄

Output     : None
Return     : 返回文件句柄fd
*****************************************************************************/
int getxsfileno(void *handle)
{
    struct xs_handle *head;

    if (NULL == handle)
    {
        return -1;
    }
    head = (struct xs_handle *)handle;
    return xs_fileno(head);
}

/*****************************************************************************
Function   : readWatch
Description: 读取watch路径
Input      : None
Output     : None
Return     : 返回watch的路径
*****************************************************************************/
char **readWatch(void *handle)
{
    unsigned int num;
    struct xs_handle *head;
    if (NULL == handle)
    {
        return NULL;
    }
    head = (struct xs_handle *)handle;
    return xs_read_watch(head, &num);
}

