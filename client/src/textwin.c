/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 3 of the License, or     *
* (at your option) any later version.                                   *
*                                                                       *
* This program is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
* GNU General Public License for more details.                          *
*                                                                       *
* You should have received a copy of the GNU General Public License     *
* along with this program; if not, write to the Free Software           *
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
*                                                                       *
* The author can be reached at admin@atrinik.org                        *
************************************************************************/

#include <include.h>

/* this depends on the text win png*/
#define TEXT_WIN_XLEN 265
#define TEXT_WIN_YLEN 190

int  textwin_flags;

_textwin_set txtwin[TW_SUM];
static int old_slider_pos= 0;

int txtwin_start_size = 0;

/******************************************************************
  definition of keyword:
    a keyword is the text between two '^' chars.
    the size can vary between a single word and a complete sentence.
    the max length of a keyword is 1 row (inclusive '^') because only
    one LF within is allowed.
******************************************************************/

/******************************************************************
 returns the startpos of a keyword(-part).
******************************************************************/
static char *get_keyword_start(int actWin, int mouseX, int *row, int wID){
    int pos, pos2=cur_widget[wID].x1, key_start;
    char *text;

  if (actWin > TW_SUM)
    return 0;

    pos= (txtwin[actWin].top_drawLine + (*row))%TEXT_WIN_MAX - txtwin[actWin].scroll;
    if (pos <0)
        pos+=TEXT_WIN_MAX;

    if (mouseX < 0) goto row2; /* check only for 2. half of keyword */
    /* check if keyword starts in the row before */
    if (txtwin[actWin].scroll+1 != txtwin[actWin].act_bufsize- txtwin[actWin].size /* dont check in first row */
     && txtwin[actWin].text[pos].key_clipped)
    {
        /* was a clipped keyword clicked? */
        int index=-1;
        text = txtwin[actWin].text[pos].buf;
        while (text[++index] && pos2 <= mouseX && text[index]!='^')
        pos2+= SystemFont.c[(int)text[index]].w+ SystemFont.char_offset;
        if (text[index]!='^'){
         /* clipped keyword was clicked, so we must start one row before */
         /* TODO not start one row before if its the first row in buffer ! */
            (*row)--;
            pos = (txtwin[actWin].top_drawLine + (*row))%TEXT_WIN_MAX - txtwin[actWin].scroll;
            if (pos <0)
                pos+=TEXT_WIN_MAX;
            mouseX = 800; /* detect last keyword in this row */
        }
    }

row2:
    text = txtwin[actWin].text[pos].buf;
    /* find the first char of the keyword */
    if (txtwin[actWin].text[pos].key_clipped)
    key_start =0;
    else key_start =-1;
    pos =0; pos2 = cur_widget[wID].x1;
    while (text[pos] && pos2 <= mouseX){
        if (text[pos++]=='^'){
            if (key_start<0) key_start = pos; /* start of a key */
            else key_start =-1; /* end of a key */
            continue;
        }
        pos2+= SystemFont.c[(int)text[pos]].w+ SystemFont.char_offset;
    }
    if (key_start <0)
        return NULL; /* no keyword here */
    (*row)++;
    return &text[key_start];
}

/******************************************************************
 check for keyword and send it to server.
******************************************************************/
void say_clickedKeyword(int actWin, int mouseX, int mouseY){
	char    cmdBuf[MAX_KEYWORD_LEN + 1]     =
        {
            "/say "
        };
    char    cmdBuf2[MAX_KEYWORD_LEN + 1]    =
        {
            ""
        };
    char   *text;
    int     clicked_row, pos = 5;
	int wID = 0;

    if (actWin == TW_CHAT)
        wID = CHATWIN_ID;
    else if (actWin == TW_MSG)
        wID = MSGWIN_ID;

    clicked_row = (mouseY - cur_widget[wID].y1) / 10;

    text = get_keyword_start(actWin, mouseX, &clicked_row, wID);
    if (text == NULL)
        return;
    while (*text && *text != '^')
        cmdBuf[pos++] = *text++;
    if (*text != '^')
    {
        text = get_keyword_start(actWin, -1, &clicked_row, wID);
        if (text != NULL)
            while (*text && *text != '^')
                cmdBuf[pos++] = *text++;
    }
    cmdBuf[pos++] = '\0';

    /* clickable keywords that can be commands too.
     * Commands will
     * get executed instead of /said. */

	/* get rid of /say */
    if (cmdBuf[5] == '/')
    {
        pos = 5;
        while (cmdBuf[pos] != '\0')
        {
            cmdBuf2[pos - 5] = cmdBuf[pos];
            pos++;
        }
        send_command(cmdBuf2, -1, SC_NORMAL);
    }
    else
        send_command(cmdBuf, -1, SC_NORMAL);
}

/******************************************************************
 clear the screen of a text-window.
******************************************************************/
void textwin_init()
{
    int i;

    for (i = TW_MIX; i < TW_SUM; i++)
    {
        txtwin[i].bot_drawLine = 0;
        txtwin[i].act_bufsize = 0;
        txtwin[i].scroll = 0;
        txtwin[i].size = 9;
    }
}

void draw_info_format(int flags,char *format,...)
{
    char    buf[HUGE_BUF];
    va_list ap;

    va_start(ap, format);

    vsprintf(buf, format, ap);

    va_end(ap);

    draw_info(buf, flags);
}

/******************************************************************
 add string to the text-window (perform auto-clipping).
******************************************************************/
void draw_info (char *str, int flags )
{
    static int key_start=0;
    static int key_count=0;
    int i, len,a, color, mode;
    int winlen = 239;
    char buf[4096];
    char *text;
  int actWin, z;

    color = flags &0xff;
    mode = flags;
    /*
     * first: we set all white spaces (char<32) to 32 to remove really all odd stuff.
     * except 0x0a - this is EOL for us and will be set to
     * 0 to mark C style end of string
     */
    for(i=0;str[i]!=0;i++)
    {
        if(str[i] < 32 && str[i] != 0x0a && strcmp(strndup(str + i, 5), "[IMG]"))
        str[i]= 32;
    }
    /*
     * ok, here we must cut a string to make it fit in window
     * for it we must add the char length
     * we assume this standard font in the windows...
     */
    len = 0;
    for(a=i=0;;i++)
    {
        if(str[i] != '^')
            len += SystemFont.c[(int)(str[i])].w+SystemFont.char_offset;

        if(len>=winlen || str[i] == 0x0a ||str[i]==0)
        {
            /*
                if(str[i]==0 && !a)
                    break;
             */

            /* now the special part - lets look for a good point to cut */
            if(len>=winlen && a>10)
            {
                    int ii=a,it=i,ix=a,tx=i;

                        while(ii>=a/2)
                        {
                            if(str[it]==' ' || str[it]==':' || str[it]=='.' || str[it]==','
                                || str[it]=='(' || str[it]==';'|| str[it]=='-'
                                || str[it]=='+'|| str[it]=='*'|| str[it]=='?'|| str[it]=='/'
                                || str[it]=='='|| str[it]=='.'|| str[it]==0|| str[it]==0x0a)
                            {
                                tx=it;
                                ix=ii;
                                break;
                            }
                            it--;
                            ii--;
                        };
                        i=tx;
                        a=ix;
            }
            buf[a]=0;

      actWin = TW_MIX;
      		for (z = 0; z < 2; z++)
            {
                /* add messages to mixed-textwin and either to msg OR chat-textwin */
                strcpy(txtwin[actWin].text[txtwin[actWin].bot_drawLine % TEXT_WIN_MAX].buf, buf);
                txtwin[actWin].text[txtwin[actWin].bot_drawLine % TEXT_WIN_MAX].color = color;
                txtwin[actWin].text[txtwin[actWin].bot_drawLine % TEXT_WIN_MAX].flags = mode;
                txtwin[actWin].text[txtwin[actWin].bot_drawLine % TEXT_WIN_MAX].key_clipped = key_start;
                if (txtwin[actWin].scroll)
                    txtwin[actWin].scroll++;
                if (txtwin[actWin].act_bufsize < TEXT_WIN_MAX)
                    txtwin[actWin].act_bufsize++;
                txtwin[actWin].bot_drawLine++;
                txtwin[actWin].bot_drawLine %= TEXT_WIN_MAX;
                actWin++; /* next window => MSG_WIN */
                if (mode & NDI_PLAYER)
                {
                    actWin++; /* next window => MSG_CHAT */
                    WIDGET_REDRAW(CHATWIN_ID);
                }
                else
                    WIDGET_REDRAW(MSGWIN_ID);
            }

            /* hack: because of autoclip we must scan every line again */
            for (text=buf; *text; text++) if (*text =='^') key_count = (key_count+1) & 1;
            if (key_count) key_start =0x1000;
            else key_start =0;

            a=len = 0;
            if(str[i]==0)
                break;
        }
        if(str[i] != 0x0a)
            buf[a++] = str[i];
    }
}

/******************************************************************
 draw a text-window.
******************************************************************/
static void show_window(int actWin, int x, int y, _BLTFX *bltfx)
{
    int i, temp;

    txtwin[actWin].x = x;
    txtwin[actWin].y = y;

    if (actWin!=TW_MIX)
    {
        x = y = 0;
    }

    txtwin[actWin].top_drawLine = txtwin[actWin].bot_drawLine - (txtwin[actWin].size + 1);
    if (txtwin[actWin].top_drawLine < 0 && txtwin[actWin].act_bufsize == TEXT_WIN_MAX)
        txtwin[actWin].top_drawLine += TEXT_WIN_MAX;
    else if (txtwin[actWin].top_drawLine < 0)
        txtwin[actWin].top_drawLine = 0;
    if (txtwin[actWin].scroll > txtwin[actWin].act_bufsize - (txtwin[actWin].size + 1))
        txtwin[actWin].scroll = txtwin[actWin].act_bufsize - (txtwin[actWin].size + 1);
    if (txtwin[actWin].scroll < 0)
        txtwin[actWin].scroll = 0;

    for (i = 0; i <= txtwin[actWin].size && i < txtwin[actWin].act_bufsize; i++)
    {
        temp = (txtwin[actWin].top_drawLine + i) % TEXT_WIN_MAX;
        if (txtwin[actWin].act_bufsize > txtwin[actWin].size)
        {
            temp -= txtwin[actWin].scroll;
            if (temp < 0)
                temp = TEXT_WIN_MAX + temp;
        }
        StringBlt(bltfx->surface, &SystemFont, &txtwin[actWin].text[temp].buf[0], x + 2,
                  (y + 1 + i * 10) | txtwin[actWin].text[temp].key_clipped, txtwin[actWin].text[temp].color, NULL, NULL);
    }

    /* only draw scrollbar if needed */
    if (txtwin[actWin].act_bufsize > txtwin[actWin].size)
    {
        SDL_Rect    box;

        box.x = box.y = 0;
        box.w = Bitmaps[BITMAP_SLIDER]->bitmap->w;
        box.h = txtwin[actWin].size * 10 + 1;
            temp = -9; /* no textinput-line */
        sprite_blt(Bitmaps[BITMAP_SLIDER_UP], x + 250, y + 2, NULL, bltfx);
        sprite_blt(Bitmaps[BITMAP_SLIDER_DOWN], x + 250, y + 13 + temp + txtwin[actWin].size * 10, NULL, bltfx);
        sprite_blt(Bitmaps[BITMAP_SLIDER], x + 250, y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 2 + temp, &box, bltfx);
        box.h += temp - 2;
        box.w -= 2;

        txtwin[actWin].slider_y = ((txtwin[actWin].act_bufsize - (txtwin[actWin].size + 1) - txtwin[actWin].scroll) * box.h)
                                  / txtwin[actWin].act_bufsize;
        txtwin[actWin].slider_h = (box.h * (txtwin[actWin].size + 1)) / txtwin[actWin].act_bufsize; /* between 0.0 <-> 1.0 */
        if (txtwin[actWin].slider_h < 1)
            txtwin[actWin].slider_h = 1;

        if (!txtwin[actWin].scroll && txtwin[actWin].slider_y + txtwin[actWin].slider_h < box.h)
            txtwin[actWin].slider_y++;

        box.h = txtwin[actWin].slider_h;
        sprite_blt(Bitmaps[BITMAP_TWIN_SCROLL], x + 252,
                   y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 3 + txtwin[actWin].slider_y, &box, bltfx);

        if (txtwin[actWin].highlight == TW_HL_UP)
        {
            box.x = x + 250;
            box.y = y + 2;
            box.h = Bitmaps[BITMAP_SLIDER_UP]->bitmap->h;
            box.w = 1;
            SDL_FillRect(bltfx->surface, &box, -1);
            box.x += Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
            SDL_FillRect(bltfx->surface, &box, -1);
            box.w = Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
            box.h = 1;
            box.x = x + 250;
            SDL_FillRect(bltfx->surface, &box, -1);
            box.y += Bitmaps[BITMAP_SLIDER_UP]->bitmap->h - 1;
            SDL_FillRect(bltfx->surface, &box, -1);
        }
        else if (txtwin[actWin].highlight == TW_ABOVE)
        {
            box.x = x + 252;
            box.y = y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 2;
            box.h = txtwin[actWin].slider_y + 1;
            box.w = 5;
            SDL_FillRect(bltfx->surface, &box, 0);
        }
        else if (txtwin[actWin].highlight == TW_HL_SLIDER)
        {
            box.x = x + 252;
            box.y = y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 3 + txtwin[actWin].slider_y;
            box.w = 1;
            SDL_FillRect(bltfx->surface, &box, -1);
            box.x += 4;
            SDL_FillRect(bltfx->surface, &box, -1);
            box.x -= 4;
            box.h = 1;
            box.w = 4;
            SDL_FillRect(bltfx->surface, &box, -1);
            box.y += txtwin[actWin].slider_h - 1;
            SDL_FillRect(bltfx->surface, &box, -1);
        }
        else if (txtwin[actWin].highlight == TW_UNDER)
        {
            box.x = x + 252;
            box.y = y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 3 + txtwin[actWin].slider_y + box.h;
            box.h = txtwin[actWin].size * 10 - txtwin[actWin].slider_y - txtwin[actWin].slider_h - 10;
            box.w = 5;
            SDL_FillRect(bltfx->surface, &box, 0);
        }
        else if (txtwin[actWin].highlight == TW_HL_DOWN)
        {
            box.x = x + 250;
            box.y = y + txtwin[actWin].size * 10 + 4;
            box.h = Bitmaps[BITMAP_SLIDER_UP]->bitmap->h;
            box.w = 1;
            SDL_FillRect(bltfx->surface, &box, -1);
            box.x += Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
            SDL_FillRect(bltfx->surface, &box, -1);
            box.w = Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
            box.h = 1;
            box.x = x + 250;
            SDL_FillRect(bltfx->surface, &box, -1);
            box.y += Bitmaps[BITMAP_SLIDER_UP]->bitmap->h - 1;
            SDL_FillRect(bltfx->surface, &box, -1);
        }
    }
    else
        txtwin[actWin].slider_h = 0;
}

/******************************************************************
 display all text-windows - used now only at startup
******************************************************************/
void textwin_show(int x, int y)
{
    int         len;
    SDL_Rect    box;
    _BLTFX      bltfx;
    bltfx.alpha = options.textwin_alpha;
    bltfx.flags = BLTFX_FLAG_SRCALPHA;
    bltfx.surface = NULL;
    box.x = box.y = 0;
    box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
    y = Screensize.y - (Screensize.y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2) + 25;

    box.h = len = txtwin[TW_MIX].size * 10 + 23;
    y -= len;
    if (options.use_TextwinAlpha)
    {
        sprite_blt(Bitmaps[BITMAP_TEXTWIN_MASK], x, y, &box, &bltfx);
    }
    else
        sprite_blt(Bitmaps[BITMAP_TEXTWIN], x, y, &box, NULL);

    bltfx.alpha=255;
    bltfx.surface = ScreenSurface;
    bltfx.flags=0;
    show_window(TW_MIX, x, y - 1, &bltfx);
}

/******************************************************************
 display widget text-wins
******************************************************************/
void widget_textwin_show(int x, int y, int actWin)
{
    int         len, wID = 0;
    SDL_Rect    box;
    _BLTFX      bltfx;

    if (actWin==TW_CHAT)
        wID=CHATWIN_ID;
    else if (actWin==TW_MSG)
        wID=MSGWIN_ID;


    /* backbuffering is a bit trickier
       we always blit the background extra because of the alpha
       TODO: maybe use a 32bit alpha chanel surface */

    box.x = box.y = 0;
    box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
    box.h = len = txtwin[actWin].size * 10 + 13;

    if (options.use_TextwinAlpha)
    {
        bltfx.alpha = options.textwin_alpha;
        bltfx.flags = BLTFX_FLAG_SRCALPHA;
        bltfx.surface = ScreenSurface;
        sprite_blt(Bitmaps[BITMAP_TEXTWIN_MASK], x, y, &box, &bltfx);
    }
    else
    {
        sprite_blt(Bitmaps[BITMAP_TEXTWIN], x, y, &box, NULL);
    }

    /* if we don't have a backbuffer, create it */
    if (!widgetSF[wID] || cur_widget[wID].redraw)
    {
        widgetSF[wID]=SDL_ConvertSurface(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap,
                Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap->format,Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap->flags);
        SDL_SetColorKey(widgetSF[wID], SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(widgetSF[wID]->format, 0, 0, 0));
    }

    /* lets draw the widgets in the backbuffer */
    if (cur_widget[wID].redraw)
    {
        cur_widget[wID].redraw=FALSE;

        SDL_FillRect(widgetSF[wID],NULL, 0);

        bltfx.surface=widgetSF[wID];
        bltfx.flags = 0;
        bltfx.alpha = 0;
        if (options.use_TextwinAlpha)
        {
            box.x = 0;
            box.y = 0;
            box.h = 1;
            box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
            SDL_FillRect(widgetSF[wID], &box, sdl_gray4);
            box.y = len;
            box.h = 1;
            box.x = 0;
            box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
            SDL_FillRect(widgetSF[wID], &box, sdl_gray4);
            box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
            box.x = box.w-1;
            box.w = 1;
            box.y = 0;
            box.h = len;
            SDL_FillRect(widgetSF[wID], &box, sdl_gray4);
            box.x = 0;
            box.y = 0;
            box.h = len;
            box.w = 1;
            SDL_FillRect(widgetSF[wID], &box, sdl_gray4);
        }
        show_window(actWin, x, y - 2, &bltfx);
    }
    box.x=x;
    box.y=y;
    box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
    box.h = len = txtwin[actWin].size * 10 + 13;
    SDL_BlitSurface(widgetSF[wID], NULL, ScreenSurface, &box);
}

/******************************************************************
 mouse-button event in the textwindow.
*****************************************************************/
void textwin_button_event(int actWin, SDL_Event event)
{
    int wID = 0;

    if (actWin==TW_CHAT)
        wID=CHATWIN_ID;
    else if (actWin==TW_MSG)
        wID=MSGWIN_ID;

    if (event.motion.x < cur_widget[wID].x1 || (textwin_flags & (TW_SCROLL | TW_RESIZE) && (textwin_flags & actWin))) /* scrolling || resising */
        return;

    WIDGET_REDRAW(wID);

    if (event.button.button == 4) /* mousewheel up */
        txtwin[actWin].scroll++;
    else if (event.button.button == 5) /* mousewheel down */
        txtwin[actWin].scroll--;
    else if (event.button.button == SDL_BUTTON_LEFT)
    {
        if (txtwin[actWin].highlight == TW_HL_UP) /* clicked scroller-button up */
            txtwin[actWin].scroll++;
        else if (txtwin[actWin].highlight == TW_ABOVE) /* clicked above the slider */
            txtwin[actWin].scroll += txtwin[actWin].size;
        else if (txtwin[actWin].highlight == TW_HL_SLIDER)
        {
            /* clicked on the slider */
            textwin_flags |= (actWin | TW_SCROLL);
            old_slider_pos = event.motion.y - txtwin[actWin].slider_y;
        }
        else if (txtwin[actWin].highlight == TW_UNDER) /* clicked under the slider */
            txtwin[actWin].scroll -= txtwin[actWin].size;
        else if (txtwin[actWin].highlight == TW_HL_DOWN) /* clicked scroller-button down */
            txtwin[actWin].scroll--;
        else if (event.motion.x<cur_widget[wID].x1+246
                 && event.motion.y>cur_widget[wID].y1 + 2
                 && event.motion.y < cur_widget[wID].y1 + 7
                 && cursor_type == 1)
            textwin_flags |= (actWin | TW_RESIZE);      /* size-change */
        else if (event.motion.x < cur_widget[wID].x1 + 250)
            say_clickedKeyword(actWin, event.motion.x, event.motion.y);
    }
}

/******************************************************************
 mouse-move event in the textwindow.
*****************************************************************/
int textwin_move_event(int actWin, SDL_Event event)
{
    int wID = 0;

	map_udate_flag = 1;

    txtwin[actWin].highlight = TW_HL_NONE;

    if (actWin==TW_CHAT)
        wID=CHATWIN_ID;
    else if (actWin==TW_MSG)
        wID=MSGWIN_ID;

    WIDGET_REDRAW(wID);

    /* show resize-cursor */
    if ((event.motion.y > cur_widget[wID].y1 + 2 && event.motion.y < cur_widget[wID].y1 + 7 && (event.motion.x < cur_widget[wID].x1+246))
            || (event.button.button == SDL_BUTTON_LEFT && (textwin_flags & (TW_SCROLL | TW_RESIZE))))
    {
        if (!(textwin_flags & TW_SCROLL) && event.motion.x > cur_widget[wID].x1)
            cursor_type = 1;
    }
    else
    {
        cursor_type = 0;
        textwin_flags &= ~(TW_ACTWIN | TW_SCROLL | TW_RESIZE);
    }

    /* mouse out of window */
    /* we have to leave this here!!! for sanity, also the widgetstuff does some area checking */
    if (event.motion.y < cur_widget[wID].y1
            || event.motion.x > cur_widget[wID].x1 + Bitmaps[BITMAP_TEXTWIN]->bitmap->w
            || event.motion.y > cur_widget[wID].y1 + txtwin[actWin].size * 10 + 13 + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h)
    {
        if (!(textwin_flags & TW_RESIZE))
            return 1;
    }

    /* highlighting */
    if (event.motion.x > cur_widget[wID].x1 + 250
            && event.motion.y > cur_widget[wID].y1
            && event.button.button != SDL_BUTTON_LEFT
            && event.motion.x < cur_widget[wID].x1 + Bitmaps[BITMAP_TEXTWIN]->bitmap->w)
    {
#define OFFSET (txtwin[actWin].y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h)
        if (event.motion.y < OFFSET)
            txtwin[actWin].highlight = TW_HL_UP;
        else if (event.motion.y < OFFSET + txtwin[actWin].slider_y)
            txtwin[actWin].highlight = TW_ABOVE;
        else if (event.motion.y < OFFSET + txtwin[actWin].slider_y + txtwin[actWin].slider_h + 3)
            txtwin[actWin].highlight = TW_HL_SLIDER;
        else if (event.motion.y < cur_widget[wID].y1 + txtwin[actWin].size * 10 + 4)
            txtwin[actWin].highlight = TW_UNDER;
        else if (event.motion.y < cur_widget[wID].y1 + txtwin[actWin].size * 10 + 13)
            txtwin[actWin].highlight = TW_HL_DOWN;
#undef OFFSET
        return 0;
    }

    /* slider scrolling */
    if (textwin_flags & TW_SCROLL)
    {
        actWin = textwin_flags & TW_ACTWIN;
        txtwin[actWin].slider_y = event.motion.y - old_slider_pos;
        txtwin[actWin].scroll = txtwin[actWin].act_bufsize
                                - (txtwin[actWin].size + 1)
                                - (txtwin[actWin].act_bufsize * txtwin[actWin].slider_y)
                                / (txtwin[actWin].size * 10 - 1);
        WIDGET_REDRAW(wID);
        return 0;
    }

    /* resizing */
    if (textwin_flags & TW_RESIZE)
    {
        actWin = textwin_flags & TW_ACTWIN;
        if (actWin == TW_CHAT)
        {
            int newsize;
            newsize = ((cur_widget[CHATWIN_ID].y1+cur_widget[CHATWIN_ID].ht)-event.motion.y) / 10;
            if (newsize < 3)
                newsize = 3;
            /* we need to calc thenew x for the widget, and set the new size */
            cur_widget[CHATWIN_ID].y1+=(txtwin[actWin].size-newsize)*10;
            cur_widget[CHATWIN_ID].ht=newsize*10+13;
            txtwin[actWin].size=newsize;
        }
        else if (actWin == TW_MSG)
        {
            int newsize;
            newsize = ((cur_widget[MSGWIN_ID].y1+cur_widget[MSGWIN_ID].ht)-event.motion.y) / 10;
            if (newsize < 9)
                newsize = 9;
            /* we need to calc thenew x for the widget, and set the new size */
            cur_widget[MSGWIN_ID].y1+=(txtwin[actWin].size-newsize)*10;
            cur_widget[MSGWIN_ID].ht=newsize*10+13;
            txtwin[actWin].size=newsize;
        }
        else
        {
            int newsize;
            newsize = ((cur_widget[MIXWIN_ID].y1+cur_widget[MIXWIN_ID].ht)-event.motion.y) / 10;
            if (newsize < 9)
                newsize = 9;
            /* we need to calc thenew x for the widget, and set the new size */
            cur_widget[MIXWIN_ID].y1+=(txtwin[actWin].size-newsize)*10;
            cur_widget[MIXWIN_ID].ht=newsize*10+13;
            txtwin[actWin].size=newsize;
        }
    }
    return 0;
}

/******************************************************************
 textwin-events.
*****************************************************************/
void textwin_event(int e, SDL_Event *event, int WidgetID)
{
    if (e == TW_CHECK_BUT_DOWN)
    {
        switch (WidgetID)
        {
            case CHATWIN_ID:
                textwin_button_event(TW_CHAT, *event);
                break;
            case MSGWIN_ID:
                textwin_button_event(TW_MSG, *event);
                break;
            case MIXWIN_ID:
                textwin_button_event(TW_MIX, *event);
                break;
        }
    }
    else
    {
        switch (WidgetID)
        {
            case CHATWIN_ID:
                textwin_move_event(TW_CHAT, *event);
                break;
            case MSGWIN_ID:
                textwin_move_event(TW_MSG, *event);
                break;
            case MIXWIN_ID:
                textwin_move_event(TW_MIX, *event);
                break;
        }
    }
}

void textwin_addhistory(char* text)
{
    register int i;

    /* If new line is empty identical to last inserted one, skip it */
    if (!text[0] || strcmp(InputHistory[0], text)==0) return;

    for (i = MAX_HISTORY_LINES-1; i > 1; i--) /* shift history lines */
    {
        strncpy(InputHistory[i], InputHistory[i-1], MAX_INPUT_STRING);
    }

    strncpy(InputHistory[1], text, MAX_INPUT_STRING); /* insert new one */
    *InputHistory[0]=0; /* clear tmp editing line */
    HistoryPos = 0;
}

void textwin_clearhistory()
{
    register int i;
    for (i=0; i<MAX_HISTORY_LINES; i++)
    {
        InputHistory[i][0]=0; /* it's enough to clear only the first byte of each history line */
    }
    HistoryPos = 0;
}

void textwin_putstring(char* text)
{
int len;

      len=strlen(text);
      strncpy(InputString,text,MAX_INPUT_STRING); /* copy buf to input buffer */
      CurrentCursorPos=InputCount=len;           /* set cursor after inserted text */
}
