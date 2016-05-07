/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: strtok_s.c
* History:   
*     1. Date:
*         Author:    
*         Modification:
********************************************************************************
*/

#include "securec.h"

/*******************************************************************************
 * <NAME>
 *    strtok_s
 *
 * <SYNOPSIS>
 *    char* strtok_s(char* strToken, const char* strDelimit, char** context);
 *
 * <FUNCTION DESCRIPTION>
 *    The strtok_s function finds the next token in strToken.
 *
 * <INPUT PARAMETERS>
 *    strToken            String containing token or tokens.
 *    strDelimit          Set of delimiter characters.
 *    context             Used to store position information between calls
 *                        to strtok_s
 *
 * <OUTPUT PARAMETERS>
 *
 * <RETURN VALUE>
 *    Returns a pointer to the next token found in strToken.
 *    They return NULL when no more tokens are found.
 *    Each call modifies strToken by substituting a NULL character for the first
 *    delimiter that occurs after the returned token.
 *
 *    return value        condition
 *    NULL                context == NULL, strDelimit == NULL, strToken == NULL
 *                        && (*context) == NULL, or no token is found.
 *******************************************************************************
*/

#define MAP_SIZE 32

char* strtok_s(char* strToken, const char* strDelimit, char** context)
{
    uint8_t* str;
    const uint8_t* ctl = (uint8_t*)strDelimit;
    uint8_t map[MAP_SIZE];
    int count;

    /* validation section */
    if (context == NULL || strDelimit == NULL)
    {
        return NULL;
    }

    if (strToken == NULL && (*context) == NULL)
    {
        return NULL;
    }

    /* Clear control map */
    for (count = 0; count < MAP_SIZE; count++)
    {
        map[count] = 0;
    }

    /* Set bits in delimiter table */
    do
    {
        map[*ctl >> 3] |= (1 << (*ctl & 7));
    }
    while (*ctl++);

    /* If string is NULL, set str to the saved
    * pointer (i.e., continue breaking tokens out of the string
    * from the last strtok call)
    */
    if (strToken != NULL)
    {
        str = (uint8_t*)strToken;
    }
    else
    {
        str = (uint8_t*)(*context);
    }

    /* Find beginning of token (skip over leading delimiters). Note that
    * there is no token iff this loop sets str to point to the terminal
    * null (*str == 0)
    */
    while ((map[*str >> 3] & (1 << (*str & 7))) && *str != '\0')
    {
        str++;
    }

    strToken = (char*)str;

    /* Find the end of the token. If it is not the end of the string,
    * put a null there. 
    */
    for ( ; *str != 0 ; str++ )
    {
        if (map[*str >> 3] & (1 << (*str & 7)))
        {
            *str++ = 0;
            break;
        }
    }

    /* Update context */
    *context = (char*)str;

    /* Determine if a token has been found. */
    if (strToken == (char*)str)
    {
        return NULL;
    }
    else
    {
        return strToken;
    }
}


