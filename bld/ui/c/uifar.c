/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2018 The Open Watcom Contributors. All Rights Reserved.
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  Routines accessing far memory.
*
****************************************************************************/


#include <string.h>
#ifdef __WATCOMC__
    #if defined( _M_IX86 )
        #include <i86.h>
    #endif
    #include <conio.h>
#endif
#include "uidef.h"
#include "uifar.h"

#if defined( _M_I86 )

extern LP_PIXEL _snowput( LP_PIXEL, PIXEL );
extern PIXEL _snowget( LP_PIXEL );
extern void _snowcopy( LP_PIXEL, LP_PIXEL, int );
extern void _forward( void );
extern void _backward( void );

#pragma aux             _forward = "cld" modify []

#pragma aux             _backward = "std" modify []

#pragma aux             _snowget = \
       0x1e                       /*     push    ds         */  \
       0x8e 0xda                  /*     mov     ds,dx      */  \
       0xba 0xda 0x03             /*     mov     dx,03daH   */  \
       0xec                       /* L3  in      al,dx      */  \
       0xd0 0xc8                  /*       ror     al,1     */  \
       0x72 0xfb                  /*       jb      L3       */  \
       0xfa                       /*     cli                */  \
       0xec                       /* L4  in      al,dx      */  \
       0xd0 0xc8                  /*       ror     al,1     */  \
       0x73 0xfb                  /*       jae     L4       */  \
       0xad                       /*     lodsw              */  \
       0xfb                       /*     sti                */  \
       0x1f                       /*     pop     ds         */  \
       parm caller [dx si]                                      \
       value [ax]                                               \
       modify [ax dx];

#pragma aux             _snowput = \
       0xba 0xda 0x03             /*     mov     dx,03daH   */  \
       0xec                       /* L3  in      al,dx      */  \
       0xd0 0xc8                  /*       ror     al,1     */  \
       0x72 0xfb                  /*       jb      L3       */  \
       0xfa                       /*     cli                */  \
       0xec                       /* L4  in      al,dx      */  \
       0xd0 0xc8                  /*       ror     al,1     */  \
       0x73 0xfb                  /*       jae     L4       */  \
       0x8b 0xc3                  /*       mov     ax,bx    */  \
       0xab                       /*     stosw              */  \
       0xfb                       /*     sti                */  \
       parm caller [es di] [bx]                                 \
       value [es di]                                            \
       modify [ax dx];

#pragma aux             _snowcopy = \
       0x1e                       /*     push    ds         */  \
       0x8e 0xda                  /*     mov     ds,dx      */  \
       0xba 0xda 0x03             /*     mov     dx,03daH   */  \
       0xec                       /* top in      al,dx      */  \
       0xd0 0xc8                  /*       ror     al,1     */  \
       0x72 0xfb                  /*       jb      L3       */  \
       0xfa                       /*     cli                */  \
       0xec                       /*     in      al,dx      */  \
       0xd0 0xc8                  /*       ror     al,1     */  \
       0x73 0xfb                  /*       jae     in       */  \
       0xad                       /*     lodsw              */  \
       0xfb                       /*     sti                */  \
       0x8b 0xd8                  /*     mov     bx,ax      */  \
       0xec                       /*     in      al,dx      */  \
       0xd0 0xc8                  /*       ror     al,1     */  \
       0x72 0xfb                  /*       jb      in       */  \
       0xfa                       /*     cli                */  \
       0xec                       /*     in      al,dx      */  \
       0xd0 0xc8                  /*       ror     al,1     */  \
       0x73 0xfb                  /*       jae     in       */  \
       0x8b 0xc3                  /*     mov     ax,bx      */  \
       0xab                       /*     stosw              */  \
       0xfb                       /*     sti                */  \
       0xe2 0xe0                  /*     loop top           */  \
       0x1f                       /*     pop     ds         */  \
       parm caller [es di] [dx si] [cx]                         \
       modify [ax dx bx cx];

#endif

#define ATTR_FLIP_MASK      0x77

intern void cdecl farfill( LP_PIXEL start, PIXEL fill, size_t len, bool snow )
{
    size_t      i;

#if defined( _M_I86 )
    if( snow ) {
        for( i = 0 ; i < len ; ++i ) {
            start = _snowput( start, fill );
        }
    } else {
        for( i = 0 ; i < len ; ++i ) {
            *start++ = fill;
        }
    }
#else
    /* unused parameters */ (void)snow;

    for( i = 0 ; i < len ; ++i ) {
        *start++ = fill;
    }
#endif
}


intern void cdecl farcopy( LP_PIXEL src, LP_PIXEL dst, size_t len, bool snow )
{
#if defined( _M_I86 )
    if( snow ) {
        if( FP_SEG(src) == FP_SEG(dst) && FP_OFF(src) < FP_OFF(dst) ) {
            src += len - 1;
            dst += len - 1;
            _backward();
        }
        _snowcopy( dst, src, len );
        _forward();
    } else {
        _fmemmove( dst, src, len * sizeof( PIXEL ) );
    }
#elif defined( __NETWARE__ )
    /* unused parameters */ (void)snow;

    // Netware compiled with "far" defined, but pointers aren't really
    // far, and there is no _fmemmove function, and we were getting
    // "pointer truncated" warnings before, so just cast. (SteveM)
    // ?? why it is compiled with "far" ??
    // LP_PIXEL should be declared as near
    // then these cast are useless
    memmove( (PIXEL *)dst, (PIXEL *)src, len * sizeof( PIXEL ) );
#elif defined( __386__ ) && !defined( __UNIX__ )
    /* unused parameters */ (void)snow;

    _fmemmove( dst, src, len * sizeof( PIXEL ) );
#else
    /* unused parameters */ (void)snow;

    memmove( dst, src, len * sizeof( PIXEL ) );
#endif
}


intern void cdecl farstring( LP_PIXEL start, int attr, size_t len,
                                          bool snow, LPC_STRING str )
{
    size_t      i;
    PIXEL       p;

    p.attr = attr;
#if defined( _M_I86 )
    if( snow ) {
        for( i = 0 ; i < len ; ++i ) {
            p.ch = *str;
            if( p.ch == '\0' )
                break;
            ++str;
            start = _snowput( start, p );
        }
    } else
#else
    /* unused parameters */ (void)snow;
#endif
    {
        for( i = 0 ; i < len ; ++i ) {
            if( str != NULL ) {
                p.ch = *str;
            } else {
                p.ch = '\0';
            }
            if( p.ch == '\0' )
                break;
            ++str;
            *start = p;
            ++start;
        }
    }
    p.ch = ' ';
    farfill( start, p, len - i, snow );
}

intern void cdecl farattrib( LP_PIXEL start, int attr, size_t len, bool snow )
{
    size_t      i;
    PIXEL       p;

#if defined( _M_I86 )
    if( snow ) {
        for( i = 0 ; i < len ; ++i ) {
            p = _snowget( start );
            p.attr = attr;
            start = _snowput( start, p );
        }
    } else
#else
    /* unused parameters */ (void)snow;
#endif
    {
        for( i = 0 ; i < len ; ++i ) {
            p = *start;
            p.attr = attr;
            *start = p;
            ++start;
        }
    }
}

intern void cdecl farattrflip( LP_PIXEL start, size_t len, bool snow )
{
    size_t      i;
    PIXEL       p;

#if defined( _M_I86 )
    if( snow ) {
        for( i = 0 ; i < len ; ++i ) {
            p = _snowget( start );
            p.attr = p.attr ^ ATTR_FLIP_MASK;
            start = _snowput( start, p );
        }
    } else
#else
    /* unused parameters */ (void)snow;
#endif
    {
        for( i = 0 ; i < len ; ++i ) {
            p = *start;
            p.attr = p.attr ^ ATTR_FLIP_MASK;
            *start = p;
            ++start;
        }
    }
}
