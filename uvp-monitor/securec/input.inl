/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.
* File name: input.inl
* Description:
*           used by secureinput_a.c and secureinput_w.c to include. This file
*           provides a template function for ANSI and UNICODE compiling by
*           different type definition. The functions of securec_input_s or
*           securec_winput_s provides internal implementation for scanf family
*           API, such as sscanf_s, fscanf_s.
* History:
*     1. Date: 2014/4/11
*        Author: LiShunda
*        Modification: add "#ifdef va_copy ... #endif" wraper to variable "arglistBeenCopied", which make the meaning
*                      of this variable more clean and can save a a variable space if va_copy macro NOT exist.
*     2. Date: 2014/4/11
*        Author: LiShunda
*        Modification: the code "if (!suppress)" at Line:1435 missing brace, which can cause logic error. Add "{ }" to
*                      wrap the code.
*     3. Date: 2014/4/22
*        Author: LiShunda
*        Modification: Line851, change "memcpy(pointer, tmpbuf, temp)" to 
*                      memcpy_s(pointer, array_width, tmpbuf, temp), which is more secure.
*
********************************************************************************
*/

#ifndef __INPUT_INL__5D13A042_DC3F_4ED9_A8D1_882811274C27
#define __INPUT_INL__5D13A042_DC3F_4ED9_A8D1_882811274C27

#ifndef _INTEGRAL_MAX_BITS
#define _INTEGRAL_MAX_BITS 64
#endif  /* _INTEGRAL_MAX_BITS */

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <stdlib.h>
#include <assert.h>

#if defined(UNICODE) || defined(_UNICODE)
#include <wchar.h>
#include <wctype.h>
#endif

#define MUL10(x) ((((x) << 2) + (x)) << 1)
#define MULTI_BYTE_MAX_LEN (6)

#if !defined(UNALIGNED)
#if defined(_M_IA64) || defined(_M_AMD64)
#define UNALIGNED __unaligned
#else
#define UNALIGNED
#endif
#endif


#define _T(x) (x)
#ifdef _UNICODE
#define _TEOF WEOF

#ifdef MB_LEN_MAX
#undef MB_LEN_MAX
#endif
#define MB_LEN_MAX 5                     /* max. # bytes in multibyte char */
#else                                    /*None unicode*/
#define _TEOF EOF
#endif

#if defined (UNICODE)
#define ALLOC_TABLE 1
#else                                    /* defined (UNICODE) */
#define ALLOC_TABLE 0
#endif                                   /* defined (UNICODE) */


#define LEFT_BRACKET ('[' | ('a' - 'A')) /* 'lowercase' version */

static _TINT  _hextodec(_TCHAR);

#define INC() (++charcount, _inc(stream))
#define UN_INC(chr) (--charcount, _un_inc(chr, stream))
#define EAT_WHITE() _whiteout(&charcount, stream)

static _TINT _inc(SEC_FILE_STREAM*);
static void _un_inc(_TINT, SEC_FILE_STREAM*);
static _TINT _whiteout(int*, SEC_FILE_STREAM*);


#ifndef _UNICODE
#define _ISDIGIT(chr) isdigit((uint8_t)(chr))
#define _ISXDIGIT(chr) isxdigit((uint8_t)(chr))
#else   /* _UNICODE */
#define _ISDIGIT(chr) (!((chr) & 0xff00) && isdigit( ((chr) & 0x00ff) ))
#define _ISXDIGIT(chr) (!((chr) & 0xff00) && isxdigit( ((chr) & 0x00ff)))
#endif  /* _UNICODE */

/* 1 means long long is same as int64, 
*  0 means long long is same as long
*/
#define LONGLONG_IS_INT64 1

static void _safecrt_fassign(int flag, char* argument, char* number, int dec_point)
{
    char* endptr;
    double d = strtod(number, &endptr);

    if (flag > 0)
    {
        *(double UNALIGNED*)argument = (double)d; /*lint !e826*/
    }
    else
    {
        *(float UNALIGNED*)argument = (float)d; /*lint !e826*/
    }
}

/******************************************************************************
*  int __check_float_string(size_t,size_t *, _TCHAR**, _TCHAR*, int*)
*
*  Purpose:
*       Check if there is enough space insert one more character in the given
*       block, if not then allocate more memory.
*
*  Return:
*       0 if more memory needed and the reallocation failed.
*
*******************************************************************************
*/

static int __check_float_string(size_t nFloatStrUsed,
                                size_t* pnFloatStrSz,
                                _TCHAR** pFloatStr,
                                _TCHAR* floatstring,
                                int* pmalloc_FloatStrFlag)
{
    void* tmpPointer;
   
    assert(nFloatStrUsed <= (*pnFloatStrSz));  /*lint !e506*/

    if (nFloatStrUsed == (*pnFloatStrSz))
    {
        if ((*pFloatStr) == floatstring)
        {
            if (((*pFloatStr ) = (_TCHAR*)calloc((*pnFloatStrSz), 2 * sizeof(_TCHAR))) == NULL)
            {
                return 0;
            }
            (*pmalloc_FloatStrFlag) = 1;
            (void)memcpy_s((*pFloatStr), (*pnFloatStrSz) * 2 * sizeof(_TCHAR), floatstring, (*pnFloatStrSz) * sizeof(_TCHAR));
            (*pnFloatStrSz) *= 2;
        }
        else
        {
            /* LSD 2014.3.6 fix, replace realloc to malloc to avoid heap injection */
            size_t oriBufSize = (*pnFloatStrSz) * sizeof(_TCHAR);
            size_t nextSize = oriBufSize * 2;
            if (nextSize > SECUREC_MEM_MAX_LEN)
            {
                return 0;
            }
            tmpPointer = malloc(nextSize);
            if (tmpPointer == NULL)
            {
                return 0;
            }
            (void)memcpy_s(tmpPointer, nextSize, (*pFloatStr), oriBufSize );
            (void)memset_s((*pFloatStr), oriBufSize, 0 ,  oriBufSize);
            free( (*pFloatStr) );


            (*pFloatStr) = (_TCHAR*)(tmpPointer);
            (*pnFloatStrSz) *= 2;
        }
    }
    return 1;
}

#ifndef _UNICODE
/*LSD only multi-bytes string need isleadbyte() function*/
#if !(defined(_WIN32) || defined(_INC_WCTYPE))
/*#define isleadbyte(c) ( (c) & 0x80 )*/

static int isleadbyte(uint8_t c) 
{   /*lint !e401*/
    return (c & 0x80 );
}

#endif
#endif


#define ASCII       (32)           /* # of bytes needed to hold 256 bits */

#ifndef _CVTBUFSIZE
#define _CVTBUFSIZE (309+40)       /* # of digits in max. dp value + slop */
#endif

#ifndef _UNICODE
#define TABLESIZE    ASCII
#else  /* _UNICODE */
#define TABLESIZE    (ASCII * 256)
#endif  /* _UNICODE */

/*******************************************************************************
* int securec_input_s(stream, format, arglist), static int securec_input_s(format, arglist)
*
* Purpose:
*   get input items (data items or literal matches) from the input stream
*   and assign them if appropriate to the items thru the arglist. this
*   function is intended for internal library use only, not for the user
*
*   The _input entry point is for the normal scanf() functions
*   The input entry point is used when compiling for _cscanf() [CPRFLAF
*   defined] and is a static function called only by _cscanf() -- reads from
*   console.
*
*   This code also defines _input_s, which works differently for %c, %s & %[.
*   For these, _input_s first picks up the next argument from the variable
*   argument list & uses it as the maximum size of the character array pointed
*   to by the next argument in the list.
*
* Entry:
*   FILE *stream - file to read from
*   char *format - format string to determine the data to read
*   arglist - list of pointer to data items
*
* Exit:
*   returns number of items assigned and fills in data items
*   returns EOF if error or EOF found on stream before 1st data item matched
*
* Exceptions:
*
********************************************************************************
*/

#ifdef _UNICODE
#define _tfgetc fgetwc
#define _tCharMask 0xffff
#else
#define _tfgetc fgetc
#define _tCharMask 0xff
#endif

/* LSD 2014 1 24 add to protect NULL pointer access */
#define CHECK_INPUT_ADDR(p) if (!p) { paraIsNull = 1; goto error_return; }

#ifdef _UNICODE
int securec_winput_s (SEC_FILE_STREAM* stream, const wchar_t* cformat, va_list arglist)
#else
int securec_input_s (SEC_FILE_STREAM* stream, const char* cformat, va_list arglist)
#endif

{
    _TCHAR floatstring[_CVTBUFSIZE + 1];
    _TCHAR* pFloatStr = floatstring;
    const _TUCHAR* format = ( _TUCHAR*)cformat;
    size_t nFloatStrUsed = 0;
    size_t nFloatStrSz = sizeof(floatstring) / sizeof(floatstring[0]);
    int malloc_FloatStrFlag = 0;

    unsigned long number = 0;               /* temp hold-value */
#if ALLOC_TABLE
    char* table = NULL;                     /* which chars allowed for %[]*/
    int malloc_flag = 0;                    /* is "table" allocated on the heap? */
#else  /* ALLOC_TABLE */
    char AsciiTable[TABLESIZE];
    char* table = AsciiTable;
#endif  /* ALLOC_TABLE */

#if _INTEGRAL_MAX_BITS >= 64
    UINT64T num64 = 0;                     /* temp for 64-bit integers */
#endif                                      /* _INTEGRAL_MAX_BITS >= 64 */
    void* pointer = NULL;                   /* points to user data receptacle */
    void* start;                            /* indicate non-empty string */


#ifndef _UNICODE
    wchar_t wctemp = L'\0';
#endif  /* _UNICODE */
    _TUCHAR* scanptr;                       /* for building "table" data */
    _TINT ch = 0;
    int charcount = 0;                      /* total number of chars read */
    int comchr = 0;                         /* holds designator type */
    int count = 0;                          /* return value.  # of assignments */

    int started = 0;                        /* indicate good number */
    int width = 0;                          /* width of field */
    int widthset = 0;                       /* user has specified width */
    size_t array_width = 0;
    int enomem = 0;
    int format_error = 0;

    /* Neither coerceshort nor farone are need for the 386 */

    char done_flag;                         /* general purpose loop monitor */
    char longone;                           /* 0 = SHORT, 1 = LONG, 2 = L_DOUBLE */  /* LSD change to  0 = SHORT, 1 = int, > 1  long or L_DOUBLE */
#if _INTEGRAL_MAX_BITS >= 64
    int integer64;                          /* 1 for 64-bit integer, 0 otherwise */
#endif                                      /* _INTEGRAL_MAX_BITS >= 64 */
    signed char widechar;                   /* -1 = char, 0 = ????, 1 = wchar_t */
    char reject;                            /* %[^ABC] instead of %[ABC] */
    char negative;                          /* flag for '-' detected */
    char suppress;                          /* don't assign anything */
    char match;                             /* flag: !0 if any fields matched */
	va_list arglistsave;                    /* save arglist value */
#if defined(va_copy) || defined(__va_copy)
    int arglistBeenCopied = 0;
#endif
    char fl_wchar_arg;                      /* flags wide char/string argument */

    _TCHAR decimal;


    _TUCHAR rngch;
    _TUCHAR last;
    _TUCHAR prevchar;
    _TCHAR tch;

    /* int is64Bits = sizeof(int*) == 8;    LSD add */
    int paraIsNull = 0;
    
    assert(format != NULL); /*lint !e506*/
    assert(stream != NULL); /*lint !e506*/

    /*
    * count = # fields assigned
    * charcount = # chars read
    * match = flag indicating if any fields were matched

    * [Note that we need both count and match.  For example, a field
    * may match a format but have assignments suppressed.  In this case,
    * match will get set, but 'count' will still equal 0.  We need to
    * distinguish 'match vs no-match' when terminating due to EOF.]
    */

    count = charcount = match = 0;

    while (format != NULL && *format)
    {
        if (isspace((_TUCHAR)*format))
        {

            UN_INC(EAT_WHITE()); /* put first non-space char back */

            do
            {
                tch = (_TCHAR)*(++format);
            }
            while (isspace((_TUCHAR)tch));

            continue;

        }

        if (_T('%') == *format)
        {
            number = 0;
            prevchar = 0;
            width = widthset = started = 0;
            array_width = 0;
            enomem = 0;
            fl_wchar_arg = 0;
            done_flag = 0;
            suppress = 0;
            negative = 0;
            reject = 0;
            widechar = 0;

            longone = 1;

#if _INTEGRAL_MAX_BITS >= 64
            integer64 = 0;
#endif  /* _INTEGRAL_MAX_BITS >= 64    */

            while (!done_flag)
            {
                comchr = *(++format);
                if (_ISDIGIT((_TUCHAR)comchr))
                {
                    ++widthset;
                    width = (int)MUL10(width) + (comchr - _T('0')); /*lint !e701*/
                }
                else
                {
                    switch (comchr)
                    {
                        case _T('F') :
                        case _T('N') :   /* no way to push NEAR in large model */
                            break;  /* NEAR is default in small model */
                        case _T('h') :
                            /* set longone to 0 */
                            --longone;
                            --widechar;         /* set widechar = -1 */
                            break;

#if _INTEGRAL_MAX_BITS >= 64
                        case _T('I'):
                            if ( (*(format + 1) == _T('6')) &&
                                 (*(format + 2) == _T('4')) )
                            {
                                format += 2;
                                ++integer64;
                                num64 = 0;
                                break;
                            }
                            else if ( (*(format + 1) == _T('3')) &&
                                      (*(format + 2) == _T('2')) )
                            {
                                format += 2;
                                break;
                            }
                            else if ( (*(format + 1) == _T('d')) ||
                                      (*(format + 1) == _T('i')) ||
                                      (*(format + 1) == _T('o')) ||
                                      (*(format + 1) == _T('x')) ||
                                      (*(format + 1) == _T('X')) )
                            {
                                /*lint -e506*/
                                if (sizeof(void*) == sizeof(INT64T))   /*lint !e774*/
                                {
                                    ++integer64;
                                    num64 = 0;
                                }
                                break;
                            }
                            /*lint -e506*/
                            if (sizeof(void*) == sizeof(INT64T))  /*lint !e774*/
                            {
                                ++integer64;
                                num64 = 0;
                            }
                            goto DEFAULT_LABEL; /*lint !e801*/
#endif  /* _INTEGRAL_MAX_BITS >= 64    */

                        case _T('L') :
                            ++longone;
                            break;

                        case _T('l') :
                            if (*(format + 1) == _T('l'))
                            {
                                ++format;
#if LONGLONG_IS_INT64   /*LSD change from #ifdef to #if*/
                                ++integer64;
                                num64 = 0;
                                break;
#else  /* LONGLONG_IS_INT64 */
                                ++longone;
                                /* NOBREAK */
#endif  /* LONGLONG_IS_INT64 */
                            }
                            else
                            {
                                ++longone;
                                /* NOBREAK */
                            }
                            /*lint -fallthrough*/
                        case _T('w') :
                            ++widechar;         /* set widechar = 1 */
                            break;

                        case _T('*') :
                            ++suppress;
                            break;

                        default:
                        DEFAULT_LABEL:
                            ++done_flag;
                            break;
                    }
                }
            }

            if (!suppress)
            {
                /* LSD change, for gcc compile arglistsave = arglist; */
#if defined(va_copy)
                va_copy(arglistsave, arglist);
#elif defined(__va_copy) /*for vxworks*/
                __va_copy(arglistsave, arglist);
#else
                arglistsave = arglist;
#endif
                pointer = va_arg(arglist, void*); /*lint !e826*/
                CHECK_INPUT_ADDR(pointer); /*lint !e801*/
            }
            else
            {
                pointer = NULL;         /* doesn't matter what value we use here - we're only using it as a flag*/
            }

            done_flag = 0;

            if (!widechar)
            {
                /* use case if not explicitly specified */
                if ((*format == _T('S')) || (*format == _T('C')))
#ifdef _UNICODE
                    --widechar;
                else
                { ++widechar; }
#else  /* _UNICODE */
                    ++widechar;
                else
                { --widechar; }
#endif  /* _UNICODE */
            }

            /* switch to lowercase to allow %E,%G, and to
            keep the switch table small */

            comchr = *format | (_T('a') - _T('A'));

            if (_T('n') != comchr)
            {
                if (_T('c') != comchr && LEFT_BRACKET != comchr)
                {
                    ch = EAT_WHITE();
                }
                else
                {
                    ch = INC();
                }
            }

            if (_T('n') != comchr)
            {
                if (_TEOF == ch)
                {
                    goto error_return; /*lint !e801*/
                }
            }

            if (!widthset || width)
            {

                if (!suppress && (comchr == _T('c') || comchr == _T('s') || comchr == LEFT_BRACKET))
                {

#if defined(va_copy)
                    va_copy(arglist, arglistsave);
                    va_end(arglistsave);
                    arglistBeenCopied = 1;
#elif defined(__va_copy) /*for vxworks*/
                    __va_copy(arglist, arglistsave);
                    va_end(arglistsave);
                    arglistBeenCopied = 1;
#else
                    arglist = arglistsave;
#endif
                    /* Reinitialize pointer to point to the array to which we write the input */
                    pointer = va_arg(arglist, void*); /*lint !e826*/
                    CHECK_INPUT_ADDR(pointer);    /*lint !e801*/


#if defined(va_copy)
                    va_copy(arglistsave, arglist);
#elif defined(__va_copy) /*for vxworks*/
                    __va_copy(arglistsave, arglist);
#else
                    arglistsave = arglist;
#endif
                    /* Get the next argument - size of the array in characters */
#ifdef SECUREC_ON_64BITS
                    array_width = ((size_t)(va_arg(arglist, size_t))) & 0xFFFFFFFFUL; /*LSD change "va_arg(arglist, unsigned int)" to va_arg(arglist, unsigned int)*/
#else  /* !SECUREC_ON_64BITS */
                    array_width = va_arg(arglist, size_t); /*lint !e826*/
#endif

                    if (array_width < 1)
                    {

                        if (widechar > 0)
                        {
                            *(wchar_t UNALIGNED*)pointer = L'\0';
                        }
                        else
                        {
                            *(char*)pointer = '\0';
                        }

                        goto error_return; /*lint !e801*/
                    }

                    /*LSD add string maxi width protection*/
                    if (widechar > 0 )
                    {
                        if (array_width > SECUREC_WCHAR_STRING_MAX_LEN)
                        {
                            goto error_return; /*lint !e801*/
                        }
                    }
                    else
                    {
                        /*for char* buffer*/
                        if ( array_width > SECUREC_STRING_MAX_LEN)
                        {
                            goto error_return; /*lint !e801*/
                        }
                    }

                }
                switch (comchr)
                {

                    case _T('c'):
                        /*  case _T('C'):  */
                        if (!widthset)
                        {
                            ++widthset;
                            ++width;
                        }
                        if (widechar > 0)
                        {
                            fl_wchar_arg++;
                        }
                        goto scanit; /*lint !e801*/


                    case _T('s'):
                        /*  case _T('S'):  */
                        if (widechar > 0)
                        {
                            fl_wchar_arg++;
                        }
                        goto scanit; /*lint !e801*/


                    case LEFT_BRACKET :   /* scanset */
                        if (widechar > 0)
                        {
                            fl_wchar_arg++;
                        }
                        scanptr = (_TUCHAR*)(++format);

                        if (_T('^') == *scanptr)
                        {
                            ++scanptr;
                            --reject; /* set reject to 255 */
                        }

                        /* Allocate "table" on first %[] spec */
#if ALLOC_TABLE
                        if (table == NULL)
                        {
                            /*LSD the table will be freed after error_return label of this function */
                            table = (char*)malloc(TABLESIZE);
                            if ( table == NULL)
                            {
                                goto error_return; /*lint !e801*/
                            }
                            malloc_flag = 1;
                        }
#endif  /* ALLOC_TABLE */
                        (void)memset_s(table, TABLESIZE, 0, TABLESIZE);


                        if (LEFT_BRACKET == comchr) /*lint !e774*/
                        {
                            if (_T(']') == *scanptr)
                            {
                                prevchar = _T(']');
                                ++scanptr;

                                table[ _T(']') >> 3] = 1 << (_T(']') & 7);

                            }
                        }

                        /*2014 3.14 LSD add _T('\0') != *scanptr*/
                        while (_T('\0') != *scanptr && _T(']') != *scanptr)
                        {

                            rngch = *scanptr++;

                            if (_T('-') != rngch ||
                                !prevchar ||           /* first char */
                                _T(']') == *scanptr) /* last char */
                            {

                                table[(prevchar = rngch) >> 3] |= 1 << (rngch & 7); /*lint !e734*/
                            }
                            else
                            {
                                /* handle a-z type set */

                                rngch = *scanptr++; /* get end of range */

                                if (prevchar < rngch)  /* %[a-z] */
                                {
                                    last = rngch;
                                }
                                else
                                {
                                    /* %[z-a] */
                                    last = prevchar;
                                    prevchar = rngch;
                                }
                                for (rngch = prevchar; rngch <= last; ++rngch)
                                {
                                    table[rngch >> 3] |= 1 << (rngch & 7); /*lint !e734*/
                                }

                                prevchar = 0;

                            }
                        }


                        if (!*scanptr)
                        {
                            if (array_width >= sizeof(_TCHAR) )
                            {
                                *(_TCHAR*)pointer = _T('\0');  /*lint !e613*/
                            }
                            goto error_return;      /* trunc'd format string */ /*lint !e801*/
                        }

                        /* scanset completed.  Now read string */

                        if (LEFT_BRACKET == comchr) /*lint !e774*/
                        {
                            format = scanptr;
                        }

                    scanit:
                        start = pointer;

                        /*
                        * execute the format directive. that is, scan input
                        * characters until the directive is fulfilled, eof
                        * is reached, or a non-matching character is
                        * encountered.
                        *
                        * it is important not to get the next character
                        * unless that character needs to be tested! other-
                        * wise, reads from line-buffered devices (e.g.,
                        * scanf()) would require an extra, spurious, newline
                        * if the first newline completes the current format
                        * directive.
                        */
                        UN_INC(ch);

                        /* One element is needed for '\0' for %s & %[ */
                        if (comchr != _T('c'))
                        {
                            --array_width;
                        }
                        while ( !widthset || width-- )
                        {

                            ch = INC();
                            if (
                                (_TEOF != ch) &&
                                /* char conditions*/
                                ( ( comchr == _T('c')) ||
                                  /* string conditions !isspace()*/
                                  ( ( comchr == _T('s') &&
                                      (!(ch >= _T('\t') && ch <= _T('\r')) &&
                                       ch != _T(' ')))) ||
                                  /* BRACKET conditions*/
                                  ( (comchr == LEFT_BRACKET) && table && /*LSD add && table is NOT NULL condition*/
                                    ((table[(size_t)ch >> 3] ^ reject) & (1 << (ch & 7)))
                                  )
                                )
                            )
                            {
                                if (!suppress)
                                {
                                    if (!array_width)
                                    {
                                        /* We have exhausted the user's buffer */

                                        enomem = 1;
                                        break;
                                    }
                                    CHECK_INPUT_ADDR(pointer);  /*lint !e801*/

#ifndef _UNICODE
                                    if (fl_wchar_arg)
                                    {
                                        char temp[MULTI_BYTE_MAX_LEN];
                                        int convRes = 0, di = 1;
                                        wctemp = L'?';    /* set default char as ?*/

                                        (void)memset(temp, 0, MULTI_BYTE_MAX_LEN);
                                        temp[0] = (char) ch;

                                        if ( isleadbyte((uint8_t)ch))
                                        {

#if defined(_WIN32) || defined(_WIN64)
                                            temp[1] = (char) INC();

#else
                                            /*in Linux like system, the string is encoded in UTF-8*/
                                            while (di < MB_CUR_MAX)
                                            {
                                                temp[di++] = (char) INC();
                                                if ( (convRes = mbtowc(&wctemp, temp, (size_t)MB_CUR_MAX)) > 0 ) 
                                                {
                                                    break; /*convert succeed*/
                                                }
                                            }
#endif
                                        }

                                        if (di == 1)
                                        {
                                            /*for windows system*/
                                            (void)mbtowc(&wctemp, temp, (size_t)MB_CUR_MAX);    /*no string termination error for Fortify*/
                                        }
                                        else /*di > 1*/
                                        {
                                            assert(convRes > 0); /*lint !e506*/
                                            if (convRes <= 0 )
                                            {
                                                wctemp = L'?';
                                            }
                                        }

                                        *(wchar_t UNALIGNED*)pointer = wctemp;
                                        /* just copy L'?' if mbtowc fails, errno is set by mbtowc */
                                        pointer = (wchar_t*)pointer + 1;
                                        --array_width;

                                    }
                                    else
#else  /* _UNICODE */
                                    if (fl_wchar_arg)
                                    {
                                        *(wchar_t UNALIGNED*)pointer = ch;
                                        pointer = (wchar_t*)pointer + 1;
                                        --array_width;
                                    }
                                    else
#endif  /* _UNICODE */
                                    {
#ifndef _UNICODE
                                        *(char*)pointer = (char)ch;
                                        pointer = (char*)pointer + 1;
                                        --array_width;

#else  /* _UNICODE */
                                        int temp = 0;
                                        /* convert wide to multibyte */
                                        if (array_width >= ((size_t)MB_CUR_MAX))
                                        {
                                            temp = wctomb((char*)pointer, ch);
                                        }
                                        else
                                        {
                                            char tmpbuf[MB_LEN_MAX];
                                            temp = wctomb(tmpbuf, ch);
                                            if (temp > 0 && ((size_t)temp) > array_width)
                                            {
                                                /* We have exhausted the user's buffer */
                                                enomem = 1;
                                                break;
                                            }
                                            if (temp > 0)
                                            {
                                                (void)memcpy_s(pointer, array_width, tmpbuf, (size_t)temp); 
                                            }
                                        }
                                        if (temp > 0)
                                        {
                                            /* do nothing if wctomb fails, errno will be set to EILSEQ */
                                            pointer = (char*)pointer + temp;
                                            array_width -= temp; /*lint !e737*/
                                        }
#endif  /* _UNICODE */
                                    }
                                } /* suppress */
                                else
                                {
                                    /* just indicate a match */
                                    
                                    start = (_TCHAR*)start + 1; /*lint !e613 */
                                }
                            }
                            else
                            {
                                UN_INC(ch);
                                break;
                            }
                        }

                        /* make sure something has been matched and, if
                        * assignment is not suppressed, null-terminate
                        * output string if comchr != c 
                        */

                        if (enomem)
                        {
                            /* In case of error, blank out the input buffer */
                            if (fl_wchar_arg)
                            {
                                if (start )
                                {
                                    *(wchar_t UNALIGNED*)start = 0;
                                }
                            }
                            else
                            {
                                if (start )
                                {
                                    *(char*)start = 0;     /*LSD add if (start )*/
                                }
                            }

                            goto error_return; /*lint !e801*/
                        }

                        if (start != pointer)
                        {
                            if (!suppress)
                            {
                                ++count;
                                CHECK_INPUT_ADDR(pointer);  /*lint !e801*/

                                if ('c' != comchr)
                                {
                                    /* null-terminate strings */
                                    if (fl_wchar_arg)
                                    {
                                        *(wchar_t UNALIGNED*)pointer = L'\0';
                                    }
                                    else
                                    {
                                        *(char*)pointer = '\0';
                                    }
                                }
                            }
                            else /*NULL*/;
                        }
                        else
                        {
                            goto error_return; /*lint !e801*/
                        }

                        break;

                    case _T('i') :      /* could be d, o, or x */

                        comchr = _T('d'); /* use as default */
                    /*lint -fallthrough*/
                    case _T('x'):

                        if (_T('-') == ch)
                        {
                            ++negative;

                            goto x_incwidth; /*lint !e801*/

                        }
                        else if (_T('+') == ch)
                        {
                        x_incwidth:
                            if (!--width && widthset)
                            { ++done_flag; }
                            else
                            { ch = INC(); }
                        }

                        if (_T('0') == ch)
                        {

                            if (_T('x') == (_TCHAR)(ch = INC()) || _T('X') == (_TCHAR)ch)
                            {
                                ch = INC();
                                if (widthset)
                                {
                                    width -= 2;
                                    if (width < 1)
                                    { ++done_flag; }
                                }
                                comchr = _T('x');
                            }
                            else
                            {
                                ++started;
                                if (_T('x') != comchr)
                                {
                                    if (widthset && !--width)
                                    {
                                        ++done_flag;
                                    }
                                    comchr = _T('o');
                                }
                                else
                                {
                                    /* scanning a hex number that starts */
                                    /* with a 0. push back the character */
                                    /* currently in ch and restore the 0 */
                                    UN_INC(ch);
                                    ch = _T('0');
                                }
                            }
                        }
                        goto getnum; /*lint !e801*/

                        /* NOTREACHED */
                    /*lint -fallthrough*/
                    case _T('p') :
                        /* force %hp to be treated as %p */
                        longone = 1;
#ifdef SECUREC_ON_64BITS
                        /* force %p to be 64 bit in 64bits system */
                        ++integer64;
                        num64 = 0;
#endif
                    /*lint -fallthrough*/
                    case _T('o') :
                    case _T('u') :
                    case _T('d') :

                        if (_T('-') == ch)
                        {
                            ++negative;
                            goto d_incwidth;  /*lint !e801*/
                        }
                        else if (_T('+') == ch)
                        {
                        d_incwidth:
                            if (!--width && widthset)
                            {
                                ++done_flag;
                            }
                            else
                            {
                                ch = INC();
                            }
                        }

                    getnum:
#if _INTEGRAL_MAX_BITS >= 64
                        if ( integer64 )
                        {

                            while (!done_flag)
                            {

                                if (_T('x') == comchr || _T('p') == comchr)

                                    if (_ISXDIGIT(ch))
                                    {
                                        num64 <<= 4;
                                        ch = _hextodec((_TCHAR)ch);
                                    }
                                    else
                                    { ++done_flag; }

                                else if (_ISDIGIT(ch))

                                    if (_T('o') == comchr)
                                        if (_T('8') > ch)
                                        { num64 <<= 3; }
                                        else
                                        {
                                            ++done_flag;
                                        }
                                    else /* _T('d') == comchr */
                                    {
                                        num64 = MUL10(num64);
                                    }

                                else
                                {
                                    ++done_flag;
                                }

                                if (!done_flag)
                                {
                                    ++started;
                                    num64 += ch - _T('0'); /*lint !e737*/

                                    if (widthset && !--width)
                                    {
                                        ++done_flag;
                                    }
                                    else
                                    {
                                        ch = INC();
                                    }
                                }
                                else
                                {
                                    UN_INC(ch);
                                }

                            } /* end of WHILE loop */

                            if (negative)
                            {
                                num64 = (UINT64T )(-(INT64T)num64);
                            }
                        }
                        else
                        {
#endif  /* _INTEGRAL_MAX_BITS >= 64    */
                            while (!done_flag)
                            {

                                if (_T('x') == comchr || _T('p') == comchr)
                                {

                                    if (_ISXDIGIT(ch))
                                    {
                                        number = (number << 4);
                                        ch = _hextodec((_TCHAR)ch);
                                    }
                                    else
                                    {
                                        ++done_flag;
                                    }
                                }

                                else if (_ISDIGIT(ch))
                                {

                                    if (_T('o') == comchr)
                                    {
                                        if (_T('8') > ch)
                                        {
                                            number = (number << 3);
                                        }
                                        else
                                        {
                                            ++done_flag;
                                        }
                                    }
                                    else /* _T('d') == comchr */
                                    {
                                        number = MUL10(number);
                                    }
                                }

                                else
                                {
                                    ++done_flag;
                                }

                                if (!done_flag)
                                {
                                    ++started;
                                    number += ch - _T('0'); /*lint !e737*/

                                    if (widthset && !--width)
                                    { ++done_flag; }
                                    else
                                    { ch = INC(); }
                                }
                                else
                                { UN_INC(ch); }

                            } /* end of WHILE loop */

                            if (negative)
                            {
                                number = (unsigned long)(-(long)number);
                            }
#if _INTEGRAL_MAX_BITS >= 64
                        }
#endif  /* _INTEGRAL_MAX_BITS >= 64    */
                        if (_T('F') == comchr) /* expected ':' in long pointer */
                        {
                            started = 0;
                        }

                        if (started)
                            if (!suppress)
                            {

                                ++count;
                            assign_num:
                                CHECK_INPUT_ADDR(pointer);    /*lint !e801*/
#if _INTEGRAL_MAX_BITS >= 64
                                if ( integer64 )
                                {
                                    *(INT64T UNALIGNED*)pointer = (UINT64T)num64; /*lint !e713, take num64 as unsigned number*/
                                }
                                else

#endif  /* _INTEGRAL_MAX_BITS >= 64    */
                                {
                                    if (longone > 1)
                                    {
                                        *(long UNALIGNED*)pointer = (unsigned long)number; /*lint !e713, take number as unsigned number*/
                                    }
                                    else if (longone == 1)
                                    {
                                        *(int UNALIGNED*)pointer = (int)number;
                                    }
                                    else
                                    {
                                        *(short UNALIGNED*)pointer = (unsigned short)number; /*lint !e713, take number as unsigned number*/
                                    }
                                }
                            }
                            else /*NULL*/;
                        else
                        {
                            goto error_return;   /*lint !e801*/
                        }

                        break;

                    case _T('n') :      /* char count, don't inc return value */
                        number = charcount; /*lint !e732*/
                        if (!suppress)
                        {
                            goto assign_num;   /*lint !e801*/
                        } /* found in number code above */
                        break;


                    case _T('e') :
                        /* case _T('E') : */
                    case _T('f') :
                    case _T('g') : /* scan a float */
                        /* case _T('G') : */
                        nFloatStrUsed = 0;

                        if (_T('-') == ch)
                        {
                            pFloatStr[nFloatStrUsed++] = _T('-');
                            goto f_incwidth; /*lint !e801*/

                        }
                        else if (_T('+') == ch)
                        {
                        f_incwidth:
                            --width;
                            ch = INC();
                        }

                        if (!widthset)              /* must watch width */
                        {
                            width = -1;
                        }


                        /* now get integral part */

                        while (_ISDIGIT(ch) && width--)
                        {
                            ++started;
                            pFloatStr[nFloatStrUsed++] = (_TCHAR)ch; /* ch must be '0' - '9'*/
                            if (__check_float_string(nFloatStrUsed,
                                                     &nFloatStrSz,
                                                     &pFloatStr,
                                                     floatstring,
                                                     &malloc_FloatStrFlag
                                                    ) == 0)
                            {
                                goto error_return;  /*lint !e801*/
                            }
                            ch = INC();
                        }

#ifdef _UNICODE
                        /* convert decimal point to wide-char */
                        /* if mbtowc fails (should never happen), we use L'.' */
                        decimal = L'.';
                        (void)mbtowc(&decimal, localeconv()->decimal_point, (size_t)MB_CUR_MAX);

#else  /* _UNICODE */
                        decimal = *localeconv()->decimal_point;
#endif  /* _UNICODE */

                        /* now check for decimal */
                        if (decimal == (char)ch && width--)
                        {
                            ch = INC();
                            pFloatStr[nFloatStrUsed++] = decimal;
                            if (__check_float_string(nFloatStrUsed,
                                                     &nFloatStrSz,
                                                     &pFloatStr,
                                                     floatstring,
                                                     &malloc_FloatStrFlag
                                                    ) == 0)
                            {
                                goto error_return;  /*lint !e801*/
                            }

                            while (_ISDIGIT(ch) && width--)
                            {
                                ++started;
                                pFloatStr[nFloatStrUsed++] = (_TCHAR)ch;
                                if (__check_float_string(nFloatStrUsed,
                                                         &nFloatStrSz,
                                                         &pFloatStr,
                                                         floatstring,
                                                         &malloc_FloatStrFlag
                                                        ) == 0)
                                {
                                    goto error_return;  /*lint !e801*/
                                }
                                ch = INC();
                            }
                        }

                        /* now check for exponent */

                        if (started && (_T('e') == ch || _T('E') == ch) && width--)
                        {
                            pFloatStr[nFloatStrUsed++] = _T('e');
                            if (__check_float_string(nFloatStrUsed,
                                                     &nFloatStrSz,
                                                     &pFloatStr,
                                                     floatstring,
                                                     &malloc_FloatStrFlag
                                                    ) == 0)
                            {
                                goto error_return;  /*lint !e801*/
                            }

                            if (_T('-') == (ch = INC()))
                            {

                                pFloatStr[nFloatStrUsed++] = _T('-');
                                if (__check_float_string(nFloatStrUsed,
                                                         &nFloatStrSz,
                                                         &pFloatStr,
                                                         floatstring,
                                                         &malloc_FloatStrFlag
                                                        ) == 0)
                                {
                                    goto error_return;  /*lint !e801*/
                                }
                                goto f_incwidth2; /*lint !e801*/

                            }
                            else if (_T('+') == ch)
                            {
                            f_incwidth2:
                                if (!width--)
                                {
                                    ++width;
                                }
                                else
                                {
                                    ch = INC();
                                }
                            }


                            while (_ISDIGIT(ch) && width--)
                            {
                                ++started;
                                pFloatStr[nFloatStrUsed++] = (_TCHAR)ch;
                                if (__check_float_string(nFloatStrUsed,
                                                         &nFloatStrSz,
                                                         &pFloatStr,
                                                         floatstring,
                                                         &malloc_FloatStrFlag
                                                        ) == 0)
                                {
                                    goto error_return; /*lint !e801*/
                                }
                                ch = INC();
                            }

                        }

                        UN_INC(ch);

                        if (started)
                            if (!suppress)
                            {
                                ++count;
                                pFloatStr[nFloatStrUsed] = _T('\0');
#ifdef _UNICODE
                                {
                                    /* convert floatstring to char string */
                                    /* and do the conversion */
                                    size_t cfslength;
                                    char* cfloatstring;

                                    /*
                                    * Basically the code below assumes that the MULTI BYTE
                                    * Characters are at max 2 bytes. This is true for CRT
                                    * because currently we don't support UTF8.
                                    */
                                    cfslength = (size_t)(nFloatStrSz + 1) * WCHAR_SIZE;

                                    if ((cfloatstring = (char*)malloc (cfslength)) == NULL)
                                    {
                                        goto error_return;  /*lint !e801*/
                                    }

                                    if (wcstombs (cfloatstring, pFloatStr, cfslength - 1) > 0)
                                    {
                                        /*LSD from wcstombs_s tp wcstombs add >0*/
                                        _safecrt_fassign( longone - 1, (char*)pointer , cfloatstring, (char)decimal);
                                    }
                                    else
                                    {
                                        free (cfloatstring);
                                        goto error_return;  /*lint !e801*/
                                    }
                                    free (cfloatstring);
                                }
#else  /* _UNICODE */
                                _safecrt_fassign( longone - 1, (char*)pointer , pFloatStr, (char)decimal);
#endif  /* _UNICODE */
                            }
                            else /*NULL */;
                        else
                        {
                            goto error_return; /*lint !e801*/
                        }

                        break;


                    default:    /* either found '%' or something else */

                        if ((int)*format != (int)ch)
                        {
                            UN_INC(ch);
                            /* error_return ASSERT's if format_error is true */
                            format_error = 1;
                            goto error_return; /*lint !e801*/
                        }
                        else
                        {
                            match--; /* % found, compensate for inc below */
                        }

                        if (!suppress)
                        {
#if defined(va_copy)
                        va_copy(arglist, arglistsave);
                        arglistBeenCopied = 1;
                        va_end(arglistsave);
#elif defined(__va_copy) /*for vxworks*/
                        __va_copy(arglist, arglistsave);
                        arglistBeenCopied = 1;
                        va_end(arglistsave);
#else
                         arglist = arglistsave;
#endif
                        }
                } /* SWITCH */

                match++;        /* matched a format field - set flag */

            } /* WHILE (width) */

            else
            {
                /* zero-width field in format string */
                UN_INC(ch);  /* check for input error */
                goto error_return; /*lint !e801*/
            }

            ++format;  /* skip to next char */

        }
        else  /*  ('%' != *format) */
        {

            if ((int)*format++ != (int)(ch = INC()))
            {
                UN_INC(ch);
                goto error_return; /*lint !e801*/
            }
#ifndef _UNICODE
            if (isleadbyte((uint8_t)ch))
            {
                int ch2;
                char temp[MULTI_BYTE_MAX_LEN];

                if ((int)*format++ != (ch2 = INC()))
                {
                    UN_INC(ch2);    /*LSD in console mode, UN_INC twice will cause problem */
                    UN_INC(ch);
                    goto error_return;  /*lint !e801*/
                }
                if (MB_CUR_MAX > 2 && (((uint8_t)ch & 0xE0) == 0xE0) && (((uint8_t)ch & 0x80) == 0x80) )
                {
                    /*this char is very likely to be a UTF-8 char*/
                    int ch3 = INC();
                    temp[0] = (char)ch;
                    temp[1] = (char)ch2;
                    temp[2] = (char)ch3;

                    if (  mbtowc(&wctemp, temp, (size_t)MB_CUR_MAX) > 0 )
                    {
                        /*succeed*/
                        if ((int)*format++ != (int)ch3)
                        {
                            UN_INC(ch3);
                            goto error_return;  /*lint !e801*/
                        }
                        --charcount;
                    }
                    else
                    {
                        UN_INC(ch3);
                    }
                }
                --charcount; /* only count as one character read */
            }
#endif  /* _UNICODE */
        }

        if ( (_TEOF == ch) && ((*format != _T('%')) || (*(format + 1) != _T('n'))) )
        {
            break;
        }

    }  /* WHILE (*format) */

error_return:
#if ALLOC_TABLE
    if (malloc_flag == 1)
    {
        free(table);
    }
#endif  /* ALLOC_TABLE */

#if  defined(va_copy) || defined(__va_copy)
    if (arglistBeenCopied)
    {
        va_end(arglist);
    }
#endif
    /*LSD 2014.3.6 add, clear the stack data*/
    (void)memset_s(floatstring, (_CVTBUFSIZE + 1) * sizeof(_TCHAR), 0 ,  (_CVTBUFSIZE + 1) * sizeof(_TCHAR) );
    if (malloc_FloatStrFlag == 1)
    {
        /* pFloatStr can be alloced in __check_float_string function, cleat and free it*/
        (void)memset_s(pFloatStr, nFloatStrSz * sizeof(_TCHAR), 0 ,  nFloatStrSz * sizeof(_TCHAR) );
        free(pFloatStr); /*lint !e424 */
    }

    if (stream != NULL && (stream->_flag & FILE_STREAM_FLAG) && stream->_base != NULL)
    {
        free(stream->_base); /*lint !e613*/

        /*LSD seek to original position, bug fix 2014 1 21*/
        if ( fseek(stream->pf, stream->oriFilePos + stream->fileRealRead, SEEK_SET) ) /*lint !e613*/
        {
            /*seek failed*/
            assert(0); /*lint !e506*/
        }

    }

    if (_TEOF == ch)
    {
        /* If any fields were matched or assigned, return count */
        return ( (count || match) ? count : EOF);
    }
    else if (format_error == 1)
    {
        /*Invalid Input Format*/
        return -2;
    }
    else if (paraIsNull)
    {
        return -2;
    }

    return count;
}

/* _hextodec() returns a value of 0-15 and expects a char 0-9, a-f, A-F */
/* _inc() is the one place where we put the actual getc code. */
/* _whiteout() returns the first non-blank character, as defined by isspace() */

static _TINT  _hextodec ( _TCHAR chr)
{
    return _ISDIGIT(chr) ? chr : ((chr & ~(_T('a') - _T('A'))) - _T('A')) + 10 + _T('0');
}

#ifdef _UNICODE
#define _gettc_nolock   getWch____
#define _UN_T_INC     _un_w_inc
#else
#define _gettc_nolock   getCh____
#define _UN_T_INC     _un_c_inc
#endif

#define BUFFERED_BLOK_SIZE 1024


#ifdef _UNICODE
static _TINT getWch____(SEC_FILE_STREAM* str)
#else
static _TINT getCh____(SEC_FILE_STREAM* str)
#endif
{

    _TINT ch = 0;
    int firstReadOnFile = 0;
    do
    {
        if ((str->_flag & FROM_STDIN_FLAG) > 0 )
        {

            if (str->fUnget)
            {
                ch = (_TINT)str->lastChar;
                str->fUnget = 0;

            }
            else
            {
                ch = _tfgetc(stdin);
                str->lastChar = (unsigned int)ch; /*lint !e732*/

            }
            break;
        }
        if ((str->_flag & FILE_STREAM_FLAG) > 0 && str->_cnt == 0 )
        {
            /*load file to buffer*/
            if (!str->_base )
            {
                str->_base = (char*)malloc(BUFFERED_BLOK_SIZE);
                if (!str->_base )
                {
                    ch = _TEOF;
                    break;
                }
            }
            /*LSD add 2014.3.21*/
            if (UNINITIALIZED_FILE_POS == str->oriFilePos)
            {
                str->oriFilePos = ftell(str->pf);    /* save original file read position*/
                firstReadOnFile = 1;
            }
            str->_cnt = (int)fread(str->_base,  1, BUFFERED_BLOK_SIZE, str->pf);

            if (0 == str->_cnt || str->_cnt > BUFFERED_BLOK_SIZE)
            {
                ch = _TEOF;
                break;
            }
            str->_ptr = str->_base;
            str->_flag |= LOAD_FILE_TO_MEM_FLAG;
            if (firstReadOnFile)
            {
#ifdef _UNICODE
                /*LSD fix 2014.3.24, add (uint8_t) to (str->_base[0])*/
                if ( str->_cnt > 1 && (uint8_t)(str->_base[0]) == 0xFFU && (uint8_t)(str->_base[1]) == 0xFEU)
                {
                    /*it's BOM header, UNICODE little endian*/
                    str->_cnt -= BOM_HEADER_SIZE;
                    str->_ptr += BOM_HEADER_SIZE;
                }
                if ( str->_cnt > 1 && (uint8_t)(str->_base[0]) == 0xFEU && (uint8_t)(str->_base[1]) == 0xFFU)
                {
                    /*it's BOM header, UNICODE big endian*/
                    str->_cnt -= BOM_HEADER_SIZE;
                    str->_ptr += BOM_HEADER_SIZE;
                }
#else
                if ( str->_cnt > 2 && (uint8_t)(str->_base[0]) == 0xEFU && (uint8_t)(str->_base[1]) == 0xBBU && (uint8_t)(str->_base[2]) == 0xBFU)
                {
                    /*it's BOM header,  little endian*/
                    str->_cnt -= UTF8_BOM_HEADER_SIZE;
                    str->_ptr += UTF8_BOM_HEADER_SIZE;
                }
#endif
            }
        }


        if ((str->_flag & MEM_STR_FLAG) > 0 || (str->_flag & LOAD_FILE_TO_MEM_FLAG) > 0  )
        {
            ch = (_TINT)((str->_cnt -= (int)sizeof(_TCHAR)) >= 0 ? (_tCharMask & * ((_TCHAR*)str->_ptr)) : _TEOF); /*lint !e826*/
            str->_ptr += sizeof(_TCHAR);
        }
    }
    while (0); /*lint !e717, use break in do-while to skip some code*/

    if (_TEOF != ch && (str->_flag & FILE_STREAM_FLAG) > 0 && str->_base)
    {
        str->fileRealRead  += (int)sizeof(_TCHAR);  
    }
    return ch;

}

static _TINT  _inc(SEC_FILE_STREAM* fileptr)
{
    return (_gettc_nolock(fileptr));
}

/*FIXIT _un_inc w version*/
#ifndef TRUE
#define TRUE 1
#endif

#ifdef _UNICODE
static void  _un_w_inc(_TINT chr, SEC_FILE_STREAM* str)
#else
static void  _un_c_inc(_TINT chr, SEC_FILE_STREAM* str)
#endif
{
    if (_TEOF != chr)
    {

        if ((str->_flag & FROM_STDIN_FLAG) > 0)
        {
            str->lastChar = (unsigned int)chr;
            str->fUnget = TRUE;
        }
        else if ( (str->_flag & MEM_STR_FLAG) || (str->_flag & LOAD_FILE_TO_MEM_FLAG) > 0)
        {
            if (str->_ptr > str->_base)
            {
                str->_ptr -= sizeof(_TCHAR);
                str->_cnt += (int)sizeof(_TCHAR);
            }
        }

        if ( (str->_flag & FILE_STREAM_FLAG) > 0 && str->_base)
        {
            str->fileRealRead -= (int)sizeof(_TCHAR);    /*LSD fix, change from -- str->fileRealRead to str->fileRealRead -= sizeof(_TCHAR). 2014.2.21*/
        }

    }
}

static void  _un_inc(_TINT chr, SEC_FILE_STREAM* str)
{
    _UN_T_INC(chr, str);
}

static _TINT  _whiteout(int* counter, SEC_FILE_STREAM* fileptr)
{
    _TINT ch;

    do
    {
        ++*counter;
        ch = _inc(fileptr);

        if (ch == _TEOF)
        {
            break;
        }
    }
#ifdef _UNICODE
    while (iswspace((_TUCHAR)ch));
#else
    while (isspace((_TUCHAR)ch));
#endif
    return ch;
}

#endif /*__INPUT_INL__5D13A042_DC3F_4ED9_A8D1_882811274C27*/


