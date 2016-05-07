/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: secureprintoutput_w.c
* Decription: 
*             by defining corresponding marco for UNICODE string and including
*             "output.inl", this file generates real underlying function used by
*             printf family API.
* History:   
*     1. Date:
*         Author:    
*         Modification:
********************************************************************************
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "securectype.h"
#include "secureprintoutput.h"

/***************************/
#define _XXXUNICODE
#define TCHAR wchar_t
#define _T(x) L ## x
#define write_char write_char_w
#define write_multi_char write_multi_char_w
#define write_string write_string_w
/***************************/

extern const uint8_t securec__lookuptable_s[];

#include "output.inl"


