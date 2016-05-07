/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: secureprintoutput.h
* Decription: 
*             define marco, enum, data struct, and declare internal used function
*             prototype, which is used by output.inl, secureprintoutput_w.c and
*             secureprintoutput_a.c.
* History:   
*     1. Date:
*         Author:    
*         Modification:
********************************************************************************
*/

#ifndef __SECUREPRINTOUTPUT_H__E950DA2C_902F_4B15_BECD_948E99090D9C
#define __SECUREPRINTOUTPUT_H__E950DA2C_902F_4B15_BECD_948E99090D9C

/* flag definitions */
#define FL_SIGN       0x00001   /* put plus or minus in front */
#define FL_SIGNSP     0x00002   /* put space or minus in front */
#define FL_LEFT       0x00004   /* left justify */
#define FL_LEADZERO   0x00008   /* pad with leading zeros */
#define FL_LONG       0x00010   /* long value given */
#define FL_SHORT      0x00020   /* short value given */
#define FL_SIGNED     0x00040   /* signed data given */
#define FL_ALTERNATE  0x00080   /* alternate form requested */
#define FL_NEGATIVE   0x00100   /* value is negative */
#define FL_FORCEOCTAL 0x00200   /* force leading '0' for octals */
#define FL_LONGDOUBLE 0x00400   /* long double value given */
#define FL_WIDECHAR   0x00800   /* wide characters */
#define FL_LONGLONG   0x01000   /* long long value given */
#define FL_I64        0x08000   /* INT64T value given */
#ifdef POSITIONAL_PARAMETERS
/* We set this flag if %I is passed without I32 or I64 */
#define FL_PTRSIZE    0x10000   /* platform dependent number */
#endif                          /* POSITIONAL_PARAMETERS */

/* state definitions */
enum STATE
{
    ST_NORMAL,                  /* normal state; outputting literal chars */
    ST_PERCENT,                 /* just read '%' */
    ST_FLAG,                    /* just read flag character */
    ST_WIDTH,                   /* just read width specifier */
    ST_DOT,                     /* just read '.' */
    ST_PRECIS,                  /* just read precision specifier */
    ST_SIZE,                    /* just read size specifier */
    ST_TYPE,                    /* just read type specifier */
    ST_INVALID                  /* Invalid format */
};

#define NUMSTATES (ST_INVALID + 1)

/* character type values */
enum CHARTYPE
{
    CH_OTHER,                   /* character with no special meaning */
    CH_PERCENT,                 /* '%' */
    CH_DOT,                     /* '.' */
    CH_STAR,                    /* '*' */
    CH_ZERO,                    /* '0' */
    CH_DIGIT,                   /* '1'..'9' */
    CH_FLAG,                    /* ' ', '+', '-', '#' */
    CH_SIZE,                    /* 'h', 'l', 'L', 'N', 'F', 'w' */
    CH_TYPE                     /* type specifying character */
};

#define BUFFERSIZE    512
#define MAXPRECISION  BUFFERSIZE

#ifndef MB_LEN_MAX
#define MB_LEN_MAX 5
#endif
#define CVTBUFSIZE (309+40)      /* # of digits in max. dp value + slop */

#define FIND_CHAR_CLASS(lookuptbl, c)      \
    ((c) < _T(' ') || (c) > _T('x') ?      \
     CH_OTHER                              \
     :                                     \
     (enum CHARTYPE)(lookuptbl[(c)-_T(' ')] & 0xF))

#define FIND_NEXT_STATE(lookuptbl, class, state)   \
    (enum STATE)(lookuptbl[(class) * NUMSTATES + (state)] >> 4)

typedef struct _SECUREC_XXXPRINTF_STREAM
{
    int _cnt;
    char* _ptr;
} SECUREC_XPRINTF_STREAM;

/*LSD remove int util_get_int_arg (va_list *pargptr);
* long util_get_long_arg (va_list *pargptr);
* INT64T util_get_int64_arg (va_list *pargptr);
* LSD this function is deprecated short util_get_short_arg (va_list *pargptr); 
* void* util_get_ptr_arg (va_list *pargptr);
* long long is int64
* #define util_get_long_long_arg(x) util_get_int64_arg(x)
*/

void cfltcvt(double value, char* buffer, int bufSize, char fmt, int precision, int capexp);

void write_char_a
(
    char ch,
    SECUREC_XPRINTF_STREAM* f,
    int* pnumwritten
);

void write_char_w
(
    wchar_t ch,
    SECUREC_XPRINTF_STREAM* f,
    int* pnumwritten
);

void write_multi_char_a
(
    char ch,
    int num,
    SECUREC_XPRINTF_STREAM* f,
    int* pnumwritten
);

void write_multi_char_w
(
    wchar_t ch,
    int num,
    SECUREC_XPRINTF_STREAM* f,
    int* pnumwritten
);

void write_string_a
(
    char* string,
    int len,
    SECUREC_XPRINTF_STREAM* f,
    int* pnumwritten
);

void write_string_w
(
    wchar_t* string,
    int len,
    SECUREC_XPRINTF_STREAM* f,
    int* pnumwritten
);

int securec_output_s
(
    SECUREC_XPRINTF_STREAM* stream,
    const char* format,
    va_list argptr
);

int securec_woutput_s
(
    SECUREC_XPRINTF_STREAM* stream,
    const wchar_t* format,
    va_list argptr
);


#endif


