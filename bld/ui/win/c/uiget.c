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
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#include <dos.h>
#include "biosui.h"
#include "uidef.h"
#include "uiforce.h"


MOUSETIME UIAPI uiclock( void )
/*****************************
 * this routine get time in platform dependant units,
 * used for mouse & timer delays
 */
{
    return( *(unsigned long __far *)FIRSTMEG( BIOS_PAGE, BIOS_SYSTEM_CLOCK ) );
}

unsigned UIAPI uiclockdelay( unsigned milli )
/*******************************************
 * this routine converts milli-seconds into platform
 * dependant units - used to set mouse & timer delays
 */
{
    return( ( milli * 18L ) / 1000L );
}

void UIAPI uiflush( void )
/*************************/
{
    uiflushevent();
    flushkey();
}

ui_event UIAPI uieventsource( bool update )
/*****************************************/
{
    static int      ReturnIdle = 1;
    ui_event        ui_ev;
    MOUSETIME       start;

    start = uiclock();
    for( ;; ) {
        ui_ev = forcedevent();
        if( ui_ev > EV_NO_EVENT )
            break;
        ui_ev = mouseevent();
        if( ui_ev > EV_NO_EVENT )
            break;
        ui_ev = keyboardevent();
        if( ui_ev > EV_NO_EVENT ) {
            uihidemouse();
            break;
        }
        if( ReturnIdle ) {
            ReturnIdle--;
            return( EV_IDLE );
        } else {
            if( update )
                uirefresh();
            if( uiclock() - start >= UIData->tick_delay ) {
                return( EV_CLOCK_TICK );
            } else if( UIData->busy_wait ) {
                return( EV_SINK );
            }
        }
    }
    ReturnIdle = 1;
    return( ui_ev );
}

ui_event UIAPI uiget( void )
/**************************/
{
    return( uieventsource( true ) );
}
