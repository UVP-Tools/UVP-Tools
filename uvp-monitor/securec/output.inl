/*******************************************************************************
* Copyright @ Huawei Technologies Co., Ltd. 1998-2014. All rights reserved.  
* File name: output.inl
* Description: 
*             used by secureprintoutput_a.c and secureprintoutput_w.c to include.
*             This file provides a template function for ANSI and UNICODE compiling
*             by different type definition. The functions of securec_output_s or
*             securec_woutput_s  provides internal implementation for printf family
*             API, such as sprintf, swprintf_s.
* History:   
*     1. Date:
*        Author:    
*        Modification:
********************************************************************************
*/

#ifndef OUTPUT_INL_2B263E9C_43D8_44BB_B17A_6D2033DECEE5
#define OUTPUT_INL_2B263E9C_43D8_44BB_B17A_6D2033DECEE5

#define DISABLE_N_FORMAT

#ifndef INT_MAX
#define INT_MAX (0x7FFFFFFF)
#endif

#ifndef _CVTBUFSIZE
#define _CVTBUFSIZE (309+40)       /* # of digits in max. dp value + slop */
#endif

#define STR_NULL_STRING  "(null)"       /* string to print on null ptr */
#define WSTR_NULL_STRING  L"(null)"  /* string to print on null ptr */

#ifdef _XXXUNICODE
int securec_woutput_s
#else
int securec_output_s
#endif
(
    SECUREC_XPRINTF_STREAM* stream,
    const TCHAR* format,
    va_list argptr
)
{
    int hexadd = 0;                 /* offset to add to number to get 'a'..'f' */
    TCHAR ch;                       /* character just read */
    int flags = 0;                  /* flag word -- see #defines above for flag values */
    enum STATE state;               /* current state */
    enum CHARTYPE chclass;          /* class of current character */
    unsigned int radix;                      /* current conversion radix */
    int charsout;                   /* characters currently written so far, -1 = IO error */
    int fldwidth = 0;               /* selected field width -- 0 means default */
    int precision = 0;              /* selected precision  -- -1 means default */
    TCHAR prefix[2];                /* numeric prefix -- up to two characters */
    int prefixlen = 0;              /* length of prefix -- 0 means no prefix */
    int capexp = 0;                 /* non-zero = 'E' exponent signifient, zero = 'e' */
    int no_output = 0;              /* non-zero = prodcue no output for this specifier */
    union
    {
        char* sz;                   /* pointer text to be printed, not zero terminated */
        wchar_t* wz;
    } text;

    int textlen;                    /* length of the text in bytes/wchars to be printed.
                                    * textlen is in multibyte or wide chars if _UNICODE
                                    */
    union
    {
        char sz[BUFFERSIZE];
#ifdef _XXXUNICODE
        wchar_t wz[BUFFERSIZE];
#endif  /* _XXXUNICODE */
    } buffer;
    wchar_t wchar = 0;                /* temp wchar_t */
    int buffersize = 0;               /* size of text.sz (used only for the call to _cfltcvt) */
    int bufferiswide = 0;             /* non-zero = buffer contains wide chars already */

    char* heapbuf = NULL;             /* non-zero = test.sz using heap buffer to be freed */

    assert(format != NULL);           /*lint !e506*/

    charsout = 0;                     /* no characters written yet */
    textlen = 0;                               /* no text yet */
    state = ST_NORMAL;                /* starting state */
    text.sz = NULL;

    /* main loop -- loop while format character exist and no I/O errors */
    while (format != NULL && (ch = *format++) != _T('\0') && charsout >= 0)
    {
        chclass = FIND_CHAR_CLASS(securec__lookuptable_s, ch);  /* find character class */
        state = FIND_NEXT_STATE(securec__lookuptable_s, chclass, state); /* find next state */

        /* execute code for each state */
        switch (state)
        {
            case ST_INVALID:
                /*_VALIDATE_RETURN(("Incorrect format specifier", 0), EINVAL, -1); */
                return -1;
            case ST_NORMAL:

            NORMAL_STATE:

                /* normal state -- just write character */
#ifdef _XXXUNICODE
                bufferiswide = 1;
#else  /* _XXXUNICODE */
                bufferiswide = 0;
#endif  /* _XXXUNICODE */
                write_char(ch, stream, &charsout);
                break;

            case ST_PERCENT:
                /* set default value of conversion parameters */
                prefixlen = fldwidth = no_output = capexp = 0;
                flags = 0;
                precision = -1;
                bufferiswide = 0;   /* default */
                break;

            case ST_FLAG:
                /* set flag based on which flag character */
                switch (ch)
                {
                    case _T('-'):
                        flags |= FL_LEFT;       /* '-' => left justify */
                        break;
                    case _T('+'):
                        flags |= FL_SIGN;       /* '+' => force sign indicator */
                        break;
                    case _T(' '):
                        flags |= FL_SIGNSP;     /* ' ' => force sign or space */
                        break;
                    case _T('#'):
                        flags |= FL_ALTERNATE;  /* '#' => alternate form */
                        break;
                    case _T('0'):
                        flags |= FL_LEADZERO;   /* '0' => pad with leading zeros */
                        break;
                    default:
                        break;
                }
                break;

            case ST_WIDTH:
                /* update width value */
                if (ch == _T('*'))
                {
                    /* get width from arg list */
                    fldwidth = va_arg(argptr, int); /*lint !e826*/
                    if (fldwidth < 0)
                    {
                        /* ANSI says neg fld width means '-' flag and pos width */
                        flags |= FL_LEFT;
                        fldwidth = -fldwidth;
                    }
                }
                else
                {
                    /* add digit to current field width */
                    fldwidth = fldwidth * 10 + (ch - _T('0'));
                }
                break;

            case ST_DOT:
                /* zero the precision, since dot with no number means 0
                   not default, according to ANSI */
                precision = 0;
                break;

            case ST_PRECIS:
                /* update precison value */
                if (ch == _T('*'))
                {
                    /* get precision from arg list */
                    precision =  va_arg(argptr, int); /*lint !e826*/
                    if (precision < 0)
                    {
                        precision = -1; /* neg precision means default */
                    }
                }
                else
                {
                    /* add digit to current precision */
                    precision = precision * 10 + (ch - _T('0'));
                }
                break;

            case ST_SIZE:
                /* just read a size specifier, set the flags based on it */
                switch (ch)
                {
                    case _T('l'):
                        /*
                         * In order to handle the ll case, we depart from the
                         * simple deterministic state machine.
                         */
                        if (*format == _T('l'))
                        {
                            ++format;
                            flags |= FL_LONGLONG;   /* 'll' => long long */
                        }
                        else
                        {
                            flags |= FL_LONG;       /* 'l' => long int or wchar_t */
                        }
                        break;

                    case _T('I'):
                        /*
                         * In order to handle the I, I32, and I64 size modifiers, we
                         * depart from the simple deterministic state machine. The
                         * code below scans for characters following the 'I',
                         * and defaults to 64 bit on WIN64 and 32 bit on WIN32
                         */
#ifdef SECUREC_ON_64BITS
                        flags |= FL_I64;    /* 'I' => INT64T on 64bits systems */
#endif
                        if ( (*format == _T('6')) && (*(format + 1) == _T('4')) )
                        {
                            format += 2;
                            flags |= FL_I64;    /* I64 => INT64T */
                        }
                        else if ( (*format == _T('3')) && (*(format + 1) == _T('2')) )
                        {
                            format += 2;
                            flags &= ~FL_I64;   /* I32 => __int32 */
                        }
                        else if ( (*format == _T('d')) ||
                                  (*format == _T('i')) ||
                                  (*format == _T('o')) ||
                                  (*format == _T('u')) ||
                                  (*format == _T('x')) ||
                                  (*format == _T('X')) )
                        {
                            /*
                             * Nothing further needed.  %Id (et al) is
                             * handled just like %d, except that it defaults to 64 bits
                             * on WIN64.  Fall through to the next iteration.
                             */
                        }
                        else
                        {
                            state = ST_NORMAL;
                            goto NORMAL_STATE; /*lint !e801*/
                        }
                        break;

                    case _T('h'):
                        flags |= FL_SHORT;  /* 'h' => short int or char */
                        break;

                    case _T('w'):
                        flags |= FL_WIDECHAR;  /* 'w' => wide character */
                        break;
                    default:
                       break;

                }
                break;

            case ST_TYPE:
                /* we have finally read the actual type character, so we 
                * now format and "print" the output.  We use a big switch
                * statement that sets 'text' to point to the text that should
                * be printed, and 'textlen' to the length of this text.
                * Common code later on takes care of justifying it and
                * other miscellaneous chores.  Note that cases share code,
                * in particular, all integer formatting is done in one place.
                * Look at those funky goto statements! 
                */

                switch (ch)
                {

                    case _T('C'):   /* ISO wide character */
                        if (!(flags & (FL_SHORT | FL_LONG | FL_WIDECHAR)))
                        {

#ifdef _XXXUNICODE
                            flags |= FL_SHORT;
#else  /* _XXXUNICODE */
                            flags |= FL_WIDECHAR;   /* ISO std. */
#endif  /* _XXXUNICODE */
                        }

                        /* fall into 'c' case */
                        /*lint -fallthrough */
                    case _T('c'):
                    {
                        /* print a single character specified by int argument */
#ifdef _XXXUNICODE
                        bufferiswide = 1;
                        wchar =  (wchar_t)va_arg(argptr, int); /*lint !e826*/
                        if (flags & FL_SHORT)
                        {
                            /* format multibyte character */
                            /* this is an extension of ANSI */
                            char tempchar[2];
                            {
                                tempchar[0] = (char)(wchar & 0x00ff);
                                tempchar[1] = '\0';
                            }

                            if (mbtowc(buffer.wz, tempchar, (size_t)MB_CUR_MAX) < 0)
                            {
                                /* ignore if conversion was unsuccessful */
                                no_output = 1;
                            }
                        }
                        else
                        {
                            buffer.wz[0] = wchar;
                        }
                        text.wz = buffer.wz;
                        textlen = 1;    /* print just a single character */
#else  /* _XXXUNICODE */
                        if (flags & (FL_LONG | FL_WIDECHAR))
                        {
                            wchar = (wchar_t)va_arg(argptr, int);  /*lint !e826*/
                            /* convert to multibyte character */
                            /*
                            if (wctomb_s(&textlen, buffer.sz, sizeof(buffer.sz)/sizeof(buffer.sz[0]), wchar) != 0)
                            {
                                no_output = 1;
                            }*/
                            textlen = wctomb(buffer.sz, wchar);
                            if (textlen < 0)
                            {
                                no_output = 1;
                            }
                        }
                        else
                        {
                            /* format multibyte character */
                            /* this is an extension of ANSI */
                            unsigned short temp;
                            temp = (unsigned short) va_arg(argptr, int); /*lint !e826*/
                            {
                                buffer.sz[0] = (char) temp;
                                textlen = 1;
                            }
                        }
                        text.sz = buffer.sz;
#endif  /* _XXXUNICODE */
                    }
                    break;

                    case _T('Z'):
                    {
                        /* print a Counted String */
                        struct _count_string
                        {
                            short Length;
                            short MaximumLength;
                            char* Buffer;
                        } *pstr;

                        pstr = va_arg(argptr, struct _count_string*);    /*lint !e826*/
                        if (pstr == NULL || pstr->Buffer == NULL)
                        {
                            /* null ptr passed, use special string */
                            text.sz = STR_NULL_STRING;
                            textlen = (int)strlen(text.sz);
                        }
                        else
                        {
                            if (flags & FL_WIDECHAR)
                            {
                                text.wz = (wchar_t*)pstr->Buffer; /*lint !e826*/
                                textlen = pstr->Length / (int)WCHAR_SIZE;
                                bufferiswide = 1;
                            }
                            else
                            {
                                bufferiswide = 0;
                                text.sz = pstr->Buffer;
                                textlen = pstr->Length;
                            }
                        }
                    }
                    break;

                    case _T('S'):   /* ISO wide character string */
#ifndef _XXXUNICODE
                        if (!(flags & (FL_SHORT | FL_LONG | FL_WIDECHAR)))
                        {
                            flags |= FL_WIDECHAR;
                        }
#else  /* _XXXUNICODE */
                        if (!(flags & (FL_SHORT | FL_LONG | FL_WIDECHAR)))
                        {
                            flags |= FL_SHORT;
                        }
#endif  /* _XXXUNICODE */
                    /*lint -fallthrough */
                    case _T('s'):
                    {
                        /* print a string --
                        * ANSI rules on how much of string to print:
                        * all if precision is default, 
                        * min(precision, length) if precision given.
                        * prints '(null)' if a null string is passed 
                        */

                        int i;
                        char* p;       /* temps */
                        wchar_t* pwch;

                        /* At this point it is tempting to use strlen(), but 
                        * if a precision is specified, we're not allowed to
                        * scan past there, because there might be no null
                        * at all.  Thus, we must do our own scan.
                        */

                        i = (precision == -1) ? INT_MAX : precision;
                        text.sz = va_arg(argptr, char*);  /*lint !e826*/

                        /* scan for null upto i characters */
#ifdef _XXXUNICODE
                        if (flags & FL_SHORT)
                        {
                            if (text.sz == NULL) /* NULL passed, use special string */
                            {
                                text.sz = STR_NULL_STRING;
                            }
                            p = text.sz;
                            for (textlen = 0; textlen < i && *p; textlen++)
                            {
                                ++p;
                            }
                            /* textlen now contains length in multibyte chars */
                        }
                        else
                        {
                            if (text.wz == NULL) /* NULL passed, use special string */
                            {
                                text.wz = WSTR_NULL_STRING;
                            }
                            bufferiswide = 1;
                            pwch = text.wz;
                            while (i-- && *pwch)
                            {
                                ++pwch;
                            }
                            textlen = (int)(pwch - text.wz);       /* in wchar_ts */
                            /* textlen now contains length in wide chars */
                        }
#else  /* _XXXUNICODE */
                        if (flags & (FL_LONG | FL_WIDECHAR))
                        {
                            if (text.wz == NULL) /* NULL passed, use special string */
                            {
                                text.wz = WSTR_NULL_STRING;
                            }
                            bufferiswide = 1;
                            pwch = text.wz;
                            while ( i-- && *pwch )
                            {
                                ++pwch;
                            }
                            textlen = (int)(pwch - text.wz);
                            /* textlen now contains length in wide chars */
                        }
                        else
                        {
                            if (text.sz == NULL) /* NULL passed, use special string */
                            {
                                text.sz = STR_NULL_STRING;
                            }
                            p = text.sz;
                            while (i-- && *p)
                            {
                                ++p;
                            }
                            textlen = (int)(p - text.sz);    /* length of the string */
                        }

#endif  /* _XXXUNICODE */
                    }
                    break;


                    case _T('n'):
                    {
                        /* write count of characters seen so far into
                        * short/int/long thru ptr read from args 
                        */

                        /* if %n is disabled, we skip an arg and print 'n' */
#ifdef DISABLE_N_FORMAT
                        /* _VALIDATE_RETURN(("'n' format specifier disabled", 0), EINVAL, -1); */
                        return -1;
#else
                        void* p;        /* temp */
                        p =  va_arg(argptr, void*);  /* LSD util_get_ptr_arg(&argptr);*/

                        /* store chars out into short/long/int depending on flags */
#ifdef SECUREC_ON_64BITS
                        if (flags & FL_LONG)
                        {
                            *(long*)p = charsout;
                        }
                        else
#endif  /* SECUREC_ON_64BITS */

                            if (flags & FL_SHORT)
                            {
                                *(short*)p = (short) charsout;
                            }
                            else
                            {
                                *(int*)p = charsout;
                            }

                        no_output = 1;              /* force no output */
                        break;
#endif
                    }

                    case _T('E'):
                    case _T('G'):
                    case _T('A'):
                        capexp = 1;                 /* capitalize exponent */
                        ch += _T('a') - _T('A');    /* convert format char to lower */
                        /* DROP THROUGH */
                        /*lint -fallthrough */
                    case _T('e'):
                    case _T('f'):
                    case _T('g'):
                    case _T('a'):
                    {
                        /* floating point conversion -- we call cfltcvt routines */
                        /* to do the work for us.                                */
                        flags |= FL_SIGNED;         /* floating point is signed conversion */
                        text.sz = buffer.sz;        /* put result in buffer */
                        buffersize = BUFFERSIZE;

                        /* compute the precision value */
                        if (precision < 0)
                        {
                            precision = 6;          /* default precision: 6 */
                        }
                        else if (precision == 0 && ch == _T('g'))
                        {
                            precision = 1;          /* ANSI specified */
                        }
                        else if (precision > MAXPRECISION)
                        {
                            precision = MAXPRECISION;
                        }

                        if (precision > BUFFERSIZE - _CVTBUFSIZE)
                        {
                            /* conversion will potentially overflow local buffer */
                            /* so we need to use a heap-allocated buffer.        */
                            heapbuf = (char*)malloc((size_t)(_CVTBUFSIZE + precision));
                            if (heapbuf != NULL)
                            {
                                text.sz = heapbuf;
                                buffersize = _CVTBUFSIZE + precision;
                            }
                            else
                            {
                                /* malloc failed, cap precision further */
                                precision = BUFFERSIZE - _CVTBUFSIZE;
                            }
                        }

                        /* for safecrt, we pass along the FL_ALTERNATE flag to _safecrt_cfltcvt */
                        if (flags & FL_ALTERNATE)
                        {
                            capexp |= FL_ALTERNATE;
                        }

                        {
                            /*_CRT_DOUBLE tmp;*/
                            double tmp;
                            tmp = va_arg(argptr, double); /*lint !e826*/
                            /* Note: assumes ch is in ASCII range
                            * In safecrt, we provide a special version of _cfltcvt which internally calls printf (see safecrt_output_s.c)
                            */

                            /*_CFLTCVT(&tmp, text.sz, buffersize, (char)ch, precision, capexp);*/
                            cfltcvt(tmp, text.sz, buffersize, (char)ch, precision, capexp);
                        }


                        /* check if result was negative, save '-' for later
                        * and point to positive part (this is for '0' padding) 
                        */
                        if (*text.sz == '-')
                        {
                            flags |= FL_NEGATIVE;
                            ++text.sz;
                        }

                        textlen = (int)strlen(text.sz);     /* compute length of text */
                    }
                    break;

                    case _T('d'):
                    case _T('i'):
                        /* signed decimal output */
                        flags |= FL_SIGNED;
                        radix = 10;
                        goto COMMON_INT; /*lint !e801*/

                    case _T('u'):
                        radix = 10;
                        goto COMMON_INT;  /*lint !e801*/

                    case _T('p'):
                        /* write a pointer -- this is like an integer or long
                        * except we force precision to pad with zeros and
                        * output in big hex. 
                        */

                        precision = 2 * sizeof(void*);      /* number of hex digits needed */
#ifdef SECUREC_ON_64BITS
                        flags |= FL_I64;                    /* assume we're converting an int64 */
#else
                        flags |= FL_LONG;                   /* assume we're converting a long */
#endif
                        /* DROP THROUGH to hex formatting */
                    /*lint -fallthrough */
                    case _T('X'):
                        /* unsigned upper hex output */
                        hexadd = (_T('A') - _T('9')) - 1;     /* set hexadd for uppercase hex */
                        goto COMMON_HEX;  /*lint !e801*/

                    case _T('x'):
                        /* unsigned lower hex output */
                        hexadd = (_T('a') - _T('9')) - 1;     /* set hexadd for lowercase hex */
                        /* DROP THROUGH TO COMMON_HEX */

                    COMMON_HEX:
                        radix = 16;
                        if (flags & FL_ALTERNATE)
                        {
                            /* alternate form means '0x' prefix */
                            prefix[0] = _T('0');
                            prefix[1] = (TCHAR)((_T('x') - _T('a')) + _T('9') + 1 + hexadd);  /* 'x' or 'X' */
                            prefixlen = 2;
                        }
                        goto COMMON_INT;  /*lint !e801*/

                    case _T('o'):
                        /* unsigned octal output */
                        radix = 8;
                        if (flags & FL_ALTERNATE)
                        {
                            /* alternate form means force a leading 0 */
                            flags |= FL_FORCEOCTAL;
                        }
                        /* DROP THROUGH to COMMON_INT */

                    COMMON_INT:
                        {
                            /* This is the general integer formatting routine.
                            * Basically, we get an argument, make it positive
                            * if necessary, and convert it according to the
                            * correct radix, setting text and textlen
                            * appropriately.
                            */

                            UINT64T number;    /* number to convert */
                            int digit;              /* ascii value of digit */
                            INT64T l;              /* temp long value */

                            /* 1. read argument into l, sign extend as needed */
                            if (flags & FL_I64)
                            {
                                l = va_arg(argptr, INT64T); /*lint !e826*/
                            }
                            else if (flags & FL_LONGLONG)
                            {
                                l = va_arg(argptr, INT64T); /*lint !e826*/
                            }

                            else

#ifdef SECUREC_ON_64BITS
                                if (flags & FL_LONG)
                                {
                                    l = va_arg(argptr, long);    /* LSD util_get_long_arg(&argptr);*/
                                }
                                else
#endif  /* SECUREC_ON_64BITS */

                                    if (flags & FL_SHORT)
                                    {
                                        if (flags & FL_SIGNED)
                                        {
                                            l = (short) va_arg(argptr, int); /* sign extend */ /*lint !e826*/
                                        }
                                        else
                                        {
                                            l = (unsigned short) va_arg(argptr, int); /* zero-extend*/ /*lint !e826*/
                                        }

                                    }
                                    else
                                    {
                                        if (flags & FL_SIGNED)
                                        {
                                            l = va_arg(argptr, int);  /*lint !e826*/
                                        }
                                        else
                                        {
                                            l = (unsigned int) va_arg(argptr, int);  /* zero-extend*/ /*lint !e826*/
                                        }

                                    }

                            /* 2. check for negative; copy into number */
                            if ( (flags & FL_SIGNED) && l < 0)
                            {
                                number = -l;  /*lint !e732*/
                                flags |= FL_NEGATIVE;   /* remember negative sign */
                            }
                            else
                            {
                                number = l; /*lint !e732*/
                            }

                            if ( (flags & FL_I64) == 0 && (flags & FL_LONGLONG) == 0 )
                            {
                                /*
                                 * Unless printing a full 64-bit value, insure values
                                 * here are not in cananical longword format to prevent
                                 * the sign extended upper 32-bits from being printed.
                                 */
                                number &= 0xffffffff;
                            }

                            /* 3. check precision value for default; non-default */
                            /*    turns off 0 flag, according to ANSI. */
                            if (precision < 0)
                            {
                                precision = 1;  /* default precision */
                            }
                            else
                            {
                                flags &= ~FL_LEADZERO;
                                if (precision > MAXPRECISION)
                                {
                                    precision = MAXPRECISION;
                                }
                            }

                            /* 4. Check if data is 0; if so, turn off hex prefix */
                            if (number == 0)
                            {
                                prefixlen = 0;
                            }

                            /* 5. Convert data to ASCII -- note if precision is zero
                            * and number is zero, we get no digits at all. 
                            */

                            text.sz = &buffer.sz[BUFFERSIZE - 1];  /* last digit at end of buffer */

                            while (precision-- > 0 || number != 0)
                            {
                                digit = (int)(number % radix) + '0';
                                number /= radix;                /* reduce number */
                                if (digit > '9')
                                {
                                    /* a hex digit, make it a letter */
                                    digit += hexadd;
                                }
                                *text.sz-- = (char)digit;       /* store the digit */
                            }

                            textlen = (int)((char*)&buffer.sz[BUFFERSIZE - 1] - text.sz); /* compute length of number */
                            ++text.sz;          /* text points to first digit now */


                            /* 6. Force a leading zero if FORCEOCTAL flag set */
                            if ((flags & FL_FORCEOCTAL) && (textlen == 0 || text.sz[0] != '0'))
                            {
                                *--text.sz = '0';
                                ++textlen;      /* add a zero */
                            }
                        }
                        break;
                        default:
                        break;
                }

                /* At this point, we have done the specific conversion, and
                * 'text' points to text to print; 'textlen' is length.  Now we
                * justify it, put on prefixes, leading zeros, and then
                * print it. 
                */

                if (!no_output)
                {
                    int padding;    /* amount of padding, negative means zero */

                    if (flags & FL_SIGNED)
                    {
                        if (flags & FL_NEGATIVE)
                        {
                            /* prefix is a '-' */
                            prefix[0] = _T('-');
                            prefixlen = 1;
                        }
                        else if (flags & FL_SIGN)
                        {
                            /* prefix is '+' */
                            prefix[0] = _T('+');
                            prefixlen = 1;
                        }
                        else if (flags & FL_SIGNSP)
                        {
                            /* prefix is ' ' */
                            prefix[0] = _T(' ');
                            prefixlen = 1;
                        }
                    }

                    /* calculate amount of padding -- might be negative, */
                    /* but this will just mean zero */
                    padding = (fldwidth - textlen) - prefixlen;

                    /* put out the padding, prefix, and text, in the correct order */

                    if (!(flags & (FL_LEFT | FL_LEADZERO)))
                    {
                        /* pad on left with blanks */
                        write_multi_char(_T(' '), padding, stream, &charsout);
                    }

                    /* write prefix */
                    write_string(prefix, prefixlen, stream, &charsout);

                    if ((flags & FL_LEADZERO) && !(flags & FL_LEFT))
                    {
                        /* write leading zeros */
                        write_multi_char(_T('0'), padding, stream, &charsout);
                    }

                    /* write text */
#ifndef _XXXUNICODE
                    if (bufferiswide && (textlen > 0))
                    {
                        wchar_t* p;
                        int retval;
                        int count;
                        /* int e = 0;*/
                        char L_buffer[MB_LEN_MAX + 1];

                        p = text.wz;
                        count = textlen;
                        while (count--)
                        {
                            /*
                            e = wctomb_s(&retval, L_buffer, sizeof(L_buffer)/sizeof(L_buffer[0]), *p++);
                            if (e != 0 || retval == 0)*/
                            retval = wctomb(L_buffer, *p++);
                            if (retval <= 0)
                            {
                                charsout = -1;
                                break;
                            }
                            write_string(L_buffer, retval, stream, &charsout);
                        }
                    }
                    else
                    {
                        write_string(text.sz, textlen, stream, &charsout);
                    }
#else  /* _XXXUNICODE */
                    if (!bufferiswide && textlen > 0)
                    {
                        char* p;
                        int retval, count;

                        p = text.sz;
                        count = textlen;
                        while (count-- > 0)
                        {
                            retval = mbtowc(&wchar, p, (size_t)MB_CUR_MAX);
                            if (retval <= 0)
                            {
                                charsout = -1;
                                break;
                            }
                            write_char(wchar, stream, &charsout);
                            p += retval;
                        }
                    }
                    else
                    {
                        write_string(text.wz, textlen, stream, &charsout);
                    }
#endif  /* _XXXUNICODE */

                    if (charsout >= 0 && (flags & FL_LEFT))
                    {
                        /* pad on right with blanks */
                        write_multi_char(_T(' '), padding, stream, &charsout);
                    }

                    /* we're done! */
                }
                if (heapbuf != NULL)
                {
                    free(heapbuf);
                    heapbuf = NULL;
                }
                break;
        }
    }

    /* The format string shouldn't be incomplete - i.e. when we are finished
    * with the format string, the last thing we should have encountered
    * should have been a regular char to be output or a type specifier. Else
    * the format string was incomplete
    */
    if (state != ST_NORMAL && state != ST_TYPE)
    {
        return -1;
    }

    return charsout;        /* return value = number of characters written */
}
#endif /*OUTPUT_INL_2B263E9C_43D8_44BB_B17A_6D2033DECEE5*/


