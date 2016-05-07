/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: securecutil.c
* Decription: 
*             provides internal functions used by this library, such as memory
*             copy and memory move. Besides, include some helper function for
*             printf family API, such as vsnprintf_helper, __putc_nolock,
*             __putwc_nolock. Also includes some folat converting function, such
*             as cvt, ecvtbuf, fcvtbuf, cfltcvt.
* History:   
*     1. Date: 2014/4/10
*        Author: LiShunda
*        Modification: move vswprintf_helper() function from this file to vswprintf.c,
*                      which make this file only contain ANSI string function. This will
*                      facilitate the ANSI string API building.
*     2. Date: 2014/4/10
*        Author: LiShunda
*        Modification: add int putWcharStrEndingZero(SECUREC_XPRINTF_STREAM* str, int zerosNum)
*                      function. In vswprintf.c, the original code use (__putc_nolock('\0', str) != EOF )
*                      four times and a do-while wrapper which is NOT easy to read and understand,
*                      so abstract this function.
*     3. Date: 2014/4/10
*        Author: LiShunda
*        Modification: change a variabel name "exp" in function "cfltcvt" to "expVal", for "exp" is a function name in <math.h>,
*                      which make pclint warning.
*     4. Date: 2014/4/10
*        Author: LiShunda
*        Modification: remove 'char* s__nullstring = "(null)"' and 'wchar_t* s__wnullstring = L"(null)"' to
*                      to avoid pclint warning on "redundant statement declared symbol's__nullstring'".
********************************************************************************
*/

#include <math.h>
#include "securec.h"
#include "secureprintoutput.h"

#include <assert.h>
#include <string.h>
#include <stdarg.h>

void memcpy_8b(void* dest, const void* src, size_t count)
{
    uint8_t* pDest = (uint8_t*)dest;
    uint8_t* pSrc = (uint8_t*)src;

    while (count-- )
    {
        *pDest = *pSrc;
        ++pDest;
        ++pSrc;
    }
}

#ifndef CALL_LIBC_COR_API

#define SIZE_OF_4_BYTES (4)
typedef unsigned long int DWORD;

void memcpy_32b(void* dest, const void* src, size_t count)
{
    uint8_t* pDest = (uint8_t*)dest;
    uint8_t* pSrc = (uint8_t*)src;
    uint8_t* pEndPos = pDest + count;
    size_t loops = 0;
    DWORD* pdwDest = NULL;
    DWORD* pdwSrc = NULL;

    if (((DWORD)pDest % SIZE_OF_4_BYTES) != 0)
    {
        /*copy header unaligned bytes*/
        do
        {
            *pDest = *pSrc;
            ++pDest;
            ++pSrc;
        }
        while (pDest < pEndPos && ((DWORD)pDest % SIZE_OF_4_BYTES != 0));

        if (pDest == pEndPos)
        {
            return;
        }
    }

    loops = (pEndPos - pDest) / SIZE_OF_4_BYTES;
    if (loops > 0)
    {
        pdwDest = (DWORD*)pDest;
        pdwSrc = (DWORD*)pSrc;

        while (loops)
        {
            *pdwDest = *pdwSrc;
            ++pdwDest;
            ++pdwSrc;
            --loops;
        }
        pDest = (uint8_t*)pdwDest;
        pSrc = (uint8_t*)pdwSrc;

    }

    loops = pEndPos - pDest;

    /* copy ending unaligned bytes */
    while (loops )
    {
        *pDest = *pSrc;
        ++pDest;
        ++pSrc;
        --loops;
    }
}

#define SIZE_OF_8_BYTES (8)

void memcpy_64b(void* dest, const void* src, size_t count)
{
    uint8_t* pDest = (uint8_t*)dest;
    uint8_t* pSrc = (uint8_t*)src;
    uint8_t* pEndPos = pDest + count;
    size_t loops = 0;
    UINT64T* pqwDest = NULL;
    UINT64T* pqwSrc = NULL;

    if (((UINT64T)pDest % SIZE_OF_8_BYTES) != 0)
    {
        /*copy header unaligned bytes*/
        do
        {
            *pDest = *pSrc;
            ++pDest;
            ++pSrc;
        }
        while (pDest < pEndPos && ((UINT64T)pDest % SIZE_OF_8_BYTES != 0) );

        if (pDest == pEndPos)
        {
            return;
        }
    }

    loops = (pEndPos - pDest) / SIZE_OF_8_BYTES;

    if (loops > 0)
    {
        pqwDest = (UINT64T*)pDest;
        pqwSrc = (UINT64T*)pSrc;

        while (loops)
        {
            *pqwDest = *pqwSrc;
            ++pqwDest;
            ++pqwSrc;
            --loops;
        }
        pDest = (uint8_t*)pqwDest;
        pSrc = (uint8_t*)pqwSrc;
    }

    loops = pEndPos - pDest;

    /*copy ending unaligned bytes*/
    while (loops )
    {
        *pDest = *pSrc;
        ++pDest;
        ++pSrc;
        --loops;
    }
}
#endif    /*CALL_LIBC_COR_API*/

void util_memmove (void* dst, const void* src, size_t count)
{
    uint8_t* pDest = (uint8_t*)dst;
    uint8_t* pSrc = (uint8_t*)src;

    if (dst <= src || pDest >= (pSrc + count))
    {
        /*
        * Non-Overlapping Buffers
        * copy from lower addresses to higher addresses
        */
        while (count--)
        {
            *pDest = *(uint8_t*)pSrc;
            ++pDest;
            ++pSrc;
        }
    }
    else
    {
        /*
        * Overlapping Buffers
        * copy from higher addresses to lower addresses
        */
        pDest = pDest + count - 1;
        pSrc = pSrc + count - 1;

        while (count--)
        {
            *pDest = *pSrc;

            --pDest;
            --pSrc;
        }
    }
}
/*put a char to output stream */
#define __putc_nolock(_c,_stream)    (--(_stream)->_cnt >= 0 ? 0xff & (*(_stream)->_ptr++ = (char)(_c)) :  EOF)

/*the following code can't be compiled under gcc*/
/* #define __putwc_nolock(_c, _stream)  (((_stream)->_cnt -= sizeof(wchar_t)) >= 0 ? (unsigned short) (0xffff & (* (( (wchar_t *)(_stream->_ptr))++)  = (wchar_t)_c)  ) : WEOF) */
/* #define __putwc_nolock(_c, _stream)  (((_stream)->_cnt -= sizeof(wchar_t)) >= 0 ? (unsigned short) (0xffff & (*((wchar_t *)((wchar_t*)(((_stream)->_ptr)))++) = (wchar_t)_c)) : WEOF) */
/* unsigned short __putwc_nolock (wchar_t _c, SECUREC_XPRINTF_STREAM * _stream);*/

/*put a wchar to output stream */
unsigned short __putwc_nolock (wchar_t _c, SECUREC_XPRINTF_STREAM* _stream)
{
    wchar_t wcRet = 0;
    if (((_stream)->_cnt -= (int)WCHAR_SIZE ) >= 0 )
    {
        *(wchar_t*)(_stream->_ptr)  = (wchar_t)_c; /*lint !e826*/
        _stream->_ptr += sizeof (wchar_t);
        wcRet =  (unsigned short) (0xffff & (wchar_t)_c);
    }
    else
    {
        wcRet = WEOF;
    }
    return wcRet;
}

int putWcharStrEndingZero(SECUREC_XPRINTF_STREAM* str, int zerosNum)
{
    int succeed = 0, i = 0;

    for (; i < zerosNum && (__putc_nolock('\0', str) != EOF ); ++i)
    {
    }
    if (i == zerosNum)
    {
        succeed = 1;
    }
    return succeed;
}
int vsnprintf_helper (char* string, size_t count, const char* format, va_list arglist)
{
    SECUREC_XPRINTF_STREAM str;
    int retval;
    
    assert(format != NULL);                   /*lint !e506*/
    assert(count != 0);                       /*lint !e506*/
    assert(count <= SECUREC_STRING_MAX_LEN);  /*lint !e506*/
    assert(string != NULL);                   /*lint !e506*/

    str._cnt = (int)count;

    str._ptr = string;

    retval = securec_output_s(&str, format, arglist );

    if ((retval >= 0) && (__putc_nolock('\0', &str) != EOF))
    {
        return (retval);
    }

    if (string != NULL)
    {
        string[count - 1] = 0;
    }

    if (str._cnt < 0)
    {
        /* the buffer was too small; we return -2 to indicate truncation */
        return -2;
    }
    return -1;
}



/*
below functions used for output/woutput
*/


const uint8_t securec__lookuptable_s[] =
{
    /* ' ' */  0x06,
    /* '!' */  0x80,
    /* '"' */  0x80,
    /* '#' */  0x86,
    /* '$' */  0x80,
    /* '%' */  0x81,
    /* '&' */  0x80,
    /* ''' */  0x00,
    /* '(' */  0x00,
    /* ')' */  0x10,
    /* '*' */  0x03,
    /* '+' */  0x86,
    /* ',' */  0x80,
    /* '-' */  0x86,
    /* '.' */  0x82,
    /* '/' */  0x80,
    /* '0' */  0x14,
    /* '1' */  0x05,
    /* '2' */  0x05,
    /* '3' */  0x45,
    /* '4' */  0x45,
    /* '5' */  0x45,
    /* '6' */  0x85,
    /* '7' */  0x85,
    /* '8' */  0x85,
    /* '9' */  0x05,
    /* ':' */  0x00,
    /* ';' */  0x00,
    /* '<' */  0x30,
    /* '=' */  0x30,
    /* '>' */  0x80,
    /* '?' */  0x50,
    /* '@' */  0x80,
    /* 'A' */  0x80,
    /* 'B' */  0x00,
    /* 'C' */  0x08,
    /* 'D' */  0x00,
    /* 'E' */  0x28,
    /* 'F' */  0x27,
    /* 'G' */  0x38,
    /* 'H' */  0x50,
    /* 'I' */  0x57,
    /* 'J' */  0x80,
    /* 'K' */  0x00,
    /* 'L' */  0x07,
    /* 'M' */  0x00,
    /* 'N' */  0x37,
    /* 'O' */  0x30,
    /* 'P' */  0x30,
    /* 'Q' */  0x50,
    /* 'R' */  0x50,
    /* 'S' */  0x88,
    /* 'T' */  0x00,
    /* 'U' */  0x00,
    /* 'V' */  0x00,
    /* 'W' */  0x20,
    /* 'X' */  0x28,
    /* 'Y' */  0x80,
    /* 'Z' */  0x88,
    /* '[' */  0x80,
    /* '\' */  0x80,
    /* ']' */  0x00,
    /* '^' */  0x00,
    /* '_' */  0x00,
    /* '`' */  0x60,
    /* 'a' */  0x60,
    /* 'b' */  0x60,
    /* 'c' */  0x68,
    /* 'd' */  0x68,
    /* 'e' */  0x68,
    /* 'f' */  0x08,
    /* 'g' */  0x08,
    /* 'h' */  0x07,
    /* 'i' */  0x78,
    /* 'j' */  0x70,
    /* 'k' */  0x70,
    /* 'l' */  0x77,
    /* 'm' */  0x70,
    /* 'n' */  0x70,
    /* 'o' */  0x08,
    /* 'p' */  0x08,
    /* 'q' */  0x00,
    /* 'r' */  0x00,
    /* 's' */  0x08,
    /* 't' */  0x00,
    /* 'u' */  0x08,
    /* 'v' */  0x00,
    /* 'w' */  0x07,
    /* 'x' */  0x08
};
/*
int util_get_int_arg (va_list *pargptr)
{
    return va_arg(*pargptr, int);
}

long util_get_long_arg (va_list *pargptr)
{
    return va_arg(*pargptr, long);
}

INT64T util_get_int64_arg (va_list *pargptr)
{
    return va_arg(*pargptr, INT64T);
}
*/
/* LSD this function is deprecated
short util_get_short_arg (va_list *pargptr)
{
    return va_arg(*pargptr, short);
}

typedef void* PVOID;
void* util_get_ptr_arg (va_list *pargptr)
{
    return (PVOID)va_arg(*pargptr, PVOID);
}
*/
static char* cvt(double arg, int ndigits, int* decpt, int* sign, char* buf, int eflag)
{
    int r2;
    double fi, fj;
    char* p, *p1;

    if (ndigits < 0)
    {
        ndigits = 0;
    }
    if (ndigits >= CVTBUFSIZE - 1)
    {
        ndigits = CVTBUFSIZE - 2;
    }

    r2 = 0;
    *sign = 0;
    p = &buf[0];
    if (arg < 0)
    {
        *sign = 1;
        arg = -arg;
    }
    arg = modf(arg, &fi);
    p1 = &buf[CVTBUFSIZE];

    if (fi != 0)
    {
        p1 = &buf[CVTBUFSIZE];
        while (fi != 0 && p1 > buf)
        {
            fj = modf(fi / 10, &fi);
            *--p1 = (int)((fj + .03) * 10) + '0'; /*lint !e734, it should be '0'-'9'*/
            r2++;
        }
        while (p1 < &buf[CVTBUFSIZE] && p < &buf[CVTBUFSIZE])
        {
            *p++ = *p1++;
        }
    }
    else if (arg > 0)
    {
        while ((fj = arg * 10) < 1)
        {
            arg = fj;
            r2--;
        }
    }
    p1 = &buf[ndigits];
    if (eflag == 0) 
    {
        p1 += r2; 
    }
    *decpt = r2;
    if (p1 < &buf[0])
    {
        buf[0] = '\0';
        return buf;
    }
    while (p <= p1 && p < &buf[CVTBUFSIZE])
    {
        arg *= 10;
        arg = modf(arg, &fj);
        *p++ = (int) fj + '0';  /*lint !e734*/
    }
    if (p1 >= &buf[CVTBUFSIZE])
    {
        buf[CVTBUFSIZE - 1] = '\0';
        return buf;
    }
    p = p1;
    *p1 += 5;
    while (*p1 > '9')
    {
        *p1 = '0';
        if (p1 > buf)
        {
            ++*--p1;
        }
        else
        {
            *p1 = '1';
            (*decpt)++;
            if (eflag == 0)
            {
                if (p > buf) 
                {
                    *p = '0'; 
                }
                p++;
            }
        }
    }
    *p = '\0';
    return buf;
}

char* ecvtbuf(double arg, int ndigits, int* decpt, int* sign, char* buf)
{
    return cvt(arg, ndigits, decpt, sign, buf, 1);
}

char* fcvtbuf(double arg, int ndigits, int* decpt, int* sign, char* buf)
{
    return cvt(arg, ndigits, decpt, sign, buf, 0);
}

void cfltcvt(double value, char* buffer, int bufSize, char fmt, int precision, int capexp)
{
    int decpt, sign, expVal, pos;
    char* digits = NULL;
    char cvtbuf[CVTBUFSIZE];
    char* oriPos = buffer;
    int magnitude;

    if (fmt == 'g')
    {
        /*digits =*/
        (void)ecvtbuf(value, precision, &decpt, &sign, cvtbuf);
        magnitude = decpt - 1;
        if (magnitude < -4  ||  magnitude > precision - 1)
        {
            fmt = 'e';
            precision -= 1;
        }
        else
        {
            fmt = 'f';
            precision -= decpt;
        }
    }

    if (fmt == 'e')
    {
        digits = ecvtbuf(value, precision + 1, &decpt, &sign, cvtbuf);

        if (sign) 
        { 
            *buffer++ = '-'; 
        }
        *buffer++ = *digits;
        if (precision > 0)
        {
            *buffer++ = '.';
        }
        (void)memcpy(buffer, digits + 1, precision); /*lint !e732*/
        buffer += precision;
        *buffer++ = capexp ? 'E' : 'e';

        if (decpt == 0)
        {
            if (value == 0.0)
            {
                expVal = 0;
            }
            else
            {
                expVal = -1;
            }
        }
        else
        {
            expVal = decpt - 1;
        }

        if (expVal < 0)
        {
            *buffer++ = '-';
            expVal = -expVal;
        }
        else
        {
            *buffer++ = '+';
        }

        buffer[2] = (expVal % 10) + '0';
        expVal = expVal / 10;
        buffer[1] = (expVal % 10) + '0';
        expVal = expVal / 10;
        buffer[0] = (expVal % 10) + '0';
        buffer += 3;
    }
    else if (fmt == 'f')
    {
        digits = fcvtbuf(value, precision, &decpt, &sign, cvtbuf);
        if (sign)
        {
            *buffer++ = '-';
        }
        if (*digits)
        {
            if (decpt <= 0)
            {
                *buffer++ = '0';
                *buffer++ = '.';
                for (pos = 0; pos < -decpt; pos++)
                {
                    *buffer++ = '0';
                }
                while (*digits)
                {
                    if ( buffer - oriPos >= bufSize)
                    {
                        break;
                    }
                    *buffer++ = *digits++;
                }
            }
            else
            {
                pos = 0;
                while (*digits)
                {
                    if ( buffer - oriPos >= bufSize)
                    {
                        break;
                    }
                    if (pos++ == decpt)
                    { 
                        *buffer++ = '.'; 
                    }
                    *buffer++ = *digits++;
                }
            }
        }
        else
        {
            *buffer++ = '0';
            if (precision > 0)
            {
                *buffer++ = '.';
                for (pos = 0; pos < precision; pos++)
                {
                    *buffer++ = '0';
                }
            }
        }
    }

    if ( buffer - oriPos >= bufSize)
    {
        /*buffer overflow*/
        assert(0);  /*lint !e506*/
        (void)memset_s(oriPos, (size_t)bufSize, 0, (size_t)bufSize);
    }else
    {
        *buffer = '\0';
    }
}

void write_char_a( char ch, SECUREC_XPRINTF_STREAM* f, int* pnumwritten)
{
    if (__putc_nolock(ch, f) == EOF)
    {
        *pnumwritten = -1;
    }
    else
    {
        ++(*pnumwritten);
    }
}

void write_char_w(wchar_t ch, SECUREC_XPRINTF_STREAM* f, int* pnumwritten)
{
    if (__putwc_nolock(ch, f) == WEOF)
    {
        *pnumwritten = -1;
    }
    else
    {
        ++(*pnumwritten);
    }
}

void write_multi_char_a(char ch, int num, SECUREC_XPRINTF_STREAM* f, int* pnumwritten)
{
    while (num-- > 0)
    {
        write_char_a(ch, f, pnumwritten);
        if (*pnumwritten == -1)
        {
            break;
        }
    }
}

void write_multi_char_w(wchar_t ch, int num, SECUREC_XPRINTF_STREAM* f, int* pnumwritten)
{
    while (num-- > 0)
    {
        write_char_w(ch, f, pnumwritten);
        if (*pnumwritten == -1)
        {
            break;
        }
    }
}

void write_string_a (char* string, int len, SECUREC_XPRINTF_STREAM* f, int* pnumwritten)
{
    while (len-- > 0)
    {
        write_char_a(*string++, f, pnumwritten);
        if (*pnumwritten == -1)
        {
            /*
            if (errno == EILSEQ)
                write_char(_T('?'), f, pnumwritten);
            else
                break;
            */
            break;
        }
    }
}

void write_string_w (wchar_t* string, int len, SECUREC_XPRINTF_STREAM* f, int* pnumwritten)
{
    while (len-- > 0)
    {
        write_char_w(*string++, f, pnumwritten);
        if (*pnumwritten == -1)
        {
            /*
            if (errno == EILSEQ)
                write_char(_T('?'), f, pnumwritten);
            else
                break;
            */
            break;
        }
    }
}


