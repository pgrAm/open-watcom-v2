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


#include <string.h>
#include "uidef.h"
#include "uibox.h"
#include "uigchar.h"

#include "clibext.h"


static void update( SAREA area, VSCREEN *vptr )
/*********************************************/
{
    register    int                     row;
    register    int                     vrow;
    register    int                     vcol;

    for( row = area.row; row < area.row + area.height; ++row ) {
        vrow = row - (int)vptr->area.row;
        vcol = (int)area.col - (int)vptr->area.col;
        uibcopy( &(vptr->window.type.buffer), vrow, vcol, &UIData->screen, row, area.col, area.width );
    }
}


VSCREEN* UIAPI uivopen( VSCREEN *vptr )
/*************************************/
{
    unsigned char           *box;
    ATTR                    attr;
    int                     priority;
    void                    (_FAR *updatertn)( struct sarea, void * );
    bool                    okbuffer;
    int                     len;
    ORD                     col;
    unsigned int            flags;
    bool                    covered;
    SAREA                   area;

    okarea( vptr->area );
    flags = vptr->flags;
    area = vptr->area;
    if( ( flags & V_DIALOGUE ) != 0 ) {
        if( flags & V_LISTBOX ) {
            box = SBOX_CHARS();
            attr = UIData->attrs[ATTR_NORMAL];
        } else {
            box = BOX_CHARS();
            attr = UIData->attrs[ATTR_DIAL_FRAME];
        }
        priority = P_DIALOGUE;
    } else {
        flags &= ~V_UNBUFFERED;
        box = SBOX_CHARS();
        attr = UIData->attrs[ATTR_FRAME];
        priority = P_VSCREEN;
    }
    if( ( flags & V_UNFRAMED ) == 0 ) {
        (area.row)--;
        (area.col)--;
        (area.height) += 2;
        (area.width) += 2;
        okarea( area );
    }
    if( ( flags & V_UNBUFFERED ) != 0 ) {
        priority = P_UNBUFFERED;
        bfake( &(vptr->window.type.buffer), area.row, area.col );
        okbuffer = true;
        updatertn = NULL;
    } else {
        okbuffer = balloc( &(vptr->window.type.buffer), area.height, area.width );
        updatertn = (void(*)(struct sarea,void *))update;
    }
    if( okbuffer ) {
        vptr->window.area = area;
        vptr->window.priority = priority;
        vptr->window.update = updatertn;
        vptr->window.parm = vptr;
        covered = openwindow( &(vptr->window) );
        vptr->flags = flags;
        if( ( flags & V_UNFRAMED ) == 0 ) {
            if( !UIData->no_blowup && !covered && ( (flags & V_NO_ZOOM) == 0 ) ) {
                blowup( &UIData->screen, area, box, attr );
            }
            area.row = 0;
            area.col = 0;
            drawbox( &(vptr->window.type.buffer), area, box, attr, false );
            if( vptr->title != NULL ) {
#if 0
do not delete this stuff
                col = 0;
                len = area.width;
                bstring( &(vptr->window.type.buffer), 0, col,
                         UIData->attrs[ATTR_CURR_SELECT_DIAL], " ", len );
                len = strlen( vptr->name );
                if( len > area.width )
                    len = area.width;
                col = ( area.width - len ) / 2;
                bstring( &(vptr->window.type.buffer), 0, col,
                         UIData->attrs[ATTR_CURR_SELECT_DIAL], vptr->name, len );
#else
                len = strlen( vptr->title );
                if( len > area.width )
                    len = area.width;
                col = ( area.width - len ) / 2;
                bstring( &(vptr->window.type.buffer), 0, col, attr, vptr->title, len );
#endif
            }
            bframe( &(vptr->window.type.buffer ) );
        }
        area = vptr->area;
        area.row = 0;
        area.col = 0;
        vptr->open = true;
        uivfill( vptr, area, UIData->attrs[ATTR_NORMAL], ' ' );
        uivsetcursor( vptr );

        return( vptr );
    }
    return( NULL );
}


void UIAPI uivclose( VSCREEN *vptr )
/***********************************/
{
    if( vptr->open ) {
        closewindow( &(vptr->window) );
        if( ( vptr->flags & V_UNBUFFERED ) == 0 ) {
            if( ( vptr->flags & V_UNFRAMED ) == 0 ) {
                bunframe( &(vptr->window.type.buffer) );
            }
            bfree( &(vptr->window.type.buffer) );
        }
        vptr->open = false;
    }
}

/*
 * uivresize -- change vptr to have new SAREA, coping all screen data that
 *              persists instead of losing the data
 */

VSCREEN * UIAPI uivresize( VSCREEN *vptr, SAREA new )
/****************************************************/
{
    BUFFER      old_buff;
    int         i;
    UI_WINDOW   *wptr;
    SAREA       old;
    int         min_width;
    int         min_height;

    wptr = &(vptr->window);
    if( vptr->open ) {
        closewindow( wptr );
    }
    memcpy( &old_buff, &(wptr->type.buffer), sizeof( BUFFER ) );
    old = vptr->area;
    if( balloc( &(wptr->type.buffer), new.height, new.width ) ) {
        vptr->area = new;
        if( (vptr->flags & V_UNFRAMED) == 0 ) {
            (new.row)--;
            (new.col)--;
            (new.height) += 2;
            (new.width) += 2;
            okarea( new );
        }
        wptr->area = new;
        okarea( vptr->area );
        min_width = new.width;
        if( min_width > old.width )
            min_width = old.width;
        min_height = new.height;
        if( min_height > old.height )
            min_height = old.height;
        for( i = 0; i < min_height; i++ ) {
            uibcopy( &(old_buff), i, 0, &(wptr->type.buffer), i, 0, min_width );
        }
        bfree( &old_buff );
        openwindow( wptr );
        return( vptr );
    } else {
        memcpy( &(vptr->window.type.buffer), &old_buff, sizeof( BUFFER ) );
        vptr->area = old;
        wptr->area = old;
        openwindow( wptr );
        return( NULL );
    }
}

void UIAPI uivmove( VSCREEN *vptr, ORD row, ORD col )
/****************************************************/
{
    int         rdiff;
    int         cdiff;
    UI_WINDOW   *wptr;

    rdiff = (int)row - (int)vptr->area.row;
    cdiff = (int)col - (int)vptr->area.col;
    vptr->area.row = row;
    vptr->area.col = col;
    okarea( vptr->area );
    wptr = &(vptr->window);
    movewindow( wptr, rdiff + wptr->area.row, cdiff + wptr->area.col );
}
