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
#include <ifaddrs.h>
#include <netdb.h>

#define UPFLAG  1
#define DOWNFLAG 0
#define MAX_NICINFO_LENGTH 256
#define MAX_COMMAND_LENGTH 128
#define VIF_NAME_LENGTH 16
#define MAC_NAME_LENGTH 18
#define PACKAGE_LENGTH 31
//define for ipv4/6 info
#define XENSTORE_COUNT 6
#define XENSTORE_LEN 1024
#define NETINFO_PATH_LEN 30
#define BONDBRIDGE_PATH_LEN 64
#define IPADDR_LEN 4
/*can get 128 ip, but Result: you should filter remain 36 ip*/
#define IPV6_NIC_MAX 128
#define IPV6_ADDR_LEN 40
#define IPV6_FLUX_LEN 64
#define IPV6_IPNUM 35

#define IPV6_ADDR_LINKLOCAL    0x0020U
#define IPV6_ADDR_SITELOCAL    0x0040U
#define IPV6_ADDR_COMPATv4    0x0080U

#ifndef IN6_IS_ADDR_UNSPECIFIED
#define IN6_IS_ADDR_UNSPECIFIED(a) \
        (((__u32 *) (a))[0] == 0 && ((__u32 *) (a))[1] == 0 && \
         ((__u32 *) (a))[2] == 0 && ((__u32 *) (a))[3] == 0)
#endif

char ArrRetNet[XENSTORE_COUNT][XENSTORE_LEN] = {0};
//end define
//added by huanglingling for ipv4/6 info
typedef struct
{
    char  ifname[VIF_NAME_LENGTH];
    char  ipaddr[IPV6_ADDR_LEN];
    char  mac[MAC_NAME_LENGTH];
    char  tp[IPV6_FLUX_LEN];
    char  packs[IPV6_FLUX_LEN];
    char  gateway[IPV6_ADDR_LEN];
    long  sentdrop;
    long  recievedrop;
    /*interface status*/
    int   netstatusflag;
    /*if has ip info, then mark ipv4: 4, ipv6: 6*/
    int   ipversionflag;
    
} IPV6_VIF_DATA;

typedef struct
{
    int count;
    IPV6_VIF_DATA info[IPV6_NIC_MAX];

} IPV6_VIF_INFO;

/*all ipv4/6 info value*/
IPV6_VIF_INFO gtNicIpv6Info;
/*filter ipv4/6 info Result value*/
IPV6_VIF_INFO gtNicIpv6InfoResult;
//added end

extern FILE *opendevfile(const char *path);
extern int openNetSocket(void);
extern void NetworkDestroy(int sktd);
extern char *GetVifName(char **namep, char *p);
extern int GetVifFlag(int skt, const char *ifname);
extern int getVifData(char *pline, int nNumber, char *out);
extern FILE *openPipe(const char *pszCommand, const char *pszType);
extern int getFluxinfoLine(char *ifname, char *pline);

/*****************************************************************************
Function   : GetIpv6NetFlag
Description: get interface flag 
Input       : None
Output     : None
Return     : 
*****************************************************************************/
void GetIpv6NetFlag(int skt,char *vifname)
{

    char *vlan_path = "/proc/net/vlan/config";
    char bond_path[BONDBRIDGE_PATH_LEN] = {0};
    char bridge_path[BONDBRIDGE_PATH_LEN] = {0};
    char namebuf[VIF_NAME_LENGTH] = {0};
    char *vifnamebuf = NULL;
    int vlan_flag = 0;
    int vlanfile_flag = 1;
    int bond_flag = 0;
    int bridge_flag = 0;
    int vifnamelen = 0;
    FILE *file = NULL;
    char *begin = NULL;
    char pline[128] = {0};
    int i = 0;


    if (NULL == vifname)
    {
        return ;
    }
    vifnamebuf = vifname;
    
    (void)memset_s(namebuf,VIF_NAME_LENGTH,0,VIF_NAME_LENGTH);
    /*if has bond0:0 br0:0  muti ip Etc,you should get bond0 br0 name*/
    if(NULL != strstr(vifname, ":"))
    {
        while(*vifname != ':')
        {
            namebuf[i++] = *vifname;
            vifname++;
        }
        namebuf[i] = '\0';
        vifnamebuf = namebuf;
    }
    
    /*vlan*/
    if(!access(vlan_path,0))
    {
        if(NULL == (file = opendevfile(vlan_path)))
        {
            DEBUG_LOG("GetIpv6NetFlag: failed to open /proc/net/vlan/config.");
            vlanfile_flag = 0;
        }

        vifnamelen = strlen(vifnamebuf);
        if(1 == vlanfile_flag)
        {
            /*if has vlan, then query /proc/net/vlan/config to get interface name*/
			/*lint -e668 */ 
            while (fgets(pline, 128, file))
            {
                begin = pline;
                /*skip blank*/
                while (' ' == *begin)  begin++;
                /*if find matched name,then vlan_flag = 1 and break*/
                if (0 == strncmp(begin, vifnamebuf, vifnamelen))
                {
                    if (' ' == *(begin + vifnamelen)  )
                    {
                        vlan_flag = 1;
                        break;
                    }
                }
                /*init pline 0*/
                memset_s(pline, 128, 0, 128);
            }
			/*lint +e668 */ 
            fclose(file);
        }
    }

    /*assemble bond path*/
    snprintf_s(bond_path,BONDBRIDGE_PATH_LEN,BONDBRIDGE_PATH_LEN,"/proc/net/bonding/%s",vifnamebuf);
    if(!access(bond_path,0))
    {
        bond_flag = 1;
    }

    /*assemble bridge path*/
    snprintf_s(bridge_path,BONDBRIDGE_PATH_LEN,BONDBRIDGE_PATH_LEN,"/sys/class/net/%s/bridge",vifnamebuf);
    if(!access(bridge_path,0))
    {
        bridge_flag = 1;
    }
    
    /*if has vlan and status is down, then vlan_flag=2*/
    if(vlan_flag == 1 && DOWNFLAG == GetVifFlag(skt, vifnamebuf))
    {
        vlan_flag = 2;
    }
    

    if(UPFLAG == GetVifFlag(skt, vifnamebuf))
    {
        /*status:  bond upflag:2, vlan upflag:3, normal NIC upflag:1,bridge upflag:4*/
        if(1 == bond_flag)
        {
                gtNicIpv6Info.info[gtNicIpv6Info.count].netstatusflag = 2;
        }
        else if(1 == vlan_flag)
        {
                gtNicIpv6Info.info[gtNicIpv6Info.count].netstatusflag = 3;
        }
        else if(1 == bridge_flag)
        {
                gtNicIpv6Info.info[gtNicIpv6Info.count].netstatusflag = 4;
        }
        else
        {
                gtNicIpv6Info.info[gtNicIpv6Info.count].netstatusflag = 1;
        }
    }
    else
    {
        /*status:  bond/vlan/bridge downflag:9, normal NIC downflag:0*/
        if(1 == bond_flag || vlan_flag == 2 || 1 == bridge_flag)
        {
            gtNicIpv6Info.info[gtNicIpv6Info.count].netstatusflag = 9;
        }
        else
        {
            gtNicIpv6Info.info[gtNicIpv6Info.count].netstatusflag = 0;
        }
    }
}

/*****************************************************************************
Function   : GetIpv6VifMac
Description: get Ipv4/6 mac address
Input       : None
Output     : None
Return     : 
*****************************************************************************/
void GetIpv6VifMac(int skt, const char *ifname)
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
        (void)snprintf_s(gtNicIpv6Info.info[gtNicIpv6Info.count].mac, 
                        sizeof(gtNicIpv6Info.info[gtNicIpv6Info.count].mac), 
                        sizeof(gtNicIpv6Info.info[gtNicIpv6Info.count].mac), 
                        "%02x:%02x:%02x:%02x:%02x:%02x",
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[0],
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[1],
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[2],
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[3],
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[4],
                       (unsigned char)ifrequest.ifr_hwaddr.sa_data[5]);

    }
    return ;

}

/*****************************************************************************
Function   : GetIpv6Flux
Description: get Ipv4/6 Flux
Input       : None
Output     : None
Return     : 
*****************************************************************************/
int GetIpv6Flux(int skt,char *ifname)
{
    char Sent[PACKAGE_LENGTH] = {0};
    char Recived[PACKAGE_LENGTH] = {0};
    char SentPkt[PACKAGE_LENGTH] = {0};
    char RecivedPkt[PACKAGE_LENGTH] = {0};
    char SentPktDrop[PACKAGE_LENGTH] = {0};
    char RecivedPktDrop[PACKAGE_LENGTH] = {0};
    char *ptmp = NULL;
    /*get flux info line*/
    char fluxline[255] = {0};
    /* get vifname info line*/
    char *vifline = NULL;
    size_t linelen = 0;
    char *foundStr = NULL;
    FILE *file;
    char *path="/proc/net/dev";
    char *namebuf = NULL;
    char *p = NULL;
    int ifnamelen = 0;

    
    if(NULL == (file = opendevfile(path)))
    {
        DEBUG_LOG("GetVifInfo:failed to open /proc/net/dev.");
        return ERROR;
    }
    if (NULL == ifname)//for pclint warning
    {
        DEBUG_LOG("ifname is NULL.");
        fclose(file);
        return ERROR;
    }
    
    /*multi ip: such as eth0:0, you should match eth0 flux*/
    if (getline(&vifline, &linelen, file) == -1 /* eat line */
        || getline(&vifline, &linelen, file) == -1) {}
    while (getline(&vifline, &linelen, file) != -1)
    {
        (void)GetVifName(&namebuf, vifline);
        ifnamelen = strlen(namebuf);
        /*consider this scene: such as ifname = "eth10:0" namebuf="eth1"  and ifname = "eth1:0" namebuf="eth1"*/
        if(NULL != strstr(ifname,namebuf) && NULL != strstr(ifname,":"))
         {
                p = ifname + ifnamelen;
                if(':' == *p)
                {
                    ifname = namebuf;
                    break;
                }
         }
         
    } 
    
    /*get flux line*/
    if (ERROR == getFluxinfoLine(ifname, fluxline))
    {
        strncpy_s(gtNicIpv6Info.info[gtNicIpv6Info.count].tp, IPV6_FLUX_LEN, ERR_STR, strlen(ERR_STR));
        gtNicIpv6Info.info[gtNicIpv6Info.count].tp[strlen(ERR_STR)] = '\0';
        DEBUG_LOG("getFluxinfoLine is ERROR.");
        fclose(file);
        return ERROR;
    }

    if(0 == strlen(fluxline))
    {
        DEBUG_LOG("line is NULL.");
        fclose(file);
        return ERROR;
    }

    foundStr = strstr(fluxline, ifname);
    if (NULL == foundStr)
    {
        DEBUG_LOG("foundStr is NULL.");
        fclose(file);
        return ERROR;
    }
    /*get first data*/
    ptmp = foundStr + strlen(ifname) + 1;


    if (NULL == ptmp)
    {
        DEBUG_LOG("ptmp is NULL.");
        fclose(file);
        return ERROR;
    }

    /*get each column data info*/
    if(ERROR == getVifData(ptmp, 1, Recived)
            || ERROR ==  getVifData(ptmp, 2, RecivedPkt)
        || ERROR == getVifData(ptmp, 4, RecivedPktDrop)
            || ERROR == getVifData(ptmp, 9, Sent)
            || ERROR == getVifData(ptmp, 10, SentPkt)
            || ERROR == getVifData(ptmp, 12, SentPktDrop))
    {
        DEBUG_LOG("getVifData is ERROR.");
        fclose(file);
        return ERROR;
    }

    (void)snprintf_s(gtNicIpv6Info.info[gtNicIpv6Info.count].tp, 
                    sizeof(gtNicIpv6Info.info[gtNicIpv6Info.count].tp), 
                    sizeof(gtNicIpv6Info.info[gtNicIpv6Info.count].tp), 
                    "%s:%s", 
                    Sent, Recived);
    gtNicIpv6Info.info[gtNicIpv6Info.count].tp[strlen(Sent) + strlen(Recived) + 1] = '\0';

    (void)snprintf_s(gtNicIpv6Info.info[gtNicIpv6Info.count].packs, 
                    sizeof(gtNicIpv6Info.info[gtNicIpv6Info.count].packs), 
                    sizeof(gtNicIpv6Info.info[gtNicIpv6Info.count].packs), 
                    "%s:%s",
                    SentPkt, RecivedPkt);
    gtNicIpv6Info.info[gtNicIpv6Info.count].packs[strlen(SentPkt) + strlen(RecivedPkt) + 1] = '\0';
    
    /*networkloss*/
    (void)sscanf_s(SentPktDrop,"%ld", &gtNicIpv6Info.info[gtNicIpv6Info.count].sentdrop);
    (void)sscanf_s(RecivedPktDrop,"%ld",&gtNicIpv6Info.info[gtNicIpv6Info.count].recievedrop);
    fclose(file);
    free(vifline);
    return SUCC;
}

/*****************************************************************************
Function   : GetiIpv6VifGateway
Description: get Ipv4/6 Gateway
Input       : None
Output     : None
Return     : 
*****************************************************************************/
int GetIpv4VifGateway(int skt, const char *ifname)
{
    char pathBuf[MAX_NICINFO_LENGTH] = {0};
    char pszGateway[VIF_NAME_LENGTH] = {0};
    char pszGatewayBuf[VIF_NAME_LENGTH] = {0};
    FILE *iRet;

    if(NULL == ifname)
    {
        return ERROR;
    }
    
    (void)memset_s(pathBuf, MAX_NICINFO_LENGTH, 0, MAX_NICINFO_LENGTH);
    /*exec shell command to get ipv4 route gataway info*/
    (void)snprintf_s(pathBuf, MAX_NICINFO_LENGTH, MAX_NICINFO_LENGTH,
                "route -n | grep -i \"%s$\" | grep UG | awk '{print $2}'", ifname);
    iRet = openPipe(pathBuf, "r");
    if (NULL == iRet)
    {
       DEBUG_LOG("Failed to exec route shell command.");
       gtNicIpv6Info.info[gtNicIpv6Info.count].gateway[0] = '\0';
       return ERROR;
    }
    
    /*save default gw*/
    if(NULL != fgets(pszGatewayBuf,sizeof(pszGatewayBuf),iRet))
    {
       (void)sscanf_s(pszGatewayBuf,"%s",pszGateway,sizeof(pszGateway));
    }
    trim(pszGateway);
    
    /*if strlen(pszGateway) < 1, then 0*/
    if(strlen(pszGateway) < 1)
    {      
       pszGateway[0]='0';
       pszGateway[1]='\0';
    }
    (void)pclose(iRet);
    (void)strncpy_s(gtNicIpv6Info.info[gtNicIpv6Info.count].gateway, IPV6_ADDR_LEN, pszGateway, strlen(pszGateway));
    return SUCC;
}

int GetIpv6VifGateway(int skt, const char *ifname)
{
    char pathBuf[MAX_NICINFO_LENGTH] = {0};
    char pszGateway[VIF_NAME_LENGTH] = {0};
    char pszGatewayBuf[VIF_NAME_LENGTH] = {0};
    FILE *iRet;

    if(NULL == ifname)
    {
        return ERROR;
    }
    
    (void)memset_s(pathBuf, MAX_NICINFO_LENGTH, 0, MAX_NICINFO_LENGTH);
     /*exec shell command to get ipv6 route gataway info*/
    (void)snprintf_s(pathBuf, MAX_NICINFO_LENGTH, MAX_NICINFO_LENGTH,
                "route -A inet6 -n | grep -w \"%s\" | grep UG | awk '{print $2}'", ifname);
    iRet = openPipe(pathBuf, "r");
    if (NULL == iRet)
    {
       INFO_LOG("Failed to exec route shell command.");
       gtNicIpv6Info.info[gtNicIpv6Info.count].gateway[0] = '\0';
       return ERROR;
    }
    
     /*save default gw*/
    if(NULL != fgets(pszGatewayBuf,sizeof(pszGatewayBuf),iRet))
    {
       (void)sscanf_s(pszGatewayBuf,"%s",pszGateway,sizeof(pszGateway));
    }
    trim(pszGateway);
    
    /*if strlen(pszGateway) < 1, then 0*/
    if(strlen(pszGateway) < 1)
    {
       pszGateway[0]='0';
       pszGateway[1]='\0';
    }
    (void)pclose(iRet);
    (void)strncpy_s(gtNicIpv6Info.info[gtNicIpv6Info.count].gateway, IPV6_ADDR_LEN, pszGateway, strlen(pszGateway));
    return SUCC;
}

/*****************************************************************************
Function   : GetIpv4VifIp
Description: get Ipv4 ip info
Input       : None
Output     : None
Return     : 
*****************************************************************************/
int GetIpv4VifIp(int skt, int num, char *ifname)
{
    struct ifaddrs *ifaddr, *ifa;
    int ret;
    char address[IPV6_ADDR_LEN];

    if (getifaddrs(&ifaddr) == -1)
    {
        ERR_LOG("call getifaddrs failed\n");
        return ERROR;
    }

    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next)
    {
        if (NULL == ifa->ifa_addr)
            continue;
        if( AF_INET != ifa->ifa_addr->sa_family)
            continue;

        ret = getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),address, IPV6_ADDR_LEN, NULL, 0, NI_NUMERICHOST);
        if (0 != ret)
        {
            ERR_LOG("call getnameinfo for nic %s failed: %s\n", ifname, gai_strerror(ret));
            continue;
        }

        if((NULL != ifa->ifa_name) && (strcmp(ifa->ifa_name,ifname) == 0) && (AF_INET == ifa->ifa_addr->sa_family))
        {
            snprintf_s(gtNicIpv6Info.info[gtNicIpv6Info.count].ifname, VIF_NAME_LENGTH, VIF_NAME_LENGTH, "%s", ifname);
            GetIpv6VifMac(skt,ifname);
            gtNicIpv6Info.info[gtNicIpv6Info.count].ipversionflag = 4;
            snprintf_s(gtNicIpv6Info.info[gtNicIpv6Info.count].ipaddr, IPV6_ADDR_LEN, IPV6_ADDR_LEN, "%s", address);

            GetIpv4VifGateway(skt, ifname);

            GetIpv6Flux(skt,ifname);
            GetIpv6NetFlag(skt,ifname);

            num++;
            gtNicIpv6Info.count = num;
        }
    }

    freeifaddrs(ifaddr);
    return gtNicIpv6Info.count;
}

/*****************************************************************************
Function   : Inet6Rresolve
Description: get abbreviation Ipv6 info
Input       : None
Output     : None
Return     : int
*****************************************************************************/
int Inet6Rresolve(char *name, struct sockaddr_in6 *sin6, int numeric)
{
    int s;

    /* Grmpf. -FvK */
    if (sin6->sin6_family != AF_INET6) 
    {
        errno = EAFNOSUPPORT;
        return -1;
    }
    
    if (numeric & 0x7FFF) 
    {
        inet_ntop(AF_INET6, &sin6->sin6_addr, name, 80);
        return 0;
    }
    
    if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) 
    {
        if (numeric & 0x8000)
        {
            strncpy_s(name, strlen("default")+1, "default", strlen("default"));
        }
        else
        {
            strncpy_s(name, strlen("*")+1, "*", strlen("*"));
        }
        return 0;
    }
    s = getnameinfo((struct sockaddr *) sin6, sizeof(struct sockaddr_in6),name, 255 , NULL, 0, 0);
    if (s) 
    {
        DEBUG_LOG("getnameinfo failed\n");
        return -1;
    }
    
    return 0;
}
/*****************************************************************************
Function   : Inet6Sprint
Description: get abbreviation Ipv6 info
Input       : None
Output     : None
Return     : buf
*****************************************************************************/
char *Inet6Sprint(struct sockaddr *sap, int numeric)
{
    static char buff[128] = {0};

    (void)Inet6Rresolve(buff, (struct sockaddr_in6 *) sap, numeric);
    
    return buff;
}
/*****************************************************************************
Function   : Inet6Resolve
Description: resolve ipv6addr
Input       : None
Output     : None
Return     : int
*****************************************************************************/
int Inet6Resolve(char *name, struct sockaddr_in6 *sin6)
{
    struct addrinfo req;
    struct addrinfo *ai;
    int s;

    memset_s (&req, sizeof(struct addrinfo), '\0', sizeof(struct addrinfo));
    req.ai_family = AF_INET6;
    
    if ((s = getaddrinfo(name, NULL, &req, &ai)))
    {
        INFO_LOG("getaddrinfo: %s: %d\n", name, s);
        return -1;
    }
    
    memcpy_s(sin6, sizeof(struct sockaddr_in6), ai->ai_addr, sizeof(struct sockaddr_in6));

    freeaddrinfo(ai);

    return 0;
}
/*****************************************************************************
Function   : Inet6Input
Description: return INET6_resolve result
Input       : None
Output     : None
Return     : int
*****************************************************************************/
int Inet6Input(int type, char *bufp, struct sockaddr *sap)
{
    return (Inet6Resolve(bufp, (struct sockaddr_in6 *) sap));
}

/*****************************************************************************
Function   : GetIpv6VifIp
Description: get Ipv6 ip info
Input       : None
Output     : None
Return     : 
*****************************************************************************/
int GetIpv6VifIp(int skt,int num, char *vifname)
{
   char *path="/proc/net/if_inet6";
   FILE *file;
   char addr6p[8][5] = {0};
   char devname[20] = {0};
   char *line = NULL;
   char addr6[40] = {0};
   size_t linelen = 0;
   int plen, scope, dad_status, if_idx;
   struct sockaddr_in6 sap;
   char *abbreviationIpv6 = NULL;


   if(NULL == vifname)
   {
           return ERROR;
   }

   /*if no support ipv6, return error*/
   if(0 != access(path,F_OK))
   {
        return ERROR;
   }
	
   if(NULL == (file = opendevfile(path)))
   {
        DEBUG_LOG("GetIpv6VifInfo:failed to open /proc/net/if_inet6.");
        return ERROR;
   }

   
   /*query /proc/net/if_inet6 to get ipv6 info*/
   while(getline(&line, &linelen, file) != -1)
   {
        (void)memset_s(devname,20,0,20);
        sscanf_s(line, "%4s%4s%4s%4s%4s%4s%4s%4s %02x %02x %02x %02x %20s\n",
                      addr6p[0], sizeof(addr6p[0]), addr6p[1], sizeof(addr6p[1]),
                      addr6p[2], sizeof(addr6p[2]), addr6p[3], sizeof(addr6p[3]),
                      addr6p[4], sizeof(addr6p[4]), addr6p[5], sizeof(addr6p[5]),
                      addr6p[6], sizeof(addr6p[6]), addr6p[7], sizeof(addr6p[7]),
                      &if_idx, &plen, &scope, &dad_status, devname, sizeof(devname));
        /*do not modify strcmp -> strncmp*/
        if(0 == strcmp(devname,vifname) && (scope == 0 || scope == IPV6_ADDR_LINKLOCAL || scope == IPV6_ADDR_SITELOCAL || scope == IPV6_ADDR_COMPATv4))
        {
            snprintf_s(gtNicIpv6Info.info[gtNicIpv6Info.count].ifname, VIF_NAME_LENGTH, VIF_NAME_LENGTH, "%s", vifname);
            snprintf_s(addr6, IPV6_ADDR_LEN, IPV6_ADDR_LEN, "%s:%s:%s:%s:%s:%s:%s:%s",
                            addr6p[0], addr6p[1], addr6p[2], addr6p[3],
                            addr6p[4], addr6p[5], addr6p[6], addr6p[7]);
            /*abbreviation Ipv6 info*/
            Inet6Input(1, addr6, (struct sockaddr *) &sap);
            abbreviationIpv6 = Inet6Sprint((struct sockaddr *) &sap, 1);
            snprintf_s(gtNicIpv6Info.info[gtNicIpv6Info.count].ipaddr, IPV6_ADDR_LEN, IPV6_ADDR_LEN,
                        "%s", abbreviationIpv6);
            
            GetIpv6VifMac(skt,vifname);

            /*gw info has switch function*/
            if(g_exinfo_flag_value & EXINFO_FLAG_GATEWAY)
            {
                GetIpv6VifGateway(skt,vifname);
            }
            else
            {
                (void)strncpy_s(gtNicIpv6Info.info[gtNicIpv6Info.count].gateway,
                    IPV6_ADDR_LEN,
                    "0", 
                    strlen("0"));
            }
            gtNicIpv6Info.info[gtNicIpv6Info.count].ipversionflag = 6;
            GetIpv6Flux(skt,vifname);
            GetIpv6NetFlag(skt,vifname);
            num++;
            gtNicIpv6Info.count = num;
        }
    }
   fclose(file);
   free(line);
   return gtNicIpv6Info.count;
}

/*****************************************************************************
Function   : GetIpv6Info
Description: get Ipv4/6 info
Input       : None
Output     : None
Return     : 
*****************************************************************************/
int GetIpv6Info()
{
    struct ifconf ifconfigure;
    struct ifreq *ifreqIdx;
    struct ifreq *ifreq;
    char *path="/proc/net/dev";
    FILE *file;
    char *line = NULL;
    size_t linelen = 0;
    char namebuf[16] = {0};
    char buf[4096] = {0};
    int uNICCount = 0;
    int num = 0;
    int i = 0;
    int novifnameFlag = 0;
    int lastcount = 0;
    int getipv6flag = 0;
    int vifnameLen = 0;
    int skt;

    
    ifconfigure.ifc_len = 4096;
    ifconfigure.ifc_buf = buf;
    
    skt = openNetSocket(); 
    if (ERROR == skt)
    {
        DEBUG_LOG("GetIpv6Info:failed to openNetSocket.");
        return ERROR;
    }
    memset_s(&gtNicIpv6Info, sizeof(gtNicIpv6Info), 0, sizeof(gtNicIpv6Info)); 


    /*ioctl interface: interface has ipv4 and its status is up*/
    if(!ioctl(skt, SIOCGIFCONF, (char *) &ifconfigure))
    {
        ifreq = (struct ifreq *)buf;
        uNICCount = ifconfigure.ifc_len / (int)sizeof(struct ifreq);
        for(i=0; i<uNICCount; i++)
        {
            int nicRepeatFlag = 0;
            int j = 0;
            ifreqIdx = ifreq + i;
            memset_s(namebuf, 16, 0, 16);
            (void)snprintf_s(namebuf, sizeof(namebuf), sizeof(namebuf), "%s", ifreqIdx->ifr_name);

            for(j = 0; num > 0 && j < num; j++)
            {
                /*do not modify strcmp -> strncmp*/
                if(0 == strcmp(namebuf, gtNicIpv6Info.info[j].ifname))
                {
                    nicRepeatFlag = 1;
                    break;
                }
            }

            vifnameLen = strlen(namebuf);
            if(nicRepeatFlag || 0 == strncmp(namebuf,"lo",vifnameLen) || 0 == strncmp(namebuf,"sit0",vifnameLen))
            {
                continue;
            }

            /*get ipv4 info */
            if(UPFLAG == GetVifFlag(skt, namebuf))
            {
                getipv6flag = GetIpv4VifIp(skt,num,namebuf);
                if(ERROR != getipv6flag)
                {
                    num = getipv6flag;
                }

                if(num >= IPV6_NIC_MAX)
                    break;
            }

            /*get ipv6 info, /proc/net/if_inet6: the interface status is up */
            getipv6flag = GetIpv6VifIp(skt,num,namebuf);
            if(ERROR != getipv6flag)
            {
               num = getipv6flag;
            }

            if(num >= IPV6_NIC_MAX)
                  break;

        }
    }
    
    /*patch interface by query /proc/net/dev*/ 
   if(NULL == (file = opendevfile(path)))
    {
        NetworkDestroy(skt);
        DEBUG_LOG("GetVifInfo:failed to open /proc/net/dev.");
        return ERROR;
    }
    
   if(getline(&line, &linelen, file) == -1 /* eat line */
        || getline(&line, &linelen, file) == -1) {}
   while (getline(&line, &linelen, file) != -1)
    {
        novifnameFlag = 0;
        char *vifname;
        (void)GetVifName(&vifname, line);
        vifnameLen = strlen(vifname);
        if(0 == strncmp(vifname,"lo",vifnameLen) || 0== strncmp(vifname,"sit0",vifnameLen))
        {
           continue;
        }
        
        for(i=0;i<num;i++)
        {
           /*do not modify strcmp -> strncmp*/
           if(0 == strcmp(vifname,gtNicIpv6Info.info[i].ifname))
           {
                   /*has exist vifname*/
                novifnameFlag = 1;
           }
        }
        if(num >= IPV6_NIC_MAX)
                  break;
          /*new vifname*/
        if(novifnameFlag == 0)
        {
            /*if interface status is down, then xenstore info only has mac and its status*/
            if(DOWNFLAG == GetVifFlag(skt,vifname))
            {
                num++;
                GetIpv6VifMac(skt, vifname);
                GetIpv6NetFlag(skt,vifname);
                gtNicIpv6Info.count=num;

            }
            else
            {
                /*if interface status is up and no ip, then xenstore info has mac,staus,ip(none),gw,flux*/
                /*if interface status is up and has ipv6 info,then xenstore info has mac,staus,ip6,gw,flux*/
                lastcount = gtNicIpv6Info.count;
                getipv6flag = GetIpv6VifIp(skt,num,vifname);
                if(ERROR != getipv6flag)
                {
                    num = getipv6flag;
                }
                if(num == lastcount)
                {
                    num++;
                    GetIpv6VifMac(skt, vifname);
                    gtNicIpv6Info.info[gtNicIpv6Info.count].gateway[0] = '\0';
                    (void)strncpy_s(gtNicIpv6Info.info[gtNicIpv6Info.count].ipaddr,
                        IPV6_ADDR_LEN, 
                        "none", 
                        strlen("none"));
                    gtNicIpv6Info.info[gtNicIpv6Info.count].ipaddr[sizeof(gtNicIpv6Info.info[gtNicIpv6Info.count].ipaddr)-1]='\0';
                    GetIpv6Flux(skt,vifname);

                    if(g_exinfo_flag_value & EXINFO_FLAG_GATEWAY)
                    {
                        GetIpv4VifGateway(skt,vifname);
                    }
                    else
                    {
                        (void)strncpy_s(gtNicIpv6Info.info[gtNicIpv6Info.count].gateway, 
                            IPV6_ADDR_LEN, 
                            "0", 
                            strlen("0"));
                    }
                    GetIpv6NetFlag(skt,vifname);
                    gtNicIpv6Info.count =num;
                }
                else
                {
                    gtNicIpv6Info.count =num;
                }
            }

        }
    }
   fclose(file);
   free(line);
   NetworkDestroy(skt);
   return gtNicIpv6Info.count; 
}

/*****************************************************************************
Function   : Ipv6PrintInfo
Description: print Ipv4/6 info
Input       : None
Output     : None
Return     : 
*****************************************************************************/
void Ipv6PrintInfo(void * handle)
{
    unsigned int i = 0;
    char vif_path[NETINFO_PATH_LEN] = {0};
    /*xenstore print info*/
    for (i = 0; i < XENSTORE_COUNT; i++)
    {

        memset_s(vif_path,NETINFO_PATH_LEN,0,NETINFO_PATH_LEN);
        /*assemble xenstore path*/
        if(i == 0)
        {
            (void)snprintf_s(vif_path, NETINFO_PATH_LEN, NETINFO_PATH_LEN, "%s", IPV6_VIF_DATA_PATH);
        }
        else
        {
            (void)snprintf_s(vif_path, NETINFO_PATH_LEN, NETINFO_PATH_LEN, "%s_%u", IPV6_VIF_DATA_PATH, i);
        }

        if(xb_write_first_flag == 0)
        {
            (void)write_to_xenstore(handle, vif_path, ArrRetNet[i]);
        }
        else
        {
            (void)write_weak_to_xenstore(handle, vif_path, ArrRetNet[i]);
        }
    }
}

/*****************************************************************************
Function   : NetinfoNetworkctlmon
Description: ip4/6 main entry
Input       : None
Output     : None
Return     : 
*****************************************************************************/
void NetinfoNetworkctlmon(void *handle)
{
   char NetworkLoss[32] = {0};
   int i = 0;
   int num = 0;
   int count = 0;
   int BuffLen = 0;
   long sumrecievedrop = 0;
   long sumsentdrop = 0;
   
   num = GetIpv6Info();
   memset_s(ArrRetNet,sizeof(ArrRetNet),0,sizeof(ArrRetNet));

   /*init ArrRetNet*/
   for(i=0;i<XENSTORE_COUNT;i++)
   {
        (void)snprintf_s(ArrRetNet[i],sizeof("0"),sizeof("0"),"%s","0");
   }
   
   if (ERROR == num)
   {
        write_to_xenstore(handle, IPV6_VIF_DATA_PATH, "error");
        DEBUG_LOG("GetIpv6VifInfo:num is ERROR.");
        return;
   }
   
   if (0 == num)
   {
        Ipv6PrintInfo(handle);
        write_to_xenstore(handle, IPV6_VIF_DATA_PATH, "0");
        return;
   }
   
   memset_s(&gtNicIpv6InfoResult, sizeof(gtNicIpv6InfoResult), 0, sizeof(gtNicIpv6InfoResult));
   gtNicIpv6InfoResult.count = 0;

   /*IP Prioritization: if interface has ip, then put on the front of array*/
   for(i=0;i<num;i++)
   {
        if(IPADDR_LEN < strlen(gtNicIpv6Info.info[i].ipaddr))
        {
            (void)memcpy_s(&gtNicIpv6InfoResult.info[gtNicIpv6InfoResult.count],
                sizeof(IPV6_VIF_DATA),
                &gtNicIpv6Info.info[i],
                sizeof(IPV6_VIF_DATA));
            gtNicIpv6InfoResult.count++;
        }
        if(gtNicIpv6InfoResult.count >IPV6_IPNUM )
            break;
   }
   for(i=0;i<num;i++)
   {
        if(gtNicIpv6InfoResult.count >IPV6_IPNUM )
                break;
        if(IPADDR_LEN >= strlen(gtNicIpv6Info.info[i].ipaddr))
        {
                (void)memcpy_s(&gtNicIpv6InfoResult.info[gtNicIpv6InfoResult.count],
                    sizeof(IPV6_VIF_DATA),
                    &gtNicIpv6Info.info[i],
                    sizeof(IPV6_VIF_DATA));
                gtNicIpv6InfoResult.count++;
        }
        
   }
   
   /*assemble array*/
   for(i=0;i<gtNicIpv6InfoResult.count;i++)
    {
        /*The total number of packet loss statistics*/
        if(g_exinfo_flag_value & EXINFO_FLAG_NET_LOSS)
        {
          sumsentdrop += gtNicIpv6InfoResult.info[i].sentdrop;
          sumrecievedrop += gtNicIpv6InfoResult.info[i].recievedrop;
        }

        /*6 xenstore info*/
        count = i/XENSTORE_COUNT;
        if(i%XENSTORE_COUNT == 0)
            BuffLen = 0;
        else
            BuffLen = strlen(ArrRetNet[count]);

        if(0!=strlen(gtNicIpv6InfoResult.info[i].ipaddr))
        {
            (void)snprintf_s(ArrRetNet[count] + BuffLen, 
                sizeof(ArrRetNet[count]) - BuffLen,
                sizeof(ArrRetNet[count]) - BuffLen,
                "[%s-%d-%s-%s-<%s>-<%s>]",
                gtNicIpv6InfoResult.info[i].mac,
                gtNicIpv6InfoResult.info[i].netstatusflag,
                gtNicIpv6InfoResult.info[i].ipaddr,
                trim(gtNicIpv6InfoResult.info[i].gateway),
                gtNicIpv6InfoResult.info[i].tp,
                gtNicIpv6InfoResult.info[i].packs);
        }
        else
        {
           (void)snprintf_s(ArrRetNet[count] + BuffLen, 
                sizeof(ArrRetNet[count]) - BuffLen,
                sizeof(ArrRetNet[count]) - BuffLen,
                "[%s-%d]",
                gtNicIpv6InfoResult.info[i].mac,
                gtNicIpv6InfoResult.info[i].netstatusflag);
        }
    }
    
   Ipv6PrintInfo(handle);
   
   if(g_exinfo_flag_value & EXINFO_FLAG_NET_LOSS)
   {
        (void)snprintf_s(NetworkLoss, sizeof(NetworkLoss), sizeof(NetworkLoss), "%ld:%ld",sumrecievedrop, sumsentdrop);
   }
   
   if(xb_write_first_flag == 0)
   {
        if(g_exinfo_flag_value & EXINFO_FLAG_NET_LOSS)
        {
           write_to_xenstore(handle, VIF_DROP_PATH, NetworkLoss);
        }
   }
   else
   {
        if(g_exinfo_flag_value & EXINFO_FLAG_NET_LOSS)
        {
           write_weak_to_xenstore(handle, VIF_DROP_PATH, NetworkLoss);
        }
   }
   return ;
}

