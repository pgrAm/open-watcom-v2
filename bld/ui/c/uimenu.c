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


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "uidef.h"
#include "uimenu.h"
#include "uibox.h"
#include "uishift.h"
#include "uigchar.h"

#include "clibext.h"


#define TABCHAR                 '\t'
#define TITLE_OFFSET            2
#define BETWEEN_TITLES          2

static  int                     BetweenTitles = BETWEEN_TITLES;

extern  ui_event                Event;

static  VBARMENU                MenuList;
static  VBARMENU*               Menu;

static  DESCMENU                Describe[MAX_MENUS];
static  int                     NumMenus = 0;

static  UI_WINDOW               BarWin;

static ui_event    menu_list[] = {
    EV_FIRST_EDIT_CHAR, EV_LAST_EDIT_CHAR,
    EV_ALT_Q,           EV_ALT_M,
    EV_SCROLL_PRESS,    EV_CAPS_RELEASE,
    __rend__,
    EV_MOUSE_PRESS,
    EV_MOUSE_DRAG,
    EV_MOUSE_RELEASE,
    EV_ESCAPE,
    EV_ENTER,
    EV_CURSOR_LEFT,
    EV_CURSOR_RIGHT,
    EV_CURSOR_DOWN,
    EV_ALT_PRESS,
    EV_ALT_RELEASE,
    EV_F10,
    __end__
};

static char     *alt = "qwertyuiop\0\0\0\0asdfghjkl\0\0\0\0\0zxcvbnm";

static bool     InitMenuPopupPending = false;

extern void uisetbetweentitles( int between )
{
    BetweenTitles = between;
}

extern char uialtchar( ui_event ui_ev )
/*************************************/
{
    if( ( ui_ev >= EV_ALT_Q ) && ( ui_ev <= EV_ALT_M ) ) {
        return( alt[ui_ev - EV_ALT_Q] );
    } else {
        return( '\0' );
    }
}


static void mstring( BUFFER *bptr, ORD row, ORD col, ATTR attr,
                             LPC_STRING string, int len )
/**************************************************************/
{
    SAREA       area;

    bstring( bptr, row, col, attr, string, len );
    area.row = row;
    area.col = col;
    area.height = 1;
    area.width = len;
    physupdate( &area );
}

static void mfill( BUFFER *bptr, ORD row, ORD col, ATTR attr, unsigned char ch, int len, int height )
/***************************************************************************************************/
{
    SAREA       area;

    area.row = row;
    area.col = col;
    area.width = len;
    area.height = height;
    while( height != 0 ) {
        bfill( bptr, row, col, attr, ch, len );
        ++row;
        --height;
    }
    physupdate( &area );
}

static void menutitle( int menu, bool current )
/*********************************************/
{
    register    DESCMENU                *desc;
    register    UIMENUITEM              *mptr;
    register    ATTR                    attr;
    register    ATTR                    chattr;

    desc = &Describe[menu - 1];
    mptr = &Menu->titles[menu - 1];
    if( MENUGRAYED( *mptr ) ) {
        if( current ) {
            attr = UIData->attrs[ATTR_CURR_INACTIVE];
        } else {
            attr = UIData->attrs[ATTR_INACTIVE];
        }
        chattr = attr;
    } else {
        if( Menu->active ) {
            if( current ) {
                attr = UIData->attrs[ATTR_CURR_ACTIVE];
                chattr = UIData->attrs[ATTR_HOT_CURR];
            } else {
                attr = UIData->attrs[ATTR_ACTIVE];
                chattr = UIData->attrs[ATTR_HOT];
            }
        } else {
            attr = UIData->attrs[ATTR_ACTIVE];
            chattr = UIData->attrs[ATTR_HOT_QUIET];
        }
    }
    mstring( &UIData->screen, MENU_GET_ROW( desc ), desc->titlecol + TITLE_OFFSET,
             attr, mptr->name, desc->titlewidth );
    mstring( &UIData->screen, MENU_GET_ROW( desc ),
             desc->titlecol + TITLE_OFFSET + CHAROFFSET( *mptr ),
             chattr, &mptr->name[CHAROFFSET( *mptr )], 1 );
}

void UIAPI uidisplayitem( UIMENUITEM *menu, DESCMENU *desc, int item, bool curr )
/*******************************************************************************/
{
    bool                    active;
    ORD                     choffset;
    int                     len;
    char                    ch;
    char*                   tab_loc;
    int                     tab_len;
    ORD                     start_col;
    char*                   str;
    ATTR                    attr;
    ATTR                    chattr;
    int                     str_len;

    active = !MENUGRAYED( *menu ) && uiinlists( menu->event );
    if( active ) {
        if( curr ) {
            attr = UIData->attrs[ATTR_CURR_ACTIVE];
            chattr = UIData->attrs[ATTR_HOT_CURR];
        } else {
            attr = UIData->attrs[ATTR_ACTIVE];
            chattr = UIData->attrs[ATTR_HOT];
        }
    } else {
        if( curr ) {
            attr = UIData->attrs[ATTR_CURR_INACTIVE];
        } else {
            attr = UIData->attrs[ATTR_INACTIVE];
        }
        chattr = attr;
    }
    if( item > 0 ) {
        len = desc->area.width - 2;
        str = menu->name;
        if( MENUSEPARATOR( *menu ) ) {
            ch = BOX_CHAR( SBOX_CHARS(), LEFT_TACK );
            mstring( &UIData->screen,
                    (ORD) desc->area.row + item,
                    (ORD) desc->area.col,
                     UIData->attrs[ATTR_MENU], &ch, 1 );
            mfill( &UIData->screen,
                    (ORD) desc->area.row + item,
                    (ORD) desc->area.col + 1,
                    UIData->attrs[ATTR_MENU],
                    BOX_CHAR( SBOX_CHARS(), HORIZ_LINE ),
                    len, 1 );
            ch = BOX_CHAR( SBOX_CHARS(), RIGHT_TACK );
            mstring( &UIData->screen,
                    (ORD) desc->area.row + item,
                    (ORD) desc->area.col + len + 1,
                    UIData->attrs[ATTR_MENU], &ch, 1 );
        } else {
            if( len < 0 ) {
                len = 0;
            }
            choffset = CHAROFFSET( *menu );
            mfill( &UIData->screen,                     /* blank line */
                    (ORD) desc->area.row + item,
                    (ORD) desc->area.col + 1,
                    attr, ' ', len, 1 );
            if( desc->flags & MENU_HAS_CHECK ) {
                start_col = desc->area.col + 1;
                len--;
            } else {
                start_col = desc->area.col;
            }
            if( menu->flags & ITEM_CHECKED ) {
                mfill( &UIData->screen,                 /* checkmark */
                       (ORD) desc->area.row + item,
                       (ORD) start_col,
                       attr, UiGChar[UI_CHECK_MARK], 1, 1 );
            }
            if( menu->popup != NULL ) {
                mfill( &UIData->screen,                 /* > for popup */
                       (ORD) desc->area.row + item,
                       (ORD) start_col + len,
                       attr, UiGChar[UI_POPUP_MARK], 1, 1 );
            }
            if( desc->flags & MENU_HAS_POPUP ) {
                len--;
            }
            if( str != NULL ) {
                tab_loc = strchr( str, TABCHAR );
                if( tab_loc != NULL ) {
                    tab_loc++;
                    if( tab_loc != NULL ) {
                        tab_len = strlen( tab_loc ) + 1;
                    } else {
                        tab_len = 0;
                    }
                } else {
                    tab_len = 0;
                }
                str_len = strlen( str ) - tab_len;
                if( desc->flags & MENU_HAS_TAB ) {
                    if( str_len > TAB_OFFSET( desc ) ) {
                        str_len = TAB_OFFSET( desc ) - 1;
                    }
                }
                /* text */
                mstring( &UIData->screen, (ORD) desc->area.row + item,
                         (ORD) start_col + 2, attr, str, str_len );
                if( tab_loc != NULL ) {
                    mstring( &UIData->screen,           /* tabbed text */
                             (ORD) desc->area.row + item,
                             (ORD) start_col + TAB_OFFSET( desc ) + 2,
                             attr, tab_loc, tab_len );
                }
                mstring( &UIData->screen,               /* short cut key */
                         (ORD) desc->area.row + item,
                         (ORD) start_col + choffset + 2,
                         chattr, &str[choffset], 1 );
            }
        }
    }
}


extern void uidrawmenu( UIMENUITEM *menu, DESCMENU *desc, int curr )
{
    register    int             item;

    forbid_refresh();
    if( desc->area.height > 0 ) {
        drawbox( &UIData->screen, desc->area, SBOX_CHARS(), UIData->attrs[ATTR_MENU], false );
        for( item = 1 ; item < desc->area.height - 1 ; ++item ) {
            uidisplayitem( &menu[item - 1], desc, item, item == curr );
        }
    }
    permit_refresh();
}

void UIAPI uiclosepopup( UI_WINDOW *window )
{
    closewindow( window );
    window->update = NULL;
}

void UIAPI uiopenpopup( DESCMENU *desc, UI_WINDOW *window )
{
    window->area = desc->area;
    window->priority = P_DIALOGUE;
    window->update = NULL;
    window->parm = NULL;
    openwindow( window );
}

static int process_char( int ch, DESCMENU **desc, int *menu, bool *select )
{
    register    int                     index;
    register    UIMENUITEM              *itemptr;
    register    int                     handled;
    register    int                     hotchar;

    ch = tolower( ch );
    handled = false;
    itemptr = Menu->titles;
    for( index = 0 ; !MENUENDMARKER( itemptr[index] ); ++index ) {
        if( !MENUSEPARATOR( itemptr[index] ) && !MENUGRAYED( itemptr[index] ) ) {
            hotchar = itemptr[index].name[CHAROFFSET( itemptr[index] )];
            if( tolower( hotchar ) == ch ) {
                *desc = &Describe[index];
                *menu = index + 1;
                *select = ( (*desc)->area.height == 0 );
                Menu->popuppending = true;
                handled = true;
                break;
            }
        }
    }
    return( handled );
}

static ui_event createpopup( DESCMENU *desc, ui_event *new_ui_ev )
{
    ui_event    ui_ev;
    UIMENUITEM  *curr_menu;
    SAREA       keep_inside;
    SAREA       return_exclude;

    ui_ev = EV_NO_EVENT;
    if( MENUGRAYED( Menu->titles[Menu->menu - 1] ) ) {
        curr_menu = NULL;
    } else {
        curr_menu = Menu->titles[Menu->menu - 1].popup;
    }
    if( curr_menu != NULL ) {
        keep_inside.row = 0;
        keep_inside.col = 0;
        keep_inside.width = UIData->width;
        keep_inside.height = UIData->height;

        return_exclude.row = 0;
        return_exclude.col = desc->titlecol;
        return_exclude.width = desc->titlewidth + 2;
        return_exclude.height = 1;

        uimenudisable( true );

        *new_ui_ev = uicreatesubpopupinarea( curr_menu, desc, true, false,
                                            curr_menu[0].event, &keep_inside,
                                            &BarWin.area, &return_exclude );
        uimenudisable( false );

        switch( *new_ui_ev ) {
        case EV_CURSOR_RIGHT :
        case EV_CURSOR_LEFT :
        case EV_ALT_PRESS :
        case EV_ESCAPE :
        case EV_MOUSE_DRAG :
        case EV_MOUSE_DRAG_R :
        case EV_MOUSE_PRESS:
        case EV_MOUSE_PRESS_R:
        case EV_MOUSE_RELEASE:
        case EV_MOUSE_RELEASE_R:
              break;
        default :
            ui_ev = *new_ui_ev;
        }
    }
    return( ui_ev );
}


static ui_event intern process_menuevent( VSCREEN *vptr, ui_event ui_ev )
/***********************************************************************/
{
    int         index;
    int         oldmenu = 0;
    ui_event    itemevent;
    ui_event    new_ui_ev;
    DESCMENU    *desc;
    int         menu;
    bool        select;
    ORD         mouserow;
    ORD         mousecol;
    bool        mouseon;

    new_ui_ev = ui_ev;
    if( iskeyboardchar( ui_ev ) ) {
        /* this allows alt numeric keypad stuff to not activate the menus */
        Menu->altpressed = false;
    }
    if( !isdialogue( vptr ) ) {
        if( NumMenus > 0 ) {
            desc = &Describe[Menu->menu - 1];
            new_ui_ev = EV_NO_EVENT; /* Moved here from "else" case below */
            if( Menu->popuppending ) {
                Menu->popuppending = false;
                itemevent = createpopup( desc, &ui_ev );
            } else {
                itemevent = EV_NO_EVENT;
            }
            if( Menu->active ) {
                oldmenu = menu = Menu->menu;
            } else {
                oldmenu = menu = 0;
            }
            select = false;
            if( ui_ev == EV_ALT_PRESS && !Menu->ignorealt ) {
                Menu->altpressed = true;
            } else if( ui_ev == EV_ALT_RELEASE && Menu->altpressed ) {
                if( Menu->active ) {
                    menu = 0;
                } else {
                    desc = &Describe[0];
                    menu = 1;
                }
                Menu->altpressed = false;
            } else if( ui_ev == EV_F10 && UIData->f10menus ) {
                desc = &Describe[0];
                menu = 1;
            } else if( ui_ev == EV_MOUSE_PRESS_R || ui_ev == EV_MOUSE_PRESS_M  ) {
                new_ui_ev = ui_ev;
                menu = 0;
                Menu->draginmenu = false;
            } else if( ( ui_ev == EV_MOUSE_PRESS ) ||
                ( ui_ev == EV_MOUSE_DRAG ) ||
                ( ui_ev == EV_MOUSE_REPEAT ) ||
                ( ui_ev == EV_MOUSE_RELEASE ) ||
                ( ui_ev == EV_MOUSE_DCLICK ) ) {
                uigetmouse( &mouserow, &mousecol, &mouseon );
                if( ( mouserow < uimenuheight() ) &&
                    ( Menu->active  ||
                      ui_ev == EV_MOUSE_PRESS  || ui_ev == EV_MOUSE_DCLICK  ||
                      ui_ev == EV_MOUSE_DRAG || ui_ev == EV_MOUSE_REPEAT ) ) {
                    if( ui_ev == EV_MOUSE_DCLICK ) {
                        ui_ev = EV_MOUSE_PRESS;
                    }
                    menu = 0;
                    for( index = 0 ; !MENUENDMARKER( Menu->titles[index] ); ++index ) {
                        desc = &Describe[index];
                        if( ( MENU_GET_ROW( desc ) == mouserow ) &&
                            ( desc->titlecol <= mousecol ) &&
                            ( mousecol < desc->titlecol + desc->titlewidth + 2 ) ) {
                            Menu->draginmenu = true;
                            Menu->popuppending = true;
                            menu = index + 1;
                            break;
                        }
                    }
                } else if( Menu->active || Menu->draginmenu ) {
                    if( ( desc->area.col < mousecol )
                        && ( mousecol < desc->area.col + desc->area.width - 1 )
                        && ( mouserow < desc->area.row + desc->area.height - 1 )
                        && ( desc->area.row <= mouserow ) ) {
                        Menu->movedmenu = true;
                    } else if( ui_ev == EV_MOUSE_PRESS  ) {
                        new_ui_ev = ui_ev;
                        menu = 0;
                        Menu->draginmenu = false;
                    } else if( ui_ev == EV_MOUSE_RELEASE ) {
                        menu = 0;
                        Menu->draginmenu = false;
                    }
                } else {
                    new_ui_ev = ui_ev;
                }
                if( ui_ev != EV_MOUSE_RELEASE && menu != oldmenu ) {
                    Menu->movedmenu = true;
                }
                if( ui_ev == EV_MOUSE_RELEASE ) {
                    if( !Menu->movedmenu ) {
                        menu = 0;
                    } else {
                        select = true;
                    }
                    Menu->movedmenu = false;
                }
            } else if( uialtchar( ui_ev ) != '\0'  ) {
                process_char( uialtchar( ui_ev ), &desc, &menu, &select );
                new_ui_ev = EV_NO_EVENT;
            } else if( Menu->active ) {
                switch( ui_ev ) {
                case EV_ESCAPE :
                    menu = 0;
                    break;
                case EV_ENTER :
                    if( menu > 0 ) {
                        Menu->popuppending = true;
                    }
                    break;
                case EV_CURSOR_LEFT :
                    menu -= 1;
                    if( menu == 0 ) {
                        menu = NumMenus;
                    }
                    Menu->popuppending = true;
                    desc = &Describe[menu - 1];
                    break;
                case EV_CURSOR_RIGHT :
                    menu += 1;
                    if( menu > NumMenus ) {
                        menu = 1;
                    }
                    Menu->popuppending = true;
                    desc = &Describe[menu - 1];
                    break;
                case EV_CURSOR_DOWN :
                    Menu->popuppending = true;
                    break;
                case EV_NO_EVENT :
                    break;
                default :
                    if( iskeyboardchar( ui_ev ) ) {
                        if( process_char( ui_ev, &desc, &menu, &select ) ) {
                            break;
                        }
                    }
                    if( itemevent != EV_NO_EVENT ) {
                        new_ui_ev = itemevent;
                        select = true;
                    } else {
                        new_ui_ev = ui_ev;
                    }
                }
            } else {
                new_ui_ev = ui_ev;
            }
            if( menu != oldmenu ) {
                if( menu > 0 && !Menu->active ) {
                    new_ui_ev = EV_MENU_ACTIVE;
                }
                Menu->active = ( menu > 0 );
                if( oldmenu > 0 ) {
                    menutitle( oldmenu, false );
                }
                if( menu > 0 ) {
                    Menu->menu = menu;
                    menutitle( menu, true );
                }
                if( menu == 0 || oldmenu == 0 ) {
                    uimenutitlebar();
                }
            }
            if( Menu->active ) {
                if( itemevent == EV_NO_EVENT ) {
                    if( MENUGRAYED( Menu->titles[menu - 1] ) )  {
                        Menu->popuppending = false;
                    } else {
                        itemevent = Menu->titles[menu - 1].event;
                    }
                }
                Menu->event = itemevent;
                if( select ) {
                    new_ui_ev = Menu->event;
                    Menu->active = false;
                    uimenutitlebar();
                }
            }
        }
    }
    if( ui_ev == EV_MOUSE_RELEASE ) {
        Menu->draginmenu = false;
    }
    if( Menu->ignorealt ) {
        Menu->ignorealt = false;
    }
    if( ( !Menu->active && ( oldmenu != 0 ) ) ||
        ( Menu->active && ( oldmenu != Menu->menu ) ) ) {
        if( ( Menu->menu > 0 ) && Menu->active ) {
            uimenucurr( &Menu->titles[Menu->menu - 1] );
        } else {
            /* no current menu */
            uimenucurr( NULL );
        }
    }

    if ( Menu->popuppending ) {
        InitMenuPopupPending = true;
    }

    return( new_ui_ev );
}

#if 0
ui_event uigeteventfrompos( ORD row, ORD col )
/********************************************/
{
    unsigned            index;
    DESCMENU*           desc;

    if( row < uimenuheight() ) {
        for( index = 0 ; !MENUENDMARKER( Menu->titles[index] ); ++index ) {
            desc = &Describe[index];
            if( ( MENU_GET_ROW( desc ) == row ) &&
                ( desc->titlecol <= col ) &&
                ( col < desc->titlecol + desc->titlewidth + 2 ) ) {
                return( Menu->event );
            }
        }
    }
    return( EV_NO_EVENT );
}
#endif

ui_event intern menuevent( VSCREEN *vptr )
/****************************************/
{
    register ui_event       new_ui_ev;
    register ui_event       ui_ev;

    new_ui_ev = EV_NO_EVENT;

    if ( InitMenuPopupPending ) {
        InitMenuPopupPending = false;
        if( Menu->titles[Menu->menu - 1].popup != NULL ) {
            new_ui_ev = EV_MENU_INITPOPUP;
        }
    }

    if( new_ui_ev == EV_NO_EVENT ) {
        if ( uimenuson() && !uimenuisdisabled() ) {
            uipushlist( menu_list );
            if( !Menu->active || isdialogue( vptr ) ) {
                ui_ev = getprime( vptr );
            } else {
                ui_ev = getprime( NULL );
            }
            switch( ui_ev ) {
            case EV_SCROLL_PRESS:
                Menu->scroll = true;
                break;
            case EV_SCROLL_RELEASE:
                Menu->scroll = false;
                break;
            case EV_NUM_PRESS:
                Menu->num = true;
                break;
            case EV_NUM_RELEASE:
                Menu->num = false;
                break;
            case EV_CAPS_PRESS:
                Menu->caps = true;
                break;
            case EV_CAPS_RELEASE:
                Menu->caps = false;
                break;
            default:
                new_ui_ev = process_menuevent( vptr, ui_ev );
            }
            uipoplist( /* menu_list */ );
        } else {
            new_ui_ev = getprime( vptr );
        }
    }

    return( new_ui_ev );
}


void UIAPI uidescmenu( UIMENUITEM *iptr, DESCMENU *desc )
/*******************************************************/
{
    register    int                     item;
    register    int                     len;
    register    char*                   tab_loc;
    register    int                     tab_length;
                int                     to_add;

    desc->flags = 0;
    if( iptr != NULL ) {
        desc->area.width = 0;
        tab_length = 0;
        for( item = 0 ; !MENUENDMARKER( *iptr ) ; ++item ) {
            if( !MENUSEPARATOR( *iptr ) ) {
                len = strlen( iptr->name );
                tab_loc = strchr( iptr->name, TABCHAR );
                if( tab_loc != NULL ) {
                    desc->flags |= MENU_HAS_TAB;
                    tab_loc++;
                    if( tab_loc != NULL ) {
                        if( tab_length < strlen( tab_loc ) )
                            tab_length = strlen( tab_loc );
                        len -= strlen( tab_loc ); /* for text after TABCHAR */
                    }
                    len--;  /* for TABCHAR */
                }
                if( iptr->flags & ITEM_CHECKED ) {
                    desc->flags |= MENU_HAS_CHECK;
                }
                if( iptr->popup != NULL ) {
                    desc->flags |= MENU_HAS_POPUP;
                }
                if( desc->area.width < len ) {
                    desc->area.width = len;
                }
            }
            ++iptr;
        }
        to_add = 0;
        if( desc->flags & MENU_HAS_TAB ) {
            to_add += tab_length + 1;
        }
        if( desc->flags & MENU_HAS_POPUP ) {
            to_add++;
        }
        if( desc->flags & MENU_HAS_CHECK ) {
            to_add++;
        }
        to_add += 4;
        if( desc->area.width > UIData->width - to_add )
            desc->area.width = UIData->width - to_add;
        desc->flags |= ( ( desc->area.width + 1 ) & MENU_TAB_OFFSET );
        desc->area.width += to_add;
        desc->area.height = (ORD) item + 2;
        if( desc->area.col + desc->area.width >= UIData->width ) {
            desc->area.col = UIData->width - desc->area.width;
        }
    } else {
        desc->area.height = 0;
    }
}

static void descmenu( int menu, DESCMENU *desc )
{
    UIMENUITEM          *nptr;
    UIMENUITEM          *iptr;
    unsigned            next;
    #define             MENUSTRLEN(x)   ((x) ? (ORD) strlen((x)) : (ORD) 0)

    --menu;
    iptr = Menu->titles[menu].popup;
    desc->area.row = 1;
    desc->area.col = 0;
    nptr = Menu->titles;
    for( ;; ) {
        next =  (ORD)desc->area.col +  MENUSTRLEN( nptr->name ) + BetweenTitles;
        if( next >= UIData->width ) {
            next -= desc->area.col;
            desc->area.col = 0;
            desc->area.row++;
        }
        if( menu == 0 ) break;
        desc->area.col = next;
        --menu;
        ++nptr;
    }
    desc->titlecol = desc->area.col;
    desc->titlewidth = MENUSTRLEN( nptr->name );
    uidescmenu( iptr, desc );
    /* Have to call this here since uidescmenu initializes field */
    MENU_SET_ROW( desc, desc->area.row - 1 );
}

void uimenutitlebar( void )
{
    register    int                     menu;

    forbid_refresh();
    for( menu = 1; menu <= NumMenus; ++menu ) {
        menutitle( menu, menu == Menu->menu );
    }
    permit_refresh();
}

static void drawbar( SAREA area, void *dummy )
/********************************************/
{
    /* unused parameters */ (void)dummy;

    forbid_refresh();
    if( area.row < uimenuheight() ) {
        mfill( &UIData->screen, area.row, 0,
           UIData->attrs[ATTR_ACTIVE], ' ', UIData->width, area.height );
        uimenutitlebar();
    }
    permit_refresh();
}

bool uienablemenuitem( unsigned menu, unsigned item, bool enable )
{
    bool        prev;
    UIMENUITEM  *pitem;

    pitem = &Menu->titles[menu - 1].popup[item - 1];
    prev = !MENUGRAYED( *pitem );
    if( enable ) {
        pitem->flags &= ~ITEM_GRAYED;
    } else {
        pitem->flags |= ITEM_GRAYED;
    }
    return( prev );
}


void UIAPI uimenuindicators( bool status )
/*****************************************/
{
    Menu->indicators = status;
}

/* this code was split out of uimenubar to facilitate the updating of
 * menu's without constant redrawing
 */
void UIAPI uisetmenudesc( void )
/*******************************/
{
    register int  count;

    count = NumMenus;
    for( ; count > 0 ; --count ) {
        descmenu( count, &Describe[count - 1] );
    }
}

VBARMENU* UIAPI uimenubar( VBARMENU *bar )
/*****************************************/
{
    register    int                     count;
    register    UIMENUITEM              *menus;
    register    VBARMENU                *prevMenu;

    if( NumMenus > 0 ) {
        closewindow( &BarWin );
        NumMenus = 0;
    }
    prevMenu = Menu;
    Menu = bar;
    /* resetting old_shift is a bit kludgy but it's either this or */
    /* rewrite a bunch of code that somebody else wrote - yuk      */
    /* UIData->old_shift = 0;                                      */
    if( Menu != NULL ) {
        Menu->active = false;
        Menu->draginmenu = false;
        Menu->indicators = true;
        Menu->altpressed = false;
        Menu->ignorealt = false;
        Menu->movedmenu = false;
        Menu->popuppending = false;
        Menu->disabled = false;
        count = 0;
        for( menus = Menu->titles; !MENUENDMARKER( *menus ); ++menus ) {
            if( ++count >= MAX_MENUS ) {
                break;
            }
        }
        NumMenus = count;
        uisetmenudesc();
        BarWin.area.row = 0;
        BarWin.area.col = 0;
        BarWin.area.height = uimenuheight();
        BarWin.area.width = UIData->width;
        BarWin.priority = P_MENU;
        BarWin.update = drawbar;
        BarWin.parm = NULL;
        openwindow( &BarWin );
        InitMenuPopupPending = false;
    }
    return( prevMenu );
}

bool UIAPI uimenuson( void )
/***************************/
{
    return( Menu != NULL );
}

unsigned UIAPI uimenuheight( void )
/**********************************/
{
    if( Menu == NULL ) return( 0 );
    return( MENU_GET_ROW( &Describe[NumMenus - 1] ) + 1 );
}

void UIAPI uimenudisable( bool disabled )
/****************************************/
{
    if ( uimenuson() ) {
        Menu->disabled = disabled;
    }
}

bool UIAPI uimenuisdisabled( void )
/**********************************/
{
    return( uimenuson() && Menu->disabled );
}

bool UIAPI uimenugetaltpressed( void )
/*************************************/
{
    return( uimenuson() && Menu->altpressed );
}

void UIAPI uimenusetaltpressed( bool altpressed )
/************************************************/
{
    if( uimenuson() ) {
        Menu->altpressed = altpressed;
    }
}

void UIAPI uinomenus( void )
/***************************/
{
    uimenubar( NULL );
}


void UIAPI uimenus( UIMENUITEM *menus, UIMENUITEM **items, ui_event hot )
/***********************************************************************/
{
    register    int                     index;

    /* unused parameters */ (void)hot;

    uimenubar( NULL );
    MenuList.titles = menus;
    for( index = 0 ; !MENUENDMARKER( menus[index] ); ++index ) {
        menus[index].popup = items[index];
    }
    MenuList.menu = 1;
    uimenubar( &MenuList );
}

void UIAPI uiactivatemenus( void )
/*********************************/
{
    if( Menu != NULL ) {
        if( !Menu->active ) {
            Menu->altpressed = true;
            process_menuevent( NULL, EV_ALT_RELEASE );
        }
    }
}

void UIAPI uiignorealt( void )
/*****************************/
{
    if( Menu != NULL ) {
        Menu->ignorealt = true;
    }
}

int UIAPI uigetcurrentmenu( UIMENUITEM *menu )
{
    if( Menu->menu ) {
        *menu = Menu->titles[Menu->menu - 1];
    }
    return( Menu->menu != 0 );
}

