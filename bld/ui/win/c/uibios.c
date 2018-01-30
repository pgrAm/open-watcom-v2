/****************************************************************************
*
*                            Open Watcom Project
*
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
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#include <dos.h>
#include <conio.h>
#include "uidef.h"
#include "uidos.h"
#include "biosui.h"

static MONITOR ui_data = {
    25,
    80,
    M_VGA,
    NULL,
    NULL,
    NULL,
    NULL,
    4,
    1
};

extern char  _B000h[],_B800h[];
extern unsigned int  VIDPort;
#define VIDMONOINDXREG  0X03B4
#define VIDCOLRINDXREG  0X03D4


void UIAPI win_uisetmono( void )
{
        ui_data.colour = M_MONO;
        ui_data.screen.origin = MK_FP( _B000h, 0 );
        VIDPort = VIDMONOINDXREG;
}

void UIAPI win_uisetcolor( int clr )
{
        ui_data.colour = clr;
        ui_data.screen.origin = MK_FP( _B800h, 0 );
        VIDPort = VIDCOLRINDXREG;
}

static bool initmonitor( void )
/*****************************/
{
    if( UIData == NULL ) {
        UIData = &ui_data;
    }
//    UIData->height = 25;
    return( true );
}


//static void (__far __pascal *HookFunc)(void __far *) = NULL;
extern void __far HookRtn( unsigned , unsigned );

bool intern initbios( void )
/**************************/
{
    bool        initialized;

    initialized = false;
    if( initmonitor() ) {
        UIData->desqview = 0;
        UIData->f10menus = true;
        if( UIData->colour == M_MONO ) {
            UIData->screen.origin = MK_FP( _B000h, 0 );
        } else {
            UIData->screen.origin = MK_FP( _B800h, 0 );
        }

        UIData->screen.increment = UIData->width;
        uiinitcursor();
        initkeyboard();
        UIData->mouse_acc_delay = uiclockdelay( 277 /* ms */ );  /* 5 ticks */
        UIData->mouse_rpt_delay = uiclockdelay( 55  /* ms */ );  /* 1 ticks */
        UIData->mouse_clk_delay = uiclockdelay( 277 /* ms */ );  /* 5 ticks */
        UIData->tick_delay      = uiclockdelay( 500 /* ms */ );  /* 9 ticks */
        initialized = true;
    }
    return( initialized );
}


void intern finibios( void )
/**************************/
{
    finikeyboard();
    uifinicursor();
}


void intern physupdate( SAREA *area )
{
    area = area;
}

