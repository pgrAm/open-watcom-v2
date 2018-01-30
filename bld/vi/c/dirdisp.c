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
#ifdef __WIN__
    #include "utils.h"
    #include "color.h"
    #include "font.h"
#endif
#include <assert.h>

static window_id    dir_wid = NO_WINDOW;
static int          oldFilec, lastFilec;
static int          oldPage = -1;
static int          maxJ, perPage, maxPage, cPage;
static bool         hasWrapped, isDone;
static int          mouseFilec = -1;
static char         strFmt[] = " %c%S";

static bool hasMouseHandler;

/*
 * FileCompleteMouseHandler - handle mouse events for file completion
 */
static bool FileCompleteMouseHandler( window_id wid, int win_x, int win_y )
{
    if( wid != dir_wid ) {
        return( false );
    }
    if( LastMouseEvent != VI_MOUSE_PRESS && LastMouseEvent != VI_MOUSE_DCLICK ) {
        return( false );
    }
    if( LastMouseEvent == VI_MOUSE_DCLICK ) {
        isDone = true;
    }
    if( !InsideWindow( wid, win_x, win_y ) ) {
        return( false );
    }

    mouseFilec = cPage + (win_x - 1) / NAMEWIDTH + (win_y - 1) * maxJ;

    return( true );

} /* FileCompleteMouseHandler */


/*
 * appendExtra - add extra to end
 */
static vi_rc appendExtra( char *data, int start, int max, direct_ent *fi, int len )
{
    int     i;
    vi_rc   rc;

    for( i = start; i < start + len; i++ ) {
        if( i >= max ) {
            break;
        }
        data[i] = fi->name[i - start];
    }
    if( IS_SUBDIR( fi ) ) {
        rc = ERR_NO_ERR;
    } else {
        rc = FILE_COMPLETE;
    }
    data[i] = '\0';
    return( rc );

} /* appendExtra */

/*
 * doFileComplete - complete file name
 */
static vi_rc doFileComplete( char *data, int start, int max, bool getnew, vi_key key )
{
    int         i, j, k, newstart;
    char        buff[MAX_STR * 2];
    vi_rc       rc;

    newstart = -1;
    for( i = start; !isspace( data[i] ) && i >= 0; --i ) {
#ifdef __UNIX__
        if( data[i] == '/' && newstart < 0 ) {
#else
        if( (data[i] == DRV_SEP || data[i] == '/' || data[i] == '\\') && newstart < 0 ) {
#endif
            newstart = i + 1;
        }
    }
    if( newstart < 0 ) {
        newstart = i + 1;
    }
    if( getnew ) {

        k = 0;
        for( j = i + 1; j <= start; j++ ) {
            buff[k++] = data[j];
        }
        lastFilec = -1;
        buff[k++] = '*';
        buff[k] = '\0';
        rc = GetSortDir( buff, false );
        if( rc != ERR_NO_ERR ) {
            return( rc );
        }
        /*
         * remove any crap from the list
         */
        for( i = 0; i < DirFileCount; i++ ) {
            if( !IsTextFile( DirFiles[i]->name ) || (DirFiles[i]->name[0] == '.') ) {
                MemFree( DirFiles[i] );
                for( j = i + 1; j < DirFileCount; j++ ) {
                    DirFiles[j - 1] = DirFiles[j];
                }
                i--;
                DirFileCount--;
            }
        }

    }

    if( DirFileCount == 0 ) {
        return( ERR_FILE_NOT_FOUND );
    }
    if( DirFileCount == 1 ) {
        if( !BAD_ID( dir_wid ) ) {
            ClearWindow( dir_wid );
        }
        return( appendExtra( data, newstart,max, DirFiles[0], strlen( DirFiles[0]->name ) ) );
    }

    /*
     * okay, so we have multiple matches; add in the next match
     */
    oldFilec = lastFilec;
    switch( key ) {
    case VI_KEY( TAB ):
    case VI_KEY( RIGHT ):
        lastFilec++;
        break;
    case VI_KEY( SHIFT_TAB ):
    case VI_KEY( LEFT ):
        lastFilec--;
        break;
    case VI_KEY( DOWN ):
        lastFilec += maxJ;
        break;
    case VI_KEY( UP ):
        lastFilec -= maxJ;
        break;
    case VI_KEY( FAKEMOUSE ):
    case VI_KEY( MOUSEEVENT ):
        lastFilec = mouseFilec;
        break;
    }
    for( ; lastFilec >= DirFileCount; lastFilec -= DirFileCount ) {
        hasWrapped = true;
    }
    for( ; lastFilec < 0; lastFilec += DirFileCount ) {
        hasWrapped = true;
    }

    appendExtra( data, newstart, max, DirFiles[lastFilec], strlen( DirFiles[lastFilec]->name ) );

    return( ERR_NO_ERR );

} /* doFileComplete */

#ifdef __WIN__
static int calcColumns( window_id wid )
{
    RECT        rect;
    int         columns;
    window      *w;

    if( BAD_ID( wid ) )
        return( 0 );
    w = WINDOW_FROM_ID( wid );
    GetClientRect( wid, &rect );
    columns = rect.right - rect.left;
    columns = columns / (NAMEWIDTH * FontAverageWidth( WIN_TEXT_FONT( w ) ));
    return( columns );
}

void FileCompleteMouseClick( window_id wid, int x, int y, bool dclick )
{
    int         file, column_width, column_height, c;
    int         left_margin, columns;
    RECT        rect;
    window      *w;

    if( BAD_ID( wid ) )
        return;
    w = WINDOW_FROM_ID( wid );
    /* figure out which file_name the user clicked on */
    columns = calcColumns( wid );
    GetClientRect( wid, &rect );
    column_width = NAMEWIDTH * FontAverageWidth( WIN_TEXT_FONT( w ) );
    column_height = FontHeight( WIN_TEXT_FONT( w ) );
    left_margin = (rect.right - rect.left - column_width * columns) >> 1;
    if( x < left_margin || x + left_margin > rect.right ) {
        return;
    }
    c = (x - left_margin) / column_width;
    file = (y / column_height) * columns + c;
    file += cPage * perPage;
    if( dclick ) {
        isDone = true;
    }
    mouseFilec = file;
    KeyAdd( VI_KEY( FAKEMOUSE ) );
}

static void parseFileName( int i, char *buffer )
{
    char        ch;

    if( i >= DirFileCount ) {
        MySprintf( buffer, strFmt, ' ', SingleBlank );
    } else {
        if( IS_SUBDIR( DirFiles[i] ) ) {
            ch = FILE_SEP;
        } else {
            ch = ' ';
        }
        MySprintf( buffer, strFmt, ch, DirFiles[i]->name );
    }
    buffer[NAMEWIDTH] = '\0';
}

#define ROW( i )    ((i) / maxJ)
#define COL( i )    ((i) % maxJ)

static void getBounds( int *start, int *end )
{
    int         first, last;

    if( oldFilec < lastFilec ) {
        first = oldFilec;
        last = lastFilec;
    } else {
        first = lastFilec;
        last = oldFilec;
    }
    *start = ROW( first ) * maxJ;
    *end = ROW( last ) * maxJ + maxJ - 1;
}

static void displayFiles( void )
{
    int         i, start, end;
    int         column, right_edge, left_edge;
    int         outer_bound;
    int         font_height, column_width;
    window      *w;
    RECT        rect;
    type_style  *style;
    char        buffer[FILENAME_MAX];

    if( BAD_ID( dir_wid ) )
        return;
    w = WINDOW_FROM_ID( dir_wid );

    if( hasWrapped ) {
        ClearWindow( dir_wid );
        hasWrapped = false;
    }

    font_height = FontHeight( WIN_TEXT_FONT( w ) );
    GetClientRect( dir_wid, &rect );
    column_width = NAMEWIDTH * FontAverageWidth( WIN_TEXT_FONT( w ) );
    outer_bound = rect.right;
    left_edge = rect.right - rect.left;
    left_edge -= maxJ * column_width;
    left_edge >>= 1;
    right_edge = rect.right - left_edge - 1;

    cPage = lastFilec / perPage;
    if( cPage != oldPage ) {
        start = cPage * perPage;
        end = start + perPage + maxJ;
    } else {
        getBounds( &start, &end );
    }

    // assert( start <= lastFilec && lastFilec <= end );
    rect.top = (ROW( start ) - cPage * perPage / maxJ) * font_height;
    rect.bottom = rect.top + font_height;
    rect.left = 0;
    rect.right = left_edge;
    BlankRectIndirect( dir_wid, WIN_TEXT_BACKCOLOR( w ), &rect );
    column = 0;
    for( i = start; i <= end; i++ ) {
        parseFileName( i, &buffer[0] );
        style = (i == lastFilec) ? WIN_HILIGHT_STYLE( w ) : WIN_TEXT_STYLE( w );
        rect.left = column * column_width + left_edge;
        rect.right = rect.left + column_width;
        BlankRectIndirect( dir_wid, style->background, &rect );
        WriteString( dir_wid, rect.left, rect.top, style, &buffer[0] );
        column = (column + 1) % maxJ;
        if( column == 0 ) {
            /* blat out the rest of the row and continue on */
            rect.left = right_edge;
            rect.right = outer_bound;
            BlankRectIndirect( dir_wid, WIN_TEXT_BACKCOLOR( w ), &rect );
            rect.top = rect.bottom;
            rect.bottom = rect.top + font_height;
            rect.left = 0;
            rect.right = left_edge;
            BlankRectIndirect( dir_wid, WIN_TEXT_BACKCOLOR( w ), &rect );
        }
    }
    oldPage = cPage;
}

#else

/*
 * displayFiles - display files according to specified type
 *                (comment free code - my favourite kind!)
 */
static void displayFiles( void )
{
    char        tmp[FILENAME_MAX], tmp2[FILENAME_MAX], dirc;
    int         j, i, k, hilite, z;
    int         st, end, l;

    tmp[0] = '\0';
    j = 0;

    st = 0;
    end = perPage;
    cPage = lastFilec / perPage;
    if( cPage > 0 ) {
        cPage *= perPage;
        st += cPage;
        end += cPage;
    }
    if( hasWrapped ) {
        ClearWindow( dir_wid );
        hasWrapped = false;
    }

    hilite = -1;
    l = 1;
    for( i = st; i < end; i++ ) {

        if( i == lastFilec ) {
            hilite = j;
        }
        if( i >= DirFileCount ) {
            MySprintf( tmp2, strFmt, ' ', SingleBlank );
        } else {
            if( IS_SUBDIR( DirFiles[i] ) ) {
                dirc = FILE_SEP;
            } else {
                dirc = ' ';
            }
            MySprintf( tmp2, strFmt, dirc, DirFiles[i]->name );
            tmp2[NAMEWIDTH] = '\0';
        }
        strcat( tmp, tmp2 );
        j++;
        if( j == maxJ || i == ( end - 1 ) ) {
            DisplayLineInWindow( dir_wid, l++, tmp );
            if( hilite >= 0 ) {
                j = hilite * NAMEWIDTH;
                if( IS_SUBDIR( DirFiles[lastFilec] ) ) {
                    dirc = FILE_SEP;
                } else {
                    dirc = ' ';
                }
                MySprintf( tmp2, strFmt, dirc, DirFiles[lastFilec]->name );
                z = j + strlen( tmp2 );
                for( k = j; k < z; k++ ) {
                    SetCharInWindowWithColor( dir_wid, l - 1, k + 1, tmp2[k - j], &filecw_info.hilight_style );
                }
                hilite = -1;
            }
            j = 0;
            tmp[0] = '\0';
        }

    }
    if( mouseFilec >= 0 ) {
        mouseFilec = -1;
    }

} /* displayFiles */
#endif

/*
 * StartFileComplete - handle file completion
 */
vi_rc StartFileComplete( char *data, int start, int max, int what )
{
    vi_rc   rc;
    int     maxl;

    isDone = false;
    rc = doFileComplete( data, start, max, true, what );
    if( rc > ERR_NO_ERR || rc == FILE_COMPLETE ) {
        return( rc );
    }

    if( BAD_ID( dir_wid ) ) {
        // ensure uniform font before opening window
        if( filecw_info.text_style.font != filecw_info.hilight_style.font )
            filecw_info.hilight_style.font = filecw_info.text_style.font;

        rc = NewWindow2( &dir_wid, &filecw_info );
        if( rc != ERR_NO_ERR ) {
            return( rc );
        }
    }
    WindowTitle( dir_wid, "File Completion List" );

#ifdef __WIN__
    maxJ = calcColumns( dir_wid );
#else
    maxJ = WindowAuxInfo( dir_wid, WIND_INFO_TEXT_COLS ) / NAMEWIDTH;
#endif
    maxl = WindowAuxInfo( dir_wid, WIND_INFO_TEXT_LINES ) - 1;
    perPage = ( maxl + 1 ) * maxJ;
    oldPage = -1;
    maxPage = ( DirFileCount + perPage - 1 ) / perPage;
    displayFiles();
    PushMouseEventHandler( FileCompleteMouseHandler );
    hasMouseHandler = true;
    return( ERR_NO_ERR );

} /* StartFileComplete */

/*
 * ContinueFileComplete
 */
vi_rc ContinueFileComplete( char *data, int start, int max, int what )
{
    vi_rc   rc;

    rc = doFileComplete( data, start, max, false, what );
    if( rc > ERR_NO_ERR || rc == FILE_COMPLETE ) {
        return( rc );
    }
    displayFiles();
    if( isDone ) {
        return( FILE_COMPLETE_ENTER );
    }
    return( ERR_NO_ERR );

} /* ContinueFileComplete */

/*
 * FinishFileComplete
 */
void FinishFileComplete( void )
{
    if( BAD_ID( dir_wid ) ) {
        return;
    }
    CloseAWindow( dir_wid );
    dir_wid = NO_WINDOW;
    if( hasMouseHandler ) {
        PopMouseEventHandler();
        hasMouseHandler = false;
    }

} /* FinishFileComplete */

/*
 * PauseFileComplete
 */
void PauseFileComplete( void )
{
    WindowTitle( dir_wid, NULL );
    if( hasMouseHandler ) {
        PopMouseEventHandler();
        hasMouseHandler = false;
    }

} /* PauseFileComplete */
