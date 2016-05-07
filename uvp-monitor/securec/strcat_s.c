/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: strcat_s.c
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
 *    strcat_s
 * <SYNOPSIS>
 *    errno_t strcat_s(char* strDest, size_t destMax, const char* strSrc);
 *
 * <FUNCTION DESCRIPTION>
 *    The strcat_s function appends strSrc to strDest and terminates the resulting
 *    string with a null character. The initial character of strSrc overwrites
 *    the terminating null character of strDest. strcat_s will return EOVERLAP
 *    if the source and destination strings overlap.
 *
 *    Note that the second parameter is the total size of the buffer, not the
 *    remaining size.
 *
 * <INPUT PARAMETERS>
 *    strDest                Null-terminated destination string buffer.
 *    destMax                Size of the destination string buffer.
 *    strSrc                 Null-terminated source string buffer.
 *
 * <OUTPUT PARAMETERS>
 *    strDest                is updated
 *
 * <RETURN VALUE>
 *    EOK                    Successful operation
 *    EINVAL                 strDest not terminated or NULL, strSrc is NULL
 *    ERANGE                 destMax is 0, too small, or destMax > SECUREC_STRING_MAX_LEN
 *    EOVERLAP               dest buffer and source buffer are overlapped
 *******************************************************************************
*/

errno_t strcat_s(char* strDest, size_t destMax, const char* strSrc)
{
    char*    pHeader = strDest;
    size_t  availableSize = destMax;
    IN_REGISTER const char* overlapGuard = NULL;

    if (destMax == 0 || destMax > SECUREC_STRING_MAX_LEN)
    {
        SECUREC_ERROR_INVALID_RANGE("strcat_s");
        return ERANGE;
    }

    if (strDest == NULL || strSrc == NULL)
    {
        if (strDest != NULL)
        {
            pHeader[0] = '\0';
        }
        SECUREC_ERROR_INVALID_PARAMTER("strcat_s");
        return EINVAL;
    }


    if (strDest < strSrc)
    {
        overlapGuard = strSrc;
        while (availableSize > 0 && *strDest != 0)
        {
            if (strDest == overlapGuard)
            {
                pHeader[0] = '\0';
                SECUREC_ERROR_BUFFER_OVERLAP("strcat_s");
                return EOVERLAP;
            }
            /*seek to string end*/
            strDest++;
            availableSize--;
        }

        /* strDest unterminated, return error. */
        if (availableSize == 0)
        {
            pHeader[0] = '\0';
            SECUREC_ERROR_INVALID_PARAMTER("strcat_s");
            return EINVAL;
        }
        /* if available > 0, then excute the strcat operation */
        while ((*strDest++ = *strSrc++) != 0 && --availableSize > 0)
        {
            if ( strDest == overlapGuard)
            {
                pHeader[0] = '\0';
                SECUREC_ERROR_BUFFER_OVERLAP("strcat_s");
                return EOVERLAP;
            }
        }
    }
    else
    {
        overlapGuard = strDest;
        while (availableSize > 0 && *strDest != '\0')
        {
            /*seek to string end, and no need to check overlap*/
            strDest++;
            availableSize--;
        }

        /* strDest unterminated, return error. */
        if (availableSize == 0)
        {
            pHeader[0] = '\0';
            SECUREC_ERROR_INVALID_PARAMTER("strcat_s");
            return EINVAL;
        }
        while ((*strDest++ = *strSrc++) != '\0' && --availableSize > 0)
        {
            if ( strSrc == overlapGuard)
            {
                pHeader[0] = '\0';
                SECUREC_ERROR_BUFFER_OVERLAP("strcat_s");
                return EOVERLAP;
            }
        }
    }

    /* strDest have not enough space, return error */
    if (availableSize == 0)
    {
        pHeader[0] = '\0';
        SECUREC_ERROR_INVALID_RANGE("strcat_s");
        return ERANGE;
    }

    return EOK;
}


