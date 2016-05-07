/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: secureinput_w.c
* Decription: 
*             by defining data type for UNICODE string and including "input.inl",
*             this file generates real underlying function used by scanf family
*             API.
* History:   
*     1. Date:
*         Author:    
*         Modification:
********************************************************************************
*/

#include "securec.h"
#include "secinput.h"
#include <wchar.h>
#include <stddef.h>

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

typedef wchar_t _TCHAR;
typedef wchar_t _TUCHAR;
/*typedef wchar_t wint_t;*/
typedef wint_t _TINT;

#include "input.inl"


