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
/*xenstore֧��4096���ֽڣ�����������ƺ������ֵ�ٽ���filesystem��Ϣ����д*/
#define FILE_DATA_EXTRA_PATH_PREFIX        "control/uvp/filesystem_extra"
/*filesystem��ֵ����Ŀ*/
#define FILE_NUM_PATH               "control/uvp/filesystem_extra_num"
#define DEVPATH                     "/dev/disk/by-id/"
#define CMPSTRING                   "../../"
#define SCSI_DISK_PATH              "device/vscsi"
/*must be equal with the value defined in uvpext*/
#define MAX_ROWS 4
#define MAX_XS_LENGTH 100
/*ÿ��xenstore��ֵ��д�Ĵ��������ʸ���*/
#define MAX_DISKUSAGE_NUM_PER_KEY 30

#define RAM_MAJOR                   1
#define LOOP_MAJOR                  7
#define MAX_DISKUSAGE_LEN           1024
#define MAX_NAME_LEN                64
#define MAX_STR_LEN                 4096
#define UNIT_TRANSFER_CYCLE         1024
#define MAX_DISK_NUMBER             128          /* TODO:1.�ֽ׶�֧��11��xvda���̣�17��scsi���̣��Ժ�֧�ֵĴ����������˴�Ӧ��Ӧ����. 
                                                         2. Ϊ��֧��60�����̣���ֵ����Ϊ128*/
/*֧��50������filesystem����*/
#define MAX_FILENAMES_SIZE          52800
/*xenstore��ֵ��󳤶�Ϊ4096����ȥ��ֵ���ȣ�ʣ�³���Ϊfilesystem�ɷŵĳ���*/
#define MAX_FILENAMES_XENSTORLEN    4042
/*���ֻ�ϱ�60�����̵���������Ϣ*/
#define MAX_DISKUSAGE_STRING_NUM 60
#define SECTOR_SIZE 512
#define MEGATOBYTE (1024 * 1024)
/*FilenamesArr�����װ���ļ�ϵͳ����ʹ����*/
char FilenameArr[MAX_FILENAMES_SIZE] = {0};
/* device-mapper��Ӧ�����豸�� */
int g_deviceMapperNum = 0;

/* �豸���Ƽ��������豸�Žṹ�壨/proc/partitions�� */
struct DevMajorMinor
{
    int  devMajor;                              /* �豸���豸�� */
    int  devMinor;                              /* �豸���豸�� */
    char devBlockSize[MAX_STR_LEN];             /* �豸�Ŀ��С(KB) */
    char devName[MAX_NAME_LEN];                 /* �豸�� */
};

/* ����������Ƽ���ʹ�ÿռ䡢�ܿռ�Ľṹ�� */
struct DeviceInfo
{
    char phyDevName[MAX_NAME_LEN];                     /* ������� */
    char deviceTotalSpace[MAX_STR_LEN];                /* ���̵��ܿռ�(MB) */
    char diskUsage[MAX_DISKUSAGE_LEN];                 /* ���̵�ʹ�ÿռ�(MB) */
};

/* ���ص��Ӧ�ļ�ϵͳ��Ϣ������ʹ�ÿռ�Ľṹ�� */
struct DiskInfo
{
    int  st_rdev;                                  /* �洢�ļ�ϵͳ��Ӧ�豸ID */
    char filesystem[MAX_STR_LEN];                  /* �ļ�ϵͳ������ӦFileSystem�� */
    char usage[MAX_DISKUSAGE_LEN];                 /* �ļ�ϵͳ��ʹ�ÿռ�(MB) */
    char mountPoint[MAX_STR_LEN];                  /* �ļ�ϵͳ�Ĺ��ص� */
};

/* ��¼ǰ�������ṹ��������ʵ�ʴ洢�ĸ��� */
struct NumberCount
{
    int partNum;             /* ��¼DevMajorMinor���� */
    int diskNum;             /* ��¼DeviceInfo���� */
    int mountNum;            /* ��¼DiskInfo���� */
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
 Description: �������ַ�����Ӻ���
 Input      : inputNum1          ��һ�������ַ���
              inputNum2          �ڶ��������ַ���
 Output     : outputResult       ���������ַ����͵��ַ���
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

    /* ���������һ��ΪNULL�����ش��� */
    if (NULL == pszFirstNum || NULL == pszSecondNum || NULL == outputResult)
    {
        return ERROR;
    }

    nFirstNumLen = strlen(pszFirstNum);
    nSecondNumLen = strlen(pszSecondNum);

    /* �������������нϳ���һ�����ݵĳ��� */
    nTmpStrLen = (nFirstNumLen < nSecondNumLen) ? nSecondNumLen : nFirstNumLen;
    if (MAX_STR_LEN <= nTmpStrLen)
    {
        return ERROR;
    }
    /* ѭ����ȡÿһλ������ */
    for (i = 0; i < nTmpStrLen; i++)
    {
        /* �����Ӧ�����ݳ���С��ѭ�����������λ������Ϊ0������ֱ�ӻ�ȡ��λ���ݣ����ҽ�ASCII��ת��Ϊ���� */
        j = nFirstNumLen - i - 1;
        k = nSecondNumLen - i - 1;
        (j >= 0) ? (nNum1 = pszFirstNum[j] - 48) : (nNum1 = 0);
        (k >= 0) ? (nNum2 = pszSecondNum[k] - 48) : (nNum2 = 0);
        /* ����������ʱ������������־λ�ĺͣ�ȡ��λ����ʮλ�� */
        sum = nNum1 + nNum2 + ten;
        ten = sum / 10;
        sum = sum % 10;
        /* ����Ӧ�ĺ͵ĸ�λ���ָ�����ʱ�ַ��� */
        *(szTmpResult + i) = sum + 48;
    }
    /* ���˳�ѭ���������λ��־����1�����ٽ����λ��1������ʱ�ַ��� */
    if (1 == ten)
    {
        *(szTmpResult + i) = ten + 48;
    }

    /* ��ȡ��ʱ�ַ����ĳ��ȣ����䰴�����Ƹ����� */
    nTmpStrLen = strlen(szTmpResult);
    for (i = 0; i < nTmpStrLen; i++)
    {
        outputResult[i] = szTmpResult[nTmpStrLen - i - 1];
    }
    /* �ֶ����ַ���ĩβ����\0����ֹ�ظ�ʹ�ô��������� */
    outputResult[i] = '\0';

    return SUCC;
}

/*****************************************************************************
 Function   : strMinus()
 Description: �������ַ����������
 Input      : inputNum1          ������
              inputNum2          ����
 Output     : outputResult       ���������ַ�������ַ���
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
    /* ÿλ������ŵ���ʱ�� */
    int wanting = 0;
    /* ��ǽ�λ */
    int isNegative = 0;
    /* ��Ž�����ַ����±� */
    int nTmpLocation = 0;
    int i;

    /* ���������һ��ΪNULL�����ش��� */
    if (NULL == pszFirstNum || NULL == pszSecondNum || NULL == outputResult)
    {
        return ERROR;
    }

    nFirstNumLen = strlen(pszFirstNum);
    nSecondNumLen = strlen(pszSecondNum);

    /* ��������ĳ��ȴ��ڱ������ĳ���(���Ϊ��)�����ش��� */
    if (nFirstNumLen < nSecondNumLen)
    {
        return ERROR;
    }

    /* ѭ����ȡÿһλ�����ݣ�Ȼ����ж�Ӧ�����ݲ��� */
    for (i = 0; i < nSecondNumLen; i++)
    {
        /* ��ȡ��������Ӧλ�ϵ����� */
        nNum1 = pszFirstNum[nFirstNumLen - i - 1] - 48;
        nNum2 = pszSecondNum[nSecondNumLen - i - 1] - 48;
        /* �жϱ�������Ӧ���ݱ���λ���Ƿ���ڼ����Ķ�Ӧ�������С�ڣ�����Ҫ�����һλ��λ�����򣬲���Ҫ */
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
        /* ��������Ӧλ����ֵ�ַ�������ʱ�ַ��� */
        *(szTmpResult + i) = wanting + 48;
    }

    /* ����ѭ����ֱ���������ĳ���Ϊֹ */
    for (; i < nFirstNumLen; i++)
    {
        nNum1 = pszFirstNum[nFirstNumLen - i - 1] - 48;
        /* �����λ��־����0���������Ӧ�����㣬����ֱ�ӽ���ǰλ�����ݴ��ݸ���� */
        if (1 == isNegative)
        {
            /* �����ǰλ�õ���ֵ��С��1����ֱ������������������һλ��λ */
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
    /* ��������걻����������λ������λ��ǻ��Ǵ���0���򷵻ش��� */
    if (1 == isNegative)
    {
        return ERROR;
    }

    /* ��ȡ��ʱ�ַ����ĳ��ȣ����䰴�����Ƹ����� */
    nTmpStrLen = strlen(szTmpResult);
    for (i = 0; i < nTmpStrLen; i++)
    {
        /* ȥ��������λ��0 */
        if (0 == strlen(outputResult) && '0' == szTmpResult[nTmpStrLen - i - 1])
        {
            continue;
        }
        outputResult[nTmpLocation] = szTmpResult[nTmpStrLen - i - 1];
        nTmpLocation++;
    }

    /* ���������ַ�������Ϊ0��������θ�ֵһ��0 */
    if (0 == strlen(outputResult))
    {
        (void)strncpy_s(outputResult, 3, "0", 2);
    }

    return SUCC;
}

/*****************************************************************************
 Function   : strMulti()
 Description: �������ַ�����˺���
 Input      : inputNum1         ��һ�������ַ���
                inputNum1         �ڶ��������ַ���
 Output     : outputResult      ���������ַ��������ַ���
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
    /* ��Ž�λ����ʱ��� */
    int carry = 0;
    int nTempResult = 0;
    int nLen;
    int i;
    int j = 0;

    /* ���������һ��ΪNULL�����ش��� */
    if (NULL == pszFirstNum || NULL == pszSecondNum || NULL == outputResult)
    {
        return ERROR;
    }

    nFirstNumLen = strlen(pszFirstNum);
    nSecondNumLen = strlen(pszSecondNum);

    /* ѭ������һ�������ַ�����ÿһλ�������һλ��ʼ */
    for (i = nFirstNumLen; i > 0; i--)
    {
        /* ��ȡ��Ӧλ�õ����� */
        nNum1 = *(pszFirstNum + i - 1) - 48;
        /* ѭ��������һ�������ַ�����ÿһλ�������һλ��ʼ */
        for (j = nSecondNumLen; j > 0; j--)
        {
            /* ��ȡ��Ӧλ�õ����� */
            nNum2 = *(pszSecondNum + j - 1) - 48;
            /* ����ȡ��������˺����λ��� */
            nTempResult = nNum1 * nNum2 + carry;
            /* �ֱ��ȡ��λ��β��������β�����ݸ���ʱ�ַ��� */
            carry = nTempResult / 10;
            nTempResult = nTempResult % 10;
            *(szTmpResult + nSecondNumLen - j) = nTempResult + 48;
        }
        /* ���������ɺ󣬽�λ�Բ�Ϊ0���򽫽�λ���ݸ���ʱ�ַ��� */
        if (0 != carry)
        {
            *(szTmpResult + nSecondNumLen) = carry + 48;
            carry = 0;
        }
        /* ��ȡ��ʱ�ַ����ĳ��ȣ����䰴�����ƴ������ݸ�����ִ�е���ʱ�ַ��� */
        nTmpStrLen = strlen(szTmpResult);
        for (j = 0; j < nTmpStrLen; j++)
        {
            szOneTimeResult[j] = szTmpResult[nTmpStrLen - j - 1];
        }
        /* ��ÿ�ν���ĵ�λ��0���Թ��˺���ַ�����Ӧλ��� */
        for (j = 0; j < nFirstNumLen - i; j++)
        {
            (void)strncat_s(szOneTimeResult, MAX_STR_LEN, "0", 2);
        }
        /* ������ִ�н����������˽�������ۼ� */
        (void)strAdd(szOneTimeResult, szFinalResult, szFinalResult);
        /* ����ʱ�ַ����͵��ν���ַ������� */
        memset_s(szTmpResult, MAX_STR_LEN, 0, sizeof(szTmpResult));
        memset_s(szOneTimeResult, MAX_STR_LEN, 0, sizeof(szOneTimeResult));
    }

    /* �������ַ���������ɣ�����������������ս�����ݸ����� */
    nLen = strlen(szFinalResult);
    /* �����ַ�Ϊ0ʱ��˵�����Ϊ0 */
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
 Description: ��λת�����������ֽں�KBת��ΪMB��ʹ�õ��ǽ�λ��
 Input      : sourceStr      ��Σ��ֽڴ�С/KB��С
              level          ��Σ���λ����ı���
 Output     : targetStr      ���Σ����ڴ洢������
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
    /* ���������̳�ʼ����Ϊ0 */
    int divide = 0;
    /* ������ʼ����Ϊ0 */
    unsigned long remainder = 0;
    int nLen;
    int i;
    /* ����̵��ַ������±� */
    int j = 0;

    /* �ж�����ַ����Ƿ�ΪNULL���߳���Ϊ0��������򷵻ش��� */
    if (NULL == pszFirstNum || NULL == targetStr || 0 == nCycle)
    {
        return ERROR;
    }

    nNumLen = strlen(pszFirstNum);

    /* ѭ����ȡÿһλ������ */
    for (i = 0; i < nNumLen; i++)
    {
        /* ��λ��ȡ�������ַ�������ֵ */
        tmpNum = remainder * 10 + (pszFirstNum[i] - 48);
        /* �ж���ʱ��ֵ�Ƿ���ڳ��������̵��ַ����Ƿ�Ϊ�գ����С����Ϊ�գ�����ʱ�ַ�����ֵ������ */
        if(tmpNum < nCycle && 0 == strlen(szResult))
        {
            remainder = tmpNum;
        }
        else
        {
            /* �����ʱ���ݴ��ڳ�������ʼ��ʽ�ļ��㣬�̵�����ʱ���ݳ��Գ���������������ʱ�����������ģ */
            divide = tmpNum / nCycle;
            remainder = tmpNum % nCycle;
            /* ���̸�ֵ����Ӧ�Ľ������ */
            *(szResult + j) = divide + 48;
            j++;
        }
    }
    /* ��������������ַ���ȫ����ȡ��ϣ�û�д��ڳ������򽫽����������Ϊ0 */
    if(0 == j)
    {
        *(szResult + j) = divide + 48;
    }
    /* ���ִ�����֮��������Ϊ0�����λ(�����+1) */
    if (0 != remainder)
    {
        (void)strAdd(szResult, "1", szResult);
    }

    /* �����ս���ַ������Ƹ����� */
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
 Description: ��popen�������з�װ������UT
 Input      : command        popen����Σ�Ҫִ�е�����
 Output     : type           popen����Σ�����ģʽ
 Return     : FILE*          �ļ�ָ��
 Other      : N/A
 *****************************************************************************/
FILE *openPipe(const char *pszCommand, const char *pszType)
{
    return popen(pszCommand, pszType);
}

/*****************************************************************************
 Function   : openFile()
 Description: ��fopen�������з�װ������UT
 Input      : filepath       fopen����Σ��ļ���·��
 Output     : type           fopen����Σ�����ģʽ
 Return     : FILE*          �ļ�ָ��
 Other      : N/A
 *****************************************************************************/
FILE *openFile(const char *pszFilePath, const char *pszType)
{
    return fopen(pszFilePath, pszType);
}

/*****************************************************************************
 Function   : getDeviceMapperNumber()
 Description: ���Device-Mapper���͵����豸��
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

    /* ��/proc/devices�ļ� */
    fpDevices = openFile(PROC_DEVICES, "r");
    if (NULL == fpDevices)
    {
        return ERROR;
    }

    while (fgets(szLine, sizeof(szLine), fpDevices))
    {
        /* ��ȡ�豸���ͺͶ�Ӧ�����豸�� */
        if (sscanf_s(szLine, "%s %[^\n ]", szMajor, sizeof(szMajor), szDeviceType, sizeof(szDeviceType)) != 2)
        {
            continue;
        }
        /* �ȶ��������Ƿ�Ϊdevice-mapper�����߼������� */
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
 Description: �ͷ�����ָ�����뵽�Ŀռ�
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
 Description: �ж��ַ���str�Ƿ����ַ���prefix��ʼ
 Input      : str           ���
              prefix        ���
 Output     : N/A
 Return     : 1:    �ַ���str���ַ���prefix��ʼ
              0:    �ַ���str�����ַ���prefix��ʼ
 Other      : N/A
 History    : 2013.09.02, h00227765 created this function.
 *****************************************************************************/
static int startsWith(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

/*****************************************************************************
 Function   : getScsiBackendPath()
 Description: ��ö�Ӧ���xenstore scsi�豸��·��
 Input      : struct xs_handle* handle      handle of xenstore
              dir                           ǰ��xenstoreĿ¼��
 Output     : N/A
 Return     : ��NULL = success, NULL = failure
 Other      : N/A
 History    : 2013.09.02, h00227765 created this function.
 *****************************************************************************/
static char* getScsiBackendPath(struct xs_handle *handle, char *dir)
{
    char    szXsPath[MAX_STR_LEN];
    int     ret;

    /* �磺��ȡdevice/vscsi/2048/backend��ú��xenstore·�� */
    ret = snprintf_s(szXsPath, sizeof(szXsPath), sizeof(szXsPath), "%s/%s/%s", SCSI_DISK_PATH, dir, "backend");
    if (ret < 0)
    {
        return NULL;
    }

    return read_from_xenstore(handle, szXsPath);
}

/*****************************************************************************
 Function   : getScsiBackendParams()
 Description: ��ö�Ӧ���xenstore scsi�豸·����params����
 Input      : struct xs_handle* handle      handle of xenstore
              bePath                        ���xenstore·��
 Output     : N/A
 Return     : ��NULL = success, NULL = failure
 Other      : N/A
 History    : 2013.09.02, h00227765 created this function.
 *****************************************************************************/
static char* getScsiBackendParams(struct xs_handle *handle, char *bePath)
{
    char    szXsPath[MAX_STR_LEN];
    int     ret;

    /* �磺��ȡ��˼�ֵ/local/domain/0/backend/vbd/%d/2048/params��ȡ�豸id */
    ret = snprintf_s(szXsPath, sizeof(szXsPath), sizeof(szXsPath), "%s/%s", bePath,  "params");
    if (ret < 0)
    {
        return NULL;
    }

    return read_from_xenstore(handle, szXsPath);
}

/*****************************************************************************
 Function   : getScsiBackendName()
 Description: ��ö�Ӧ���xenstore scsi�豸·�����豸������XML�豸��
 Input      : struct xs_handle* handle      handle of xenstore
              bePath                        ���xenstore·��
 Output     : N/A
 Return     : ��NULL = success, NULL = failure
 Other      : N/A
 History    : 2013.09.02, h00227765 created this function.
 *****************************************************************************/
static char* getScsiBackendName(struct xs_handle *handle, char *bePath)
{
    char    szXsPath[MAX_STR_LEN];
    int     ret;

    /* �磺��ȡ��˼�ֵ/local/domain/0/backend/vbd/%d/2048/dev��ȡxml�������豸�� */
    ret = snprintf_s(szXsPath, sizeof(szXsPath), sizeof(szXsPath), "%s/%s", bePath, "dev");
    if (ret < 0)
    {
        return NULL;
    }

    return read_from_xenstore(handle, szXsPath);
}

/*****************************************************************************
 Function   : getScsiDiskXmlName()
 Description: ͨ�����xenstore�豸��, ��������scsi�����豸��xml�����е��豸��
 Input      : struct xs_handle *handle      handle of xenstore
              char *d_name                  ��������豸������sda
 Output     : N/A
 Return     : ��NULL = success, NULL = failure
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

    /* ��ȡ���������scsi�����豸�ż�scsi�̸��� */
    xs_dir = xs_directory(handle, 0, SCSI_DISK_PATH, &num);
    if (xs_dir == NULL)
    {
        return NULL;
    }

    for (i = 0; i < num; i++)
    {
        /* �磺��ȡdevice/vscsi/2048/backend��ú��xenstore·�� */
        bePath = getScsiBackendPath(handle, xs_dir[i]);
        if (bePath == NULL)
        {
            free(xs_dir);
            return NULL;
        }

        /* �磺��ȡ��˼�ֵ/local/domain/0/backend/vbd/%d/2048/params��ȡ�豸id */
        params = getScsiBackendParams(handle, bePath);
        if (params == NULL)
        {
            free(xs_dir);
            freePath(bePath, NULL, NULL);
            return NULL;
        }

        /* �磺��ȡ��˼�ֵ/local/domain/0/backend/vbd/%d/2048/dev��ȡxml�������豸�� */
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
 Description: /dev/disk/by-id�µ��ļ��Ƿ��������Ҫ�ҵ��豸
 Input      : ptName                ���ҵ��豸��
              entry                 /dev/disk/by-id�µ�һ���ļ�
 Output     : N/A
 Return     : 1:                    �Ǵ��ҵ��豸
              0:                    ���Ǵ��ҵ��豸
 Other      : N/A
 History    : 2013.09.02, h00227765 created this function.
 *****************************************************************************/
static int isCorrectDev(const char *ptName, struct dirent *entry)
{
    char    fullPath[MAX_STR_LEN];
    char    actualPath[MAX_STR_LEN];
    ssize_t len;

    /* ��Ŀ¼��Ϊ".��..��wwn-"������ */
    if (startsWith(entry->d_name, ".") ||
        startsWith(entry->d_name, "..") ||
        startsWith(entry->d_name, "wwn-"))
    {
        return 0;
    }

    snprintf_s(fullPath, sizeof(fullPath), sizeof(fullPath), "%s%s", DEVPATH, entry->d_name);
    /*
     * ��ȡ����id�������ӣ��Եõ�����������"../../sda"
     * readlink() does not append a null byte to buf.
     */
    len = readlink(fullPath, actualPath, sizeof(actualPath) - 1);
    if (-1 == len)
    {
        return 0;
    }
    actualPath[len] = '\0'; /* On success, readlink() returns the number of bytes placed in buf. */

    /* ����ǰ���"../../"��ô������ַ���(����sda)����бȽ�sda */
    if (0 != strcmp(ptName, actualPath + strlen(CMPSTRING)))
    {
        return 0;
    }

    return 1;
}

/*****************************************************************************
 Function   : getXmlDevName()
 Description: �����������scsi����������ȡXML�豸��
 Input      : handle                handle of xenstore
              ptName                ������XML�豸����������ڷ�����
 Output     : N/A
 Return     : XML�豸���������Ҫ�ͷ�
              ��NULL = success, NULL = failure
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

    /* ����/dev/disk/by-idĿ¼�µ������ļ� */
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
 Description: ��ȡ/proc/partition���豸�Լ��豸��������Ϣ
 Input      : struct xs_handle* handle   handle of xenstore
 Output     : struct DevMajorMinor* devMajorMinor    �豸���Ƽ��������豸�ŵĽṹ������
              int* pnPartNum                         ��������
              struct DeviceInfo* diskUsage           ����������Ƽ���ʹ�ÿռ䡢�ܿռ�Ľṹ������
              int* pnDiskNum                         ��������
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

    /* ��/proc/partitions�ļ� */
    fpProcPt = openFile(PROC_PARTITIONS, "r");
    if (NULL == fpProcPt)
    {
        return ERROR;
    }

    /* ѭ����ȡÿһ�е���Ϣ */
    while (fgets(szLine, sizeof(szLine), fpProcPt))
    {
        /* ��ʽ����ȡ��Ӧ������ */
        if (sscanf_s(szLine, " %d %d %s %[^\n ]", &nMajor, &nMinor, szSize, sizeof(szSize), szPtName, sizeof(szPtName)) != 4)
        {
            continue;
        }
        /* ����loop��ram��dm���豸����Ϣ */
        if (LOOP_MAJOR == nMajor || RAM_MAJOR == nMajor)
        {
            continue;
        }
        /* ����ȫ�������ݣ��������ҵ�dmsetup�������豸ʱ����ҪѰ�Ҷ�Ӧ���豸�� */
        tmpDev = devMajorMinor + nPartitionNum;
        tmpDev->devMajor = nMajor;
        tmpDev->devMinor = nMinor;

        szSize[MAX_STR_LEN - 1] = '\0';
        szPtName[MAX_NAME_LEN - 1] = '\0';
        (void)strncpy_s(tmpDev->devBlockSize, MAX_STR_LEN, szSize, strlen(szSize));
        (void)strncpy_s(tmpDev->devName, MAX_NAME_LEN, szPtName, strlen(szPtName));
        nPartitionNum++;
        /* ĿǰUVP��֧�ֹ���xvd*��hd*��sd*���͵��豸 */
        if ('h' != szPtName[0] && 's' != szPtName[0] && 'x' != szPtName[0])
        {
            continue;
        }

        /* ĿǰUVP��֧�ֹ��ش����ֵ��豸���ͣ����Ի�ȡszPtName�е����һ���ַ����鿴�Ƿ�Ϊ���֣�����ǣ�˵���Ǵ���*/
        nPtNameLen = strlen(szPtName);
        if (szPtName[nPtNameLen - 1] >= '0' && szPtName[nPtNameLen - 1] <= '9')
        {
            continue;
        }

        tmpUsage = diskUsage + nDiskNum;

        /*
         * ����Ϣ���ݴ�����Ϣ�Ľṹ���С�
         * scsi�豸���޸�Ϊxml�����ļ����豸��(target dev)��������scsi�豸������������ڲ���ʾ����Ϊ׼��
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

        /* ��ʼ��ʹ����ϢΪ0 */
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
 Description: ��ȡswap�������ļ�ϵͳ�����Լ���Ӧ�Ŀռ��С
 Input      : N/A
 Output     : struct DiskInfo* diskMap           swap������Ϣ����ʹ�ÿռ�
              int* pnMountNum                    swap����������
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

    /* ��/proc/swaps�ļ� */
    fpProcSwap = openFile(PROC_SWAPS, "r");
    if (NULL == fpProcSwap)
    {
        return ERROR;
    }
    /* ��diskMap�����swap������Ϣ */
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
 Description: ����drbd�豸������ȡ��Ӧ�ĸ��ڵ���
 Input      : char* drbdName                     drbd�豸��
 Output     : char* parentName                   ���ڵ���
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

    /* ���ݵ�ǰdrbd�豸��������drbdsetup�����ȡ���豸 */
    (void)snprintf_s(szCmd, sizeof(szCmd), sizeof(szCmd), "drbdsetup \"%s$\" show |grep /dev", drbdName);
    fpDmCmd = openPipe(szCmd, "r");
    if (NULL == fpDmCmd)
    {
        return 0;
    }

    /* ��ȡ����ִ�н�����ļ�ָ�� */
    if (NULL != fgets(szLine, sizeof(szLine), fpDmCmd))
    {
        /* ����drbdsetup����������ȡ���豸���� */
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
 Description: ��ȡ/proc/mounts��ʵ���ļ�ϵͳ�Լ�����ص��Ӧ��Ϣ(ȥ���ظ��tmp��)
 Input      : N/A
 Output     : struct DiskInfo mountInfo[]     ���ص��Ӧ�ļ�ϵͳ��Ϣ������ʹ�ÿռ�Ľṹ��
              int* pnMountNum                 ���ص��Ӧ�ļ�ϵͳ��Ϣ�ĸ���
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
    /* �жϹ��ص��Ƿ�Ϊ��Ŀ¼��־λ */
    int nIsGetOneInfo = 0;
    int i;
    int j = 0;

    tmpMountInfo = (struct DiskInfo *)malloc(MAX_STR_LEN * sizeof(struct DiskInfo));
    if (NULL == tmpMountInfo)
    {
        return ERROR;
    }
    memset_s(tmpMountInfo, MAX_STR_LEN * sizeof(struct DiskInfo), 0, MAX_STR_LEN * sizeof(struct DiskInfo));

    /* ��/proc/mounts�ļ� */
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

    /* ѭ����ȡÿһ������ */
    while (fgets(szLine, sizeof(szLine), fpProcMount))
    {
        /* ��ʽ����ȡ��Ӧ��Ҫ������ */
        if (sscanf_s(szLine, "%s %s %s %*[^\n ]", 
                    szFilesystemName, sizeof(szFilesystemName), 
                    szMountPointName, sizeof(szMountPointName),
                    szMountType, sizeof(szMountType)) != 3)
        {
            continue;
        }

        /* ȥ����/dev/��ͷ���ļ�ϵͳ����tmpfs�ȣ�ȥ��loop�豸��ȥ���ظ����ص�/Ŀ¼�ĺ��漸���ļ�ϵͳ��
         * ȥ��cdrom�Ĺ��ص㣬�����ص�Ĺ���������ramfs����iso9660
         */
        if (0 != strncmp(szFilesystemName, "/dev/", 5) || 0 == strncmp(szFilesystemName, "/dev/loop", 9)
                || (1 == nIsGetOneInfo && 0 == strcmp(szMountPointName, "/"))
                || 5 == strspn(szMountType , "ramfs") || 7 == strspn(szMountType , "iso9660"))
        {
            continue;
        }

        /* ������ص㱻�ظ��������� */
        for (i = 0; i < nTmpMountNum; i++)
        {
            firstInfo = tmpMountInfo + i;
            /* ��ѯ��tmpMountInfo���Ƿ���ڸù��ص��Ӧ����Ϣ������Ѿ����ڣ������tmpMountInfo�иù��ص��Ӧ����Ϣ */
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
            /* ��������ж�������ͨ������˵��������ϢΪ��Ҫ��ӵ�һ���¼�¼��������ӵ���Ӧ�ṹ���У��������ݼ����ۼ� */
            strncpy_s(firstInfo->filesystem, MAX_STR_LEN, szFilesystemName, strlen(szFilesystemName));
            strncpy_s(firstInfo->mountPoint, MAX_STR_LEN, szMountPointName, strlen(szMountPointName));
            nTmpMountNum++;
        }
        nFlag = 0;
        nIsGetOneInfo = 1;
    }

    /* �����ļ�ϵͳ�ظ��������� */
    for (i = 0; i < nTmpMountNum; i++)
    {
        firstInfo = tmpMountInfo + i;

        for (j = 0; j < nMountNum; j++)
        {
            /* ��ѯ��mountInfo���Ƿ���ڸ��ļ�ϵͳ��Ӧ����Ϣ������Ѿ����ڣ������Ӹ�����¼ */
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
        /* ���mountInfo��û�и��ļ�ϵͳ��Ӧ��Ϣ�������һ���¼�¼�����ݼ����ۼ� */
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
 Description: �����豸�������豸�ţ���ȡ��Ӧ���豸���֣��Լ��豸��С
 Input      : struct DevMajorMinor* devMajorMinor    �豸���Ƽ��������豸�Žṹ������
              int partNum                            ��������
              int devID                              �豸�������豸��
 Output     : char* devName                          ���ڵ���
              char* devBlockSize                     ���ڵ�ռ��С
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
        /* ��������豸�Ŷ���/proc/partitions�����һ������˵����ͬһ���豸�����豸�����ݸ����� */
        if (nMajor == tmpDev->devMajor && nMinor == tmpDev->devMinor)
        {
            (void)strncpy_s(szDevName, MAX_STR_LEN, tmpDev->devName, strlen(tmpDev->devName));
            (void)strncpy_s(szBlockSize, MAX_STR_LEN, tmpDev->devBlockSize, strlen(tmpDev->devBlockSize));
            /* �������devName��ΪNULL���������豸�Ŷ�Ӧ���豸�����ݸ����� */
            if (NULL != devName && 0 != strlen(szDevName))
            {
                (void)strncpy_s(devName, strlen(szDevName)+1, szDevName, strlen(szDevName));
            }
            /* �������devBlockSize��ΪNULL���������豸�Ŷ�Ӧ�豸��ʵ�ʿռ��С���ݸ����� */
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
 Description: ��ȡdf�ж�Ӧ����Ϣ���洢���ṹ����
 Input      : struct DevMajorMinor* devMajorMinor     �豸���Ƽ��������豸�Žṹ������
              int partNum                             ��������
 Output     : struct DiskInfo* diskMap                ���ص��Ӧ�ļ�ϵͳ��Ϣ������ʹ�ÿռ�Ľṹ��
              int* pnMountNum                         ���ڴ洢���ش�������
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int getDiskInfo(struct DevMajorMinor *devMajorMinor, int partNum, struct DiskInfo *diskMap, int *pnMountNum)
{
    /* ����֮ǰ�ڵ��ú����У��Ȼ�ȡ��swap����������������Ƚ�swap����������ֵ�����ص��������� */
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

    /* ����getMountInfo��������ȡ���ص��ļ�ϵͳ��Ϣ������(��swap����֮���ۼ�) */
    nResult = getMountInfo(diskMap, &nMountedFsNum);
    if (ERROR == nResult)
    {
        return ERROR;
    }

    /* ѭ�����������������ļ�ϵͳ��Ϣ */
    for (i = 0; i < nMountedFsNum; i++)
    {
        tmpDisk = diskMap + i;
        /* ͨ��stat��������ȡ�ļ�ϵͳ��Ӧ�����豸����Ϣ�����浽�ṹ���Ӧ��Ա�� */
        nResult = stat(tmpDisk->filesystem, &statBuf);
        if (0 != nResult || 0 == statBuf.st_rdev)
        {
            continue;
        }

        /* ��������swap����ʱ��������ͨ�����ص��ȡ����ʹ����Ϣ���� */
        if (*pnMountNum <= i)
        {
            /* ͨ��statfs�����������ļ�ϵͳ���ص㣬��ȡ�ļ�ϵͳ�������Ϣ */
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
            /* ����ܿ�������0������mount��Ϣ�ǿգ�����ص���Ϣ�ļ�ָ��������ִ�����²��� */
            if (statfsInfo.f_blocks > 0)
            {
                /* ��ÿ��Ĵ�С��ֵ��szFsBlockSize�����ļ�ϵͳ��ֵ���ṹ������Ķ�Ӧ��Ա */
                (void)snprintf_s(szFsBlockSize, sizeof(szFsBlockSize), sizeof(szFsBlockSize), "%ld", (long)statfsInfo.f_bsize);
                /* ʣ��free�Ŀ�����С��ֵ����Ӧ�ı��� */
                (void)snprintf_s(szFsFreeBlock, sizeof(szFsFreeBlock), sizeof(szFsFreeBlock), "%lu", statfsInfo.f_bfree);
                /* ʹ���������ַ����˷�������ʣ���С���м��㣬��ת��ΪKB */
                (void)strMulti(szFsFreeBlock, szFsBlockSize, szFreeSize);
                (void)unitTransfer(szFreeSize, UNIT_TRANSFER_CYCLE, szFreeSize);
            }
        }

        tmpDisk->st_rdev = statBuf.st_rdev;
        /* ͨ��getInfoFromID�����������ļ�ϵͳ�������豸�ţ���ȡ��Ӧ������ʵ�ʿռ��С��Ϣ */
        (void)getInfoFromID(devMajorMinor, partNum, statBuf.st_rdev, NULL, szFsSize);
        /* ����ȡ�������ļ�ϵͳʵ�ʿռ��ȥ�ļ�ϵͳʣ����ÿռ䣬�����ļ�ϵͳʹ�ÿռ�(Ϊ�˼����ļ�ϵͳ��ʽռ�ÿռ�) */
        nResult = strMinus(szFsSize, szFreeSize, tmpDisk->usage);
        if (ERROR == nResult)
        {
            return ERROR;
        }
        /* ���е�λת���󣬴���ṹ����Ӧ��Ա�� */
        nResult = unitTransfer(tmpDisk->usage, UNIT_TRANSFER_CYCLE, tmpDisk->usage);
        if (ERROR == nResult)
        {
            return ERROR;
        }

        /* ��һЩ��ʱ���ַ�����Ϣȫ����� */
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
 Description: �ѷ��������е���ĸ�����ַֿ���������ĸ��Ϊ��������
 Input       : char *pPartName    ��������
 Output     : char* pDiskName   �������ڵĴ�������
 Return     : SUCC = success, ERROR = failure
 Other      :  ��LVM������PV Ϊ�������ʱ�������pPartName����pDiskName
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
 Description: ���ݷ�������ʵ���֣��Աȴ��̣������Ϣһ�£����ۼ�ʹ�ÿռ���Ϣ����Ӧ����
 Input      : struct DeviceInfo* linuxDiskUsage       ����������Ƽ���ʹ�ÿռ䡢�ܿռ�Ľṹ������
              int diskNum                             �����������
              char* partitionsName                    �����������ڱȶ��Ƿ������������һ��
              char* partitionsUsage                   ����ʹ����Ϣ�������ۼӵ����̿ռ�
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
        /* ͨ���Աȷ������豸��(��ĩβ������)��������Ƿ�һ�������һ�����򽫷���ʹ����Ϣ�ۼӵ�����ʹ�ÿռ��� */
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
 Description: ��ȡ���ļ�ϵͳ������
 Input      : char *szLine                              dmsetup��ȡ����
 Output     : char *pParentMajor                        ���ڵ����豸��
              char *pParentMinor                        ���ڵ���豸��
              int *pStripCnt                            ���ڵ����
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

    /* ѭ����ȡ����ִ�н�����ļ�ָ�� */
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
 Description: strip����£���ȡ���ļ�ϵͳ�ĸ��ڵ���
 Input      : char *szLine                              dmsetup��ȡ����
 Output     : char *pParentMajor                        ���ڵ����豸��
              char *pParentMinor                        ���ڵ���豸��
              int *pStripCnt                            ���ڵ����
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

    /* ��������0 16777216 striped 2 128 202:160 8390656 202:176 8390656���ַ���������ǰ��5���ַ��� */
    tmp = szLine;
    while (i < 5)
    {
        tmp = strchr(tmp, ' ');
        tmp+=1;
        i++;
    }

    /* ��ʼ����202:160 8390656��ȡ�����ڵ��豸�� */
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
 Description: linear����£���ȡ���ļ�ϵͳ�ĸ��ڵ���
 Input      : struct DevMajorMinor* devMajorMinor       /proc/partitions���豸�����������豸�Žṹ������
              int partNum                               ��Ӧ��ϵ��������
              int devID                                 �ļ�ϵͳ�豸��
 Output     : char* parentName                          ���ڵ���
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
 Description: crypt����£���ȡ���ļ�ϵͳ�ĸ��ڵ���
 Input      : char *szLine                              dmsetup��ȡ����
 Output     : char *pParentMajor                        ���ڵ����豸��
              char *pParentMinor                        ���ڵ���豸��
              int *pStripCnt                            ���ڵ����
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

    /* ���ܸ�ʽֻ��1�����ڵ� */
    *pCnt = 1;
    *pstDmInfo = pDmInfo;
    return SUCC;
}

/*****************************************************************************
 Function   : getDmParentNode()
 Description: �����ļ�ϵͳ�豸�ţ���ȡ���ļ�ϵͳ�ĸ��ڵ���
 Input      : struct DevMajorMinor* devMajorMinor       /proc/partitions���豸�����������豸�Žṹ������
              int partNum                               ��Ӧ��ϵ��������
              int devID                                 �ļ�ϵͳ�豸��
 Output     : char* parentName                          ���ڵ���
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
            /* ��ʱ���سɹ����������������� */
            return SUCC;
        }
    }

    for (i = 0; i < nParentCnt && usage > 0; i++)
    {
        /* ��������nParentCnt=1�������黯��������ͬ���� */
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
                /* ͨ��addDiskUsageToDevice���������������ķ���ʹ�ÿռ��ۼӵ���Ӧ�Ĵ����� */
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
 Description: ���еķ�����Ϣ�ۼӵ���Ӧ���̵�ʹ�ÿռ���
 Input      : struct DevMajorMinor* devMajorMinor      �豸�����������豸�Žṹ������
              struct DeviceInfo* linuxDiskUsage        ����������Ƽ���ʹ�ÿռ䡢�ܿռ�Ľṹ������
              struct DiskInfo diskMap                  ���ص��Ӧ�ļ�ϵͳ��Ϣ������ʹ�ÿռ�Ľṹ��
              struct NumberCount numberCount           ����������������������Լ����ص��Ӧ�ļ�ϵͳ��Ϣ�������Ľṹ��
 Output     : struct DeviceInfo* linuxDiskUsage        ��������̵�ʹ�ÿռ��ȡ����
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
    /* ���drbd�豸��Ӧ�ĸ��豸���������豸�� */
    struct stat statBuf;
    long long usage = 0L;

    for (i = 0; i < numberCount.mountNum; i++)
    {
        tmpDisk = diskMap + i;
        usage = atoll(tmpDisk->usage);

        /* ��mount��¼���ļ�ϵͳ��drbd�豸ʱ����Ҫ�ҵ���Ӧ�ĸ��豸 */
        if (NULL != strstr(tmpDisk->filesystem, "drbd"))
        {
            (void)getDrbdParent(tmpDisk->filesystem, tmpDisk->filesystem);
            /* ͨ��stat��������ȡdrbd�豸��Ӧ�ĸ��豸���������豸����Ϣ�����浽�ṹ���Ӧ��Ա�� */
            nResult = stat(tmpDisk->filesystem, &statBuf);
            if (0 != nResult || 0 == statBuf.st_rdev)
            {
                continue;
            }
            /* ����mount��¼�е������豸�� */
            tmpDisk->st_rdev = statBuf.st_rdev;
        }

        if (g_deviceMapperNum == major(tmpDisk->st_rdev))
        {
            /* ͨ��dmsetup����ȡ�߼����Ӧ���豸�� */
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
                /* ͨ��addDiskUsageToDevice���������������ķ���ʹ�ÿռ��ۼӵ���Ӧ�Ĵ����� */
                (void)addDiskUsageToDevice(linuxDiskUsage, numberCount.diskNum, szParentName, tmpDisk->usage);
                (void)memset_s(szParentName, MAX_NAME_LEN, 0, sizeof(szParentName));
            }
        }
    }

    return SUCC;
}

/*****************************************************************************
 Function   : freeSpace()
 Description: �ͷ������ṹ�����뵽�Ŀռ�
 Input      : N/A
 Output     : struct DevMajorMinor* devMajorMinor   �豸�����������豸�Žṹ������
            ��struct DeviceInfo* linuxDiskUsage     ����������Ƽ���ʹ�ÿռ䡢�ܿռ�Ľṹ������
            ��struct DiskInfo* diskMap              ���ص��Ӧ�ļ�ϵͳ��Ϣ������ʹ�ÿռ�Ľṹ��
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
 Description: ��ô���������
 Input      : struct xs_handle* handle   handle of xenstore
 Output     : char* pszDiskUsage   �洢���������ʵ��ַ���
 Return     : SUCC = success, ERROR = failure
 Other      : N/A
 *****************************************************************************/
int getDiskUsage(struct xs_handle *handle, char pszDiskUsage[][MAX_DISKUSAGE_LEN], int *row_num)
{
    /* �洢�ܴ��̿ռ���Ϣ�ַ���(��λΪMB) */
    char szTotalSize[MAX_DISKUSAGE_LEN] = {0};
    /* �洢��ʹ�ÿռ���Ϣ�ַ���(��λΪMB) */
    char szTotalUsage[MAX_DISKUSAGE_LEN] = {0};
    /* �洢ÿ��������Ϣ���ӵ��ַ��� */
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

    /* �����Ӧdevice-mapper���͵����豸��Ϊ0�������getDeviceMapperNumber��������ȡ�����豸�� */
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
     * ����getPartitionsInfo��������ȡ���̣������Լ���Ӧ�Ĵ����ܿռ��С��KB���������豸����Ϣ��
     * ����ȡ��Ӧ�������豸����ռ��С����fdisk��Ӧ��Ϣ
     */
    nResult = getPartitionsInfo(handle, devMajorMinor, &numberCount.partNum, linuxDiskUsage, &numberCount.diskNum);
    if (ERROR == nResult || 0 == numberCount.partNum || 0 == numberCount.diskNum)
    {
        freeSpace(devMajorMinor, linuxDiskUsage, diskMap);
        return ERROR;
    }

    /* ����getSwapInfo��������ȡ��ǰswap������Ϣ */
    nResult = getSwapInfo(diskMap, &numberCount.mountNum);
    if (ERROR == nResult)
    {
        freeSpace(devMajorMinor, linuxDiskUsage, diskMap);
        return ERROR;
    }
    nSwapNum = numberCount.mountNum;

    /* ����getDiskInfo��������ȡ��ǰ���ص��ļ�ϵͳ�����ص㣬���豸���Լ�ʹ�ÿռ���Ϣ */
    nResult = getDiskInfo(devMajorMinor, numberCount.partNum, diskMap, &numberCount.mountNum);
    if (ERROR == nResult || nSwapNum >= numberCount.mountNum)
    {
        freeSpace(devMajorMinor, linuxDiskUsage, diskMap);
        return ERROR;
    }

    /* ����getAllDeviceUsage������ͨ����ȡ���Ĺ��ص��ļ�ϵͳʹ����Ϣ�����Ҷ�Ӧ�ĸ��豸�������Ӧ���̵�ʹ������Ϣ */
    nResult = getAllDeviceUsage(devMajorMinor, linuxDiskUsage, diskMap, numberCount);
    if (ERROR == nResult)
    {
        freeSpace(devMajorMinor, linuxDiskUsage, diskMap);
        return ERROR;
    }

    /* ѭ����ȡ�ṹ�����ݵ�ÿ����Ա */
    for (i = 0; i < numberCount.diskNum; i++)
    {
        /* ����ȡ��������Ϣת��Ϊ�ַ��������Ҽ������ܿռ��С */
        (void)strAdd(linuxDiskUsage[i].deviceTotalSpace, szTotalSize, szTotalSize);
        (void)strAdd(linuxDiskUsage[i].diskUsage, szTotalUsage, szTotalUsage);
        //���ֻƴ��61����������Ϣ(1������+60������)
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

    /* ���ܵĴ��̿ռ��ʹ�ÿռ��Լ����յ�ÿ�����̵�ʹ���ʶ�Ӧ���ַ�����������ֵ������ */
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
    /*�ͻ��ṩ��shell���ʽ������shell�����ȡ�ļ�ϵͳ���ƣ��ܴ�С�����ô�С*/
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
    /*�������ٽ�ֵ��num+1;����:33/32=1;����1���ֽڣ�����Ҫ����д*/
    if(exceedflag)
    {
       num += 1; 
    }
    /*����xenstore��ֵ���ܳ�����filesystem_extra%d��ֵд*/
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
 Description: ����getdiskusage���󣬽�������������Ϣд��xenstore��
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

    /* ���ú������ش����������ַ��� */
    nRet = getDiskUsage(handle, szDiskUsage, &row_num);
    if (ERROR == nRet)
    {
        /*ʧ��д��error*/
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
