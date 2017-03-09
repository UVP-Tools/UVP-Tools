/*
 * Obtains disks capacity and usage, and writes these to xenstore.
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
#include <sys/vfs.h>
#include <mntent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include "securec.h"

#define PROC_PARTITIONS             "/proc/partitions"
#define PROC_SWAPS                  "/proc/swaps"
#define PROC_MOUNTS                 "/proc/mounts"
#define PROC_DEVICES                "/proc/devices"
#define DISK_DATA_PATH              "control/uvp/disk"
#define DISK_DATA_EXT_PATH          "control/uvp/disk-ext"
#define FILE_DATA_PATH              "control/uvp/filesystem"
/*xenstore支持4096个字节，超过这个限制后换这个键值再进行filesystem信息的填写*/
#define FILE_DATA_EXTRA_PATH_PREFIX        "control/uvp/filesystem_extra"
/*filesystem键值总数目*/
#define FILE_NUM_PATH               "control/uvp/filesystem_extra_num"
#define DEVPATH                     "/dev/disk/by-id/"
#define CMPSTRING                   "../../"
#define SCSI_DISK_PATH              "device/vscsi"
/*must be equal with the value defined in uvpext*/
#define MAX_ROWS 4
#define MAX_XS_LENGTH 100
/*每个xenstore键值所写的磁盘利用率个数*/
#define MAX_DISKUSAGE_NUM_PER_KEY 30

#define RAM_MAJOR                   1
#define LOOP_MAJOR                  7
#define MAX_DISKUSAGE_LEN           1024
#define MAX_NAME_LEN                64
#define MAX_STR_LEN                 4096
#define UNIT_TRANSFER_CYCLE         1024
#define MAX_DISK_NUMBER             128          /* TODO:1.现阶段支持11个xvda磁盘，17个scsi磁盘；以后支持的磁盘数增多后此处应相应增大. 
                                                         2. 为了支持60个磁盘，此值扩大为128*/
/*支持50个规格的filesystem长度*/
#define MAX_FILENAMES_SIZE          52800
/*xenstore键值最大长度为4096，减去键值长度，剩下长度为filesystem可放的长度*/
#define MAX_FILENAMES_XENSTORLEN    4042
/*最多只上报60个磁盘的利用率信息*/
#define MAX_DISKUSAGE_STRING_NUM 60
#define SECTOR_SIZE 512
#define MEGATOBYTE (1024 * 1024)
/*FilenamesArr存放组装的文件系统名及使用率*/
char FilenameArr[MAX_FILENAMES_SIZE] = {0};
/* device-mapper对应的主设备号 */
int g_deviceMapperNum = 0;

/* 设备名称及其主次设备号结构体（/proc/partitions） */
struct DevMajorMinor
{
    int  devMajor;                              /* 设备主设备号 */
    int  devMinor;                              /* 设备次设备号 */
    char devBlockSize[MAX_STR_LEN];             /* 设备的块大小(KB) */
    char devName[MAX_NAME_LEN];                 /* 设备名 */
};

/* 物理磁盘名称及其使用空间、总空间的结构体 */
struct DeviceInfo
{
    char phyDevName[MAX_NAME_LEN];                     /* 物理磁盘 */
    char deviceTotalSpace[MAX_STR_LEN];                /* 磁盘的总空间(MB) */
    char diskUsage[MAX_DISKUSAGE_LEN];                 /* 磁盘的使用空间(MB) */
};

/* 挂载点对应文件系统信息，及其使用空间的结构体 */
struct DiskInfo
{
    int  st_rdev;                                  /* 存储文件系统对应设备ID */
    char filesystem[MAX_STR_LEN];                  /* 文件系统名，对应FileSystem列 */
    char usage[MAX_DISKUSAGE_LEN];                 /* 文件系统的使用空间(MB) */
    char mountPoint[MAX_STR_LEN];                  /* 文件系统的挂载点 */
};

/* 记录前面三个结构体数组中实际存储的个数 */
struct NumberCount
{
    int partNum;             /* 记录DevMajorMinor个数 */
    int diskNum;             /* 记录DeviceInfo个数 */
    int mountNum;            /* 记录DiskInfo个数 */
};

struct DmInfo
{
    int nParentMajor;
    int nParentMinor;
    long long nSectorNum;
};

enum dm_type
{
    DM_LINEAR = 0,
    DM_STRIPED,
    DM_CRYPT,
    DM_RAID,
    DM_MIRROR,
    DM_SNAPSHOT,
    DM_UNKNOWN
};

extern CheckName(const char * name);

void write_xs_disk(struct xs_handle *handle, int is_weak, char xs_value[][MAX_DISKUSAGE_LEN], int row_num);

/*****************************************************************************
 Function   : strAdd()
 Description: 数字型字符串相加函数
 Input      : inputNum1          第一个数字字符串
              inputNum2          第二个数字字符串
 Output     : outputResult       两个数字字符串和的字符串
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int strAdd(char *inputNum1, char *inputNum2, char *outputResult)
{
    char *pszFirstNum = inputNum1;
    char *pszSecondNum = inputNum2;
    char szTmpResult[MAX_STR_LEN] = {0};
    int nFirstNumLen;
    int nSecondNumLen;
    int nTmpStrLen;
    int nNum1 = 0;
    int nNum2 = 0;
    int sum = 0;
    int ten = 0;
    int i;
    int j = 0;
    int k = 0;

    /* 如果参数有一个为NULL，返回错误 */
    if (NULL == pszFirstNum || NULL == pszSecondNum || NULL == outputResult)
    {
        return ERROR;
    }

    nFirstNumLen = strlen(pszFirstNum);
    nSecondNumLen = strlen(pszSecondNum);

    /* 计算两个变量中较长的一个数据的长度 */
    nTmpStrLen = (nFirstNumLen < nSecondNumLen) ? nSecondNumLen : nFirstNumLen;
    if (MAX_STR_LEN <= nTmpStrLen)
    {
        return ERROR;
    }
    /* 循环读取每一位的数据 */
    for (i = 0; i < nTmpStrLen; i++)
    {
        /* 如果对应的数据长度小于循环变量，则此位数据设为0。否则直接获取此位数据，并且将ASCII码转换为数字 */
        j = nFirstNumLen - i - 1;
        k = nSecondNumLen - i - 1;
        (j >= 0) ? (nNum1 = pszFirstNum[j] - 48) : (nNum1 = 0);
        (k >= 0) ? (nNum2 = pszSecondNum[k] - 48) : (nNum2 = 0);
        /* 计算两个临时变量与升级标志位的和，取个位数和十位数 */
        sum = nNum1 + nNum2 + ten;
        ten = sum / 10;
        sum = sum % 10;
        /* 将对应的和的个位部分赋给临时字符串 */
        *(szTmpResult + i) = sum + 48;
    }
    /* 在退出循环后，如果进位标志还是1，则再将最高位的1赋给临时字符串 */
    if (1 == ten)
    {
        *(szTmpResult + i) = ten + 48;
    }

    /* 获取临时字符串的长度，将其按反序复制给出参 */
    nTmpStrLen = strlen(szTmpResult);
    for (i = 0; i < nTmpStrLen; i++)
    {
        outputResult[i] = szTmpResult[nTmpStrLen - i - 1];
    }
    /* 手动在字符串末尾加上\0，防止重复使用带来的问题 */
    outputResult[i] = '\0';

    return SUCC;
}

/*****************************************************************************
 Function   : strMinus()
 Description: 数字型字符串相减函数
 Input      : inputNum1          被减数
              inputNum2          减数
 Output     : outputResult       两个数字字符串差的字符串
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int strMinus(char *inputNum1, char *inputNum2, char *outputResult)
{
    char *pszFirstNum = inputNum1;
    char *pszSecondNum = inputNum2;
    char szTmpResult[MAX_STR_LEN] = {0};
    int nFirstNumLen;
    int nSecondNumLen;
    int nTmpStrLen;
    int nNum1 = 0;
    int nNum2 = 0;
    /* 每位相减后存放的临时数 */
    int wanting = 0;
    /* 标记借位 */
    int isNegative = 0;
    /* 存放结果的字符串下标 */
    int nTmpLocation = 0;
    int i;

    /* 如果参数有一个为NULL，返回错误 */
    if (NULL == pszFirstNum || NULL == pszSecondNum || NULL == outputResult)
    {
        return ERROR;
    }

    nFirstNumLen = strlen(pszFirstNum);
    nSecondNumLen = strlen(pszSecondNum);

    /* 如果减数的长度大于被减数的长度(结果为负)，返回错误 */
    if (nFirstNumLen < nSecondNumLen)
    {
        return ERROR;
    }

    /* 循环读取每一位的数据，然后进行对应的数据操作 */
    for (i = 0; i < nSecondNumLen; i++)
    {
        /* 获取两个数对应位上的数据 */
        nNum1 = pszFirstNum[nFirstNumLen - i - 1] - 48;
        nNum2 = pszSecondNum[nSecondNumLen - i - 1] - 48;
        /* 判断被减数对应数据被借位后是否大于减数的对应数，如果小于，则需要再向高一位借位；否则，不需要 */
        if (nNum1 >= nNum2 + isNegative)
        {
            wanting = nNum1 - nNum2 - isNegative;
            isNegative = 0;
        }
        else
        {
            wanting = nNum1 + 10 - nNum2 - isNegative;
            isNegative = 1;
        }
        /* 将相减后对应位的数值字符赋给临时字符串 */
        *(szTmpResult + i) = wanting + 48;
    }

    /* 继续循环，直到被减数的长度为止 */
    for (; i < nFirstNumLen; i++)
    {
        nNum1 = pszFirstNum[nFirstNumLen - i - 1] - 48;
        /* 如果借位标志大于0，则进行相应的运算，否则直接将当前位的数据传递给结果 */
        if (1 == isNegative)
        {
            /* 如果当前位置的数值不小于1，则直接相减，否则继续向上一位借位 */
            if (nNum1 >= 1)
            {
                wanting = nNum1 - isNegative;
                *(szTmpResult + i) = wanting + 48;
                isNegative = 0;
            }
            else
            {
                wanting = nNum1 + 10 - isNegative;
                *(szTmpResult + i) = wanting + 48;
                isNegative = 1;
            }
        }
        else
        {
            *(szTmpResult + i) = nNum1 + 48;
        }
    }
    /* 如果遍历完被减数的所有位，最后借位标记还是大于0，则返回错误 */
    if (1 == isNegative)
    {
        return ERROR;
    }

    /* 获取临时字符串的长度，将其按反序复制给出参 */
    nTmpStrLen = strlen(szTmpResult);
    for (i = 0; i < nTmpStrLen; i++)
    {
        /* 去除相减后高位的0 */
        if (0 == strlen(outputResult) && '0' == szTmpResult[nTmpStrLen - i - 1])
        {
            continue;
        }
        outputResult[nTmpLocation] = szTmpResult[nTmpStrLen - i - 1];
        nTmpLocation++;
    }

    /* 如果结果的字符串长度为0，则给出参赋值一个0 */
    if (0 == strlen(outputResult))
    {
        (void)strncpy_s(outputResult, 3, "0", 2);
    }

    return SUCC;
}

/*****************************************************************************
 Function   : strMulti()
 Description: 数字型字符串相乘函数
 Input      : inputNum1         第一个数字字符串
                inputNum1         第二个数字字符串
 Output     : outputResult      两个数字字符串积的字符串
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int strMulti(char *inputNum1, char *inputNum2, char *outputResult)
{
    char *pszFirstNum = inputNum1;
    char *pszSecondNum = inputNum2;
    char szTmpResult[MAX_STR_LEN] = {0};
    char szOneTimeResult[MAX_STR_LEN] = {0};
    char szFinalResult[MAX_STR_LEN] = {0};
    int nFirstNumLen;
    int nSecondNumLen;
    int nTmpStrLen = 0;
    int nNum1 = 0;
    int nNum2 = 0;
    /* 存放进位和临时结果 */
    int carry = 0;
    int nTempResult = 0;
    int nLen;
    int i;
    int j = 0;

    /* 如果参数有一个为NULL，返回错误 */
    if (NULL == pszFirstNum || NULL == pszSecondNum || NULL == outputResult)
    {
        return ERROR;
    }

    nFirstNumLen = strlen(pszFirstNum);
    nSecondNumLen = strlen(pszSecondNum);

    /* 循环遍历一个数字字符串的每一位，从最后一位开始 */
    for (i = nFirstNumLen; i > 0; i--)
    {
        /* 获取对应位置的数字 */
        nNum1 = *(pszFirstNum + i - 1) - 48;
        /* 循环遍历另一个数字字符串的每一位，从最后一位开始 */
        for (j = nSecondNumLen; j > 0; j--)
        {
            /* 获取对应位置的数字 */
            nNum2 = *(pszSecondNum + j - 1) - 48;
            /* 将获取的数字相乘后与进位相加 */
            nTempResult = nNum1 * nNum2 + carry;
            /* 分别获取进位和尾数，并将尾数传递给临时字符串 */
            carry = nTempResult / 10;
            nTempResult = nTempResult % 10;
            *(szTmpResult + nSecondNumLen - j) = nTempResult + 48;
        }
        /* 如果遍历完成后，进位仍不为0，则将进位传递给临时字符串 */
        if (0 != carry)
        {
            *(szTmpResult + nSecondNumLen) = carry + 48;
            carry = 0;
        }
        /* 获取临时字符串的长度，将其按反序复制处理，传递给单次执行的临时字符串 */
        nTmpStrLen = strlen(szTmpResult);
        for (j = 0; j < nTmpStrLen; j++)
        {
            szOneTimeResult[j] = szTmpResult[nTmpStrLen - j - 1];
        }
        /* 给每次结果的低位添0，以供乘后的字符串对应位相加 */
        for (j = 0; j < nFirstNumLen - i; j++)
        {
            (void)strncat_s(szOneTimeResult, MAX_STR_LEN, "0", 2);
        }
        /* 将单次执行结果与最终相乘结果变量累加 */
        (void)strAdd(szOneTimeResult, szFinalResult, szFinalResult);
        /* 将临时字符串和单次结果字符串置零 */
        memset_s(szTmpResult, MAX_STR_LEN, 0, sizeof(szTmpResult));
        memset_s(szOneTimeResult, MAX_STR_LEN, 0, sizeof(szOneTimeResult));
    }

    /* 当整个字符串遍历完成，即计算结束，将最终结果传递给出参 */
    nLen = strlen(szFinalResult);
    /* 当首字符为0时，说明结果为0 */
    if ('0' == szFinalResult[0])
    {
        nLen = 1;
    }
    (void)strncpy_s(outputResult, nLen + 1, szFinalResult, nLen);
    outputResult[nLen] = '\0';

    return SUCC;
}

/*****************************************************************************
 Function   : unitTransfer()
 Description: 单位转换函数，将字节和KB转换为MB，使用的是进位法
 Input      : sourceStr      入参，字节大小/KB大小
              level          入参，单位换算的比例
 Output     : targetStr      出参，用于存储计算结果
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int unitTransfer(char *sourceStr, unsigned long level, char *targetStr)
{
    char *pszFirstNum = sourceStr;
    char szResult[MAX_STR_LEN] = {0};
    unsigned long nCycle = level;
    int nNumLen;
    unsigned long tmpNum = 0;
    /* 将两数的商初始设置为0 */
    int divide = 0;
    /* 余数初始设置为0 */
    unsigned long remainder = 0;
    int nLen;
    int i;
    /* 存放商的字符串的下标 */
    int j = 0;

    /* 判断入参字符串是否为NULL或者除数为0，如果是则返回错误 */
    if (NULL == pszFirstNum || NULL == targetStr || 0 == nCycle)
    {
        return ERROR;
    }

    nNumLen = strlen(pszFirstNum);

    /* 循环读取每一位的数据 */
    for (i = 0; i < nNumLen; i++)
    {
        /* 按位获取被除数字符串的数值 */
        tmpNum = remainder * 10 + (pszFirstNum[i] - 48);
        /* 判断临时数值是否大于除数，且商的字符串是否为空，如果小于且为空，则将临时字符串赋值给余数 */
        if(tmpNum < nCycle && 0 == strlen(szResult))
        {
            remainder = tmpNum;
        }
        else
        {
            /* 如果临时数据大于除数，则开始正式的计算，商等于临时数据除以除数，余数等于临时数据与除数的模 */
            divide = tmpNum / nCycle;
            remainder = tmpNum % nCycle;
            /* 将商赋值给对应的结果变量 */
            *(szResult + j) = divide + 48;
            j++;
        }
    }
    /* 如果整个被除数字符串全部获取完毕，没有大于除数，则将结果变量设置为0 */
    if(0 == j)
    {
        *(szResult + j) = divide + 48;
    }
    /* 如果执行完成之后，余数不为0，则进位(给结果+1) */
    if (0 != remainder)
    {
        (void)strAdd(szResult, "1", szResult);
    }

    /* 将最终结果字符串复制给出参 */
    nLen = strlen(szResult);
    if (MAX_STR_LEN <= nLen)
    {
        nLen = MAX_STR_LEN - 1;
    }
    (void)strncpy_s(targetStr, nLen + 1, szResult, nLen);
    targetStr[nLen] = '\0';

    return SUCC;
}

/*****************************************************************************
 Function   : openPipe()
 Description: 对popen函数进行封装，便于UT
 Input      : command        popen的入参，要执行的命令
 Output     : type           popen的入参，类型模式
 Return     : FILE*          文件指针
 Other      : N/A
 *****************************************************************************/
FILE *openPipe(const char *pszCommand, const char *pszType)
{
    return popen(pszCommand, pszType);
}

/*****************************************************************************
 Function   : openFile()
 Description: 对fopen函数进行封装，便于UT
 Input      : filepath       fopen的入参，文件的路径
 Output     : type           fopen的入参，类型模式
 Return     : FILE*          文件指针
 Other      : N/A
 *****************************************************************************/
FILE *openFile(const char *pszFilePath, const char *pszType)
{
    return fopen(pszFilePath, pszType);
}

/*****************************************************************************
 Function   : getDeviceMapperNumber()
 Description: 获得Device-Mapper类型的主设备号
 Input      : N/A
 Output     : N/A
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int getDeviceMapperNumber()
{
    FILE *fpDevices;
    char szLine[MAX_STR_LEN] = {0};
    const char *pszDeviceMapperName = "device-mapper";
    char szMajor[MAX_NAME_LEN] = {0};
    char szDeviceType[MAX_STR_LEN] = {0};

    /* 打开/proc/devices文件 */
    fpDevices = openFile(PROC_DEVICES, "r");
    if (NULL == fpDevices)
    {
        return ERROR;
    }

    while (fgets(szLine, sizeof(szLine), fpDevices))
    {
        /* 获取设备类型和对应的主设备号 */
        if (sscanf_s(szLine, "%s %[^\n ]", szMajor, sizeof(szMajor), szDeviceType, sizeof(szDeviceType)) != 2)
        {
            continue;
        }
        /* 比对类型名是否为device-mapper，即逻辑卷类型 */
        if (0 == strcmp(szDeviceType, pszDeviceMapperName))
        {
            g_deviceMapperNum = atoi(szMajor);
            break;
        }
    }
    (void)fclose(fpDevices);
    //lint -save -e438
    fpDevices = NULL;

    return SUCC;
    //lint -save -e438
}

/*****************************************************************************
 Function   : freePath()
 Description: 释放三个指针申请到的空间
 Input      : char *path1
              char *path2
              char *path3
 Output     : N/A
 Return     : void
 Other      : N/A
 *****************************************************************************/
void freePath(char *path1, char *path2, char *path3)
{
    if (path1 != NULL)
    {
        free(path1);
    }

    if (path2 != NULL)
    {
        free(path2);
    }

    if (path3 != NULL)
    {
        free(path3);
    }
}

/*****************************************************************************
 Function   : startsWith()
 Description: 判断字符串str是否以字符串prefix起始
 Input      : str           入参
              prefix        入参
 Output     : N/A
 Return     : 1:    字符串str以字符串prefix起始
              0:    字符串str不以字符串prefix起始
 Other      : N/A
 History    : 2013.09.02, h00227765 created this function.
 *****************************************************************************/
static int startsWith(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

/*****************************************************************************
 Function   : getScsiBackendPath()
 Description: 获得对应后端xenstore scsi设备的路径
 Input      : struct xs_handle* handle      handle of xenstore
              dir                           前端xenstore目录名
 Output     : N/A
 Return     : 非NULL = success, NULL = failure
 Other      : N/A
 History    : 2013.09.02, h00227765 created this function.
 *****************************************************************************/
static char* getScsiBackendPath(struct xs_handle *handle, char *dir)
{
    char    szXsPath[MAX_STR_LEN];
    int     ret;

    /* 如：读取device/vscsi/2048/backend获得后端xenstore路径 */
    ret = snprintf_s(szXsPath, sizeof(szXsPath), sizeof(szXsPath), "%s/%s/%s", SCSI_DISK_PATH, dir, "backend");
    if (ret < 0)
    {
        return NULL;
    }

    return read_from_xenstore(handle, szXsPath);
}

/*****************************************************************************
 Function   : getScsiBackendParams()
 Description: 获得对应后端xenstore scsi设备路径的params参数
 Input      : struct xs_handle* handle      handle of xenstore
              bePath                        后端xenstore路径
 Output     : N/A
 Return     : 非NULL = success, NULL = failure
 Other      : N/A
 History    : 2013.09.02, h00227765 created this function.
 *****************************************************************************/
static char* getScsiBackendParams(struct xs_handle *handle, char *bePath)
{
    char    szXsPath[MAX_STR_LEN];
    int     ret;

    /* 如：读取后端键值/local/domain/0/backend/vbd/%d/2048/params获取设备id */
    ret = snprintf_s(szXsPath, sizeof(szXsPath), sizeof(szXsPath), "%s/%s", bePath,  "params");
    if (ret < 0)
    {
        return NULL;
    }

    return read_from_xenstore(handle, szXsPath);
}

/*****************************************************************************
 Function   : getScsiBackendName()
 Description: 获得对应后端xenstore scsi设备路径的设备名，即XML设备名
 Input      : struct xs_handle* handle      handle of xenstore
              bePath                        后端xenstore路径
 Output     : N/A
 Return     : 非NULL = success, NULL = failure
 Other      : N/A
 History    : 2013.09.02, h00227765 created this function.
 *****************************************************************************/
static char* getScsiBackendName(struct xs_handle *handle, char *bePath)
{
    char    szXsPath[MAX_STR_LEN];
    int     ret;

    /* 如：读取后端键值/local/domain/0/backend/vbd/%d/2048/dev获取xml配置中设备名 */
    ret = snprintf_s(szXsPath, sizeof(szXsPath), sizeof(szXsPath), "%s/%s", bePath, "dev");
    if (ret < 0)
    {
        return NULL;
    }

    return read_from_xenstore(handle, szXsPath);
}

/*****************************************************************************
 Function   : getScsiDiskXmlName()
 Description: 通过后端xenstore设备名, 获得虚拟机scsi磁盘设备在xml配置中的设备名
 Input      : struct xs_handle *handle      handle of xenstore
              char *d_name                  虚拟机内设备名，如sda
 Output     : N/A
 Return     : 非NULL = success, NULL = failure
 Other      : N/A
 *****************************************************************************/
static char* getScsiDiskXmlName(struct xs_handle *handle, const char *d_name)
{
    unsigned int    i;
    unsigned int    num;
    char            **xs_dir;
    char            *fptr;
    char            *bePath = NULL;
    char            *params = NULL;
    char            *devname = NULL;

    /* 获取虚拟机所有scsi磁盘设备号及scsi盘个数 */
    xs_dir = xs_directory(handle, 0, SCSI_DISK_PATH, &num);
    if (xs_dir == NULL)
    {
        return NULL;
    }

    for (i = 0; i < num; i++)
    {
        /* 如：读取device/vscsi/2048/backend获得后端xenstore路径 */
        bePath = getScsiBackendPath(handle, xs_dir[i]);
        if (bePath == NULL)
        {
            free(xs_dir);
            return NULL;
        }

        /* 如：读取后端键值/local/domain/0/backend/vbd/%d/2048/params获取设备id */
        params = getScsiBackendParams(handle, bePath);
        if (params == NULL)
        {
            free(xs_dir);
            freePath(bePath, NULL, NULL);
            return NULL;
        }

        /* 如：读取后端键值/local/domain/0/backend/vbd/%d/2048/dev获取xml配置中设备名 */
        fptr = params + strlen(DEVPATH);
        if (0 == strcmp(fptr, d_name))
        {
            devname = getScsiBackendName(handle, bePath);

            free(xs_dir);
            freePath(bePath, params, NULL);
            return devname;
        }

        freePath(bePath, params, devname);
    }

    free(xs_dir);
    return NULL;
}

/*****************************************************************************
 Function   : isCorrectDev()
 Description: /dev/disk/by-id下的文件是否就是我们要找的设备
 Input      : ptName                待找的设备名
              entry                 /dev/disk/by-id下的一个文件
 Output     : N/A
 Return     : 1:                    是待找的设备
              0:                    不是待找的设备
 Other      : N/A
 History    : 2013.09.02, h00227765 created this function.
 *****************************************************************************/
static int isCorrectDev(const char *ptName, struct dirent *entry)
{
    char    fullPath[MAX_STR_LEN];
    char    actualPath[MAX_STR_LEN];
    ssize_t len;

    /* 若目录名为".、..、wwn-"则跳过 */
    if (startsWith(entry->d_name, ".") ||
        startsWith(entry->d_name, "..") ||
        startsWith(entry->d_name, "wwn-"))
    {
        return 0;
    }

    snprintf_s(fullPath, sizeof(fullPath), sizeof(fullPath), "%s%s", DEVPATH, entry->d_name);
    /*
     * 获取磁盘id的软连接，以得到磁盘名，如"../../sda"
     * readlink() does not append a null byte to buf.
     */
    len = readlink(fullPath, actualPath, sizeof(actualPath) - 1);
    if (-1 == len)
    {
        return 0;
    }
    actualPath[len] = '\0'; /* On success, readlink() returns the number of bytes placed in buf. */

    /* 跳过前面的"../../"获得磁盘名字符串(例如sda)后进行比较sda */
    if (0 != strcmp(ptName, actualPath + strlen(CMPSTRING)))
    {
        return 0;
    }

    return 1;
}

/*****************************************************************************
 Function   : getXmlDevName()
 Description: 给定虚拟机内scsi磁盘名，获取XML设备名
 Input      : handle                handle of xenstore
              ptName                待查找XML设备名的虚拟机内分区名
 Output     : N/A
 Return     : XML设备名，用完后要释放
              非NULL = success, NULL = failure
 Other      : N/A
 History    : 2013.09.02, h00227765 created this function.
 *****************************************************************************/
static char* getXmlDevName(struct xs_handle *handle, const char *ptName)
{
    char            *xmlDevName = NULL;
    DIR             *dir;
    struct dirent   entry;
    struct dirent   *result = NULL;
    int             ret;

    dir = opendir(DEVPATH);
    if (NULL == dir)
    {
        return NULL;
    }

    /* 遍历/dev/disk/by-id目录下的所有文件 */
    for (ret = readdir_r(dir, &entry, &result); (ret == 0) && (result != NULL); ret = readdir_r(dir, &entry, &result))
    {
        if (isCorrectDev(ptName, &entry))
        {
            xmlDevName = getScsiDiskXmlName(handle, entry.d_name);
            if (xmlDevName)
                break;
        }
    }

    (void)closedir(dir);
    return xmlDevName;
}

/*****************************************************************************
 Function   : getPartitionsInfo()
 Description: 获取/proc/partition中设备以及设备分区的信息
 Input      : struct xs_handle* handle   handle of xenstore
 Output     : struct DevMajorMinor* devMajorMinor    设备名称及其主次设备号的结构体数组
              int* pnPartNum                         分区数量
              struct DeviceInfo* diskUsage           物理磁盘名称及其使用空间、总空间的结构体数组
              int* pnDiskNum                         磁盘数量
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int getPartitionsInfo(struct xs_handle *handle,
                             struct DevMajorMinor *devMajorMinor,
                             int *pnPartNum,
                             struct DeviceInfo *diskUsage,
                             int *pnDiskNum)
{
    FILE    *fpProcPt;
    char    szLine[MAX_STR_LEN];
    char    szPtName[MAX_NAME_LEN];
    int     nMajor = 0;
    int     nMinor = 0;
    char    szSize[MAX_STR_LEN];
    int     nPartitionNum = 0;
    int     nDiskNum = 0;
    int     nPtNameLen;
    char    *xmlDevName;
    struct DevMajorMinor    *tmpDev = NULL;
    struct DeviceInfo       *tmpUsage = NULL;

    /* 打开/proc/partitions文件 */
    fpProcPt = openFile(PROC_PARTITIONS, "r");
    if (NULL == fpProcPt)
    {
        return ERROR;
    }

    /* 循环获取每一行的信息 */
    while (fgets(szLine, sizeof(szLine), fpProcPt))
    {
        /* 格式化读取对应的数据 */
        if (sscanf_s(szLine, " %d %d %s %[^\n ]", &nMajor, &nMinor, szSize, sizeof(szSize), szPtName, sizeof(szPtName)) != 4)
        {
            continue;
        }
        /* 过滤loop、ram和dm等设备的信息 */
        if (LOOP_MAJOR == nMajor || RAM_MAJOR == nMajor)
        {
            continue;
        }
        /* 保存全部的数据，后续当找到dmsetup分区的设备时，需要寻找对应父设备名 */
        tmpDev = devMajorMinor + nPartitionNum;
        tmpDev->devMajor = nMajor;
        tmpDev->devMinor = nMinor;

        szSize[MAX_STR_LEN - 1] = '\0';
        szPtName[MAX_NAME_LEN - 1] = '\0';
        (void)strncpy_s(tmpDev->devBlockSize, MAX_STR_LEN, szSize, strlen(szSize));
        (void)strncpy_s(tmpDev->devName, MAX_NAME_LEN, szPtName, strlen(szPtName));
        nPartitionNum++;
        /* 目前UVP仅支持挂载xvd*，hd*，sd*类型的设备 */
        if ('h' != szPtName[0] && 's' != szPtName[0] && 'x' != szPtName[0])
        {
            continue;
        }

        /* 目前UVP不支持挂载带数字的设备类型，所以获取szPtName中的最后一个字符，查看是否为数字，如果是，说明非磁盘*/
        nPtNameLen = strlen(szPtName);
        if (szPtName[nPtNameLen - 1] >= '0' && szPtName[nPtNameLen - 1] <= '9')
        {
            continue;
        }

        tmpUsage = diskUsage + nDiskNum;

        /*
         * 将信息传递磁盘信息的结构体中。
         * scsi设备名修改为xml配置文件中设备名(target dev)；若不是scsi设备则仍以虚拟机内部显示名称为准。
         */
        if ( 's' == szPtName[0])
        {
            xmlDevName = getXmlDevName(handle, szPtName);
            if (NULL == xmlDevName)
            {
                continue;
            }
            (void)strncpy_s(tmpUsage->phyDevName, sizeof(tmpUsage->phyDevName), xmlDevName, sizeof(tmpUsage->phyDevName)-1);
            free(xmlDevName);
        }
        else
        {
            (void)strncpy_s(tmpUsage->phyDevName, sizeof(tmpUsage->phyDevName), szPtName, sizeof(tmpUsage->phyDevName)-1);
        }
        tmpUsage->phyDevName[MAX_NAME_LEN - 1] = '\0';

        /* 初始化使用信息为0 */
        (void)strncpy_s(tmpUsage->diskUsage, MAX_DISKUSAGE_LEN, "0", 2);
        (void)unitTransfer(szSize, UNIT_TRANSFER_CYCLE, tmpUsage->deviceTotalSpace);
        nDiskNum++;
    }

    *pnPartNum = nPartitionNum;
    *pnDiskNum = nDiskNum;

    (void)fclose(fpProcPt);
    return SUCC;
}

/*****************************************************************************
 Function   : getSwapInfo()
 Description: 获取swap分区的文件系统名字以及对应的空间大小
 Input      : N/A
 Output     : struct DiskInfo* diskMap           swap分区信息及其使用空间
              int* pnMountNum                    swap分区的数量
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int getSwapInfo(struct DiskInfo *diskMap, int *pnMountNum)
{
    FILE *fpProcSwap;
    int nFsSize = 0;
    int nSwapPartitionNum = *pnMountNum;
    char szLine[MAX_STR_LEN] = {0};
    char szSwapName[MAX_STR_LEN] = {0};
    struct DiskInfo *tmpDisk = NULL;

    /* 打开/proc/swaps文件 */
    fpProcSwap = openFile(PROC_SWAPS, "r");
    if (NULL == fpProcSwap)
    {
        return ERROR;
    }
    /* 在diskMap下添加swap分区信息 */
    while (fgets(szLine, sizeof(szLine), fpProcSwap))
    {
        if (sscanf_s(szLine, "%s %*s %d %*[^\n ]", szSwapName, sizeof(szSwapName), &nFsSize) != 2)
        {
            continue;
        }
        tmpDisk = diskMap + nSwapPartitionNum;

        szSwapName[MAX_STR_LEN - 1] = '\0';
        (void)strncpy_s(tmpDisk->filesystem, MAX_STR_LEN, szSwapName, strlen(szSwapName));
        (void)snprintf_s(tmpDisk->usage, sizeof(tmpDisk->usage), sizeof(tmpDisk->usage), "%d", nFsSize / UNIT_TRANSFER_CYCLE);
        nSwapPartitionNum++;
    }
    *pnMountNum = nSwapPartitionNum;
    (void)fclose(fpProcSwap);
    //lint -save -e438
    fpProcSwap = NULL;

    return SUCC;
    //lint -restore
}

/*****************************************************************************
 Function   : getDrbdParent()
 Description: 根据drbd设备名，获取对应的父节点名
 Input      : char* drbdName                     drbd设备名
 Output     : char* parentName                   父节点名
 Return     : 0
 Other      : N/A
 *****************************************************************************/
int getDrbdParent(char *drbdName, char *parentName)
{
    char szCmd[MAX_STR_LEN] = {0};
    FILE *fpDmCmd;
    char szLine[MAX_STR_LEN] = {0};
    char tmpParentName[MAX_NAME_LEN] = {0};

    if(SUCC != CheckName(drbdName))
    {
        return ERROR;
    }

    /* 根据当前drbd设备名，串联drbdsetup命令，获取父设备 */
    (void)snprintf_s(szCmd, sizeof(szCmd), sizeof(szCmd), "drbdsetup \"%s$\" show |grep /dev", drbdName);
    fpDmCmd = openPipe(szCmd, "r");
    if (NULL == fpDmCmd)
    {
        return 0;
    }

    /* 读取命令执行结果的文件指针 */
    if (NULL != fgets(szLine, sizeof(szLine), fpDmCmd))
    {
        /* 根据drbdsetup命令结果，获取父设备名称 */
        if (sscanf_s(szLine, "%*[^\"]\"%[^\"]", tmpParentName, sizeof(tmpParentName)) != 1)
        {
            (void)pclose(fpDmCmd);
            return 0;
        }
    }

    (void)pclose(fpDmCmd);
    //lint -save -e438
    fpDmCmd = NULL;
    //lint -restore
    memset_s(parentName, strlen(parentName), 0, strlen(parentName));
    (void)strncpy_s(parentName, strlen(parentName) + 1, tmpParentName, strlen(tmpParentName));

    return 0;
}

/*****************************************************************************
 Function   : getMountInfo()
 Description: 获取/proc/mounts下实际文件系统以及其挂载点对应信息(去除重复项及tmp类)
 Input      : N/A
 Output     : struct DiskInfo mountInfo[]     挂载点对应文件系统信息，及其使用空间的结构体
              int* pnMountNum                 挂载点对应文件系统信息的个数
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int getMountInfo(struct DiskInfo *mountInfo, int *pnMountNum)
{
    FILE *fpProcMount;
    char szLine[MAX_STR_LEN] = {0};
    struct DiskInfo *tmpMountInfo;
    struct DiskInfo *firstInfo = NULL;
    struct DiskInfo *secondInfo = NULL;
    char szFilesystemName[MAX_NAME_LEN] = {0};
    char szMountPointName[MAX_STR_LEN] = {0};
    char szMountType[MAX_STR_LEN] = {0};
    int nTmpMountNum = *pnMountNum;
    int nMountNum = 0;
    int nFlag = 0;
    /* 判断挂载点是否为根目录标志位 */
    int nIsGetOneInfo = 0;
    int i;
    int j = 0;

    tmpMountInfo = (struct DiskInfo *)malloc(MAX_STR_LEN * sizeof(struct DiskInfo));
    if (NULL == tmpMountInfo)
    {
        return ERROR;
    }
    memset_s(tmpMountInfo, MAX_STR_LEN * sizeof(struct DiskInfo), 0, MAX_STR_LEN * sizeof(struct DiskInfo));

    /* 打开/proc/mounts文件 */
    fpProcMount = openFile(PROC_MOUNTS, "r");
    if (NULL == fpProcMount)
    {
        free(tmpMountInfo);
        //lint -save -e438
        tmpMountInfo = NULL;
        return ERROR;
        //lint -restore
    }

    for (i = 0; i < nTmpMountNum; i++)
    {
        firstInfo = tmpMountInfo + i;
        secondInfo = mountInfo + i;
        (void)strncpy_s(firstInfo->filesystem, MAX_STR_LEN, secondInfo->filesystem, strlen(secondInfo->filesystem));
    }

    /* 循环读取每一行数据 */
    while (fgets(szLine, sizeof(szLine), fpProcMount))
    {
        /* 格式化读取对应需要的数据 */
        if (sscanf_s(szLine, "%s %s %s %*[^\n ]", 
                    szFilesystemName, sizeof(szFilesystemName), 
                    szMountPointName, sizeof(szMountPointName),
                    szMountType, sizeof(szMountType)) != 3)
        {
            continue;
        }

        /* 去除非/dev/开头的文件系统，即tmpfs等；去除loop设备；去除重复挂载到/目录的后面几个文件系统；
         * 去除cdrom的挂载点，即挂载点的挂载类型是ramfs或者iso9660
         */
        if (0 != strncmp(szFilesystemName, "/dev/", 5) || 0 == strncmp(szFilesystemName, "/dev/loop", 9)
                || (1 == nIsGetOneInfo && 0 == strcmp(szMountPointName, "/"))
                || 5 == strspn(szMountType , "ramfs") || 7 == strspn(szMountType , "iso9660"))
        {
            continue;
        }

        /* 处理挂载点被重复挂载问题 */
        for (i = 0; i < nTmpMountNum; i++)
        {
            firstInfo = tmpMountInfo + i;
            /* 查询在tmpMountInfo中是否存在该挂载点对应的信息，如果已经存在，则更新tmpMountInfo中该挂载点对应的信息 */
            if (0 == strcmp(szMountPointName, firstInfo->mountPoint))
            {
                memset_s(firstInfo->filesystem, MAX_STR_LEN, 0, strlen(firstInfo->filesystem));
                (void)strncpy_s(firstInfo->filesystem, MAX_STR_LEN, szFilesystemName, strlen(szFilesystemName));
                nFlag = 1;
                break;
            }
        }
        if (0 == nFlag)
        {
            firstInfo = tmpMountInfo + nTmpMountNum;
            /* 如果以上判定条件均通过，则说明该行信息为需要添加的一条新记录，将其添加到对应结构体中，并将数据计数累加 */
            strncpy_s(firstInfo->filesystem, MAX_STR_LEN, szFilesystemName, strlen(szFilesystemName));
            strncpy_s(firstInfo->mountPoint, MAX_STR_LEN, szMountPointName, strlen(szMountPointName));
            nTmpMountNum++;
        }
        nFlag = 0;
        nIsGetOneInfo = 1;
    }

    /* 处理文件系统重复挂载问题 */
    for (i = 0; i < nTmpMountNum; i++)
    {
        firstInfo = tmpMountInfo + i;

        for (j = 0; j < nMountNum; j++)
        {
            /* 查询在mountInfo中是否存在该文件系统对应的信息，如果已经存在，则不增加该条记录 */
            if (0 == strcmp(firstInfo->filesystem, (mountInfo + j)->filesystem))
            {
                nFlag = 1;
                break;
            }
        }
        if (1 == nFlag)
        {
            nFlag = 0;
            continue;
        }
        secondInfo = mountInfo + nMountNum;
        /* 如果mountInfo中没有该文件系统对应信息，则添加一条新记录，数据计数累加 */
        strncpy_s(secondInfo->filesystem, MAX_STR_LEN, firstInfo->filesystem, strlen(firstInfo->filesystem));
        strncpy_s(secondInfo->mountPoint, MAX_STR_LEN, firstInfo->mountPoint, strlen(firstInfo->mountPoint));
        nMountNum++;
    }

    *pnMountNum = nMountNum;
    (void)fclose(fpProcMount);
    //lint -save -e438
    fpProcMount = NULL;

    free(tmpMountInfo);
    tmpMountInfo = NULL;

    return SUCC;
    //lint -restore
}

/*****************************************************************************
 Function   : getInfoFromID()
 Description: 根据设备的主次设备号，获取对应的设备名字，以及设备大小
 Input      : struct DevMajorMinor* devMajorMinor    设备名称及其主次设备号结构体数组
              int partNum                            分区数量
              int devID                              设备的主次设备号
 Output     : char* devName                          父节点名
              char* devBlockSize                     父节点空间大小
 Return     : 0
 Other      : N/A
 *****************************************************************************/
int getInfoFromID(struct DevMajorMinor *devMajorMinor, int partNum, int devID, char *devName, char *devBlockSize)
{
    int i;
    char szDevName[MAX_STR_LEN] = {0};
    char szBlockSize[MAX_STR_LEN] = {0};
    int nMajor = major(devID);
    int nMinor = minor(devID);
    struct DevMajorMinor *tmpDev = NULL;

    for(i = 0; i < partNum; i++)
    {
        tmpDev = devMajorMinor + i;
        /* 如果主次设备号都与/proc/partitions里面的一样，则说明是同一个设备，将设备名传递给出参 */
        if (nMajor == tmpDev->devMajor && nMinor == tmpDev->devMinor)
        {
            (void)strncpy_s(szDevName, MAX_STR_LEN, tmpDev->devName, strlen(tmpDev->devName));
            (void)strncpy_s(szBlockSize, MAX_STR_LEN, tmpDev->devBlockSize, strlen(tmpDev->devBlockSize));
            /* 如果出参devName不为NULL，则将主次设备号对应的设备名传递给出参 */
            if (NULL != devName && 0 != strlen(szDevName))
            {
                (void)strncpy_s(devName, strlen(szDevName)+1, szDevName, strlen(szDevName));
            }
            /* 如果出参devBlockSize不为NULL，则将主次设备号对应设备的实际空间大小传递给出参 */
            if (NULL != devBlockSize && 0 != strlen(szBlockSize))
            {
                (void)strncpy_s(devBlockSize, strlen(szBlockSize)+1, szBlockSize, strlen(szBlockSize));
            }
            break;
        }
    }

    return 0;
}

/*****************************************************************************
 Function   : getDiskInfo()
 Description: 获取df中对应的信息，存储到结构体中
 Input      : struct DevMajorMinor* devMajorMinor     设备名称及其主次设备号结构体数组
              int partNum                             分区数量
 Output     : struct DiskInfo* diskMap                挂载点对应文件系统信息，及其使用空间的结构体
              int* pnMountNum                         用于存储挂载磁盘数量
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int getDiskInfo(struct DevMajorMinor *devMajorMinor, int partNum, struct DiskInfo *diskMap, int *pnMountNum)
{
    /* 由于之前在调用函数中，先获取了swap分区的数量，因此先将swap分区数量赋值给挂载点数量变量 */
    int nMountedFsNum = *pnMountNum;
    int nResult;
    struct statfs statfsInfo;
    struct stat statBuf;
    char szFreeSize[MAX_STR_LEN] = {0};
    char szFsFreeBlock[MAX_STR_LEN] = {0};
    char szFsBlockSize[MAX_STR_LEN] = {0};
    char szFsSize[MAX_STR_LEN] = {0};
    int i;
    struct DiskInfo *tmpDisk = NULL;
    char *substr = NULL;
    char tmp_mountPoint[MAX_STR_LEN] = {0};
    int offset = 0;
    int len = 0;

    /* 调用getMountInfo函数，获取挂载的文件系统信息及数量(在swap分区之后累加) */
    nResult = getMountInfo(diskMap, &nMountedFsNum);
    if (ERROR == nResult)
    {
        return ERROR;
    }

    /* 循环遍历整个被挂载文件系统信息 */
    for (i = 0; i < nMountedFsNum; i++)
    {
        tmpDisk = diskMap + i;
        /* 通过stat函数，获取文件系统对应主次设备号信息，保存到结构体对应成员中 */
        nResult = stat(tmpDisk->filesystem, &statBuf);
        if (0 != nResult || 0 == statBuf.st_rdev)
        {
            continue;
        }

        /* 当分区是swap分区时，不进行通过挂载点获取分区使用信息操作 */
        if (*pnMountNum <= i)
        {
            /* 通过statfs函数，根据文件系统挂载点，获取文件系统的相关信息 */
            if (statfs(tmpDisk->mountPoint, &statfsInfo) != 0)
            {
                if (ENOENT != errno)
                {
                    return ERROR;
                }
                /* should retry, such as redhat-6.1 at GUI mode, USB DISK shown as /media/STEC\040DISK, need to replace \040 with ' '*/
                substr = strstr(tmpDisk->mountPoint, "\\040");
                if (!substr)
                {
                    return ERROR;
                }

                (void)memset_s(tmp_mountPoint, MAX_STR_LEN, 0 ,MAX_STR_LEN);
                offset = substr - tmpDisk->mountPoint;
                len = offset + strlen("\\040");

                (void)memcpy_s(tmp_mountPoint, MAX_STR_LEN, tmpDisk->mountPoint, offset);
                (void)memset_s(tmp_mountPoint + offset, MAX_STR_LEN - strlen("\\040"), '\040', 1);
                (void)memcpy_s(tmp_mountPoint + offset + 1 , MAX_STR_LEN - offset - 1, tmpDisk->mountPoint + len, strlen(tmpDisk->mountPoint) - len);

                if (statfs(tmp_mountPoint, &statfsInfo) != 0)
                {
                    return ERROR;
                }
            }
            /* 如果总块数大于0，并且mount信息非空，则挂载点信息文件指针正常，执行以下操作 */
            if (statfsInfo.f_blocks > 0)
            {
                /* 将每块的大小赋值给szFsBlockSize，将文件系统赋值给结构体数组的对应成员 */
                (void)snprintf_s(szFsBlockSize, sizeof(szFsBlockSize), sizeof(szFsBlockSize), "%ld", (long)statfsInfo.f_bsize);
                /* 剩余free的块数大小赋值给对应的变量 */
                (void)snprintf_s(szFsFreeBlock, sizeof(szFsFreeBlock), sizeof(szFsFreeBlock), "%lu", statfsInfo.f_bfree);
                /* 使用数字型字符串乘法函数对剩余大小进行计算，并转换为KB */
                (void)strMulti(szFsFreeBlock, szFsBlockSize, szFreeSize);
                (void)unitTransfer(szFreeSize, UNIT_TRANSFER_CYCLE, szFreeSize);
            }
        }

        tmpDisk->st_rdev = statBuf.st_rdev;
        /* 通过getInfoFromID函数，根据文件系统的主次设备号，获取对应分区的实际空间大小信息 */
        (void)getInfoFromID(devMajorMinor, partNum, statBuf.st_rdev, NULL, szFsSize);
        /* 将获取出来的文件系统实际空间减去文件系统剩余可用空间，计算文件系统使用空间(为了计算文件系统格式占用空间) */
        nResult = strMinus(szFsSize, szFreeSize, tmpDisk->usage);
        if (ERROR == nResult)
        {
            return ERROR;
        }
        /* 进行单位转换后，存入结构体相应成员中 */
        nResult = unitTransfer(tmpDisk->usage, UNIT_TRANSFER_CYCLE, tmpDisk->usage);
        if (ERROR == nResult)
        {
            return ERROR;
        }

        /* 将一些临时的字符串信息全部清空 */
        memset_s(szFsBlockSize, MAX_STR_LEN, 0, sizeof(szFsBlockSize));
        memset_s(szFreeSize, MAX_STR_LEN, 0, sizeof(szFreeSize));
        memset_s(szFsFreeBlock, MAX_STR_LEN, 0, sizeof(szFsFreeBlock));
        memset_s(szFsSize, MAX_STR_LEN, 0, sizeof(szFsSize));
    }
    *pnMountNum = nMountedFsNum;

    return SUCC;
}

/*****************************************************************************
 Function   : getDiskNameFromPartName()
 Description: 把分区名称中的字母和数字分开，留下字母即为磁盘名称
 Input       : char *pPartName    分区名称
 Output     : char* pDiskName   分区所在的磁盘名称
 Return     : SUCC = success, ERROR = failure
 Other      :  当LVM建立的PV 为整块磁盘时，这里的pPartName就是pDiskName
 *****************************************************************************/
int getDiskNameFromPartName(const char *pPartName, char *pDiskName)
{
    int i = 0;
    int j = 0;

    if (NULL == pPartName || 0 == strlen(pPartName) || NULL == pDiskName)
    {
        return ERROR;
    }

    while(*(pPartName + i) != '\0')
    {
        if(*(pPartName + i) <= 'z' && *(pPartName + i) >= 'A')
        {
            pDiskName[j] = *(pPartName + i);
            j++;
        }
        i++;
    }

    if(0 == i)
    {
        return ERROR;
    }
    else
    {
        return SUCC;
    }
}

/*****************************************************************************
 Function   : addDiskUsageToDevice()
 Description: 根据分区的真实名字，对比磁盘，如果信息一致，则累加使用空间信息至对应磁盘
 Input      : struct DeviceInfo* linuxDiskUsage       物理磁盘名称及其使用空间、总空间的结构体数组
              int diskNum                             物理磁盘数量
              char* partitionsName                    分区名，用于比对是否与物理磁盘名一致
              char* partitionsUsage                   分区使用信息，用于累加到磁盘空间
 Output     : struct DeviceInfo linuxDiskUsage
 Return     : 0
 Other      : N/A
 *****************************************************************************/
int addDiskUsageToDevice(struct DeviceInfo *linuxDiskUsage,
                         int diskNum,
                         const char *partitionsName,
                         char *partitionsUsage)
{
    char szTmpPartition[MAX_NAME_LEN] = {0};
    int i;
    int nPartNameLen = strlen(partitionsName);
    struct DeviceInfo *tmpUsage = NULL;

    for (i = 0; i < nPartNameLen; i++)
    {
        if (partitionsName[i] >= '0' && partitionsName[i] <= '9')
        {
            break;
        }
        szTmpPartition[i] = partitionsName[i];
    }

    for (i = 0; i < diskNum; i++)
    {
        tmpUsage = linuxDiskUsage + i;
        /* 通过对比分区父设备名(除末尾数字外)与磁盘名是否一样，如果一样，则将分区使用信息累加到磁盘使用空间中 */
        if (0 == strcmp(szTmpPartition, tmpUsage->phyDevName))
        {
            (void)strAdd(tmpUsage->diskUsage, partitionsUsage, tmpUsage->diskUsage);
            break;
        }
    }

    return 0;
}

/*****************************************************************************
 Function   : getDmType()
 Description: 获取该文件系统的类型
 Input      : char *szLine                              dmsetup读取内容
 Output     : char *pParentMajor                        父节点主设备号
              char *pParentMinor                        父节点从设备号
              int *pStripCnt                            父节点个数
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 History    : 2016.11.11
 *****************************************************************************/
int getDmType(int devID, int *pDmType)
{
    char szCmd[64] = {0};
    FILE *fpDmCmd = NULL;
    char szLine[MAX_STR_LEN] = {0};

    (void)snprintf_s(szCmd, sizeof(szCmd), sizeof(szCmd), "dmsetup table -j %d -m %d", major(devID), minor(devID));
    fpDmCmd = openPipe(szCmd, "r");
    if (NULL == fpDmCmd)
    {
        printf("openPipe error \n");
        return ERROR;
    }

    /* 循环读取命令执行结果的文件指针 */
    if(NULL == fgets(szLine, sizeof(szLine), fpDmCmd))
    {
        (void)pclose(fpDmCmd);
        fpDmCmd = NULL;
        return ERROR;
    }

    if (strstr(szLine, "striped") != NULL)
    {
        *pDmType = DM_STRIPED;
    }
    else if (strstr(szLine, "crypt") != NULL)
    {
        *pDmType = DM_CRYPT;
    }
    else if (strstr(szLine, "linear") != NULL)
    {
        *pDmType = DM_LINEAR;
    }
    else if (strstr(szLine, "snapshot") != NULL)
    {
        *pDmType = DM_SNAPSHOT;
    }
    else if (strstr(szLine, "raid") != NULL)
    {
        *pDmType = DM_RAID;
    }
    else if (strstr(szLine, "mirror") != NULL)
    {
        *pDmType = DM_MIRROR;
    }
    else
    {
        *pDmType = DM_UNKNOWN;
    }

    (void)pclose(fpDmCmd);
    fpDmCmd = NULL;
    return SUCC;
}

/*****************************************************************************
 Function   : getDmStripParentNode()
 Description: strip情况下，获取该文件系统的父节点名
 Input      : char *szLine                              dmsetup读取内容
 Output     : char *pParentMajor                        父节点主设备号
              char *pParentMinor                        父节点从设备号
              int *pStripCnt                            父节点个数
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 History    : 2016.11.11
 *****************************************************************************/
int getDmStripParentNode(int devID, struct DmInfo **pstDmInfo, int *pStripCnt)
{
    char szCmd[64] = {0};
    FILE *fpDmCmd = NULL;
    char szLine[MAX_STR_LEN] = {0};
    int i = 0;
    char *tmp = NULL;
    struct DmInfo *pDmInfo = NULL;

    (void)snprintf_s(szCmd, sizeof(szCmd), sizeof(szCmd), "dmsetup table -j %d -m %d", major(devID), minor(devID));
    fpDmCmd = openPipe(szCmd, "r");
    if (NULL == fpDmCmd)
    {
        return ERROR;
    }

    if(NULL == fgets(szLine, sizeof(szLine), fpDmCmd))
    {
        (void)pclose(fpDmCmd);
        fpDmCmd = NULL;
        return ERROR;
    }
    (void)pclose(fpDmCmd);
    fpDmCmd = NULL;

    if (sscanf_s(szLine, "%*d %*d %*s %d %*[^\n ]", pStripCnt) != 1)
    {
        return ERROR;
    }

    if(*pStripCnt <= 0)
    {
        return ERROR;
    }

    pDmInfo = (struct DmInfo *)malloc((*pStripCnt) * sizeof(struct DmInfo));
    if (NULL == pDmInfo)
    {
        return ERROR;
    }
    (void)memset_s(pDmInfo, (*pStripCnt) * sizeof(struct DmInfo), 0, (*pStripCnt) * sizeof(struct DmInfo));

    /* 处理形如0 16777216 striped 2 128 202:160 8390656 202:176 8390656的字符串，跳过前面5个字符串 */
    tmp = szLine;
    while (i < 5)
    {
        tmp = strchr(tmp, ' ');
        tmp+=1;
        i++;
    }

    /* 开始处理202:160 8390656，取出父节点设备号 */
    i = 0;
    while (i < *pStripCnt)
    {
        if (sscanf_s(tmp, "%d:%d %*[^\n ]", &pDmInfo[i].nParentMajor, &pDmInfo[i].nParentMinor) != 2)
        {
            free(pDmInfo);
            return ERROR;
        }
        tmp = strchr(tmp, ' ');
        tmp+=1;
        tmp = strchr(tmp, ' ');
        tmp+=1;
        i++;
    }

    *pstDmInfo = pDmInfo;
    return SUCC;
}

/*****************************************************************************
 Function   : getDmLinearParentNodeCount()
 Description: linear情况下，获取该文件系统的父节点名
 Input      : struct DevMajorMinor* devMajorMinor       /proc/partitions中设备名及其主次设备号结构体数组
              int partNum                               对应关系数据数量
              int devID                                 文件系统设备号
 Output     : char* parentName                          父节点名
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 History    : 2016.11.11, created this function.
 *****************************************************************************/
int getDmLinearParentNode(int devID, struct DmInfo **pstDmInfo, int *pCnt)
{
    FILE *fpDmCmd = NULL;
    char szCmd[128] = {0};
    char szLine[MAX_STR_LEN] = {0};
    int i = 0;
    int nFlag = SUCC;
    struct DmInfo *pDmInfo = NULL;

    (void)snprintf_s(szCmd, sizeof(szCmd), sizeof(szCmd), "dmsetup table -j %d -m %d | wc -l", major(devID), minor(devID));
    fpDmCmd = openPipe(szCmd, "r");
    if (NULL == fpDmCmd)
    {
        return ERROR;
    }

    if (NULL == fgets(szLine, sizeof(szLine), fpDmCmd))
    {
        (void)pclose(fpDmCmd);
        fpDmCmd = NULL;
        return ERROR;
    }
    (void)pclose(fpDmCmd);

    if (sscanf_s(szLine, "%d %*[^\n ]", pCnt) != 1)
    {
        return ERROR;
    }

    pDmInfo = (struct DmInfo *)malloc((*pCnt) * sizeof(struct DmInfo));
    if (NULL == pDmInfo)
    {
        return ERROR;
    }
    (void)memset_s(pDmInfo, (*pCnt) * sizeof(struct DmInfo), 0, (*pCnt) * sizeof(struct DmInfo));

    (void)snprintf_s(szCmd, sizeof(szCmd), sizeof(szCmd), "dmsetup table -j %d -m %d", major(devID), minor(devID));
    fpDmCmd = openPipe(szCmd, "r");
    if (NULL == fpDmCmd)
    {
        free(pDmInfo);
        pDmInfo = NULL;
        return ERROR;
    }

    while (fgets(szLine, sizeof(szLine), fpDmCmd))
    {
        if (sscanf_s(szLine, "%*d %lld %*s %d:%d %*[^\n ]", &pDmInfo[i].nSectorNum, &pDmInfo[i].nParentMajor, &pDmInfo[i].nParentMinor) != 3)
        {
            nFlag = ERROR;
            free(pDmInfo);
            pDmInfo = NULL;
            break;
        }
        i++;
    }

    (void)pclose(fpDmCmd);
    fpDmCmd = NULL;
    *pstDmInfo = pDmInfo;

    return nFlag;
}

/*****************************************************************************
 Function   : getDmCryptParentNode()
 Description: crypt情况下，获取该文件系统的父节点名
 Input      : char *szLine                              dmsetup读取内容
 Output     : char *pParentMajor                        父节点主设备号
              char *pParentMinor                        父节点从设备号
              int *pStripCnt                            父节点个数
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 History    : 2016.11.11
 *****************************************************************************/
int getDmCryptParentNode(int devID, struct DmInfo **pstDmInfo, int *pCnt)
{
    char szCmd[64] = {0};
    FILE *fpDmCmd = NULL;
    char szLine[MAX_STR_LEN] = {0};
    int i = 0;
    struct DmInfo *pDmInfo = NULL;

    (void)snprintf_s(szCmd, sizeof(szCmd), sizeof(szCmd), "dmsetup table -j %d -m %d", major(devID), minor(devID));
    fpDmCmd = openPipe(szCmd, "r");
    if (NULL == fpDmCmd)
    {
        return ERROR;
    }

    pDmInfo = (struct DmInfo *)malloc(sizeof(struct DmInfo));
    if (NULL == pDmInfo)
    {
        (void)pclose(fpDmCmd);
        fpDmCmd = NULL;
        return ERROR;
    }
    (void)memset_s(pDmInfo, sizeof(struct DmInfo), 0, sizeof(struct DmInfo));

    while (fgets(szLine, sizeof(szLine), fpDmCmd))
    {
        if(i > 0 || sscanf_s(szLine, "%*d %*d %*s %*s %*s %*d %d:%d %*[^\n ]", &pDmInfo[i].nParentMajor, &pDmInfo[i].nParentMinor) != 2)
        {
            free(pDmInfo);
            pDmInfo = NULL;
            (void)pclose(fpDmCmd);
            fpDmCmd = NULL;
            return ERROR;
        }
        i++;
    }

    (void)pclose(fpDmCmd);
    fpDmCmd = NULL;

    /* 加密格式只有1个父节点 */
    *pCnt = 1;
    *pstDmInfo = pDmInfo;
    return SUCC;
}

/*****************************************************************************
 Function   : getDmParentNode()
 Description: 根据文件系统设备号，获取该文件系统的父节点名
 Input      : struct DevMajorMinor* devMajorMinor       /proc/partitions中设备名及其主次设备号结构体数组
              int partNum                               对应关系数据数量
              int devID                                 文件系统设备号
 Output     : char* parentName                          父节点名
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 History    : 2011.09.16, created this function.
 *****************************************************************************/
int getDmParentNode(struct DeviceInfo *linuxDiskUsage, struct DevMajorMinor *devMajorMinor, struct NumberCount numCnt, int devID, long long usage)
{
    char szRealName[MAX_NAME_LEN] = {0};
    int nParentDevID = 0;
    int nFlag = SUCC;
    int nParentCnt = 0;
    struct DmInfo *pstDmInfo = NULL;
    int i = 0;
    char szUsage[64] = {0};
    char szDiskName[MAX_NAME_LEN] = {0};
    int nDmType = 0;
    long long lsubusage = 0L;
    long long lpvSize = 0L;

    if(SUCC != getDmType(devID, &nDmType))
    {
        return ERROR;
    }

    switch(nDmType)
    {
        case DM_LINEAR:
        {
            if(SUCC != getDmLinearParentNode(devID, &pstDmInfo, &nParentCnt))
            {
                return ERROR;
            }
            break;
        }
        case DM_STRIPED:
        {
            if(SUCC != getDmStripParentNode(devID, &pstDmInfo, &nParentCnt))
            {
                return ERROR;
            }
            break;
        }
        case DM_CRYPT:
        {
            if(SUCC != getDmCryptParentNode(devID, &pstDmInfo, &nParentCnt))
            {
                return ERROR;
            }
            break;
        }
        case DM_RAID:
        case DM_SNAPSHOT:
        case DM_MIRROR:
        default:
        {
            /* 暂时返回成功，继续解析其他盘 */
            return SUCC;
        }
    }

    for (i = 0; i < nParentCnt && usage > 0; i++)
    {
        /* 加密类型nParentCnt=1，和条块化类型做相同处理 */
        if (DM_CRYPT == nDmType || DM_STRIPED == nDmType)
        {
            lsubusage = usage / nParentCnt;
        }
        else
        {
            lpvSize = pstDmInfo[i].nSectorNum * SECTOR_SIZE / MEGATOBYTE;

            if (usage >= lpvSize)
            {
                lsubusage = lpvSize;
                usage -= lpvSize;
            }
            else
            {
                lsubusage = usage;
                usage = 0;
            }
        }

        nParentDevID = makedev(pstDmInfo[i].nParentMajor, pstDmInfo[i].nParentMinor);
        if (g_deviceMapperNum == pstDmInfo[i].nParentMajor)
        {
            if (SUCC != getDmParentNode(linuxDiskUsage, devMajorMinor, numCnt, nParentDevID, lsubusage))
            {
                nFlag = ERROR;
                break;
            }
        }
        else
        {
            (void)memset_s(szRealName, MAX_NAME_LEN, 0, MAX_NAME_LEN);
            (void)memset_s(szDiskName, MAX_NAME_LEN, 0, MAX_NAME_LEN);
            (void)getInfoFromID(devMajorMinor, numCnt.partNum, nParentDevID, szRealName, NULL);
            if(ERROR == getDiskNameFromPartName(szRealName, szDiskName))
            {
                nFlag = ERROR;
                break;
            }
            if (0 != strlen(szRealName))
            {
                /* 通过addDiskUsageToDevice函数，将遍历到的分区使用空间累加到对应的磁盘上 */
                (void)snprintf_s(szUsage, sizeof(szUsage), sizeof(szUsage), "%lld", lsubusage);
                (void)addDiskUsageToDevice(linuxDiskUsage, numCnt.diskNum, szRealName, szUsage);
                (void)memset_s(szRealName, MAX_NAME_LEN, 0, sizeof(szRealName));
            }
        }
    }

    free(pstDmInfo);
    return nFlag;
}


/*****************************************************************************
 Function   : getAllDeviceUsage()
 Description: 所有的分区信息累加到对应磁盘的使用空间中
 Input      : struct DevMajorMinor* devMajorMinor      设备名及其主次设备号结构体数组
              struct DeviceInfo* linuxDiskUsage        物理磁盘名称及其使用空间、总空间的结构体数组
              struct DiskInfo diskMap                  挂载点对应文件系统信息，及其使用空间的结构体
              struct NumberCount numberCount           分区数量、物理磁盘数量以及挂载点对应文件系统信息的数量的结构体
 Output     : struct DeviceInfo* linuxDiskUsage        将物理磁盘的使用空间获取出来
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int getAllDeviceUsage(struct DevMajorMinor *devMajorMinor, struct DeviceInfo *linuxDiskUsage,
                      struct DiskInfo *diskMap, struct NumberCount numberCount)
{
    char szParentName[MAX_NAME_LEN] = {0};
    int nResult = 0;
    int i;
    struct DiskInfo *tmpDisk = NULL;
    /* 存放drbd设备对应的父设备名及主次设备号 */
    struct stat statBuf;
    long long usage = 0L;

    for (i = 0; i < numberCount.mountNum; i++)
    {
        tmpDisk = diskMap + i;
        usage = atoll(tmpDisk->usage);

        /* 当mount记录的文件系统是drbd设备时，需要找到对应的父设备 */
        if (NULL != strstr(tmpDisk->filesystem, "drbd"))
        {
            (void)getDrbdParent(tmpDisk->filesystem, tmpDisk->filesystem);
            /* 通过stat函数，获取drbd设备对应的父设备名及主次设备号信息，保存到结构体对应成员中 */
            nResult = stat(tmpDisk->filesystem, &statBuf);
            if (0 != nResult || 0 == statBuf.st_rdev)
            {
                continue;
            }
            /* 更新mount记录中的主次设备号 */
            tmpDisk->st_rdev = statBuf.st_rdev;
        }

        if (g_deviceMapperNum == major(tmpDisk->st_rdev))
        {
            /* 通过dmsetup，获取逻辑卷对应父设备名 */
            nResult = getDmParentNode(linuxDiskUsage, devMajorMinor, numberCount, tmpDisk->st_rdev, usage);
            if (ERROR == nResult)
            {
                return ERROR;
            }
        }
        else
        {
            (void)getInfoFromID(devMajorMinor, numberCount.partNum, tmpDisk->st_rdev, szParentName, NULL);
            if (0 != strlen(szParentName))
            {
                /* 通过addDiskUsageToDevice函数，将遍历到的分区使用空间累加到对应的磁盘上 */
                (void)addDiskUsageToDevice(linuxDiskUsage, numberCount.diskNum, szParentName, tmpDisk->usage);
                (void)memset_s(szParentName, MAX_NAME_LEN, 0, sizeof(szParentName));
            }
        }
    }

    return SUCC;
}

/*****************************************************************************
 Function   : freeSpace()
 Description: 释放三个结构体申请到的空间
 Input      : N/A
 Output     : struct DevMajorMinor* devMajorMinor   设备名及其主次设备号结构体数组
            ：struct DeviceInfo* linuxDiskUsage     物理磁盘名称及其使用空间、总空间的结构体数组
            ：struct DiskInfo* diskMap              挂载点对应文件系统信息，及其使用空间的结构体
 Return     : void
 Other      : N/A
 *****************************************************************************/
void freeSpace(struct DevMajorMinor *devMajorMinor, struct DeviceInfo *linuxDiskUsage, struct DiskInfo *diskMap)
{
    //lint -save -e438
    if (devMajorMinor != NULL)
    {
        free(devMajorMinor);
        devMajorMinor = NULL;
    }

    if (linuxDiskUsage != NULL)
    {
        free(linuxDiskUsage);
        linuxDiskUsage = NULL;
    }

    if (diskMap != NULL)
    {
        free(diskMap);
        diskMap = NULL;
    }
    //lint -restore
}

/*****************************************************************************
 Function   : getDiskUsage()
 Description: 获得磁盘利用率
 Input      : struct xs_handle* handle   handle of xenstore
 Output     : char* pszDiskUsage   存储磁盘利用率的字符串
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int getDiskUsage(struct xs_handle *handle, char pszDiskUsage[][MAX_DISKUSAGE_LEN], int *row_num)
{
    /* 存储总磁盘空间信息字符串(单位为MB) */
    char szTotalSize[MAX_DISKUSAGE_LEN] = {0};
    /* 存储总使用空间信息字符串(单位为MB) */
    char szTotalUsage[MAX_DISKUSAGE_LEN] = {0};
    /* 存储每个磁盘信息连接的字符串 */
    char szUsageString[MAX_ROWS][MAX_DISKUSAGE_LEN] = {0};
    struct DevMajorMinor *devMajorMinor;
    struct DeviceInfo *linuxDiskUsage;
    struct DiskInfo *diskMap = NULL;
    struct NumberCount numberCount = {0};
    int nSwapNum;
    int nResult = 0;
    int i;

    if (!pszDiskUsage || !row_num)
        return ERROR;

    *row_num = 0;

    devMajorMinor = (struct DevMajorMinor *)malloc(MAX_DISKUSAGE_LEN * sizeof(struct DevMajorMinor));
    if (NULL == devMajorMinor)
    {
        return ERROR;
    }
    memset_s(devMajorMinor, 
        MAX_DISKUSAGE_LEN * sizeof(struct DevMajorMinor), 
        0, 
        MAX_DISKUSAGE_LEN * sizeof(struct DevMajorMinor));

    linuxDiskUsage = (struct DeviceInfo *)malloc(MAX_DISK_NUMBER * sizeof(struct DeviceInfo));
    if (NULL == linuxDiskUsage)
    {
        freeSpace(devMajorMinor, linuxDiskUsage, diskMap);
        return ERROR;
    }
    memset_s(linuxDiskUsage, 
        MAX_DISK_NUMBER * sizeof(struct DeviceInfo),
        0, 
        MAX_DISK_NUMBER * sizeof(struct DeviceInfo));

    diskMap = (struct DiskInfo *)malloc(MAX_DISKUSAGE_LEN * sizeof(struct DiskInfo));
    if (NULL == diskMap)
    {
        freeSpace(devMajorMinor, linuxDiskUsage, diskMap);
        return ERROR;
    }
    memset_s(diskMap, 
        MAX_DISKUSAGE_LEN * sizeof(struct DiskInfo),
        0, 
        MAX_DISKUSAGE_LEN * sizeof(struct DiskInfo));

    /* 如果对应device-mapper类型的主设备号为0，则调用getDeviceMapperNumber函数，获取其主设备号 */
    if (0 == g_deviceMapperNum)
    {
        nResult = getDeviceMapperNumber();
        if (ERROR == nResult)
        {
            freeSpace(devMajorMinor, linuxDiskUsage, diskMap);
            return ERROR;
        }
    }

    /*
     * 调用getPartitionsInfo函数，获取磁盘，分区以及对应的磁盘总空间大小（KB），主次设备号信息，
     * 并获取对应的物理设备及其空间大小，即fdisk对应信息
     */
    nResult = getPartitionsInfo(handle, devMajorMinor, &numberCount.partNum, linuxDiskUsage, &numberCount.diskNum);
    if (ERROR == nResult || 0 == numberCount.partNum || 0 == numberCount.diskNum)
    {
        freeSpace(devMajorMinor, linuxDiskUsage, diskMap);
        return ERROR;
    }

    /* 调用getSwapInfo函数，获取当前swap分区信息 */
    nResult = getSwapInfo(diskMap, &numberCount.mountNum);
    if (ERROR == nResult)
    {
        freeSpace(devMajorMinor, linuxDiskUsage, diskMap);
        return ERROR;
    }
    nSwapNum = numberCount.mountNum;

    /* 调用getDiskInfo函数，获取当前挂载的文件系统，挂载点，父设备名以及使用空间信息 */
    nResult = getDiskInfo(devMajorMinor, numberCount.partNum, diskMap, &numberCount.mountNum);
    if (ERROR == nResult || nSwapNum >= numberCount.mountNum)
    {
        freeSpace(devMajorMinor, linuxDiskUsage, diskMap);
        return ERROR;
    }

    /* 调用getAllDeviceUsage函数，通过获取到的挂载的文件系统使用信息，查找对应的父设备，计算对应磁盘的使用率信息 */
    nResult = getAllDeviceUsage(devMajorMinor, linuxDiskUsage, diskMap, numberCount);
    if (ERROR == nResult)
    {
        freeSpace(devMajorMinor, linuxDiskUsage, diskMap);
        return ERROR;
    }

    /* 循环读取结构体数据的每个成员 */
    for (i = 0; i < numberCount.diskNum; i++)
    {
        /* 将获取出来的信息转化为字符串，并且计算其总空间大小 */
        (void)strAdd(linuxDiskUsage[i].deviceTotalSpace, szTotalSize, szTotalSize);
        (void)strAdd(linuxDiskUsage[i].diskUsage, szTotalUsage, szTotalUsage);
        //最多只拼接61个利用率信息(1个光驱+60个磁盘)
        if(i <= MAX_DISKUSAGE_STRING_NUM) {
            *row_num = i / MAX_DISKUSAGE_NUM_PER_KEY;
            (void)snprintf_s(szUsageString[*row_num] + strlen(szUsageString[*row_num]), 
                            (MAX_DISKUSAGE_LEN - strlen(szUsageString[*row_num])), 
                            (MAX_DISKUSAGE_LEN - strlen(szUsageString[*row_num])), 
                            "%s:%s:%s;",
                            linuxDiskUsage[i].phyDevName, 
                            linuxDiskUsage[i].deviceTotalSpace, 
                            linuxDiskUsage[i].diskUsage);
        }
    }

    /* 将总的磁盘空间和使用空间以及最终的每个磁盘的使用率对应的字符串，串联赋值给出参 */
    for (i = 0; i <= *row_num; i++) {
        if (0 == i)
            (void)snprintf_s(pszDiskUsage[i], MAX_DISKUSAGE_LEN, MAX_DISKUSAGE_LEN, 
                    "0:%s:%s;%s", szTotalSize, szTotalUsage, szUsageString[i]);
        else
            (void)snprintf_s(pszDiskUsage[i], MAX_DISKUSAGE_LEN, MAX_DISKUSAGE_LEN, 
                    "%s", szUsageString[i]);
    }

    freeSpace(devMajorMinor, linuxDiskUsage, diskMap);

    return SUCC;
}
/*****************************************************************************
 Function   : FilesystemUsage()
 Description: get Filesystemname list and its usage
 Input      : struct xs_handle* handle   handle of xenstore
 Output     : int 
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 Remark     : 2013-9-4 create this function for suse11sp1 Linux OS
 *****************************************************************************/
int FilesystemUsage(struct xs_handle *handle)
{ 
    FILE *file;
    char buf[1056] = {0};
    char filename[1024] = {0};
    char path[32] = {0};
    char value[MAX_FILENAMES_XENSTORLEN+1] = {0};
    char numbuf[32] = {0};
    int FileNameArrLen;
    int size,used;
    int num;
    int exceedflag;
    int i;
    /*客户提供的shell命令方式解析，shell命令获取文件系统名称，总大小，已用大小*/
    file = openPipe("df -lmP | grep -v Filesystem | grep -v Used | grep -v tmpfs | grep -v shm","r");
    if(NULL == file)
    {
       DEBUG_LOG("Failed to exec df -lmP shell command.");
       (void)write_to_xenstore(handle, FILE_DATA_PATH, "error");
       return ERROR;
    }
    (void)memset_s(FilenameArr,MAX_FILENAMES_SIZE,0,MAX_FILENAMES_SIZE);
    while(NULL != fgets(buf,sizeof(buf),file))
    {
       (void)sscanf_s(buf,"%s %d %d",filename,sizeof(filename),&size,&used);
       (void)snprintf_s(FilenameArr+strlen(FilenameArr),
                        sizeof(FilenameArr) - strlen(FilenameArr),
                        sizeof(FilenameArr) - strlen(FilenameArr),
                        "%s:%d:%d;",filename,size,used);
    }
    (void)pclose(file);
    FileNameArrLen = strlen(FilenameArr);
	num = FileNameArrLen / MAX_FILENAMES_XENSTORLEN;
	exceedflag = FileNameArrLen % MAX_FILENAMES_XENSTORLEN;
    (void)snprintf_s(value, MAX_FILENAMES_XENSTORLEN, MAX_FILENAMES_XENSTORLEN, "%s", FilenameArr);
    if(xb_write_first_flag == 0)
    {
       (void)write_to_xenstore(handle, FILE_DATA_PATH, value);
    }
    else
    {
        (void)write_weak_to_xenstore(handle, FILE_DATA_PATH, value);
    }
    /*若超过临界值则num+1;比如:33/32=1;还有1个字节，则需要换行写*/
    if(exceedflag)
    {
       num += 1; 
    }
    /*超过xenstore键值的总长度则换filesystem_extra%d键值写*/
    for(i=1; i<num; i++)
    {
        (void)snprintf_s(path, sizeof(path), sizeof(path), FILE_DATA_EXTRA_PATH_PREFIX"%d", i); //filesystem_extra%d
        (void)snprintf_s(value, MAX_FILENAMES_XENSTORLEN, MAX_FILENAMES_XENSTORLEN,
                        "%s", FilenameArr+(MAX_FILENAMES_XENSTORLEN*i)); 
	if(xb_write_first_flag == 0)
	{
            (void)write_to_xenstore(handle, path, value);
	}
	else
	{
	    (void)write_weak_to_xenstore(handle, path, value);
	}
    }
    (void)snprintf_s(numbuf, sizeof(numbuf), sizeof(numbuf), "%d", num);
    if(xb_write_first_flag == 0)
    {
        (void)write_to_xenstore(handle, FILE_NUM_PATH, numbuf);
    }
    else
    {
        (void)write_weak_to_xenstore(handle, FILE_NUM_PATH, numbuf);
    }
    return SUCC;
}
/*****************************************************************************
 Function   : diskworkctlmon()
 Description: 处理getdiskusage请求，将磁盘利用率信息写入xenstore中
 Input      : struct xs_handle* handle   handle of xenstore
 Output     : N/A
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int diskworkctlmon(struct xs_handle *handle)
{
    char szDiskUsage[MAX_ROWS][MAX_DISKUSAGE_LEN] = {0};
    int nRet;
    int row_num = 0;

    if (NULL == handle)
    {
        return ERROR;
    }

    /* 调用函数返回磁盘利用率字符串 */
    nRet = getDiskUsage(handle, szDiskUsage, &row_num);
    if (ERROR == nRet)
    {
        /*失败写入error*/
        write_xs_disk(handle, xb_write_first_flag, szDiskUsage, 0);
        return ERROR;
    }
    else
    {
        write_xs_disk(handle, xb_write_first_flag, szDiskUsage, row_num + 1);
    }

    if (g_exinfo_flag_value & EXINFO_FLAG_FILESYSTEM)
    {
        (void)FilesystemUsage(handle);
    }
    return SUCC;
}

void write_xs_disk(struct xs_handle *handle, int is_weak, char xs_value[][MAX_DISKUSAGE_LEN], int row_num)
{
    int i = 0;
    char xs_path[MAX_DISKUSAGE_LEN] = {0};

    if (!handle || !xs_value)
        return;
    if (MAX_ROWS < row_num)
        return;

    if (0 == row_num) {
        for (i = 0; i < MAX_ROWS; i++) {
            if (0 == i)
                (void)snprintf_s(xs_path, MAX_DISKUSAGE_LEN, MAX_DISKUSAGE_LEN, "%s", DISK_DATA_PATH);
            else
                (void)snprintf_s(xs_path, MAX_DISKUSAGE_LEN, MAX_DISKUSAGE_LEN, "%s-%d", DISK_DATA_EXT_PATH, i);

            if (is_weak)
                write_weak_to_xenstore(handle, xs_path, "error");
            else
                write_to_xenstore(handle, xs_path, "error");
        }
    } else {
        for (i = 0; i < row_num; i++) {
            if (0 == i)
                (void)snprintf_s(xs_path, MAX_DISKUSAGE_LEN, MAX_DISKUSAGE_LEN, "%s", DISK_DATA_PATH);
            else
                (void)snprintf_s(xs_path, MAX_DISKUSAGE_LEN, MAX_DISKUSAGE_LEN, "%s-%d", DISK_DATA_EXT_PATH, i);

            if (is_weak)
                write_weak_to_xenstore(handle, xs_path, xs_value[i]);
            else
                write_to_xenstore(handle, xs_path, xs_value[i]); 
        }
        for (; i < MAX_ROWS; i++) {
            (void)snprintf_s(xs_path, MAX_DISKUSAGE_LEN, MAX_DISKUSAGE_LEN, "%s-%d", DISK_DATA_EXT_PATH, i);
            if (is_weak)
                write_weak_to_xenstore(handle, xs_path, "0");
            else
                write_to_xenstore(handle, xs_path, "0");
        }
    }
}
