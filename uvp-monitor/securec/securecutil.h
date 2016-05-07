/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: securecutil.h
* History:   
*     1. Date: 
*         Author:  
*         Modification: 
********************************************************************************
*/

#ifndef __SECURECUTIL_H__46C86578_F8FF_4E49_8E64_9B175241761F
#define __SECURECUTIL_H__46C86578_F8FF_4E49_8E64_9B175241761F

#include <assert.h>
#include <stdarg.h>

#ifdef CALL_LIBC_COR_API
/*#include <memory.h>  if memory.h don't exist, use "string.h" instead.*/
#include <string.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /* #define ERROR_HANDLER_BY_ASSERT */
#define ERROR_HANDLER_BY_PRINTF

    /* User can change the error handler by modify the following definition, 
    * such as logging the detail error in file.
    */
#if defined(_DEBUG) || defined(DEBUG)
#if defined(ERROR_HANDLER_BY_ASSERT)
#define SECUREC_ERROR_INVALID_PARAMTER(msg) assert( msg "invalid argument" == NULL)
#define SECUREC_ERROR_INVALID_RANGE(msg)    assert( msg "invalid dest buffer size" == NULL)
#define SECUREC_ERROR_BUFFER_OVERLAP(msg)   assert( msg "buffer overlap" == NULL)
#endif

#if defined(ERROR_HANDLER_BY_PRINTF)
#define SECUREC_ERROR_INVALID_PARAMTER(msg) printf( "%s invalid argument\n",msg)
#define SECUREC_ERROR_INVALID_RANGE(msg)    printf( "%s invalid dest buffer size\n", msg)
#define SECUREC_ERROR_BUFFER_OVERLAP(msg)   printf( "%s buffer overlap\n",msg)
#endif

#else
#define SECUREC_ERROR_INVALID_PARAMTER(msg) ((void)0)
#define SECUREC_ERROR_INVALID_RANGE(msg)    ((void)0)
#define SECUREC_ERROR_BUFFER_OVERLAP(msg)   ((void)0)
#endif

    void memcpy_8b(void* dest, const void* src, size_t count);

#ifndef CALL_LIBC_COR_API
    void memcpy_32b(void* dest, const void* src, size_t count);

    void memcpy_64b(void* dest, const void* src, size_t count);
#endif /*CALL_LIBC_COR_API*/

    void util_memmove (void* dst, const void* src, size_t count);

    int vsnprintf_helper (char* string, size_t count, const char* format, va_list ap);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif


