/*
 * Obtains the IP addr the number of packets received or send by a NIC, and
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


#include "libxenctl.h"
#include "public_common.h"
#include "securec.h"
#include <ctype.h>
#include "uvpmon.h"

#define NIC_MAX  15
//#define SRIOV_NIC  "00:ff:ff"
#define NORMAL_NIC  "fe:ff:ff"
//#define SRIOVNIC_PATH "control/uvp/sriov_mac"
#define VIF_MAX 7
#define UPFLAG  1
#define DOWNFLAG 0
#define MAX_NICINFO_LENGTH 256
#define MAX_COMMAND_LENGTH 128
typedef struct
{
    char  ifname[16];
    char  mac[18];
    char  ip[16];
    char  tp[64];
    char  packs[64];
    char  gateway[16];
    int   GmnExFlag;
    long   sentdrop;
    long   recievedrop;
    
} VIF_DATA;

typedef struct
{
    int count;
    VIF_DATA info[NIC_MAX];

} VIF_INFO;

VIF_INFO gtNicInfo;

#define MAC_NAME_LENGTH 18
#define VIF_NAME_LENGTH 16
VIF_INFO gtNicInfo_bond;
typedef struct
{
    char  vifname[VIF_NAME_LENGTH];
    char  mac[MAC_NAME_LENGTH];
    char  sriov_vifname[VIF_NAME_LENGTH];
    char  sriov_mac[MAC_NAME_LENGTH];
    int release_count;
    char  bondname[VIF_NAME_LENGTH];
} VIF_BOUND_DATA;

typedef struct
{
    int count;
    VIF_BOUND_DATA info[NIC_MAX];

} VIF_BOUND_INFO;

VIF_BOUND_INFO BoundInfo;
extern int uvpPopen(const char *pszCmd, char *pszBuffer, int size);
extern FILE *openPipe(const char *pszCommand, const char *pszType);

/*****************************************************************************
Function   : opendevfile
Description: open the dev file
Input       :None
Output     : None
Return     : fd
*****************************************************************************/
FILE *opendevfile(const char *path)
{
    if (NULL == path)
    {
        return NULL;
    }
    return fopen(path, "r");
}

/*****************************************************************************
Function   : NetworkDestroy
Description: network model destroy
Input      : sktd : socket句柄
Output     : None
Return     : None
*****************************************************************************/
void NetworkDestroy(int sktd)
{
    if (ERROR != sktd)
    {
        close(sktd);
    }
}
/*****************************************************************************
Function   : GetVifName
Description: get the vif name
Input      : namep p
Output     : None
Return     : p
Remark     : ifconfig 源码里的代码，抓取所有网卡设备
*****************************************************************************/
char *GetVifName(char **namep, char *p)
{
    int count = 0;
    while (isspace(*p))
        p++;
    char *name = *namep = p;
    while (*p) {
        if (isspace(*p))
            break;
        if (*p == ':') {        /* could be an alias */
            char *dot = p, *dotname = name;
            *name++ = *p++;
            count++;
            while (isdigit(*p)){
                *name++ = *p++;
                count++;
                if (count == (IFNAMSIZ-1))
                      break;
            }
            if (*p != ':') {    /* it wasn't, backup */
                p = dot;
                name = dotname;
            }
            if (*p == '\0')
                return NULL;
            p++;
            break;
        }
        *name++ = *p++;
        count++;
        if (count == (IFNAMSIZ-1))
              break;
    }
    *name++ = '\0';
    return p;
}


/*****************************************************************************
+Function   : GetVifGateway
+Description: get the NIC gateway address
+Input       :ifname   -- the NIC name			  
+Output     : None
+Return     : success : return gateway address,  fail : return error or disconnected
+*****************************************************************************/
int GetVifGateway(int skt, const char *ifname)
{
    char pathBuf[MAX_NICINFO_LENGTH] = {0};
    char pszGateway[VIF_NAME_LENGTH] = {0};
    char pszGatewayBuf[VIF_NAME_LENGTH] = {0};
    FILE *iRet;
    (void)memset_s(pathBuf, MAX_NICINFO_LENGTH, 0, MAX_NICINFO_LENGTH);
    /*比拼时提供的shell命令，通过route -n获取网关信息*/
    (void)snprintf_s(pathBuf, MAX_NICINFO_LENGTH,  MAX_NICINFO_LENGTH,
                    "route -n | grep -i \"%s$\" | grep UG | awk '{print $2}'", ifname);
    iRet = openPipe(pathBuf, "r");
    if (NULL == iRet)
    {
       DEBUG_LOG("Failed to exec route shell command.");
       gtNicInfo.info[gtNicInfo.count].gateway[0] = '\0';
       return ERROR;
    }
    /*保存读取的网关信息*/
    if(NULL != fgets(pszGatewayBuf,sizeof(pszGatewayBuf),iRet))
    {
       (void)sscanf_s(pszGatewayBuf,"%s",pszGateway,sizeof(pszGateway));
    }
    trim(pszGateway);
    /*没网关信息则置0*/
    if(strlen(pszGateway) < 1)
    {      
       pszGateway[0]='0';
       
       pszGateway[1]='\0';
    }
    (void)pclose(iRet);
    (void)strncpy_s(gtNicInfo.info[gtNicInfo.count].gateway, 16, pszGateway, strlen(pszGateway));
    return SUCC;
}
/*****************************************************************************
Function   : GetVifFlag
Description: get the NIC flags
Input       : ifname   -- the NIC name
Output     : None
Return     : SUCC or ERROR
*****************************************************************************/
int GetVifFlag(int skt, const char *ifname)
{
    struct ifreq  ifreq;

    if (NULL == ifname)
    {
        return ERROR;
    }
    (void)strncpy_s(ifreq.ifr_name, IFNAMSIZ, ifname, IFNAMSIZ-1);
    ifreq.ifr_name[IFNAMSIZ - 1] = '\0';

    if (!(ioctl(skt, SIOCGIFFLAGS, (char *) &ifreq)))
    {
        /* 判断网卡状态 */
        if (ifreq.ifr_flags & IFF_UP)
        {
            return UPFLAG;
        }
    }
    return DOWNFLAG;
}


/*****************************************************************************
Function   : GetVifIp
Description: get the NIC ip address
Input       :ifname   -- the NIC name
Output     : None
Return     : success : return ip address,  fail : return error or disconnected
*****************************************************************************/
int GetVifIp(int skt, const char *ifname)
{
    struct ifreq ifrequest;
    struct sockaddr_in *pAddr;

    (void)strncpy_s(ifrequest.ifr_name, IFNAMSIZ, ifname, IFNAMSIZ-1);
    ifrequest.ifr_name[IFNAMSIZ - 1] = '\0';



    if (!(ioctl(skt, SIOCGIFFLAGS, (char *) &ifrequest)))
    {
        /* 判断网卡状态 */
        if (ifrequest.ifr_flags & IFF_UP)
        {
            if ( ! (ioctl(skt, SIOCGIFADDR, (char *) &ifrequest) ) )
            {
                pAddr = (struct sockaddr_in *) (&ifrequest.ifr_addr);
                strncpy_s(gtNicInfo.info[gtNicInfo.count].ip, 
                    16, 
                    inet_ntoa( (pAddr->sin_addr ) ), 
                    strlen(inet_ntoa( (pAddr->sin_addr ) )));
                return SUCC;
            }
        }
    }

    return ERROR;

}


/*****************************************************************************
Function   : GetFlux
Description: get the NIC ThroughPut
Input       :ifname   -- the NIC name
Output     : None
Return     : success : return ThroughPut string,  fail : return error or disconnected
*****************************************************************************/
int GetFlux(int skt, char *ifname)
{

    char Sent[31] = {0};
    char Recived[31] = {0};
    char SentPkt[31] = {0};
    char RecivedPkt[31] = {0};
    char SentPktDrop[31] = {0};
    char RecivedPktDrop[31] = {0};
    char *ptmp = NULL;
    char line[255] = {0};
    char *foundStr = NULL;

    if (NULL == ifname)//for pclint warning
    {
    	DEBUG_LOG("ifname is NULL.");
        return ERROR;
    }

    if (ERROR == getFluxinfoLine(ifname, line))
    {
        strncpy_s(gtNicInfo.info[gtNicInfo.count].tp, 64, ERR_STR, strlen(ERR_STR));
        gtNicInfo.info[gtNicInfo.count].tp[strlen(ERR_STR)] = '\0';
        DEBUG_LOG("getFluxinfoLine is ERROR.");
        return ERROR;
    }

    if(0 == strlen(line))
    {
    	DEBUG_LOG("line is NULL.");
        return ERROR;
    }

    foundStr = strstr(line, ifname);
    if (NULL == foundStr)
    {
    	DEBUG_LOG("foundStr is NULL.");
        return ERROR;
    }
    /*找到第一个数据项*/
    ptmp = foundStr + strlen(ifname) + 1;


    if (NULL == ptmp)
    {
    	DEBUG_LOG("ptmp is NULL.");
        return ERROR;
    }

    /*找到第一项和第九项分别是接收数据量和发送数据量*/
    if(ERROR == getVifData(ptmp, 1, Recived)
            || ERROR ==  getVifData(ptmp, 2, RecivedPkt)
	    || ERROR == getVifData(ptmp, 4, RecivedPktDrop)
            || ERROR == getVifData(ptmp, 9, Sent)
            || ERROR == getVifData(ptmp, 10, SentPkt)
            || ERROR == getVifData(ptmp, 12, SentPktDrop))
    {
    	DEBUG_LOG("getVifData is ERROR.");
        return ERROR;
    }

    (void)snprintf_s(gtNicInfo.info[gtNicInfo.count].tp, 
                        sizeof(gtNicInfo.info[gtNicInfo.count].tp), 
                        sizeof(gtNicInfo.info[gtNicInfo.count].tp), 
                        "%s:%s", Sent, Recived);
    gtNicInfo.info[gtNicInfo.count].tp[strlen(Sent) + strlen(Recived) + 1] = '\0';

    (void)snprintf_s(gtNicInfo.info[gtNicInfo.count].packs, 
        sizeof(gtNicInfo.info[gtNicInfo.count].packs), 
        sizeof(gtNicInfo.info[gtNicInfo.count].packs), 
        "%s:%s", SentPkt, RecivedPkt);
    gtNicInfo.info[gtNicInfo.count].packs[strlen(SentPkt) + strlen(RecivedPkt) + 1] = '\0';
    (void)sscanf_s(SentPktDrop,"%ld", &gtNicInfo.info[gtNicInfo.count].sentdrop);
    (void)sscanf_s(RecivedPktDrop,"%ld",&gtNicInfo.info[gtNicInfo.count].recievedrop);

    return SUCC;
}

/*****************************************************************************
Function   : getData
Description: 得到每行配置信息的列数据
Input       :pline:网卡数据的一行
Output     : out 输出数据的值
Return     :
*****************************************************************************/
int getVifData(char *pline, int nNumber, char *out)
{
    char *ptr = NULL;
    int i;

    if(NULL == pline || NULL == out)
    {
    	DEBUG_LOG("pline or out is NULL.");
        return ERROR;
    }

    for(i = 1; (0 != *pline) && (i <= nNumber); i++)
    {
        /*略去空格*/
        while(*pline == ' ')
        {
            pline++;
        }

        /*找个空格首次出现的位置，下一列的开头*/
        ptr = strchr(pline, ' ');
        if(NULL == ptr)
        {
        	DEBUG_LOG("ptr is NULL.");
            return ERROR;
        }
        /*找到已接收的数据列*/
        if ((i == nNumber) && (NULL != ptr))
        {
            strncpy_s(out, ptr - pline + 1, pline, ptr - pline);
        }
        pline = ptr;
    }

    return SUCC;
}
/*****************************************************************************
Function   : getFluxinfoLine
Description: 得到配置文件的符合要查找的行
Input       :ifname   -- the NIC name
Output     : pline: 传出有符合条件的行buffer
Return     : success :
                fail :
*****************************************************************************/
int getFluxinfoLine(char *ifname, char *pline)
{
    char *path = "/proc/net/dev";
    FILE *file = NULL;
    char *begin = NULL;
    int len = 0;

    if(NULL == ifname || NULL == pline)
    {
    	DEBUG_LOG("ifname or pline is NULL.");
        return ERROR;
    }

    if(NULL == (file = opendevfile(path)))
    {
    	DEBUG_LOG("getFluxinfoLine:failed to open /proc/net/dev.");
        return ERROR;
    }

    len = strlen(ifname);

    while (fgets(pline, 255, file))
    {
        begin = pline;
        //去掉空格
        while (' ' == *begin)  begin++;
        //如果找到匹配网卡的名字退出
        if (0 == strncmp(begin, ifname, len))
        {
            if (':' == *(begin + len)  )
            {
                break;
            }
        }

        //重置pline
        memset_s(pline, 255, 0, 255);
    }

    fclose(file);

    return SUCC;
}

/*****************************************************************************
Function   : openNetSocket
Description: open the network socket connect
Input       :None
Output     : None
Return     : success : socket handle,  fail : ERROR
*****************************************************************************/
int openNetSocket(void)
{
    return socket(AF_INET, SOCK_DGRAM, 0);

}


/*****************************************************************************
Function   : GetVifMac
Description: get the NIC MAC address
Input       :ifname   -- the NIC name
Output     : None
Return     : None
*****************************************************************************/
void GetVifMac(int skt, const char *ifname)
{
    struct ifreq ifrequest;

    if (NULL == ifname)
    {
        return ;
    }

    strncpy_s(ifrequest.ifr_name, IFNAMSIZ, ifname, IFNAMSIZ-1);
    ifrequest.ifr_name[IFNAMSIZ - 1] = '\0';

    if ( ! (ioctl(skt, SIOCGIFHWADDR, (char *) &ifrequest) ) )
    {
        (void)snprintf_s(gtNicInfo.info[gtNicInfo.count].mac, 
                        sizeof(gtNicInfo.info[gtNicInfo.count].mac), 
                        sizeof(gtNicInfo.info[gtNicInfo.count].mac), 
                        "%02x:%02x:%02x:%02x:%02x:%02x",
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[0],
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[1],
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[2],
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[3],
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[4],
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[5]);

        (void)snprintf_s(gtNicInfo.info[gtNicInfo.count].ifname, 
                        sizeof(gtNicInfo.info[gtNicInfo.count].ifname),
                        sizeof(gtNicInfo.info[gtNicInfo.count].ifname),
                        "%s", ifname);
    }
    return ;

}


/*****************************************************************************
Function   : GetVifInfo
Description: 获取网卡类信息
Input       : None
Output     : None
Return     : Count [0~7]
*****************************************************************************/
int GetVifInfo()
{
	/* ifconf通常是用来保存所有接口信息的 */
	//struct ifconf ifconfigure;
	char *path="/proc/net/dev";
	FILE *file;
	char *line = NULL;
	size_t linelen = 0;
	//char buf[4096];
	int num = 0;
	int skt;

	skt = openNetSocket();
	if (ERROR == skt)
	{
		DEBUG_LOG("GetVifInfo:failed to openNetSocket.");
		return ERROR;
	}

	/* 初始化ifconf */
	//ifconfigure.ifc_len = 4096;
	//ifconfigure.ifc_buf = buf;

	memset_s(&gtNicInfo, sizeof(gtNicInfo), 0, sizeof(gtNicInfo));

	/*  control device which name is NIC */
	if(NULL == (file = opendevfile(path)))
	{
		NetworkDestroy(skt);
		DEBUG_LOG("GetVifInfo:failed to open /proc/net/dev.");
		return ERROR;
	}
	/*去掉/proc/net/dev文件的前面两行(表头信息)*/
	if (getline(&line, &linelen, file) == -1 /* eat line */
	|| getline(&line, &linelen, file) == -1) 
	{
		DEBUG_LOG("GetVifInfo:remove /proc/net/dev head");
	}
	/*按行遍历剩下的文本信息*/
	while(getline(&line, &linelen, file) != -1)
	{
		char *namebuf;
		(void)GetVifName(&namebuf, line);
		/*info bond*/
		if (NULL != strstr(namebuf, "bond"))
		{
			DEBUG_LOG("has bond model.");
			continue;
		}
		if (NULL != strstr(namebuf, "eth") || NULL != strstr(namebuf, "Gmn"))
		{
			/*if interface is Gmn br, GmnExFlag = 1*/
			if(NULL != strstr(namebuf, "Gmn"))
			{
				gtNicInfo.info[gtNicInfo.count].GmnExFlag = 1;
			}
			else
			{
				gtNicInfo.info[gtNicInfo.count].GmnExFlag = 0;
			}

			if (UPFLAG == GetVifFlag(skt, namebuf))
			{
				num++;
				GetVifMac(skt, namebuf);
				if (ERROR == GetVifIp(skt, namebuf))
				{
					gtNicInfo.info[gtNicInfo.count].gateway[0] = '\0';
					(void)strncpy_s(gtNicInfo.info[gtNicInfo.count].ip, 16, "none", strlen("none"));
					//end by '\0'
					gtNicInfo.info[gtNicInfo.count].ip[sizeof(gtNicInfo.info[gtNicInfo.count].ip)-1]='\0';
				}

				if(g_exinfo_flag_value & EXINFO_FLAG_GATEWAY)
				{
					(void)GetVifGateway(skt, namebuf);
				}
				else
				{
					(void)strncpy_s(gtNicInfo.info[gtNicInfo.count].gateway, 16, "0", strlen("0"));
				}

				if (ERROR == GetFlux(skt, namebuf))
				{
					continue;
				}

			}
			else
			{
				GetVifMac(skt, namebuf);
				num++;
			}
		}

		gtNicInfo.count=num;   
		if (num >= NIC_MAX)
		{
			DEBUG_LOG("GetVifInfo only support 15 nics ");
			break;
		}     
	}
	fclose(file);
	free(line);
	NetworkDestroy(skt);
	return gtNicInfo.count;
}


/*****************************************************************************
Function   : networkctlmon
Description: 网络功能处理入口
Input       :handle   -- xenstore句柄
Output     : 向xenstore写入网卡类信息
Return     : SUCC OR ERROR
*****************************************************************************/
void networkctlmon(void *handle)
{
	char ArrRet[1024] = {0};
	char ArrRet1[1024] = {0}; 
	char NetworkLoss[32] = {0};
	int num;
	int i;
	int j = 0;
	long sumrecievedrop = 0;
	long sumsentdrop = 0;
	VIF_DATA stVifDataTemp;
	memset_s(&stVifDataTemp, sizeof(stVifDataTemp), 0, sizeof(stVifDataTemp));

	num = GetVifInfo();
	if (ERROR == num)
	{
		write_to_xenstore(handle, VIF_DATA_PATH, "error");
		write_to_xenstore(handle, VIFEXTRA_DATA_PATH, "error");
		DEBUG_LOG("GetVifInfo:num is ERROR.");
		return;
	}
	/* 向xenstore写入字符0 */
	if (0 == num)
	{
		write_to_xenstore(handle, VIF_DATA_PATH, "0");
		write_to_xenstore(handle, VIFEXTRA_DATA_PATH, "0");
		return;
	}

	/*for Gmn br info*/
	for (i = 0; i < num; i++)
	{
		if(1 == gtNicInfo.info[i].GmnExFlag)
		{
			if(i == j)
			{
				j++;
				continue;
			}

			//temp=i
			(void)memcpy_s(&stVifDataTemp,sizeof(VIF_DATA),&gtNicInfo.info[i],sizeof(VIF_DATA));
			gtNicInfo.info[i].GmnExFlag = 0;

			//i=j
			(void)memcpy_s(&gtNicInfo.info[i],sizeof(VIF_DATA),&gtNicInfo.info[j],sizeof(VIF_DATA));
			gtNicInfo.info[i].GmnExFlag = 0;

			//j=temp
			(void)memcpy_s(&gtNicInfo.info[j],sizeof(VIF_DATA),&stVifDataTemp,sizeof(VIF_DATA));
			gtNicInfo.info[j].GmnExFlag = 0;

			j++;
		}
	}

	for (i = 0; i < num; i++)
	{
		/*计算所有网卡的收发包丢数*/
		if(g_exinfo_flag_value & EXINFO_FLAG_NET_LOSS)
		{
			sumsentdrop += gtNicInfo.info[i].sentdrop;
			sumrecievedrop += gtNicInfo.info[i].recievedrop;
		}
		if(i < VIF_MAX)
		{
			if (0 != strlen(gtNicInfo.info[i].ip))
			{
				(void)snprintf_s(ArrRet + strlen(ArrRet), 
				sizeof(ArrRet) - strlen(ArrRet),
				sizeof(ArrRet) - strlen(ArrRet),
				"[%s-1-%s-%s-<%s>-<%s>]",
				gtNicInfo.info[i].mac,
				gtNicInfo.info[i].ip,
				trim(gtNicInfo.info[i].gateway),
				gtNicInfo.info[i].tp,
				gtNicInfo.info[i].packs);

			}        
			else
			{
				(void)snprintf_s(ArrRet + strlen(ArrRet), 
				sizeof(ArrRet) - strlen(ArrRet),
				sizeof(ArrRet) - strlen(ArrRet),
				"[%s-0]",gtNicInfo.info[i].mac);
			}
		}
		else
		{
			if (0 != strlen(gtNicInfo.info[i].ip))
			{
				(void)snprintf_s(ArrRet1 + strlen(ArrRet1), 
				sizeof(ArrRet1) - strlen(ArrRet1),
				sizeof(ArrRet1) - strlen(ArrRet1),
				"[%s-1-%s-%s-<%s>-<%s>]",
				gtNicInfo.info[i].mac,
				gtNicInfo.info[i].ip,
				trim(gtNicInfo.info[i].gateway),
				gtNicInfo.info[i].tp,
				gtNicInfo.info[i].packs
				);
			}       
			else
			{
				(void)snprintf_s(ArrRet1 + strlen(ArrRet1), 
				sizeof(ArrRet1) - strlen(ArrRet1),
				sizeof(ArrRet1) - strlen(ArrRet1),
				"[%s-0]",gtNicInfo.info[i].mac);
			}
		}
	}
	if(g_exinfo_flag_value & EXINFO_FLAG_NET_LOSS)
	{
		(void)snprintf_s(NetworkLoss, sizeof(NetworkLoss), sizeof(NetworkLoss), "%ld:%ld",sumrecievedrop, sumsentdrop);
	}

	if(xb_write_first_flag == 0)
	{
		write_to_xenstore(handle, VIF_DATA_PATH, ArrRet);
		/*网卡数目小于等于7的场景下，第二个网卡键值信息置0*/
		if (VIF_MAX >= num)
		{
			write_to_xenstore(handle, VIFEXTRA_DATA_PATH, "0");
		}
		/*网卡数目大于7的场景下，第二个网卡键值信息写超过7张网卡部分的信息*/
		else
		{
			write_to_xenstore(handle, VIFEXTRA_DATA_PATH, ArrRet1);
		}
		if(g_exinfo_flag_value & EXINFO_FLAG_NET_LOSS)
		{
			write_to_xenstore(handle, VIF_DROP_PATH, NetworkLoss);
		}
	}
	else
	{
		write_weak_to_xenstore(handle, VIF_DATA_PATH, ArrRet);
		if (VIF_MAX >= num)
		{
			write_weak_to_xenstore(handle, VIFEXTRA_DATA_PATH, "0");
		}
		else
		{
			write_weak_to_xenstore(handle, VIFEXTRA_DATA_PATH, ArrRet1);
		}
		if(g_exinfo_flag_value & EXINFO_FLAG_NET_LOSS)
		{
			write_weak_to_xenstore(handle, VIF_DROP_PATH, NetworkLoss);
		}
	}
	return;
}


/*****************************************************************************
Function   : left
Description: 从左边开始截取字符串
Input       :
Output     : 
Return     : 
*****************************************************************************/
char * left(char *dst,char *src, unsigned int n)
{
    char *p = src;
    char *q = dst;
    unsigned int len = strlen(src);
    if(n>len) n = len;
    /*p += (len-n);*/   /*从右边第n个字符开始*/
    while(n--) 
    {
    	*(q++) = *(p++);
    }
    *(q++)='\0'; /*有必要吗？很有必要*/
    return dst;
}
/*****************************************************************************
Function   : right
Description: 从右边开始截取字符串
Input       :
Output     : 
Return     : 
*****************************************************************************/
/*从字符串的右边截取n个字符*/
char * right(char *dst,char *src, unsigned int n)
{
    char *p = src;
    char *q = dst;
    unsigned int len = strlen(src);
    if(n>len) n = len;
    p += (len-n);   /*从右边第n个字符开始*/
    //while(*(q++) = *(p++));
    while (*p != '\0')
    {
          *(q++) = *(p++);
    }  
    return dst;
}

/*****************************************************************************
Function   : getlineinfo
Description: 分割bond配置文件中的字符串
Input       :
Output     : 
Return     : SUCC OR ERROR
*****************************************************************************/
int getlineinfo(char *dst,char *src, unsigned int n,char c)
{
    char *p = src;
    char *q = dst;
    char chr_tem;
    int count = 0;
    unsigned int len = strlen(src);
    if(n>len) n = len;
    /*p += (len-n);*/   /*从右边第n个字符开始*/
    while(n--) 
    {
    	chr_tem = *(p++);
    	if(chr_tem == c) 
    	{
    		*(q++)='\0';
    		break;
    	}
    	*(q++) = chr_tem;
    	count++;
    }
    *(q++)='\0'; /*有必要吗？很有必要*/
    return count;
}
void InitBond()
{
    memset_s(&gtNicInfo_bond, sizeof(gtNicInfo_bond), 0, sizeof(gtNicInfo_bond));
    memset_s(&BoundInfo, sizeof(BoundInfo), 0, sizeof(BoundInfo));
}
/*****************************************************************************
Function   : getVifInfo_forbond
Description: 读取不同网卡名的mac地址
Input       :
Output     : 
Return     : SUCC OR ERROR
*****************************************************************************/
int  getVifInfo_forbond()
{
  int skfd;
  FILE *fp;
  char *line = NULL;
  size_t linelen = 0;
  char left_ifname[10] = {0};   
  struct ifreq ifr;
  fp =  opendevfile("/proc/net/dev");
  if(NULL == fp )
  {
    return -1;
  }
  skfd=socket(AF_INET, SOCK_DGRAM, 0);
  
  if (ERROR == skfd)
  {
        DEBUG_LOG("getVifInfo_forbond:failed to openNetSocket.");
        fclose(fp);
        return ERROR;
  }
  
  if (getline(&line, &linelen, fp) == -1 /* eat line */
        || getline(&line, &linelen, fp) == -1) 
  {
       DEBUG_LOG("eat two lines \n");
  }
  
  gtNicInfo_bond.count = 0;
  while (getline(&line, &linelen, fp) != -1)
  {
    char *s, *name;
    s = GetVifName(&name, line);
    if(NULL == s)
    {
        INFO_LOG("GetVifName is NULL  %s",s);
    }
    INFO_LOG("getVifInfo_forbond vif name is %s",name);
    if (strcmp(name, "lo") == 0 || strcmp(name, "sit0") == 0)
    {
       continue;
    }
    memset_s(left_ifname,10,0,10);
    left(left_ifname,name,4);
    if(strcmp(left_ifname, "bond") == 0)
    {         
        memset_s(left_ifname,10,0,10);
        continue;
    }
    memset_s(left_ifname,10,0,10);
    left(left_ifname,name,6);
    if(strcmp(left_ifname, "virbr0") == 0)
    {
        memset_s(left_ifname,10,0,10);
        continue;
    }
    strncpy_s(ifr.ifr_name, IFNAMSIZ, name, IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if (!(ioctl(skfd, SIOCGIFHWADDR, (char *)&ifr)))
    {
      
      (void)snprintf_s(gtNicInfo_bond.info[gtNicInfo_bond.count].mac, 
                        sizeof(gtNicInfo_bond.info[gtNicInfo_bond.count].mac), 
                        sizeof(gtNicInfo_bond.info[gtNicInfo_bond.count].mac), 
                        "%02x:%02x:%02x:%02x:%02x:%02x",
                       (unsigned char)ifr.ifr_hwaddr.sa_data[0],
                       (unsigned char)ifr.ifr_hwaddr.sa_data[1],
                       (unsigned char)ifr.ifr_hwaddr.sa_data[2],
                       (unsigned char)ifr.ifr_hwaddr.sa_data[3],
                       (unsigned char)ifr.ifr_hwaddr.sa_data[4],
                       (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

        (void)snprintf_s(gtNicInfo_bond.info[gtNicInfo_bond.count].ifname, 
                        sizeof(gtNicInfo_bond.info[gtNicInfo_bond.count].ifname), 
                        sizeof(gtNicInfo_bond.info[gtNicInfo_bond.count].ifname), 
                        "%s", name);
        gtNicInfo_bond.count++;
    }
    if(gtNicInfo_bond.count >= NIC_MAX)
    {
         INFO_LOG("the monitor only support 15 nics  gtNicInfo_bond.count = %d",gtNicInfo_bond.count);
         //goto END;
         break;
    }
  }
  if(line)
  {
      free(line);
  }
  fclose(fp);
  NetworkDestroy(skfd);
  return gtNicInfo_bond.count;

}

/*****************************************************************************
Function   : getXenVif
Description: 判断传入的网卡名是否是 xen  的网卡
Input       :
Output     : 
Return     : SUCC OR ERROR
*****************************************************************************/
int getXenVif(char *vifname)
{
    FILE *pFileVer;
    char  vifdriver[1024] = {0};
    char CurrentPath[128] = {0};
    if( NULL == vifname )
    {
        return 0; 
    }
    (void)snprintf_s(CurrentPath, 128, 128, "/sys/class/net/%s/device/modalias", vifname);
    pFileVer =  opendevfile(CurrentPath);
    if (NULL == pFileVer)
    {            
        return 0; 
    }
    (void)fgets(vifdriver, 1024, pFileVer);
    if(strstr((char *)vifdriver, "xen:vif"))
    {         
        INFO_LOG("this is a nomorl nic name = %s",vifdriver);
        fclose(pFileVer);
        return 1;
    }
    fclose(pFileVer);
    return 0;
}

/*****************************************************************************
Function   : getVifNameFromBond
Description: 从bond中获取网卡名,然后判断是不是xen vif 网卡
Input       :
Output     : 
Return     : SUCC OR ERROR
*****************************************************************************/
int getVifNameFromBond(char *bondname)
{
	FILE *pFileVer;
	char  vifname[256] = {0};
	char CurrentPath[128] = {0};
	char arZe[] = " ";
	char arGet[VIF_NAME_LENGTH];
	char *szGet;
	char *nexttoken1 = NULL;
	char *nexttoken2 = NULL;
	struct ifreq ifrequest;
	INFO_LOG("enter the getVifNameFromBond");
	(void)snprintf_s(CurrentPath, 128, 128, "/sys/class/net/%s/bonding/slaves", bondname);
	int skt;
	skt = openNetSocket();
	if (ERROR == skt)
	{
	    return 0;
	}
	pFileVer =  opendevfile(CurrentPath);
	if (NULL == pFileVer)
	{     
		    NetworkDestroy(skt);
	    return 0; 
	}
	(void)memset_s(vifname,256,0,256);
	(void)fgets(vifname, 256, pFileVer);
	//fget读取文件会加完换行符再加 \0
	vifname[strlen(vifname)-1] = '\0';
	INFO_LOG("vifname = %s",vifname);
	fclose(pFileVer);
	(void)memset_s(arGet,VIF_NAME_LENGTH,0,VIF_NAME_LENGTH);
	for(szGet = strtok_s(vifname, arZe, &nexttoken1); szGet != NULL; szGet = strtok_s(NULL, arZe, &nexttoken2))
	{
		(void)memset_s(arGet,VIF_NAME_LENGTH,0,VIF_NAME_LENGTH);
		strncpy_s(arGet, VIF_NAME_LENGTH, szGet, strlen(szGet));
		szGet = NULL;
		INFO_LOG("this is a  nic name arGet = %s",arGet);
		if(getXenVif(arGet))
		{
	        (void)memset_s(BoundInfo.info[BoundInfo.count].vifname, VIF_NAME_LENGTH, 0, VIF_NAME_LENGTH);
	        strncpy_s(BoundInfo.info[BoundInfo.count].vifname, VIF_NAME_LENGTH, arGet, sizeof(arGet)-1); 
	        BoundInfo.info[BoundInfo.count].vifname[VIF_NAME_LENGTH -1]='\0';
	        strncpy_s(ifrequest.ifr_name, IFNAMSIZ, (char *)arGet, IFNAMSIZ-1);
	        ifrequest.ifr_name[IFNAMSIZ - 1] = '\0';
	        if ( ! (ioctl(skt, SIOCGIFHWADDR, (char *) &ifrequest) ) )
	        {
	            (void)snprintf_s(BoundInfo.info[BoundInfo.count].mac, 
	                    sizeof(BoundInfo.info[BoundInfo.count].mac), 
	                    sizeof(BoundInfo.info[BoundInfo.count].mac), 
	                    "%02x:%02x:%02x:%02x:%02x:%02x",
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[0],
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[1],
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[2],
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[3],
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[4],
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[5]);
	        }
	        BoundInfo.count++;
		}
		else
		{
		    (void)memset_s(BoundInfo.info[BoundInfo.count].sriov_vifname, VIF_NAME_LENGTH, 0, VIF_NAME_LENGTH);
		    strncpy_s(BoundInfo.info[BoundInfo.count].sriov_vifname, VIF_NAME_LENGTH, arGet, sizeof(arGet)-1);
		    BoundInfo.info[BoundInfo.count].sriov_vifname[sizeof(arGet)-1]='\0';
	        strncpy_s(ifrequest.ifr_name, IFNAMSIZ, (char *)arGet, IFNAMSIZ-1);
	        ifrequest.ifr_name[IFNAMSIZ - 1] = '\0';
	        if ( ! (ioctl(skt, SIOCGIFHWADDR, (char *) &ifrequest) ) )
	        {
	            (void)snprintf_s(BoundInfo.info[BoundInfo.count].sriov_mac, 
	                    sizeof(BoundInfo.info[BoundInfo.count].sriov_mac), 
	                    sizeof(BoundInfo.info[BoundInfo.count].sriov_mac), 
	                    "%02x:%02x:%02x:%02x:%02x:%02x",
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[0],
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[1],
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[2],
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[3],
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[4],
	                   (unsigned char)ifrequest.ifr_hwaddr.sa_data[5]);
	        }            
		}
	}  
    INFO_LOG("exit the getVifNameFromBond IsBondNic \n"); 
	NetworkDestroy(skt);
	return BoundInfo.count;
}
/*****************************************************************************
Function   : reset_bond_file
Description: monitor启动的时候读取已经设置的bond信息并填充结构体数组
Input       :
Output     : 
Return     : 0 OR 非0
*****************************************************************************/
int  reset_bond_file()
{
  FILE *fp;
  char *line = NULL;
  size_t linelen = 0;
  char left_ifname[10] = {0};   
  char bondname_tmp[VIF_NAME_LENGTH] = {0};
  int bond_count = 0;
  INFO_LOG("enter the reset_bond_file\n");
  BoundInfo.count = 0;
  fp =  opendevfile("/proc/net/dev");
  if(NULL == fp)
  {
      ERR_LOG("open the file fail(/proc/net/dev)");
      return -1;
  }
  if (getline(&line, &linelen, fp) == -1 /* eat line */
        || getline(&line, &linelen, fp) == -1) 
  {}
  while (getline(&line, &linelen, fp) != -1)
  {
    char *s, *name;
    s = GetVifName(&name, line);
    if(NULL == s)
    {
        INFO_LOG("GetVifName is NULL = %s",s);
    }
    (void)memset_s(left_ifname,10,0,10);
    left(left_ifname,name,4);
    if(strcmp(left_ifname, "bond") == 0)
    {         
        INFO_LOG("this is a bond ,not nic name = %s",name);
        (void)memset_s(left_ifname,10,0,10);
        
        (void)memset_s(bondname_tmp, VIF_NAME_LENGTH, 0, VIF_NAME_LENGTH);
        (void)snprintf_s(bondname_tmp, VIF_NAME_LENGTH, VIF_NAME_LENGTH, "%s", name);
        (void)memset_s(BoundInfo.info[BoundInfo.count].bondname, VIF_NAME_LENGTH, 0, VIF_NAME_LENGTH);
        BoundInfo.info[BoundInfo.count].release_count = 0;
        strncpy_s(BoundInfo.info[BoundInfo.count].bondname, VIF_NAME_LENGTH, bondname_tmp, sizeof(bondname_tmp)-1);
        //end by '\0' 2013-10-23
        BoundInfo.info[BoundInfo.count].bondname[sizeof(bondname_tmp)-1]='\0';
        INFO_LOG("BoundInfo.info[%d].bondname = %s\n",BoundInfo.count,BoundInfo.info[BoundInfo.count].bondname);
        
        if(getVifNameFromBond(name))
        {
            bond_count++;  
        }
    }
  }
  if(line)
  {
      free(line);
  }
  fclose(fp);
  INFO_LOG("exit the reset_bond_file");
  return bond_count;
}

/*****************************************************************************
Function   : GetBondVifIp
Description: 获取dhcp 网卡的ip,然后拷贝给bond
Input       :   网卡名
Output     : 
Return     : NULL or IP
*****************************************************************************/
int  GetBondVifIp(const char *ifname,char **vifip)
{
    struct ifreq ifrequest;
    struct sockaddr_in *pAddr;
    //char  vifip[16] = {0};
    int skt;
    skt = openNetSocket();
    if (ERROR == skt)
    {
        return 0;
    }

    (void)strncpy_s(ifrequest.ifr_name, IFNAMSIZ, ifname, IFNAMSIZ-1);
    ifrequest.ifr_name[IFNAMSIZ - 1] = '\0';

    if (!(ioctl(skt, SIOCGIFFLAGS, (char *) &ifrequest)))
    {
        /* 判断网卡状态 */
        if (ifrequest.ifr_flags & IFF_UP)
        {
            if ( ! (ioctl(skt, SIOCGIFADDR, (char *) &ifrequest) ) )
            {
                pAddr = (struct sockaddr_in *) (&ifrequest.ifr_addr);
                *vifip =  inet_ntoa( (pAddr->sin_addr ));
                NetworkDestroy(skt);
                return 1;
            }
        }
    }
    NetworkDestroy(skt);
    return 0;

}

int GetBondVifGateway(const char *ifname,char **gateway)
{
    char pathBuf[MAX_NICINFO_LENGTH] = {0};
    char pszGateway[VIF_NAME_LENGTH] = {0};
    char pszGatewayBuf[VIF_NAME_LENGTH] = {0};
    FILE *iRet;
    int skt;
    skt = openNetSocket();
    if (ERROR == skt)
    {
        return 0;
    }
    (void)memset_s(pathBuf, MAX_NICINFO_LENGTH, 0, MAX_NICINFO_LENGTH);
	/*比拼时提供的shell命令，通过route -n获取网关信息*/
    (void)snprintf_s(pathBuf, MAX_NICINFO_LENGTH, MAX_NICINFO_LENGTH, 
            "route -n | grep -i \"%s$\" | grep 0.0.0.0 | grep UG | awk '{print $2}'", ifname);
    iRet = openPipe(pathBuf, "r");
    if (NULL == iRet)
    {
       DEBUG_LOG("Failed to exec route shell command.");
       *gateway = NULL;
       NetworkDestroy(skt);
       return 0;
    }
	/*保存读取的网关信息*/
    if(NULL != fgets(pszGatewayBuf,sizeof(pszGatewayBuf),iRet))
    {
       (void)sscanf_s(pszGatewayBuf,"%s",pszGateway,sizeof(pszGateway));
    }
    trim(pszGateway);
	/*没网关信息则置0*/
    if(strlen(pszGateway) < 1)
    {      
       //pszGateway[0]='0';
         (void)pclose(iRet);
        NetworkDestroy(skt);
       *gateway = NULL;
       return 0;
    }
    (void)pclose(iRet);
	/*lint -e684 */   
    *gateway = pszGateway;
	/*lint +e684 */   
    NetworkDestroy(skt);
    return 1;
}
/*****************************************************************************
Function   : netbond
Description: 开机把直通网卡绑定，根据mac 地址来判断哪些网卡需要绑定
Input       :handle   -- xenstore句柄
Output     : 
Return     : SUCC OR ERROR
*****************************************************************************/
int netbond()
{
    int num;
    int i;
    int j;
    int k;
    int bond_count;
    char *bondvifip;
    char *bondvifgateway;
    int iRet;
    char left_mac[MAC_NAME_LENGTH] = {0};
    char right_mac[MAC_NAME_LENGTH] = {0};
    char left_mac_1[MAC_NAME_LENGTH] = {0};
    char right_mac_1[MAC_NAME_LENGTH] = {0};   
    char pszCommand[MAX_COMMAND_LENGTH] = {0};
    char pszBuff[MAX_COMMAND_LENGTH] = {0};

    num =  getVifInfo_forbond();
    if (ERROR == num || 0 == num)
    {
        ERR_LOG("ERROR   num = %d",num);
        return 0;
    }

    bond_count = reset_bond_file();
    BoundInfo.count = bond_count;
    INFO_LOG("BoundInfo.count = %d ",BoundInfo.count);
    for (i = 0; i < num; i++)
    {
 
        INFO_LOG("gtNicInfo.info[%d].mac = %s",i,gtNicInfo_bond.info[i].mac);
        INFO_LOG("gtNicInfo.info[%d].ifname = %s",i,gtNicInfo_bond.info[i].ifname); 
        memset_s(left_mac,MAC_NAME_LENGTH,0,MAC_NAME_LENGTH);
        memset_s(right_mac,MAC_NAME_LENGTH,0,MAC_NAME_LENGTH);

        left(left_mac,(char *)gtNicInfo_bond.info[i].mac,8);
        right(right_mac,(char *)gtNicInfo_bond.info[i].mac,8);
        INFO_LOG("left_mac = %s,right_mac = %s",left_mac,right_mac);
        //strncpy_s
        for(j=i+1;j<num;j++)
        {
           memset_s(left_mac_1,MAC_NAME_LENGTH,0,MAC_NAME_LENGTH);
           memset_s(right_mac_1,MAC_NAME_LENGTH,0,MAC_NAME_LENGTH);
           left(left_mac_1,(char *)gtNicInfo_bond.info[j].mac,8);
           right(right_mac_1,(char *)gtNicInfo_bond.info[j].mac,8);
           INFO_LOG("left_mac_1 = %s,right_mac_1 = %s",left_mac_1,right_mac_1);
           //  判断方法更改为，直通网卡和普通网卡的右边三位的mac 地址相同且普通网卡的右边三位是
           //FE:FF:FF
           if(strcmp(right_mac, right_mac_1) == 0)
           {
        	   INFO_LOG("find the bond mac ");
                   (void)memset_s(BoundInfo.info[BoundInfo.count].sriov_mac, MAC_NAME_LENGTH, 0, MAC_NAME_LENGTH);
                   (void)memset_s(BoundInfo.info[BoundInfo.count].sriov_vifname, VIF_NAME_LENGTH, 0, VIF_NAME_LENGTH);
                   (void)memset_s(BoundInfo.info[BoundInfo.count].mac, MAC_NAME_LENGTH, 0, MAC_NAME_LENGTH);
                   (void)memset_s(BoundInfo.info[BoundInfo.count].vifname, VIF_NAME_LENGTH, 0, VIF_NAME_LENGTH);
        	   if(strcmp(left_mac_1, NORMAL_NIC) == 0)
        	   {
                        strncpy_s(BoundInfo.info[BoundInfo.count].sriov_mac, 
                            MAC_NAME_LENGTH, 
                            gtNicInfo_bond.info[i].mac, 
                            MAC_NAME_LENGTH-1);
                        BoundInfo.info[BoundInfo.count].sriov_mac[MAC_NAME_LENGTH-1]='\0';
                        strncpy_s(BoundInfo.info[BoundInfo.count].sriov_vifname, 
                            VIF_NAME_LENGTH, 
                            gtNicInfo_bond.info[i].ifname, 
                            VIF_NAME_LENGTH-1);
                        BoundInfo.info[BoundInfo.count].sriov_vifname[VIF_NAME_LENGTH-1]='\0';
                        strncpy_s(BoundInfo.info[BoundInfo.count].mac, 
                            MAC_NAME_LENGTH, 
                            gtNicInfo_bond.info[j].mac, 
                            MAC_NAME_LENGTH-1);
                        BoundInfo.info[BoundInfo.count].mac[MAC_NAME_LENGTH-1]='\0';
                        strncpy_s(BoundInfo.info[BoundInfo.count].vifname, 
                            VIF_NAME_LENGTH, 
                            gtNicInfo_bond.info[j].ifname, 
                            VIF_NAME_LENGTH-1);
                        BoundInfo.info[BoundInfo.count].vifname[VIF_NAME_LENGTH-1]='\0';
                        BoundInfo.count++;
        	   }
        	   if(strcmp(left_mac, NORMAL_NIC) == 0)
        	   {
                        strncpy_s(BoundInfo.info[BoundInfo.count].sriov_mac, 
                            MAC_NAME_LENGTH,
                            gtNicInfo_bond.info[j].mac, 
                            MAC_NAME_LENGTH-1);
                        BoundInfo.info[BoundInfo.count].sriov_mac[MAC_NAME_LENGTH-1]='\0';
                        strncpy_s(BoundInfo.info[BoundInfo.count].sriov_vifname, 
                            VIF_NAME_LENGTH, 
                            gtNicInfo_bond.info[j].ifname, 
                            VIF_NAME_LENGTH-1);
                        BoundInfo.info[BoundInfo.count].sriov_vifname[VIF_NAME_LENGTH-1]='\0';
                        strncpy_s(BoundInfo.info[BoundInfo.count].mac, 
                            MAC_NAME_LENGTH,
                            gtNicInfo_bond.info[i].mac, 
                            MAC_NAME_LENGTH-1);
                        BoundInfo.info[BoundInfo.count].mac[MAC_NAME_LENGTH-1]='\0';
                        strncpy_s(BoundInfo.info[BoundInfo.count].vifname,
                            VIF_NAME_LENGTH,
                            gtNicInfo_bond.info[i].ifname, 
                            VIF_NAME_LENGTH-1);
                        BoundInfo.info[BoundInfo.count].vifname[VIF_NAME_LENGTH-1]='\0';
                        BoundInfo.count++;
        	   }
           }
        }
        memset_s(left_mac,MAC_NAME_LENGTH,0,MAC_NAME_LENGTH);
        memset_s(right_mac,MAC_NAME_LENGTH,0,MAC_NAME_LENGTH);
    }
    if(BoundInfo.count == bond_count)
    {
        INFO_LOG("there is no new sriov net ");
        return 0;
    }
    (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
    (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
    (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, "modprobe bonding");
    iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
    if (0 != iRet)
    {
        ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
    }
    for(k=bond_count;k<BoundInfo.count;k++)
    {
        BoundInfo.info[k].release_count = 0;
        INFO_LOG("BoundInfo.info[%d].sriov_mac = %s",k,BoundInfo.info[k].sriov_mac);
        INFO_LOG("BoundInfo.info[%d].sriov_vifname = %s",k,BoundInfo.info[k].sriov_vifname);
        INFO_LOG("BoundInfo.info[%d].mac = %s",k,BoundInfo.info[k].mac);
        INFO_LOG("BoundInfo.info[%d].vifname = %s",k,BoundInfo.info[k].vifname);

        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        /*add bond*/
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH,
                        "echo +bond%d > /sys/class/net/bonding_masters", k);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        }
        
        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        /*down sriov then add sriov to bond*/
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, 
                        "echo 1 > /sys/class/net/bond%d/bonding/mode",k);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        }
        
        bondvifip = NULL;         
        if(GetBondVifIp(BoundInfo.info[k].sriov_vifname,&bondvifip))
        {
            (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
            (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
            (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, "ifconfig bond%d %s", k,bondvifip);
            INFO_LOG("netbond: call uvpPopen pszCommand=%s bondvifip = %s ",pszCommand, bondvifip);
            iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
            if (0 != iRet)
            {
                ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
            }
        }
        INFO_LOG("netbond:bondvifip = %s ", bondvifip);

        
        bondvifgateway = NULL;
        if(GetBondVifGateway(BoundInfo.info[k].sriov_vifname,&bondvifgateway))
        {
            (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
            (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
            (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH,
                            "route add default gw %s", bondvifgateway);
            INFO_LOG("netbond: call uvpPopen pszCommand=%s bondvigateway = %s ",pszCommand, bondvifip);
            iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
            if (0 != iRet)
            {
                ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
            }
        }
        INFO_LOG("netbond:bondvigateway = %s ", bondvifgateway);
        (void)memset_s(BoundInfo.info[k].bondname, VIF_NAME_LENGTH, 0, VIF_NAME_LENGTH);
        (void)snprintf_s(BoundInfo.info[k].bondname, VIF_NAME_LENGTH, VIF_NAME_LENGTH, "bond%d", k);
        INFO_LOG("BoundInfo.info[%d].bondname = %s",k,BoundInfo.info[k].bondname);


        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, 
                        "ifconfig %s down", BoundInfo.info[k].sriov_vifname);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        }
        
        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        /*down sriov then add sriov to bond*/
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, 
                        "echo +%s > /sys/class/net/bond%d/bonding/slaves", BoundInfo.info[k].sriov_vifname,k);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        }        

        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, "ifconfig %s down", BoundInfo.info[k].vifname);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        } 
        
        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        /*down vif then add vif to bond*/
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH,
                        "echo +%s > /sys/class/net/bond%d/bonding/slaves", BoundInfo.info[k].vifname,k);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        }  
        INFO_LOG("netbond: call openPipe pszCommand=%s Fail  ",pszCommand);      

        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        /*turn sriov to primary*/
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH,  MAX_COMMAND_LENGTH,
                        "echo %s >  /sys/class/net/bond%d/bonding/primary", BoundInfo.info[k].sriov_vifname,k);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        }
            

        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, 
                        "ifconfig %s up", BoundInfo.info[k].sriov_vifname);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        }
         

        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, "ifconfig %s up", BoundInfo.info[k].vifname);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        }
  

        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, "ifconfig bond%d up", k);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        }   
        //redhat 系统不删除IP，会导致有多个路由，导致网络不通
        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, "ifconfig %s 0", BoundInfo.info[k].vifname);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        }   
        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, 
                        "ifconfig %s 0", BoundInfo.info[k].sriov_vifname);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d ",pszCommand, iRet);
        }                 
              
    }
    INFO_LOG("BoundInfo.count = %d ",BoundInfo.count);
    return 1;
}

/*****************************************************************************
Function   : releasenetbond
Description: 热迁移或者休眠时把bond切换为普通网卡
Input       :
Output     : 
Return     : 
*****************************************************************************/
int releasenetbond(void *handle)
{
    int k;
    int iRet = 0;

    char pszCommand[MAX_COMMAND_LENGTH] = {0};
    char pszBuff[MAX_COMMAND_LENGTH] = {0};
    INFO_LOG("releasenetbond  BoundInfo.count = %d ",BoundInfo.count);

    for(k=0;k<BoundInfo.count;k++)
    {
        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        BoundInfo.info[k].release_count = 0;
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, 
                        "echo %s >  /sys/class/net/%s/bonding/primary", 
                        BoundInfo.info[k].vifname, BoundInfo.info[k].bondname);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("releasenetbond: call uvpPopen pszCommand=%s Fail ret = %d \n",pszCommand, iRet);
            //命令执行失败
             write_to_xenstore(handle, RELEASE_BOND, "-1");
            return 0;
        }  
        BoundInfo.info[k].release_count = 1;
    }
    write_to_xenstore(handle, RELEASE_BOND, "2");
    return 1;
}

/*****************************************************************************
Function   : ReGetSriovNicInfo
Description: 
Input       :handle   -- xenstore句柄
Output     : 
Return     : SUCC OR ERROR
*****************************************************************************/
int ReGetSriovNicInfo()
{
    int nic_num;
    int sriov_num;
    int i,j;
    char left_mac[MAC_NAME_LENGTH] = {0};
    char right_mac[MAC_NAME_LENGTH] = {0};
    char left_mac_1[MAC_NAME_LENGTH] = {0};
    char right_mac_1[MAC_NAME_LENGTH] = {0};   

    nic_num =  getVifInfo_forbond();
    INFO_LOG("ReGetSriovNicInfo  num = %d \n",nic_num);
    if (0 == nic_num)
    {
        INFO_LOG("getVifInfo_forbond fail  nic_num = %d \n",nic_num);
        return -1;
    }

    sriov_num = 0;
    for (i = 0; i < BoundInfo.count; i++)
    {
        INFO_LOG("gtNicInfo.info[%d].mac = %s",i,BoundInfo.info[i].mac);
        INFO_LOG("gtNicInfo.info[%d].ifname = %s",i,BoundInfo.info[i].vifname); 
        (void)memset_s(BoundInfo.info[i].sriov_mac, MAC_NAME_LENGTH, 0, MAC_NAME_LENGTH);
        (void)memset_s(BoundInfo.info[i].sriov_vifname, VIF_NAME_LENGTH, 0, VIF_NAME_LENGTH);
        memset_s(left_mac,MAC_NAME_LENGTH,0,MAC_NAME_LENGTH);
        memset_s(right_mac,MAC_NAME_LENGTH,0,MAC_NAME_LENGTH);
        left(left_mac,(char *)BoundInfo.info[i].mac,8);
        right(right_mac,(char *)BoundInfo.info[i].mac,8);
        for(j=0;j<nic_num;j++)
        {
           memset_s(left_mac_1,MAC_NAME_LENGTH,0,MAC_NAME_LENGTH);
           memset_s(right_mac_1,MAC_NAME_LENGTH,0,MAC_NAME_LENGTH);
           left(left_mac_1,(char *)gtNicInfo_bond.info[j].mac,8);
           right(right_mac_1,(char *)gtNicInfo_bond.info[j].mac,8);

           if((strcmp(right_mac, right_mac_1) == 0)&&
                (strcmp(left_mac_1, NORMAL_NIC))&&
                (strcmp(BoundInfo.info[i].vifname, gtNicInfo_bond.info[j].ifname)))
           {
                INFO_LOG("this is the sriov nic mac ");
                strncpy_s(BoundInfo.info[i].sriov_mac, MAC_NAME_LENGTH, gtNicInfo_bond.info[j].mac, MAC_NAME_LENGTH-1);
                //end by '\0' 2013-10-23
                BoundInfo.info[i].sriov_mac[MAC_NAME_LENGTH-1]='\0';
                strncpy_s(BoundInfo.info[i].sriov_vifname, 
                    VIF_NAME_LENGTH, 
                    gtNicInfo_bond.info[j].ifname, 
                    VIF_NAME_LENGTH-1);
                BoundInfo.info[i].sriov_vifname[VIF_NAME_LENGTH-1]='\0';
                sriov_num++;
           }
        }
    }
    if(sriov_num < BoundInfo.count)
    {
        INFO_LOG("there is no enough sriov net \n");
        return -1;
    }
    return sriov_num;
}
/*****************************************************************************
Function   : rebondnet
Description: 热迁移或者休眠后把检测到的直通网卡后，先移除老的bond的直通网卡
                   然后把新的直通网卡bond进去
Input       :
Output     : 
Return     : 
*****************************************************************************/
int rebondnet(void *handle)
{
    int j;
    int iRet = 0;
    char pszCommand[MAX_COMMAND_LENGTH] = {0};
    char pszBuff[MAX_COMMAND_LENGTH] = {0};
    INFO_LOG("rebondnet BoundInfo.count = %d ",BoundInfo.count);

    (void)sleep(2);
    (void)ReGetSriovNicInfo();
    for(j=0;j<BoundInfo.count;j++)
    {
        
        INFO_LOG("rebondnet:BoundInfo.info[%d].sriov_mac = %s",j,BoundInfo.info[j].sriov_mac);
        INFO_LOG("rebondnet:BoundInfo.info[%d].sriov_vifname = %s",j,BoundInfo.info[j].sriov_vifname);
        INFO_LOG("rebondnet:BoundInfo.info[%d].mac = %s",j,BoundInfo.info[j].mac);
        INFO_LOG("rebondnet:BoundInfo.info[%d].vifname = %s",j,BoundInfo.info[j].vifname);
        INFO_LOG("rebondnet:BoundInfo.info[%d].release_count = %d",j,BoundInfo.info[j].release_count);
        if(BoundInfo.info[j].release_count == 0)
        {
            INFO_LOG("rebondnet:this is no need to rebond the soriov nic  BoundInfo.info[%d].release_count = %d",j,BoundInfo.info[j].release_count);
            continue;
        }
        BoundInfo.info[j].release_count = 0;

        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH,
                        "ifconfig %s down", BoundInfo.info[j].sriov_vifname);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("rebondnet: call uvpPopen pszCommand=%s Fail ret = %d \n",pszCommand, iRet);
        }

        
        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH,
                        "echo +%s > /sys/class/net/%s/bonding/slaves", 
                        BoundInfo.info[j].sriov_vifname,BoundInfo.info[j].bondname);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("rebondnet: call uvpPopen pszCommand=%s Fail ret = %d \n",pszCommand, iRet);
        }    

        
        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH,
                        "ifconfig %s up", BoundInfo.info[j].sriov_vifname);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("rebondnet: call uvpPopen pszCommand=%s Fail ret = %d \n",pszCommand, iRet);
        }

        
        (void)memset_s(pszCommand, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)memset_s(pszBuff, MAX_COMMAND_LENGTH, 0, MAX_COMMAND_LENGTH);
        (void)snprintf_s(pszCommand, MAX_COMMAND_LENGTH, MAX_COMMAND_LENGTH, 
                        "echo %s >  /sys/class/net/%s/bonding/primary", 
                        BoundInfo.info[j].sriov_vifname,BoundInfo.info[j].bondname);
        iRet = uvpPopen(pszCommand, pszBuff, MAX_COMMAND_LENGTH);
        if (0 != iRet)
        {
            ERR_LOG("netbond: call uvpPopen pszCommand=%s Fail ret = %d \n",pszCommand, iRet);
        }   
        
    }
    write_to_xenstore(handle, REBOND_SRIOV, "2");
    (void)netbond();
    return 1;
}





