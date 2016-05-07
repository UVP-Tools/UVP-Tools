/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: strcpy_s.c
* History:   
*     1. Date:
*         Author:    
*         Modification:
********************************************************************************
*/

#include "securec.h"
#include "securecutil.h"

/*******************************************************************************
 * <NAME>
 *    strcpy_s
 *
 * <SYNOPSIS>
 *    errno_t strcpy_s(char* strDest, size_t destMax, const char* strSrc)
 *
 * <FUNCTION DESCRIPTION>
 *    The strcpy_s function copies the contents in the address of strSrc, 
 *    including the terminating null character, to the location specified by strDest. 
 *    The destination string must be large enough to hold the source string, 
 *    including the terminating null character. strcpy_s will return EOVERLAP 
 *    if the source and destination strings overlap.
 *
 * <INPUT PARAMETERS>
 *    strDest                  Location of destination string buffer
 *    destMax                  Size of the destination string buffer.
 *    strSrc                   Null-terminated source string buffer.
 *
 * <OUTPUT PARAMETERS>
 *    strDest                  is updated.
 *
 * <RETURN VALUE>
 *    0                        success
 *    EINVAL                   strDest == NULL or strSrc == NULL
 *    ERANGE                   destination buffer is NOT enough,  or size of 
 *                             buffer is zero or greater than SECUREC_STRING_MAX_LEN
 *    EOVERLAP                 dest buffer and source buffer are overlapped
 *
 *    If there is a runtime-constraint violation, then if strDest is not a null 
 *    pointer and destMax is greater than zero and not greater than 
 *    SECUREC_STRING_MAX_LEN, then strcpy_s sets strDest[0] to the null character.
 *******************************************************************************
*/

errno_t strcpy_s(char* strDest, size_t destMax, const char* strSrc)
{
    char* pHeader = strDest;
    size_t maxSize = destMax;
    IN_REGISTER const char* overlapGuard = NULL;

    if (destMax == 0 || destMax > SECUREC_STRING_MAX_LEN)
    {
        SECUREC_ERROR_INVALID_RANGE("strcpy_s");
        return ERANGE;
    }
    if (strDest == NULL || strSrc == NULL)
    {
        if (strDest != NULL)
        {
            pHeader[0] = '\0';
        }

        SECUREC_ERROR_INVALID_PARAMTER("strcpy_s");
        return EINVAL;
    }
    if (strDest == strSrc)
    {
        return EOK;
    }

    if (strDest < strSrc)
    {
        overlapGuard = strSrc;
        while ((*(strDest++) = *(strSrc++)) != '\0'  && --maxSize > 0)
        {
            if ( strDest == overlapGuard)
            {
                pHeader[0] = '\0';
                SECUREC_ERROR_BUFFER_OVERLAP("strcpy_s");
                return EOVERLAP;
            }
        }
    }
    else
    {
        overlapGuard = strDest;
        while ((*(strDest++) = *(strSrc++)) != '\0'  && --maxSize > 0)
        {
            if ( strSrc == overlapGuard)
            {
                pHeader[0] = '\0';
                SECUREC_ERROR_BUFFER_OVERLAP("strcpy_s");
                return EOVERLAP;
            }
        }
    }

    if (maxSize == 0)
    {
        pHeader[0] = '\0';
        SECUREC_ERROR_INVALID_RANGE("strcpy_s");
        return ERANGE;
    }

    return EOK;
}


