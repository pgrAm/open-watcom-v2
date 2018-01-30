/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2015-2016 The Open Watcom Contributors. All Rights Reserved.
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


#include "vi.h"
#include "posix.h"
#include "win.h"
#include "menu.h"

extern int      CurrentMenuNumber;

/*
 * SelectFileOpen - select file from specified directory
 */
vi_rc SelectFileOpen( const char *dir, char **result_ptr, const char *mask, bool want_all_dirs )
{
    char                dd[FILENAME_MAX], cdir[FILENAME_MAX];
    int                 j;
    file                *cfile;
    fcb                 *cfcb;
    line                *cline;
    selflinedata        sfd;
    bool                need_entire_path;
    char                *result = *result_ptr;
    vi_rc               rc;

    /*
     * get current directory
     */
    strcpy( dd, dir );
    strcpy( cdir, dir );
    SetCWD( dir );
    need_entire_path = false;

    /*
     * work through all files
     */
    for( ;; ) {

        if( dd[strlen( dd ) - 1] != FILE_SEP ) {
            strcat( dd, FILE_SEP_STR );
        }
        strcat( dd, mask );
        rc = GetSortDir( dd, want_all_dirs );
        if( rc != ERR_NO_ERR ) {
            return( rc );
        }

        /*
         * allocate temporary file structure
         */
        cfile = FileAlloc( NULL );

        FormatDirToFile( cfile, true );

        /*
         * go get selected line
         */
        memset( &sfd, 0, sizeof( sfd ) );
        sfd.f = cfile;
        sfd.wi = &dirw_info;
        sfd.title = CurrentDirectory;
        sfd.show_lineno = true;
        sfd.cln = 1;
        sfd.eiw = NO_WINDOW;
        rc = SelectLineInFile( &sfd );
        if( rc != ERR_NO_ERR ) {
            break;
        }
        if( sfd.sl == -1 ) {
            result[0] = '\0';
            break;
        }
        j = (int) sfd.sl - 1;
        if( j >= DirFileCount || IS_SUBDIR( DirFiles[j] ) ) {
            if( j >= DirFileCount ) {
                GimmeLinePtr( j + 1, cfile, &cfcb, &cline );
                dd[0] = cline->data[3];
                dd[1] = ':';
                dd[2] = '\0';
            } else {
                strcpy( dd, cdir );
                if( dd[strlen(dd) - 1] != FILE_SEP ) {
                    strcat( dd, FILE_SEP_STR );
                }
                strcat( dd, DirFiles[j]->name );
            }
            FreeEntireFile( cfile );
            rc = SetCWD( dd );
            if( rc != ERR_NO_ERR ) {
                return( rc );
            }
            need_entire_path = true;
            strcpy( cdir, CurrentDirectory );
            strcpy( dd, CurrentDirectory );
            continue;
        }
        if( need_entire_path ) {
            strcpy( result, CurrentDirectory );
            if( result[strlen(result) - 1] != FILE_SEP ) {
                strcat( result, FILE_SEP_STR );
            }
        } else {
            result[0] = '\0';
        }
        strcat( result, DirFiles[j]->name );
        break;

    }

    /*
     * done, free memory
     */
    FreeEntireFile( cfile );
    DCDisplayAllLines();
    return( rc );

} /* SelectFileOpen */

static window_id        cwid;
static bool             isMenu;

/*
 * displayGenericLines - display all lines in a window
 */
static vi_rc displayGenericLines( file *f, linenum pagetop, int leftcol,
                                linenum hilite, type_style *style, hilst *hilist,
                                char **vals, int valoff )
{
    int         i, j, k, text_lines;
    linenum     cl = pagetop;
    fcb         *cfcb, *tfcb;
    line        *cline;
    hilst       *ptr;
    type_style  *text_style, *hot_key_style;
    window_info *wi;
    type_style  base_style;
    char        tmp[MAX_STR];
//    bool        disabled;
    vi_rc       rc;

    /*
     * get pointer to first line on page, and window info
     */
    rc = GimmeLinePtr( pagetop, f, &cfcb, &cline );
    if( rc != ERR_NO_ERR ) {
        return( rc );
    }
    base_style.foreground = WindowAuxInfo( cwid, WIND_INFO_TEXT_COLOR );
    base_style.background = WindowAuxInfo( cwid, WIND_INFO_BACKGROUND_COLOR );
    base_style.font = WindowAuxInfo( cwid, WIND_INFO_TEXT_FONT );
    text_lines = WindowAuxInfo( cwid, WIND_INFO_TEXT_LINES );

    /*
     * mark all fcb's as being not in display
     */
    for( tfcb = f->fcbs.head; tfcb != NULL; tfcb = tfcb->next ) {
        tfcb->on_display = false;
    }
    cfcb->on_display = true;

    /*
     * run through each line in the window
     */
    ptr = hilist;
    if( ptr != NULL ) {
        ptr += pagetop - 1;
    }
    for( j = 1; j <= text_lines; j++ ) {
        if( cline != NULL ) {
            if( isMenu ) {
                if( InvokeMenuHook( CurrentMenuNumber, cl ) == -1 ) {
//                    disabled = true;
                    if( cl == hilite ) {
                        wi = &activegreyedmenu_info;
                    } else {
                        wi = &greyedmenu_info;
                    }
                } else {
//                    disabled = false;
                    if( cl == hilite ) {
                        wi = &activemenu_info;
                    } else {
                        wi = &menuw_info;
                    }
                }
                text_style = &wi->text_style;
                hot_key_style = &wi->hilight_style;
            } else {
                text_style = &base_style;
                if( cl == hilite ) {
                    text_style = style;
                }
                hot_key_style = text_style;
            }

            /*
             * now, display what we can of the line on the window
             */
            if( cline->len == 0 ) {
                DisplayCrossLineInWindow( cwid, j );
                goto evil_goto;
            } else if( cline->len > leftcol ) {
                if( vals != NULL ) {
                    i = cline->len - leftcol;
                    strncpy( tmp, &(cline->data[leftcol]), EditVars.WindMaxWidth + 5 );
                    for( k = i; k < valoff; k++ ) {
                        tmp[k] = ' ';
                    }
                    tmp[k] = '\0';
                    strcat( tmp, vals[j + pagetop - 2] );
                    DisplayLineInWindowWithColor( cwid, j, tmp, text_style, 0 );
                } else {
                    DisplayLineInWindowWithColor( cwid, j, cline->data, text_style, leftcol );
                }
            } else {
                DisplayLineInWindowWithColor( cwid, j, SingleBlank, text_style, 0 );
            }
            if( ptr != NULL ) {
                SetCharInWindowWithColor( cwid, j, 1 + ptr->_offs, ptr->_char, hot_key_style );
            }
evil_goto:  if( ptr != NULL ) {
                ptr += 1;
            }
            rc = GimmeNextLinePtr( f, &cfcb, &cline );
            if( rc != ERR_NO_ERR ) {
                if( rc == ERR_NO_MORE_LINES ) {
                    continue;
                }
                return( rc );
            }
            cl++;
            cfcb->on_display = true;
        } else {
            DisplayLineInWindow( cwid, j, "~" );
        }

    }
    return( ERR_NO_ERR );

} /* displayGenericLines */

typedef enum {
    MS_NONE,
    MS_PAGEDOWN,
    MS_PAGEUP,
    MS_DOWN,
    MS_UP,
    MS_EXPOSEDOWN,
    MS_EXPOSEUP
} ms_type;

static window_id        owid, mouse_wid;
static int              mouseLine = -1;
static ms_type          mouseScroll;
static bool             rlMenu;
static int              rlMenuNum;

/*
 * SelectLineMouseHandler - handle mouse events for line selector
 */
static bool SelectLineMouseHandler( window_id wid, int win_x, int win_y )
{
    int x, y, i;

    if( LastMouseEvent != VI_MOUSE_DRAG && LastMouseEvent != VI_MOUSE_PRESS &&
        LastMouseEvent != VI_MOUSE_DCLICK && LastMouseEvent != VI_MOUSE_RELEASE &&
        LastMouseEvent != VI_MOUSE_REPEAT && LastMouseEvent != VI_MOUSE_PRESS_R ) {
        return( false );
    }
    mouse_wid = wid;
    mouseScroll = MS_NONE;

    if( !isMenu && ( wid == cwid ) && (LastMouseEvent == VI_MOUSE_REPEAT ||
                                    LastMouseEvent == VI_MOUSE_PRESS ||
                                    LastMouseEvent == VI_MOUSE_DCLICK ) ) {
        x = WindowAuxInfo( cwid, WIND_INFO_WIDTH );
        y = WindowAuxInfo( cwid, WIND_INFO_HEIGHT );
        if( win_x == x - 1 ) {
            if( win_y == 1 ) {
                mouseScroll = MS_EXPOSEUP;
                return( true );
            } else if( win_y == y - 2 ) {
                mouseScroll = MS_EXPOSEDOWN;
                return( true );
            } else if( win_y > 1 && win_y < y / 2 ) {
                mouseScroll = MS_PAGEUP;
                return( true );
            } else if( win_y >= y / 2 && win_y < y - 1 ) {
                mouseScroll = MS_PAGEDOWN;
                return( true );
            }
        }
    }
    if( LastMouseEvent == VI_MOUSE_REPEAT ) {
        if( wid != cwid && !isMenu ) {
            y = WindowAuxInfo( cwid, WIND_INFO_Y1 );
            if( MouseRow < y ) {
                mouseScroll = MS_UP;
                return( true );
            }
            y = WindowAuxInfo( cwid, WIND_INFO_Y2 );
            if( MouseRow > y ) {
                mouseScroll = MS_DOWN;
                return( true );
            }
        }
        return( false );
    }
    if( isMenu && EditFlags.Menus && wid == menu_window_id &&
        LastMouseEvent != VI_MOUSE_PRESS_R ) {
        i = GetMenuIdFromCoord( win_x );
        if( i >= 0 ) {
            rlMenuNum = i - GetCurrentMenuId();
            if( rlMenuNum != 0 ) {
                rlMenu = true;
            }
        }
        return( true );
    }
    if( wid != cwid && wid != owid ) {
        return( true );
    }

    if( !InsideWindow( wid, win_x, win_y ) ) {
        return( false );
    }
    mouseLine = win_y - 1;
    return( true );

} /* SelectLineMouseHandler */

/*
 * adjustCLN - adjust current line number and pagetop
 */
static bool adjustCLN( linenum *cln, linenum *pagetop, int amt,
                       linenum endline, int text_lines )
{
    bool        drawbord = false;

    if( !isMenu ) {
        if( amt < 0 ) {
            if( *cln + amt > 1 ) {
                *cln += amt;
                if( *cln < *pagetop ) {
                    *pagetop += amt;
                    drawbord = true;
                }
            } else {
                *cln = 1;
                *pagetop = 1;
            }
        } else {
            if( *cln + amt < endline ) {
                *cln += amt;
                if( *cln >= *pagetop + text_lines ) {
                    *pagetop += amt;
                    drawbord = true;
                }
            } else {
                *cln = endline;
                *pagetop = endline - text_lines + 1;
            }
        }
    } else {
        *cln += amt;
        if( amt < 0 ) {
            if( *cln <= 0 ) {
                while( *cln <= 0 ) {
                    *cln += endline;
                }
                *pagetop = *cln - text_lines + 1;
                drawbord = true;
            } else if( *cln < *pagetop ) {
                *pagetop += amt;
                drawbord = true;
            }
        } else {
            if( *cln <= endline ) {
                if( *cln >= *pagetop + text_lines ) {
                    *pagetop += amt;
                    drawbord = true;
                }
            } else {
                while( *cln > endline ) {
                    *cln -= endline;
                }
                *pagetop = *cln - text_lines + 1;
                drawbord = true;
            }
        }
    }
    if( *pagetop < 1 ) {
        *pagetop = 1;
    }
    if( endline - *pagetop + 1 < text_lines ) {
        *pagetop = endline - text_lines + 1;
        drawbord = true;
        if( *pagetop < 1 ) {
            *pagetop = 1;
        }
    }
    return( drawbord );

} /* adjustCLN */

/*
 * SelectLineInFile - select a line in a given file
 */
vi_rc SelectLineInFile( selflinedata *sfd )
{
    int         i, winflag;
    int         leftcol = 0, key2;
    bool        done = false;
    bool        redraw = true;
    bool        hiflag = false;
    bool        drawbord = false;
    int         farx, text_lines;
    linenum     pagetop = 1, lln = 1;
    char        tmp[MAX_STR];
    hilst       *ptr;
    linenum     cln;
    linenum     endline;
    vi_rc       rc;
    vi_key      key;

    /*
     * create the window
     */
    cln = sfd->cln;
    endline = sfd->f->fcbs.tail->end_line;
    farx = sfd->wi->area.x2;
    if( sfd->show_lineno ) {
        farx++;
    }
    if( sfd->hilite != NULL ) {
        hiflag = true;
    }
    rc = NewWindow2( &cwid, sfd->wi );
    if( rc != ERR_NO_ERR ) {
        return( rc );
    }
    if( !sfd->is_menu ) {
        WindowAuxUpdate( cwid, WIND_INFO_HAS_SCROLL_GADGETS, true );
        DrawBorder( cwid );
    }
    owid = sfd->eiw;
    isMenu = sfd->is_menu;
    PushMouseEventHandler( SelectLineMouseHandler );
    KillCursor();
    text_lines = WindowAuxInfo( cwid, WIND_INFO_TEXT_LINES );
    sfd->sl = -1;
    if( sfd->title != NULL ) {
        WindowTitle( cwid, sfd->title );
    }
    pagetop = text_lines * (cln / text_lines);
    if( cln % text_lines != 0 ) {
        pagetop++;
    }
    key = 0;
    if( LastEvent == VI_KEY( MOUSEEVENT ) ) {
        DisplayMouse( true );
    }

    /*
     * now, allow free scrolling and selection
     */
    while( !done ) {

        if( redraw ) {
            if( sfd->show_lineno ) {
                MySprintf(tmp, "%l/%l", cln, endline );
                i = sfd->wi->area.x2 - sfd->wi->area.x1;
                WindowBorderData( cwid, tmp, i - strlen( tmp ) );
                drawbord = true;
            }
            if( hiflag ) {
                ptr = sfd->hilite;
                ptr += cln - 1;
                if( ptr->_char == (char)-1 ) {
                    if( cln > lln ) {
                        cln++;
                    } else if( cln < lln ) {
                        cln--;
                    }
                }
            }
            if( drawbord ) {
                DrawBorder( cwid );
            }
            displayGenericLines( sfd->f, pagetop, leftcol, cln, &(sfd->wi->hilight_style), sfd->hilite, sfd->vals, sfd->valoff );
        }
        lln = cln;
        redraw = true;
        drawbord = false;
        mouseLine = -1;
        rlMenu = false;
        if( key == VI_KEY( MOUSEEVENT ) ) {
            DisplayMouse( true );
        }
        key = GetNextEvent( true );
        if( hiflag && ((key >= VI_KEY( ALT_A ) && key <= VI_KEY( ALT_Z )) ||
                       (key >= VI_KEY( a ) && key <= VI_KEY( z )) || (key >= VI_KEY( A ) && key <= VI_KEY( Z )) ||
                       (key >= VI_KEY( 1 ) && key <= VI_KEY( 9 ))) ) {
            i = 0;
            if( key >= VI_KEY( ALT_A ) && key <= VI_KEY( ALT_Z ) ) {
                key2 = key - VI_KEY( ALT_A ) + 'A';
            } else if( key >= VI_KEY( a ) && key <= VI_KEY( z ) ) {
                key2 = key - VI_KEY( a ) + 'A';
            } else {
                key2 = key;
            }
            ptr = sfd->hilite;
            while( ptr->_char != '\0' ) {
                if( toupper( ptr->_char ) == key2 ) {
                    cln = i + 1;
                    key = VI_KEY( ENTER );
                    break;
                }
                ++i;
                ++ptr;
            }
        }

        /*
         * check if a return-event has been selected
         */
        if( sfd->retevents != NULL ) {
            i = 0;
            if( key == VI_KEY( MOUSEEVENT ) ) {
                if( mouse_wid == owid && LastMouseEvent == VI_MOUSE_PRESS ) {
                    DisplayMouse( false );
                    sfd->event = sfd->retevents[mouseLine];
                    key = VI_KEY( ENTER );
                }
            } else {
                while( sfd->retevents[i] != 0 ) {
                    if( key == sfd->retevents[i] ) {
                        sfd->event = key;
                        key = VI_KEY( ENTER );
                        break;
                    }
                    i++;
                }
            }
        }

        /*
         * process key stroke
         */
        switch( key ) {
        case VI_KEY( MOUSEEVENT ):
            DisplayMouse( false );
            if( hiflag ) {
                ptr = sfd->hilite;
                ptr += mouseLine;
                if( ptr->_char == (char) -1 ) {
                    break;
                }
            }
            if( rlMenu && sfd->allow_rl != NULL ) {
                *(sfd->allow_rl) = rlMenuNum;
                done = true;
                break;
            }
            if( mouseScroll != MS_NONE ) {
                switch( mouseScroll ) {
                case MS_UP: goto evil_up;
                case MS_DOWN: goto evil_down;
                case MS_PAGEUP: goto evil_pageup;
                case MS_PAGEDOWN: goto evil_pagedown;
                case MS_EXPOSEDOWN:
                    adjustCLN( &cln, &pagetop, pagetop + text_lines - cln - 1, endline, text_lines );
                    adjustCLN( &cln, &pagetop, 1, endline, text_lines );
                    drawbord = true;
                    break;
                case MS_EXPOSEUP:
                    adjustCLN( &cln, &pagetop, pagetop - cln, endline, text_lines );
                    adjustCLN( &cln, &pagetop, -1, endline, text_lines );
                    drawbord = true;
                    break;

                }
                break;
            }
            switch( LastMouseEvent ) {
            case VI_MOUSE_DRAG:
                if( mouse_wid != cwid ) {
                    break;
                }
                cln = mouseLine + pagetop;
                break;
            case VI_MOUSE_RELEASE:
                if( !sfd->is_menu ) {
                    break;
                }
                if( mouse_wid == cwid ) {
                    cln = mouseLine + pagetop;
                    if( cln <= endline ) {
                        goto evil_enter;
                    }
                }
                break;
            case VI_MOUSE_DCLICK:
                if( mouse_wid != cwid ) {
                    AddCurrentMouseEvent();
                    done = true;
                } else {
                    cln = mouseLine + pagetop;
                    if( cln <= endline ) {
                        goto evil_enter;
                    }
                }
                break;
            case VI_MOUSE_PRESS_R:
                if( mouse_wid != cwid ) {
                    AddCurrentMouseEvent();
                    done = true;
                }
                break;
            case VI_MOUSE_PRESS:
                if( mouse_wid != cwid ) {
                    AddCurrentMouseEvent();
                    done = true;
                } else {
                    cln = mouseLine + pagetop;
                }
                break;
            }
            break;

        case VI_KEY( ESC ):
            done = true;
            break;

        evil_enter:
        case VI_KEY( ENTER ):
        case VI_KEY( SPACE ):
            /*
             * see if we need to do a callback for this
             */
            if( sfd->checkres != NULL ) {
                line    *cline;
                fcb     *cfcb;
                char    *ptr;

                i = cln - 1;
                GimmeLinePtr( cln, sfd->f, &cfcb, &cline );
                ptr = SkipLeadingSpaces( cline->data );
                strcpy( tmp, sfd->vals[i] );
                rc = sfd->checkres( ptr, tmp, &winflag );
                if( winflag == 2 ) {
                    MoveWindowToFront( cwid );
                }
                if( rc == ERR_NO_ERR ) {
                    ReplaceString( &(sfd->vals[i]), tmp );
                    redraw = true;
                }
                break;

            /*
             * no value window, so just return line selected
             */
            } else {
                if( isMenu && InvokeMenuHook( CurrentMenuNumber, cln ) == -1 ) {
                    break;
                }
                sfd->sl = cln;
                done = true;
            }
            break;

        case VI_KEY( LEFT ):
        case VI_KEY( h ):
            if( sfd->allow_rl != NULL ) {
                *(sfd->allow_rl) = -1;
                done = true;
            }
            break;

        case VI_KEY( RIGHT ):
        case VI_KEY( l ):
            if( sfd->allow_rl != NULL ) {
                *(sfd->allow_rl) = 1;
                done = true;
            }
            break;

        evil_up:
        case VI_KEY( UP ):
        case VI_KEY( k ):
            drawbord = adjustCLN( &cln, &pagetop, -1, endline, text_lines );
            break;

        evil_down:
        case VI_KEY( DOWN ):
        case VI_KEY( j ):
            drawbord = adjustCLN( &cln, &pagetop, 1, endline, text_lines );
            break;

        case VI_KEY( CTRL_PAGEUP ):
            drawbord = adjustCLN( &cln, &pagetop, -cln + 1, endline, text_lines );
            break;

        case VI_KEY( CTRL_PAGEDOWN ):
            drawbord = adjustCLN( &cln, &pagetop, endline - cln, endline, text_lines );
            break;

        evil_pageup:
        case VI_KEY( PAGEUP ):
        case VI_KEY( CTRL_B ):
            drawbord = adjustCLN( &cln, &pagetop, -text_lines, endline, text_lines );
            break;

        evil_pagedown:
        case VI_KEY( PAGEDOWN ):
        case VI_KEY( CTRL_F ):
            drawbord = adjustCLN( &cln, &pagetop, text_lines, endline, text_lines );
            break;

        case VI_KEY( HOME ):
            drawbord = true;
            cln = 1;
            pagetop = 1;
            break;

        case VI_KEY( END ):
            drawbord = true;
            cln = endline;
            pagetop = endline - text_lines + 1;
            if( pagetop < 1 ) {
                pagetop = 1;
            }
            break;

        default:
            redraw = false;
            break;

        }

    }
    PopMouseEventHandler();
    CloseAWindow( cwid );
    RestoreCursor();
    SetWindowCursor();
    return( rc );

} /* SelectLineInFile */
