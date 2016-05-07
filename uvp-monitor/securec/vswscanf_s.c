/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: vswscanf_s.c
* History:   
*     1. Date:
*         Author:    
*         Modification:
********************************************************************************
*/

#include "securec.h"
#include "secinput.h"
#include "securecutil.h"
#include <string.h>
#include <wchar.h>

/*******************************************************************************
 * <NAME>
 *    vswscanf_s
 *
 * <SYNOPSIS>
 *    int vswscanf_s(const wchar_t* buffer, const wchar_t* format, va_list arglist)
 *
 * <FUNCTION DESCRIPTION>
 *    The vsscanf_s function reads data from buffer into the location given by
 *    each argument. Every argument must be a pointer to a variable with a type
 *    that corresponds to a type specifier in format.
 *    The format argument controls the interpretation of the input fields and
 *    has the same form and function as the format argument for the scanf function.
 *    If copying takes place between strings that overlap, the behavior is undefined.
 *
 * <INPUT PARAMETERS>
 *    buffer                Stored data
 *    format                Format control string, see Format Specifications.
 *    arglist               pointer to list of arguments
 *
 * <OUTPUT PARAMETERS>
 *    arglist               the converted value stored in user assigned address
 *
 * <RETURN VALUE>
 *    Each of these functions returns the number of fields successfully converted
 *    and assigned; the return value does not include fields that were read but
 *    not assigned. A return value of 0 indicates that no fields were assigned.
 *    The return value is SCANF_EINVAL(-1) for an error or if the end of the
 *    string is reached before the first conversion.
 *******************************************************************************
*/

int vswscanf_s(const wchar_t* buffer, const wchar_t* format, va_list arglist)
{
    SEC_FILE_STREAM fStr = {0};
    int retval = -1;
    size_t count;

    /* validation section */
    if (buffer == NULL || format == NULL)
    {
        SECUREC_ERROR_INVALID_PARAMTER("vswscanf_s");
        return SCANF_EINVAL;
    }
    count = wcslen(buffer);
    if (count == 0 || count > SECUREC_WCHAR_STRING_MAX_LEN )
    {
        SECUREC_ERROR_INVALID_PARAMTER("vswscanf_s");
        return SCANF_EINVAL;
    }

    fStr._flag = MEM_STR_FLAG;
    fStr._base = fStr._ptr = (char*) buffer;
    fStr._cnt =  (int)count * ((int)WCHAR_SIZE);

    retval = securec_winput_s(&fStr, format, arglist);
    if (retval < 0 )
    {
        SECUREC_ERROR_INVALID_PARAMTER("vswscanf_s");
        return SCANF_EINVAL;
    }
    return retval;
}


