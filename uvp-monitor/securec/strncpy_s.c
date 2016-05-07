/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: strncpy_s.c
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
 *    strncpy_s
 *
 * <SYNOPSIS>
 *    errno_t strncpy_s( char* strDest, size_t destMax, const char* strSrc, size_t count);
 *
 * <FUNCTION DESCRIPTION>
 *    Copy the contents from strSrc, including the terminating null character, 
 *    to the location specified by strDest.
 *
 * <INPUT PARAMETERS>
 *    strDest                     Destination string.
 *    destMax                     The size of the destination string, in characters.
 *    strSrc                      Source string.
 *    count                       Number of characters to be copied.
 *
 * <OUTPUT PARAMETERS>
 *    strDest                     is updated
 *
 * <RETURN VALUE>
 *    EOK(0)                      success
 *    EINVAL                      strDest == NULL or strSrc == NULL
 *    ERANGE                      destMax is zero or greater than SECUREC_STRING_MAX_LEN,
 *                                or count > SECUREC_STRING_MAX_LEN, or destMax is too small
 *    EOVERLAP                    dest buffer and source buffer are overlapped
 *
 *    If there is a runtime-constraint violation, then if strDest is not a null
 *    pointer and destMax is greater than zero and not greater than SECUREC_STRING_MAX_LEN,
 *    then strncpy_s sets strDest[0] to the null character.
 *******************************************************************************
*/

errno_t strncpy_s(char* strDest, size_t destMax, const char* strSrc, size_t count)
{
    char*  pHeader = strDest;
    size_t maxSize = destMax;
    IN_REGISTER const char* overlapGuard = NULL;

    if ( destMax == 0 || destMax > SECUREC_STRING_MAX_LEN )
    {
        SECUREC_ERROR_INVALID_RANGE("strncpy_s");
        return ERANGE;
    }

    if (strDest == NULL || strSrc == NULL )
    {
        if (strDest != NULL )
        {
            pHeader[0] = '\0';
        }
        SECUREC_ERROR_INVALID_PARAMTER("strncpy_s");
        return EINVAL;
    }
    if ( count > SECUREC_STRING_MAX_LEN)
    {
        pHeader[0] = '\0'; /*clear dest string*/
        SECUREC_ERROR_INVALID_RANGE("strncpy_s");
        return ERANGE;
    }
    if (count == 0)
    {
        pHeader[0] = '\0';
        return EOK;
    }
    if (strDest < strSrc)
    {
        overlapGuard = strSrc;
        while ((*(strDest++) = *(strSrc++)) != '\0' && --maxSize > 0 && --count > 0)
        {
            if ( strDest == overlapGuard)
            {
                pHeader[0] = '\0';
                SECUREC_ERROR_BUFFER_OVERLAP("strncpy_s");
                return EOVERLAP;
            }
        }
    }
    else
    {
        overlapGuard = strDest;
        while ((*(strDest++) = *(strSrc++)) != '\0' && --maxSize > 0 && --count > 0)
        {
            if ( strSrc == overlapGuard)
            {
                pHeader[0] = '\0';
                SECUREC_ERROR_BUFFER_OVERLAP("strncpy_s");
                return EOVERLAP;
            }
        }
    }

    if (count == 0)
    {
        *strDest = '\0';
    }

    if (maxSize == 0)
    {
        pHeader[0] = '\0';
        SECUREC_ERROR_INVALID_RANGE("strncpy_s");
        return ERANGE;
    }

    return EOK;
}


