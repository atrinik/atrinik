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
static char *get_keyword_start(int actWin, int mouseX, int *row){
    int pos, pos2=538, key_start;
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
    pos =0; pos2 = 538;
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
    char cmdBuf[MAX_KEYWORD_LEN+1] ={"/say "};
    char *text;
    int clicked_row, pos = 5;

  clicked_row = (mouseY-588)/10+ txtwin[actWin].size;
    text = get_keyword_start(actWin, mouseX, &clicked_row);
    if (text == NULL)
    return;
    while (*text && *text != '^')
        cmdBuf[pos++] = *text++;
    if (*text != '^')
    {
    text = get_keyword_start(actWin, -1, &clicked_row);
        if (text != NULL)
            while (*text && *text != '^')
                cmdBuf[pos++] = *text++;
    }
    cmdBuf[pos++] ='\0';
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
        txtwin[i].bot_drawLine =0;
        txtwin[i].act_bufsize =0;
        txtwin[i].scroll =0;
        txtwin[i].size = 9;
    }
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
      for (z =0; z < 2; z++)
      { /* add messages to mixed-textwin and either to msg OR chat-textwin */
        strcpy(txtwin[actWin].text[ txtwin[actWin].bot_drawLine % TEXT_WIN_MAX ].buf, buf);
        txtwin[actWin].text[txtwin[actWin].bot_drawLine%TEXT_WIN_MAX].color = color;
        txtwin[actWin].text[txtwin[actWin].bot_drawLine%TEXT_WIN_MAX].flags = mode;
        txtwin[actWin].text[txtwin[actWin].bot_drawLine%TEXT_WIN_MAX].key_clipped = key_start;
        if(txtwin[actWin].scroll)
          txtwin[actWin].scroll++;
        if(txtwin[actWin].act_bufsize < TEXT_WIN_MAX)
          txtwin[actWin].act_bufsize++;
        txtwin[actWin].bot_drawLine++;
        txtwin[actWin].bot_drawLine %= TEXT_WIN_MAX;
        actWin++; /* next window => MSG_WIN */
            if(mode & NDI_PLAYER) actWin++; /* next window => MSG_CHAT */
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
static void show_window(int actWin, int x, int y)
{
  int i, temp;

    txtwin[actWin].x = x;
  txtwin[actWin].y = y;
    txtwin[actWin].top_drawLine = txtwin[actWin].bot_drawLine - (txtwin[actWin].size +1);
    if(txtwin[actWin].top_drawLine <0 && txtwin[actWin].act_bufsize == TEXT_WIN_MAX)
        txtwin[actWin].top_drawLine+=TEXT_WIN_MAX;
    else if(txtwin[actWin].top_drawLine <0)
        txtwin[actWin].top_drawLine = 0;
    if(txtwin[actWin].scroll > txtwin[actWin].act_bufsize - (txtwin[actWin].size +1))
        txtwin[actWin].scroll= txtwin[actWin].act_bufsize - (txtwin[actWin].size +1);
    if(txtwin[actWin].scroll <0)
        txtwin[actWin].scroll=0;

    for(i=0;i<=txtwin[actWin].size && i < txtwin[actWin].act_bufsize;i++)
    {
        temp = (txtwin[actWin].top_drawLine+i)%TEXT_WIN_MAX;
        if(txtwin[actWin].act_bufsize > txtwin[actWin].size)
        {
            temp-=txtwin[actWin].scroll;
            if(temp <0)
                temp=TEXT_WIN_MAX+temp;
        }
        StringBlt(ScreenSurface, &SystemFont, &txtwin[actWin].text[temp].buf[0], x+2,
        (y+1+i*10)| txtwin[actWin].text[temp].key_clipped,
      txtwin[actWin].text[temp].color, NULL, NULL);
    }

  /* only draw scrollbar if needed */
  if (txtwin[actWin].act_bufsize > txtwin[actWin].size)
  {
    SDL_Rect box;

    box.x = box.y = 0;
    box.w = Bitmaps[BITMAP_SLIDER]->bitmap->w;
    box.h = txtwin[actWin].size*10+1;
    if (actWin == TW_CHAT)
            temp = -9; /* no textinput-line */
    else
        temp = 0;
    sprite_blt(Bitmaps[BITMAP_SLIDER_UP  ],x+250, y+2, NULL, NULL);
    sprite_blt(Bitmaps[BITMAP_SLIDER_DOWN],x+250, y+13+temp+txtwin[actWin].size*10, NULL,NULL);
    sprite_blt(Bitmaps[BITMAP_SLIDER     ],x+250, y+Bitmaps[BITMAP_SLIDER_UP]->bitmap->h+2+temp, &box,NULL);
    box.h+= temp-2;
    box.w-=2;

        txtwin[actWin].slider_y=
        ((txtwin[actWin].act_bufsize-(txtwin[actWin].size +1)-txtwin[actWin].scroll)* box.h) /txtwin[actWin].act_bufsize;
        txtwin[actWin].slider_h= (box.h*(txtwin[actWin].size+1))/ txtwin[actWin].act_bufsize; /* between 0.0 <-> 1.0 */
        if(txtwin[actWin].slider_h < 1)
            txtwin[actWin].slider_h= 1;

        if (!txtwin[actWin].scroll && txtwin[actWin].slider_y + txtwin[actWin].slider_h < box.h)
            txtwin[actWin].slider_y++;

        box.h = txtwin[actWin].slider_h;
        sprite_blt(Bitmaps[BITMAP_TWIN_SCROLL],x+252,
        y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h+3+txtwin[actWin].slider_y, &box, NULL);

        if (txtwin[actWin].highlight == TW_HL_UP){
            box.x = x+250;
            box.y = y+2;
            box.h = Bitmaps[BITMAP_SLIDER_UP]->bitmap->h;
            box.w = 1;
        SDL_FillRect(ScreenSurface, &box, -1);
            box.x+= Bitmaps[BITMAP_SLIDER_UP]->bitmap->w-1;
        SDL_FillRect(ScreenSurface, &box, -1);
            box.w = Bitmaps[BITMAP_SLIDER_UP]->bitmap->w-1;
            box.h = 1;
            box.x = x+250;
        SDL_FillRect(ScreenSurface, &box, -1);
            box.y+= Bitmaps[BITMAP_SLIDER_UP]->bitmap->h-1;
            SDL_FillRect(ScreenSurface, &box, -1);
        }
        else if (txtwin[actWin].highlight == TW_ABOVE){
            box.x = x+252;
            box.y = y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h+2;
            box.h = txtwin[actWin].slider_y + 1;
            box.w = 5;
        SDL_FillRect(ScreenSurface, &box, 0);
        }
        else if (txtwin[actWin].highlight == TW_HL_SLIDER){
            box.x = x+252;
            box.y = y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h+3+txtwin[actWin].slider_y;
            box.w = 1;
        SDL_FillRect(ScreenSurface, &box, -1);
            box.x+= 4;
        SDL_FillRect(ScreenSurface, &box, -1);
            box.x-= 4;
            box.h = 1;
            box.w = 4;
        SDL_FillRect(ScreenSurface, &box, -1);
            box.y+= txtwin[actWin].slider_h-1;
        SDL_FillRect(ScreenSurface, &box, -1);
    }
        else if (txtwin[actWin].highlight == TW_UNDER){
            box.x = x+252;
            box.y = y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h+3+txtwin[actWin].slider_y + box.h;
            box.h = txtwin[actWin].size*10-txtwin[actWin].slider_y-txtwin[actWin].slider_h;
            if (actWin==TW_CHAT) box.h-=10;
            box.w = 5;
        SDL_FillRect(ScreenSurface, &box, 0);
        }
        else if (txtwin[actWin].highlight == TW_HL_DOWN){
            box.x = x+250;
            box.y = y+txtwin[actWin].size*10+(actWin== TW_CHAT?4:13);
            box.h = Bitmaps[BITMAP_SLIDER_UP]->bitmap->h;
            box.w = 1;
        SDL_FillRect(ScreenSurface, &box, -1);
            box.x+= Bitmaps[BITMAP_SLIDER_UP]->bitmap->w-1;
        SDL_FillRect(ScreenSurface, &box, -1);
            box.w = Bitmaps[BITMAP_SLIDER_UP]->bitmap->w-1;
            box.h = 1;
            box.x = x+250;
        SDL_FillRect(ScreenSurface, &box, -1);
            box.y+= Bitmaps[BITMAP_SLIDER_UP]->bitmap->h-1;
            SDL_FillRect(ScreenSurface, &box, -1);
        }
  }
  else
    txtwin[actWin].slider_h = 0;
}

/******************************************************************
 display all text-windows.
******************************************************************/
void textwin_show(int x, int y)
{
    int len, tmp;
    Uint32 color =-1;
    SDL_Rect box;
    _BLTFX bltfx;
    bltfx.alpha= options.textwin_alpha;
    bltfx.flags= BLTFX_FLAG_SRCALPHA;
    box.x = box.y = 0;
    box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
    sprite_blt(Bitmaps[BITMAP_TEXTWIN_BLANK],x-1, y+2, NULL, NULL);
    y=599; /* to lazy to work with correct calcs */

    if (options.use_TextwinSplit)
    {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        box.h = len =(txtwin[TW_MSG].size+txtwin[TW_CHAT].size)*5+18;
        y-=len*2;
        if (options.use_TextwinAlpha){
            sprite_blt(Bitmaps[BITMAP_TEXTWIN_MASK],x, y,     &box, &bltfx);
            sprite_blt(Bitmaps[BITMAP_TEXTWIN_MASK],x, y+len, &box, &bltfx);
        }else{
            sprite_blt(Bitmaps[BITMAP_TEXTWIN],x, y,     &box,NULL);
            sprite_blt(Bitmaps[BITMAP_TEXTWIN],x, y+len, &box, NULL);
        }
        show_window(TW_CHAT, x, y-2);
        tmp = txtwin[TW_CHAT].size*10+12;
        show_window(TW_MSG, x, y+tmp);
        box.x = x;
        box.h =1;
        box.y = y+tmp+1;
        if ((SDL_GetModState() == KMOD_RALT || SDL_GetModState() == KMOD_LALT)
     && my > txtwin[TW_CHAT].y && mx > txtwin[TW_CHAT].x)
            color = 40;
        SDL_FillRect(ScreenSurface, &box, sdl_dgreen);
        box.y = y-1;
/*      SDL_FillRect(ScreenSurface, &box, color);*/
    }
    else
    {
        box.h =len= txtwin[TW_MIX].size*10 +23;
        y-=len;
        if (options.use_TextwinAlpha){
            sprite_blt(Bitmaps[BITMAP_TEXTWIN_MASK],x, y, &box, &bltfx);
        }
        else
            sprite_blt(Bitmaps[BITMAP_TEXTWIN],x, y, &box,NULL);
        show_window(TW_MIX, x, y-1);
    }
    StringBlt(ScreenSurface, &SystemFont, ">", x+2, 586, 1,NULL, NULL);
}

/******************************************************************
 mouse-button event in the textwindow.
*****************************************************************/
void textwin_button_event(int actWin, SDL_Event event)
{
  if (event.motion.x < txtwin[actWin].x
  || textwin_flags & (TW_SCROLL | TW_RESIZE)) /* scrolling || resising */
        return;

    if (event.button.button == 4) /* mousewheel up */
        txtwin[actWin].scroll++;
  else if (event.button.button == 5) /* mousewheel down */
        txtwin[actWin].scroll--;
  else if (event.button.button == SDL_BUTTON_LEFT )
  {
        if (txtwin[actWin].highlight ==TW_HL_UP) /* clicked scroller-button up */
            txtwin[actWin].scroll++;
        else if (txtwin[actWin].highlight ==TW_ABOVE) /* clicked above the slider */
            txtwin[actWin].scroll+= txtwin[actWin].size;
        else if (txtwin[actWin].highlight ==TW_HL_SLIDER){ /* clicked on the slider */
            textwin_flags |= (actWin | TW_SCROLL);
            old_slider_pos = event.motion.y -  txtwin[actWin].slider_y;
        }
        else if (txtwin[actWin].highlight ==TW_UNDER) /* clicked under the slider */
            txtwin[actWin].scroll-= txtwin[actWin].size;
        else if (txtwin[actWin].highlight ==TW_HL_DOWN) /* clicked scroller-button down */
        txtwin[actWin].scroll--;
        else if (event.motion.x< 785 && event.motion.y >txtwin[actWin].y+2 && event.motion.y< txtwin[actWin].y+7 && cursor_type ==1)
            textwin_flags |= (actWin | TW_RESIZE);      /* size-change */
        else if (event.motion.x < txtwin[actWin].x +250)
            say_clickedKeyword(actWin, event.motion.x, event.motion.y);
    }
}

/******************************************************************
 mouse-move event in the textwindow.
*****************************************************************/
int textwin_move_event(int actWin, SDL_Event event)
{

    txtwin[actWin].highlight = TW_HL_NONE;

    /* show resize-cursor */
    if ((event.motion.y > txtwin[actWin].y+2 && event.motion.y < txtwin[actWin].y+7 && event.motion.x < 785 )
  ||  (event.button.button == SDL_BUTTON_LEFT && (textwin_flags & (TW_SCROLL |TW_RESIZE))))
  {
        if (!(textwin_flags & TW_SCROLL) && event.motion.x > txtwin[actWin].x ) cursor_type = 1;
    }
    else{
        cursor_type = 0;
        textwin_flags &= ~(TW_ACTWIN | TW_SCROLL |TW_RESIZE);
    }

    /* mouse out of window */
  if (event.motion.y < txtwin[actWin].y || event.motion.x > txtwin[actWin].x + Bitmaps[BITMAP_TEXTWIN]->bitmap->w
    || event.motion.y > txtwin[actWin].y + txtwin[actWin].size *10 +(actWin== TW_CHAT?4:13)+ Bitmaps[BITMAP_SLIDER_UP]->bitmap->h)
    {
        if (!(textwin_flags & TW_RESIZE))   return 1;
    }

    /* highlighting */
    if (event.motion.x > txtwin[actWin].x +250 && event.motion.y > txtwin[actWin].y && event.button.button != SDL_BUTTON_LEFT)
    {
        #define OFFSET (txtwin[actWin].y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h)
        if (event.motion.y < OFFSET)
            txtwin[actWin].highlight = TW_HL_UP;
        else if (event.motion.y < OFFSET + txtwin[actWin].slider_y)
            txtwin[actWin].highlight = TW_ABOVE;
        else if (event.motion.y < OFFSET + txtwin[actWin].slider_y + txtwin[actWin].slider_h +3)
            txtwin[actWin].highlight = TW_HL_SLIDER;
        else if (event.motion.y < txtwin[actWin].y + txtwin[actWin].size*10 +(actWin== TW_CHAT?4:14))
            txtwin[actWin].highlight = TW_UNDER;
        else if (event.motion.y < txtwin[actWin].y + txtwin[actWin].size*10 +(actWin== TW_CHAT?13:23))
        txtwin[actWin].highlight = TW_HL_DOWN;
        #undef OFFSET
        return 0;
    }

    /* slider scrolling */
    if (textwin_flags & TW_SCROLL){
        actWin = textwin_flags & TW_ACTWIN;
        txtwin[actWin].slider_y = event.motion.y - old_slider_pos;
        txtwin[actWin].scroll = txtwin[actWin].act_bufsize- (txtwin[actWin].size+1) -
            (txtwin[actWin].act_bufsize * txtwin[actWin].slider_y) / (txtwin[actWin].size*10-1);
        return 0;
    }

    /* resizing */
    if (textwin_flags & TW_RESIZE){
        actWin = textwin_flags & TW_ACTWIN;
        if (actWin == TW_CHAT){
            txtwin[actWin].size = (570 - event.motion.y)/10- txtwin[TW_MSG].size;
            if (txtwin[TW_CHAT].size <  3) txtwin[TW_CHAT].size = 3;
        }else{
            txtwin[actWin].size = (580 - event.motion.y)/10;
            if (txtwin[actWin].size <  9) txtwin[actWin].size = 9;
        }
        if (txtwin[actWin].size > 36) txtwin[actWin].size =36;
        if (txtwin[TW_CHAT].size + txtwin[TW_MSG].size > 56)
                txtwin[TW_CHAT].size = 56 - txtwin[TW_MSG].size;
  }
    return 0;
}

/******************************************************************
 textwin-events.
*****************************************************************/
void textwin_event(int e, SDL_Event *event)
{
    if (e == TW_CHECK_BUT_DOWN){
        if (options.use_TextwinSplit){
            if ((*event).motion.y > txtwin[TW_CHAT].y){
                if ((*event).motion.y > txtwin[TW_MSG].y)
                    textwin_button_event(TW_MSG,  *event);
                else
                    textwin_button_event(TW_CHAT, *event);
            }
        }
        else
            textwin_button_event(TW_MIX, *event);
    }else{
        if (options.use_TextwinSplit){
        if (textwin_move_event(TW_CHAT, *event))
            textwin_move_event(TW_MSG, *event);
        }
        else
            textwin_move_event(TW_MIX, *event);
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
