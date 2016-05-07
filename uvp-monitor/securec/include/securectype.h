/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: securectype.h
* Decription: 
*             define internal used marco and data type. The marco of SECUREC_ON_64BITS
*             will be determined in this header file, which is a switch for part
*             of code. Some marco are used to supress warning by MS compiler.
*Note:        
*             user can change the value of SECUREC_STRING_MAX_LEN and SECUREC_MEM_MAX_LEN
*             marco to meet their special need.
* History:   
*     1. Date: 2014/4/10
*         Author:  LiShunda
*         Modification: add error detection marco. If pointer size of dest system is NOT 
*         4 bytes or 8 bytes, use #error "unsupported system..." to report the compiling error.
********************************************************************************
*/

#ifndef __SECURECTYPE_H__A7BBB686_AADA_451B_B9F9_44DACDAE18A7
#define __SECURECTYPE_H__A7BBB686_AADA_451B_B9F9_44DACDAE18A7

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#ifdef __STDC_WANT_SECURE_LIB__
    #undef __STDC_WANT_SECURE_LIB__
#endif
    #define __STDC_WANT_SECURE_LIB__ 0
#ifdef _CRTIMP_ALTERNATIVE
    #undef _CRTIMP_ALTERNATIVE
#endif
    #define _CRTIMP_ALTERNATIVE //comment microsoft *_s function
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>


#define WCHAR_SIZE sizeof(wchar_t)

#ifndef uint8_t
typedef unsigned char uint8_t;
#endif

/* define the max length of the string */
#define SECUREC_STRING_MAX_LEN (0x7fffffffUL)
#define SECUREC_WCHAR_STRING_MAX_LEN (SECUREC_STRING_MAX_LEN / WCHAR_SIZE)

/* LSD add SECUREC_MEM_MAX_LEN for memcpy and memmove*/
#define SECUREC_MEM_MAX_LEN (0x7fffffffUL)
#define SECUREC_WCHAR_MEM_MAX_LEN (SECUREC_MEM_MAX_LEN / WCHAR_SIZE)

#if SECUREC_STRING_MAX_LEN > 0x7fffffff
#error "max string is 2G, or you may remove this marco"
#endif

#define IN_REGISTER register

#if defined(_MSC_VER) || defined(__ARMCC_VERSION)
typedef  __int64 INT64T;
typedef unsigned __int64 UINT64T;
#else
typedef long long INT64T;
typedef unsigned long long UINT64T;
#endif


#ifndef WEOF
    #define WEOF 0xFFFF
#endif  /* WEOF */

#ifdef _WIN64
#define SECUREC_ON_64BITS
#endif

#if defined(__LP64__) || defined(_LP64)
#define SECUREC_ON_64BITS
#endif

#if (defined(__GNUC__ ) && defined(__SIZEOF_POINTER__ ))
#if (__SIZEOF_POINTER__ != 4) && (__SIZEOF_POINTER__ != 8)
#error "unsupported system, contact Security Design Technology Department of 2012 Labs"
#endif
#endif

#if (!defined(SECUREC_ON_64BITS) && defined(__GNUC__ ) && defined(__SIZEOF_POINTER__ ))
#if __SIZEOF_POINTER__ == 8
#define SECUREC_ON_64BITS
#endif
#endif

#endif    /*__SECURECTYPE_H__A7BBB686_AADA_451B_B9F9_44DACDAE18A7*/


