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

_media_file media_file[MEDIA_MAX];

/* keybind menu */
int keybind_status;

struct _spell_list spell_list[SPELL_LIST_MAX]; /* skill list entries */
struct _skill_list skill_list[SKILL_LIST_MAX]; /* skill list entries */

struct _dialog_list_set spell_list_set;
struct _dialog_list_set skill_list_set;
struct _dialog_list_set option_list_set;
struct _dialog_list_set bindkey_list_set;
struct _dialog_list_set create_list_set;

int media_count;        /* buffered media files*/
int media_show; /* show this media file*/
int media_show_update;
int keybind_startoff=0;
static char *get_range_item_name(int id);

_quickslot quick_slots[MAX_QUICK_SLOTS];
int quickslots_pos[MAX_QUICK_SLOTS][2] = {
	{17,1},
	{50,1},
	{83,1},
	{116,1},
	{149,1},
	{182,1},
	{215,1},
	{248,1}
};

void do_console(int x, int y)
{
	/* If ESC was pressed or console_party() returned 1, close console. */
    if (InputStringEscFlag || console_party())
    {
		if (gui_interface_party)
			clear_party_interface();

        sound_play_effect(SOUND_CONSOLE, 0, 0, 100);
        reset_keys();
        cpl.input_mode = INPUT_MODE_NO;
        map_udate_flag = 2;
    }

    /* If set, we've got a finished input */
    if (InputStringFlag == 0 && InputStringEndFlag)
    {
        sound_play_effect(SOUND_CONSOLE, 0, 0, 100);

        if (InputString[0])
        {
			char buf[MAX_INPUT_STRING + 32];

#if 0
			sprintf(buf, ":%s", InputString);
			draw_info(buf, COLOR_DGOLD);
#endif

			/* If it's not command, it's say */
			if (*InputString != '/')
			{
				sprintf(buf, "/say %s", InputString);
			}
			else
			{
				strcpy(buf, InputString);
			}

			if (!client_command_check(InputString))
            	send_command(buf, -1, SC_NORMAL);
        }

        reset_keys();
        cpl.input_mode = INPUT_MODE_NO;
        map_udate_flag = 2;
    }
    else
        show_console(x, y);
}

/* client_command_check()
 * Analyze /<cmd> type commands the player has typed in the console
 * or bound to a key. Sort out the "client intern" commands and
 * expand or pre process them for the server.
 * Return: TRUE=don't send command to server
 * FALSE: send command to server
 */

int client_command_check(char *cmd)
{
	char tmp[256];
	char cpar1[256];
	int par1, par2;

	if(!strnicmp(cmd,"/ready_spell",strlen("/ready_spell")) )
	{
        cmd = strchr(cmd, ' ');
		if(!cmd || *++cmd == 0)
		{
			draw_info("usage: /ready_spell <spell name>" ,COLOR_GREEN);
		}
		else
		{
			int i,ii;

			for(i=0;i<SPELL_LIST_MAX;i++)
			{
				for(ii=0;ii<DIALOG_LIST_ENTRY;ii++)
				{
					if(spell_list[i].entry[0][ii].flag >= LIST_ENTRY_USED)
					{
						if(!strcmp(spell_list[i].entry[0][ii].name, cmd))
						{
							if(spell_list[i].entry[0][ii].flag==LIST_ENTRY_KNOWN)
							{
								fire_mode_tab[FIRE_MODE_SPELL].spell = &spell_list[i].entry[0][ii];
								RangeFireMode = FIRE_MODE_SPELL;
								sound_play_effect(SOUND_SCROLL,0,0,MENU_SOUND_VOL);
								draw_info("spell ready." ,COLOR_GREEN);
								return TRUE;
							}
						}
					}
					if(spell_list[i].entry[1][ii].flag >= LIST_ENTRY_USED)
					{
						if(!strcmp(spell_list[i].entry[1][ii].name, cmd))
						{
							if(spell_list[i].entry[1][ii].flag==LIST_ENTRY_KNOWN)
							{
								fire_mode_tab[FIRE_MODE_SPELL].spell = &spell_list[i].entry[1][ii];
								RangeFireMode = FIRE_MODE_SPELL;
								sound_play_effect(SOUND_SCROLL,0,0,MENU_SOUND_VOL);
								draw_info("spell ready." ,COLOR_GREEN);
								return TRUE;
							}
						}
					}
				}
			}
		}
		draw_info("unknown spell." ,COLOR_GREEN);
		return TRUE;
	}
	else if(!strnicmp(cmd,"/pray",strlen("/pray")) )
	{
		/* give out "you are at full grace." when needed -
		 * server will not send us anything when this happens
		 */
		if(cpl.stats.grace == cpl.stats.maxgrace)
			draw_info("You are at full grace. You stop praying." ,COLOR_WHITE);
	}
	else if(!strnicmp(cmd,"/setwinalpha",strlen("/setwinalpha")) )
	{
		int wrong=0;
		par2=-1;
		sscanf(cmd, "%s %s %d", tmp, cpar1, &par2);

		if(!strnicmp(cpar1,"ON",strlen("ON")) )
		{
			if(par2 != -1)
			{
				if(par2 <0 ||par2>255)
					par2=128;
				options.textwin_alpha= par2;
			}
			options.use_TextwinAlpha	=1;
			sprintf(tmp, ">>set textwin alpha ON (alpha=%d).",options.textwin_alpha);
			draw_info(tmp ,COLOR_GREEN);
		}
		else if(!strnicmp(cpar1,"OFF",strlen("OFF")) )
		{
			draw_info(">>set textwin alpha mode OFF." ,COLOR_GREEN);
			options.use_TextwinAlpha=0;
		}
		else
			wrong = 1;


		if(wrong)
			draw_info("Usage: '/setwinalpha on|off |<alpha>|'\nExample:\n/setwinalpha ON 172\n/setwinalpha OFF" ,COLOR_WHITE);
		return TRUE;
	}
	else if(!strnicmp(cmd,"/setwin",strlen("/setwin")) )
	{
		int wrong = 0;
		par1=9, par2=-1;
		sscanf(cmd, "%s %d %d", tmp, &par1, &par2);
		if(par2 != -1) /* split mode */
		{
			if(par1<2 || par1>38 || par2<2 || par2>38 || (par1+par2)<10 || (par1+par2)>38)
			{
				wrong = 1;
				draw_info("/setwin: parameters out of bounds." ,COLOR_RED);
			}
			else
			{
				sprintf(tmp, ">>set textwin to split mode %d+%d rows.",par1,par2);
				draw_info(tmp ,COLOR_GREEN);

				options.use_TextwinSplit=1;
				txtwin[TW_MSG ].size =par1-1;
				txtwin[TW_CHAT].size =par2-1;
			}
		}
		else
		{
			if(par1<2 || par1>38)
			{
				wrong = 1;
				draw_info("/setwin: parameters out of bounds." ,COLOR_RED);
			}
			else
			{
				sprintf(tmp, ">>set textwin to %d rows.",par1);
				draw_info(tmp ,COLOR_GREEN);
				options.use_TextwinSplit =0;
				txtwin[TW_MIX].size = par1-1;
			}
		}

		if(wrong)
			draw_info("Usage: '/setwin <body> |<top>|'\nExample:\n/setwin 9 5\n/setwin 12" ,COLOR_WHITE);
		return TRUE;
	}
	else if(!strnicmp(cmd,"/keybind",strlen("/keybind")) )
	{
		map_udate_flag=2;
		if(cpl.menustatus != MENU_KEYBIND)
		{
			keybind_status = KEYBIND_STATUS_NO;
			cpl.menustatus = MENU_KEYBIND;
		}
		else
		{
			save_keybind_file(KEYBIND_FILE);
			cpl.menustatus = MENU_NO;
		}
		sound_play_effect(SOUND_SCROLL,0,0,100);
		reset_keys();
		return TRUE;
	}
	else if(!strncmp(cmd,"/target",strlen("/target")) )
	{
		/* logic is: if first parameter char is a digit, is enemy,friend or self.
		 * if first char a character - then its a name of a living object.
		 */
		if(!strncmp(cmd,"/target friend",strlen("/target friend")) )
			strcpy(cmd,"/target 1");
		else if(!strncmp(cmd,"/target enemy",strlen("/target enemy")) )
			strcpy(cmd,"/target 0");
		else if(!strncmp(cmd,"/target self",strlen("/target self")) )
			strcpy(cmd,"/target 2");
	}
	else if (!strncmp(cmd, "/help", 5))
	{
		cmd += 5;

		if (cmd == NULL || strcmp(cmd, "") == 0)
			show_help("main");
		else
			show_help(cmd + 1);

		return 1;
	}
	else if (!strncmp(cmd, "/party ", 7))
	{
		cmd += 7;

		if (!strncmp(cmd, "join ", 5))
		{
			cmd += 5;

			sprintf(tmp, "pt join %s", cmd);
			cs_write_string(csocket.fd, tmp, strlen(tmp));

			return 1;
		}
		else if (!strncmp(cmd, "leave", 5))
		{
			strcpy(cpl.partyname, "");
		}
		else if (!strncmp(cmd, "form ", 5))
		{
			cmd += 5;

			sprintf(tmp, "pt form %s", cmd);
			cs_write_string(csocket.fd, tmp, strlen(tmp));

			return 1;
		}
		else if (!strncmp(cmd, "password ", 9))
		{
			cmd += 9;

			sprintf(tmp, "pt password %s", cmd);
			cs_write_string(csocket.fd, tmp, strlen(tmp));

			return 1;
		}
		else if (!strncmp(cmd, "who", 3))
		{
			cs_write_string(csocket.fd, "pt who", 6);

			return 1;
		}
		else if (!strncmp(cmd, "list", 4))
		{
			cs_write_string(csocket.fd, "pt list", 7);

			return 1;
		}
	}

	return 0;
}

void show_help(char *helpfile)
{
	int mode, len, got_match = 0;
	unsigned char *data;
	char title[MAX_BUF], message[HUGE_BUF * 32] = "", buf[HUGE_BUF * 4], name[MAX_BUF], message_buf[HUGE_BUF * 32] = "";
	FILE *fp;

	if ((fp = fopen(FILE_CLIENT_HFILES, "rb")) == NULL)
		return;

	sprintf(name, "Name: %s\n", helpfile);

	while (fgets(buf, HUGE_BUF * 4, fp))
	{
		/* We got a match */
		if (got_match)
		{
			/* That's a stop */
			if (strcmp(buf, "==========\n") == 0)
				break;

			if (strncmp(buf, "Title: ", 7) == 0)
			{
				sscanf(buf, "Title: %32[^\n]\n", title);
			}
			else
			{
				sprintf(message, "%s%s", message, buf);
			}
		}

		if (strcmp(buf, name) == 0)
			got_match = 1;
	}

	fclose(fp);

	if (got_match)
		snprintf(message_buf, sizeof(message_buf), "<b t=\"%s\"><t t=\"%s\">\n%s", helpfile, title, message);
	else
		sprintf(message_buf, "<b t=\"Help not found\"><t t=\"The specified help file could not be found.\">\n");

	data = (uint8*)message_buf;

	len = strlen(data);

	cpl.menustatus = MENU_BOOK;

	mode = *((int*) data);

	gui_interface_book = load_book_interface(mode, data, len + 3);
}

void show_console(int x, int y)
{
/*        sprite_blt(Bitmaps[BITMAP_CONSOLE],x, y, NULL, NULL);*/
        StringBlt(ScreenSurface, &SystemFont,
                show_input_string(InputString,&SystemFont,239)
                , x, y, COLOR_WHITE, NULL, NULL);
}

void do_number(int x, int y)
{
    if(InputStringEscFlag==TRUE)
    {
        reset_keys();
        cpl.input_mode = INPUT_MODE_NO;
        map_udate_flag=2;
    }
    /* if set, we got a finished input!*/
    if(InputStringFlag==FALSE
        &&InputStringEndFlag==TRUE)
    {
        if(InputString[0])
        {
            int tmp;
            char buf[300];
            tmp = atoi(InputString);
            if(tmp>0 && tmp <= cpl.nrof)
            {
                client_send_move (cpl.loc, cpl.tag, tmp);
                sprintf(buf,"%s %d from %d %s", cpl.nummode == NUM_MODE_GET?"get":"drop", tmp, cpl.nrof, cpl.num_text);
				if(cpl.nummode == NUM_MODE_GET)
					sound_play_effect(SOUND_GET,0,0,100);
				else
					sound_play_effect(SOUND_DROP,0,0,100);

                draw_info(buf,COLOR_DGOLD);

            }
        }
        reset_keys();
        cpl.input_mode = INPUT_MODE_NO;
        map_udate_flag=2;
    }
    else
        show_number(x,y);
}

void do_keybind_input(void)
{
	if(InputStringEscFlag==TRUE)
	{
		reset_keys();
		sound_play_effect(SOUND_CLICKFAIL,0,0,100);
		cpl.input_mode = INPUT_MODE_NO;
		keybind_status = KEYBIND_STATUS_NO;
		map_udate_flag=2;
	}
	/* if set, we got a finished input!*/
	if(InputStringFlag==FALSE && InputStringEndFlag==TRUE)
	{
		if(InputString[0]){
			strcpy(bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].text, InputString);
			keybind_status = KEYBIND_STATUS_EDITKEY; /* now get the key code */
		}else{ /* cleared string - delete entry */
			bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].text[0]=0;
			bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].keyname[0]=0;
			bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].key=0;
			keybind_status = KEYBIND_STATUS_NO;
		}
		reset_keys();
		cpl.input_mode = INPUT_MODE_NO;
		map_udate_flag=2;
	}
}


void show_number(int x, int y)
{
	SDL_Rect tmp;
	char buf[512];

	tmp.w = 238;

    sprite_blt(Bitmaps[BITMAP_NUMBER],x, y, NULL, NULL);
    sprintf(buf,"%s how much from %d %s", cpl.nummode == NUM_MODE_GET?"get":"drop", cpl.nrof, cpl.num_text);
    StringBlt(ScreenSurface, &SystemFont, buf, x+8, y+6, COLOR_HGOLD, &tmp, NULL);
    StringBlt(ScreenSurface, &SystemFont,
			show_input_string(InputString,&SystemFont,Bitmaps[BITMAP_NUMBER]->bitmap->w-22),
			x+8, y+25, COLOR_WHITE, &tmp, NULL);
}

void show_resist(int x, int y)
{
    char buf[62];

    StringBlt(ScreenSurface, &Font6x3Out,"Armour Protection Table",x, y+1, COLOR_HGOLD, NULL, NULL);

    StringBlt(ScreenSurface, &Font6x3Out,"Physical",x, y+16, COLOR_HGOLD, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"I",x+43, y+17, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[0]);

    StringBlt(ScreenSurface, &SystemFont,buf,x+53, y+17, cpl.stats.protection[0]?COLOR_WHITE:COLOR_GREY, NULL, NULL);

    StringBlt(ScreenSurface, &SystemFont,"S",x+73, y+17, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[1]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+83, y+17, cpl.stats.protection[1]?COLOR_WHITE:COLOR_GREY, NULL, NULL);


    StringBlt(ScreenSurface, &SystemFont,"C",x+103, y+17, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[2]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+113, y+17, cpl.stats.protection[2]?COLOR_WHITE:COLOR_GREY, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"P",x+133, y+17, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[3]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+143, y+17, cpl.stats.protection[3]?COLOR_WHITE:COLOR_GREY, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"W",x+163, y+17, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[4]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+173, y+17, cpl.stats.protection[4]?COLOR_WHITE:COLOR_GREY, NULL, NULL);

    StringBlt(ScreenSurface, &Font6x3Out,"Elemental",x, y+31, COLOR_HGOLD, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"F",x+43, y+32, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[5]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+53, y+32, cpl.stats.protection[5]?COLOR_WHITE:COLOR_GREY, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"C",x+73, y+32, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[6]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+83, y+32, cpl.stats.protection[6]?COLOR_WHITE:COLOR_GREY, NULL, NULL);

    StringBlt(ScreenSurface, &SystemFont,"E",x+103, y+32, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[7]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+113, y+32, cpl.stats.protection[7]?COLOR_WHITE:COLOR_GREY, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"P",x+133, y+32, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[8]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+143, y+32, cpl.stats.protection[8]?COLOR_WHITE:COLOR_GREY, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"A",x+163, y+32, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[9]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+173, y+32, cpl.stats.protection[9]?COLOR_WHITE:COLOR_GREY, NULL, NULL);

    StringBlt(ScreenSurface, &Font6x3Out,"Magical",x, y+46, COLOR_HGOLD, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"M",x+43, y+47, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[10]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+53, y+47, cpl.stats.protection[10]?COLOR_WHITE:COLOR_GREY, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"Mi",x+70, y+47, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[11]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+83, y+47, cpl.stats.protection[11]?COLOR_WHITE:COLOR_GREY, NULL, NULL);

    StringBlt(ScreenSurface, &SystemFont,"B",x+103, y+47, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[12]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+113, y+47, cpl.stats.protection[12]?COLOR_WHITE:COLOR_GREY, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"P",x+133, y+47, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[13]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+143, y+47, cpl.stats.protection[13]?COLOR_WHITE:COLOR_GREY, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"F",x+163, y+47, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[14]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+173, y+47, cpl.stats.protection[14]?COLOR_WHITE:COLOR_GREY, NULL, NULL);

    StringBlt(ScreenSurface, &Font6x3Out,"Spherical",x, y+61, COLOR_HGOLD, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"N",x+43, y+62, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[15]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+53, y+62, cpl.stats.protection[15]?COLOR_WHITE:COLOR_GREY, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"Ch",x+69, y+62, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[16]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+83, y+62, cpl.stats.protection[16]?COLOR_WHITE:COLOR_GREY, NULL, NULL);

    StringBlt(ScreenSurface, &SystemFont,"D",x+103, y+62, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[17]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+113, y+62, cpl.stats.protection[17]?COLOR_WHITE:COLOR_GREY, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"Sp",x+130, y+62, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[18]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+143, y+62, cpl.stats.protection[18]?COLOR_WHITE:COLOR_GREY, NULL, NULL);
    StringBlt(ScreenSurface, &SystemFont,"Co",x+160, y+62, COLOR_HGOLD, NULL, NULL);
    sprintf(buf,"%02d", cpl.stats.protection[19]);
    StringBlt(ScreenSurface, &SystemFont,buf,x+173, y+62, cpl.stats.protection[19]?COLOR_WHITE:COLOR_GREY, NULL, NULL);

}

#define ICONDEFLEN 32
Boolean blt_face_centered(int face, int x, int y)
{
    register int temp;
    SDL_Rect box;

    if(!FaceList[face].sprite)
        return FALSE;

    if(FaceList[face].sprite->status != SPRITE_STATUS_LOADED)
        return FALSE;

    box.x = FaceList[face].sprite->border_left;
    box.w = FaceList[face].sprite->bitmap->w;
    temp = box.w-FaceList[face].sprite->border_left-FaceList[face].sprite->border_right;
    if(temp > 32)
    {
        box.w = 32;
        temp-=32;
        temp>>=1;
        box.x+=temp;
    }
    else if(temp < 32)
    {
        temp = 32-temp;
        box.x -= (temp>>1);
    }

    box.y = -FaceList[face].sprite->border_up;
    box.h = FaceList[face].sprite->bitmap->h;
    temp = box.h-FaceList[face].sprite->border_up-FaceList[face].sprite->border_down;
    if(temp > 32)
    {
        box.h = 32;
        temp-=32;
        temp>>=1;
        box.y+=temp;
    }
    else if(temp < 32)
    {
        temp = 32-temp;
        box.y = -(temp>>1)+FaceList[face].sprite->border_up;
    }
    sprite_blt(FaceList[face].sprite,x,y, &box, NULL);

    return TRUE;
}

void show_range(int x, int y)
{
    char buf[256];
	SDL_Rect rec_range;
	SDL_Rect rec_item;
	item *op;
    item *tmp;

	rec_range.w=160;
	rec_item.w=185;
    examine_range_inv();

        sprite_blt(Bitmaps[BITMAP_RANGE],x-2, y, NULL, NULL);

        switch(RangeFireMode)
        {
            case FIRE_MODE_BOW:
                if(fire_mode_tab[FIRE_MODE_BOW].item != FIRE_ITEM_NO)
                {
                    sprintf(buf,"using %s",
                        get_range_item_name(fire_mode_tab[FIRE_MODE_BOW].item));
                    blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_BOW].item,x+3,y+2);

                    StringBlt(ScreenSurface, &SystemFont,buf,x+3, y+38, COLOR_WHITE, &rec_range, NULL);
                    if(fire_mode_tab[FIRE_MODE_BOW].amun != FIRE_ITEM_NO)
                    {

                        tmp = locate_item_from_item(cpl.ob, fire_mode_tab[FIRE_MODE_BOW].amun);
                        if(tmp)
                        {
                            if(tmp->itype == TYPE_ARROW)
                                sprintf(buf,"ammo %s (%d)",
                                    get_range_item_name(fire_mode_tab[FIRE_MODE_BOW].amun),tmp->nrof);
                            else
                                sprintf(buf,"ammo %s",
                                get_range_item_name(fire_mode_tab[FIRE_MODE_BOW].amun));
                        }
                        else
                            strcpy(buf,"ammo not selected");
                        blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_BOW].amun,x+43,y+2);
                    }
                    else
                    {
                        sprintf(buf,"ammo not selected");
                    }
                    StringBlt(ScreenSurface, &SystemFont,buf, x+3, y+49, COLOR_WHITE, &rec_item, NULL);
                }
                else
                {
                    sprintf(buf,"no range weapon applied");
                    StringBlt(ScreenSurface, &SystemFont,buf,x+3, y+38, COLOR_WHITE, &rec_range, NULL);
                }

                sprite_blt(Bitmaps[BITMAP_RANGE_MARKER],x+3, y+2, NULL, NULL);
                break;

            /* wands, staffs, rods and horns */
            case FIRE_MODE_WAND:
                if(!locate_item_from_item(cpl.ob, fire_mode_tab[FIRE_MODE_WAND].item))
                    fire_mode_tab[FIRE_MODE_WAND].item = FIRE_ITEM_NO;
                if(fire_mode_tab[FIRE_MODE_WAND].item != FIRE_ITEM_NO)
                {
                    sprintf(buf,"%s",
                        get_range_item_name(fire_mode_tab[FIRE_MODE_WAND].item));
                    StringBlt(ScreenSurface, &SystemFont,buf,x+3, y+49, COLOR_WHITE, &rec_item, NULL);
                    sprite_blt(Bitmaps[BITMAP_RANGE_TOOL],x+3, y+2, NULL, NULL);
                    blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_WAND].item,x+43,y+2);
                }
                else
                {
                    sprite_blt(Bitmaps[BITMAP_RANGE_TOOL_NO],x+3, y+2, NULL, NULL);
                    sprintf(buf,"nothing applied");
                    StringBlt(ScreenSurface, &SystemFont,buf,x+3, y+49, COLOR_WHITE, &rec_item, NULL);
                }

                sprintf(buf,"use range tool");
                StringBlt(ScreenSurface, &SystemFont,buf,x+3, y+38, COLOR_WHITE, &rec_range, NULL);
                break;

            /* the summon range ctrl will come from server only after the player casted a summon spell */
            case FIRE_MODE_SUMMON:
                if(fire_mode_tab[FIRE_MODE_SUMMON].item != FIRE_ITEM_NO)
                {
                    sprite_blt(Bitmaps[BITMAP_RANGE_CTRL],x+3, y+2, NULL, NULL);
                    StringBlt(ScreenSurface, &SystemFont,fire_mode_tab[FIRE_MODE_SUMMON].name , x+3, y+49, COLOR_WHITE, NULL, NULL);
					blt_face_centered(fire_mode_tab[FIRE_MODE_SUMMON].item , x+43, y+2);
                }
                else
                {
                    sprite_blt(Bitmaps[BITMAP_RANGE_CTRL_NO],x+3, y+2, NULL, NULL);
                    sprintf(buf,"no golem summoned");
                    StringBlt(ScreenSurface, &SystemFont,buf, x+3, y+49, COLOR_WHITE, &rec_item, NULL);
                }
                sprintf(buf,"mind control");
                StringBlt(ScreenSurface, &SystemFont,buf, x+3, y+38, COLOR_WHITE, &rec_item, NULL);
                break;

            /* these are client only, no server signal needed */
            case FIRE_MODE_SKILL:
                if(fire_mode_tab[FIRE_MODE_SKILL].skill)
                {
                    sprite_blt(Bitmaps[BITMAP_RANGE_SKILL],x+3, y+2, NULL, NULL);
                    if(fire_mode_tab[FIRE_MODE_SKILL].skill->flag != -1)
                    {
                        sprite_blt(fire_mode_tab[FIRE_MODE_SKILL].skill->icon ,x+43, y+2, NULL, NULL);
                        StringBlt(ScreenSurface, &SystemFont,fire_mode_tab[FIRE_MODE_SKILL].skill->name , x+3, y+49, COLOR_WHITE, &rec_item, NULL);
                    }
                    else
                        fire_mode_tab[FIRE_MODE_SKILL].skill=NULL;
                }
                else
                {
                    sprite_blt(Bitmaps[BITMAP_RANGE_SKILL_NO],x+3, y+2, NULL, NULL);
                    sprintf(buf,"no skill selected");
                    StringBlt(ScreenSurface, &SystemFont,buf, x+3, y+49, COLOR_WHITE, &rec_item, NULL);
                }
                sprintf(buf,"use skill");
                StringBlt(ScreenSurface, &SystemFont,buf, x+3, y+38, COLOR_WHITE, &rec_range, NULL);

            break;
            case FIRE_MODE_SPELL:
                if(fire_mode_tab[FIRE_MODE_SPELL].spell)
                {
                        /* we use wiz spells as default */
                    sprite_blt(Bitmaps[BITMAP_RANGE_WIZARD],x+3, y+2, NULL, NULL);
                    if(fire_mode_tab[FIRE_MODE_SPELL].spell->flag != -1)
                    {
                        sprite_blt(fire_mode_tab[FIRE_MODE_SPELL].spell->icon ,x+43, y+2, NULL, NULL);
                        StringBlt(ScreenSurface, &SystemFont,fire_mode_tab[FIRE_MODE_SPELL].spell->name , x+3, y+49, COLOR_WHITE, &rec_item, NULL);
                    }
                    else
                        fire_mode_tab[FIRE_MODE_SPELL].spell=NULL;
                }
                else
                {
                    sprite_blt(Bitmaps[BITMAP_RANGE_WIZARD_NO],x+3, y+2, NULL, NULL);
                    sprintf(buf,"no spell selected");
                    StringBlt(ScreenSurface, &SystemFont,buf, x+3, y+49, COLOR_WHITE, &rec_item, NULL);
                }
                sprintf(buf,"cast spell");
                StringBlt(ScreenSurface, &SystemFont,buf, x+3, y+38, COLOR_WHITE, &rec_range, NULL);

                break;
            case FIRE_MODE_THROW:
                if(!(op=locate_item_from_item(cpl.ob, fire_mode_tab[FIRE_MODE_THROW].item)))
                    fire_mode_tab[FIRE_MODE_THROW].item = FIRE_ITEM_NO;
                if(fire_mode_tab[FIRE_MODE_THROW].item != FIRE_ITEM_NO)
                {

                    sprite_blt(Bitmaps[BITMAP_RANGE_THROW],x+3, y+2, NULL, NULL);
                    blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_THROW].item,x+43,y+2);
					if(op->nrof>1)
					{
						if(op->nrof >9999)
							strcpy(buf,"many");
						else
							sprintf(buf,"%d",op->nrof);
						StringBlt(ScreenSurface, &Font6x3Out,buf,x+43+(ICONDEFLEN/2)-(get_string_pixel_length(buf,&Font6x3Out)/2),
							y+22,COLOR_WHITE, NULL, NULL);
					}

                    sprintf(buf,"%s",
                        get_range_item_name(fire_mode_tab[FIRE_MODE_THROW].item));
                    StringBlt(ScreenSurface, &SystemFont,buf, x+3, y+49, COLOR_WHITE, &rec_item, NULL);
                }
                else
                {
                    sprite_blt(Bitmaps[BITMAP_RANGE_THROW_NO],x+3, y+2, NULL, NULL);
                    sprintf(buf,"no item ready");
                    StringBlt(ScreenSurface, &SystemFont,buf, x+3, y+49, COLOR_WHITE, &rec_item, NULL);
                }
                sprintf(buf,"throw item");
                StringBlt(ScreenSurface, &SystemFont,buf, x+3, y+38, COLOR_WHITE, &rec_range, NULL);

                break;
        };
}

static char *get_range_item_name(int tag)
{
    item *tmp;

    if(tag != FIRE_ITEM_NO)
    {
        tmp = locate_item (tag);
        if(tmp)
            return tmp->s_name;
    }
    return("Nothing");
}

void blt_inventory_face_from_tag(int tag, int x, int y)
{
    item *tmp;

    /* check item is in inventory and faces are loaded, etc */
    tmp = locate_item (tag);
    if(!tmp)
        return;
    blt_inv_item_centered(tmp, x, y);
}

void show_menu(void)
{
	SDL_Rect box;

	if(!cpl.menustatus)
		return;
	if(cpl.menustatus == MENU_KEYBIND)
		show_keybind();
	else if(cpl.menustatus == MENU_STATUS)
		show_status();
	else if (cpl.menustatus == MENU_BOOK)
		show_book(400-Bitmaps[BITMAP_JOURNAL]->bitmap->w/2,300-Bitmaps[BITMAP_JOURNAL]->bitmap->h/2);
	else if (cpl.menustatus == MENU_PARTY)
		show_party();
	else if(cpl.menustatus == MENU_SPELL)
	{
		show_spelllist();
		box.x = SCREEN_XLEN/2-Bitmaps[BITMAP_DIALOG_BG]->bitmap->w/2;
		box.y = SCREEN_YLEN/2-Bitmaps[BITMAP_DIALOG_BG]->bitmap->h/2-42;
		box.h = 42;
		box.w = Bitmaps[BITMAP_DIALOG_BG]->bitmap->w;
		SDL_FillRect(ScreenSurface, &box, 0);
		show_quickslots(box.x+100,box.y+3);
	}
	else if(cpl.menustatus == MENU_SKILL)
		show_skilllist();
	else if(cpl.menustatus == MENU_OPTION)
		show_optwin();
	else if(cpl.menustatus == MENU_CREATE)
		show_newplayer_server();
}

void show_media(int x, int y)
{
	_Sprite *bmap;
	int xtemp;

	if(media_show!=MEDIA_SHOW_NO)
	{
		/* we show a png*/
		if(media_file[media_show].type == MEDIA_TYPE_PNG)
		{
			bmap = (_Sprite*) media_file[media_show].data;
			if(bmap)
			{
				xtemp = x-bmap->bitmap->w;
				sprite_blt(bmap ,xtemp, y, NULL, NULL);
			}
		}
	}
}

void show_status(void)
{
/*
        int y, x;
        x= SCREEN_XLEN/2-Bitmaps[BITMAP_STATUS]->bitmap->w/2;
        y= SCREEN_YLEN/2-Bitmaps[BITMAP_STATUS]->bitmap->h/2;
        sprite_blt(Bitmaps[BITMAP_STATUS],x, y, NULL, NULL);
*/
}



int init_media_tag(char *tag)
{
        char *p1, *p2, buf[256];
        int temp;
		int ret=0;

        if(tag == NULL)
        {
                LOG(LOG_MSG, "MediaTagError: Tag == NULL\n");
                return ret;
        }
        p1 = strchr(tag, '|');
        p2 = strrchr(tag, '|');
        if(p1 == NULL || p2==NULL)
        {
                LOG(LOG_MSG, "MediaTagError: Parameter == NULL (%x %x)\n", p1,
                        p2);
                return ret;
        }
        *p1++=0;
        *p2++=0;

        if(strstr(tag,".ogg"))
        {
            sound_play_music(tag, options.music_volume,2000,atoi(p2),atoi(p1),MUSIC_MODE_NORMAL);
			ret = 1; /* because we have called sound_play_music, we don't must fade out extern */
        }
        else if(strstr(tag,".png"))
        {
            media_show_update = 2;
            /* because we chain this to map_scroll, but map_scroll can
            * come behind the draw_info cmd... sigh*/

            /* first, we look in our media buffers.. perhaps this is still buffered
            * is so, just update the paramter and fire it up */
            if(!strcmp(media_file[media_count].name, tag))
            {
                media_show = media_count;
                media_file[media_count].p1=atoi(p1);
                media_file[media_count].p2=atoi(p2);
                return ret;
            }
            /* if not loaded, we overwrite our oldest buffered media file */

            media_show = MEDIA_SHOW_NO;
            temp = (media_count+1)%MEDIA_MAX;
            if(media_file[temp].data) /* if some here, kick it*/
            {
                media_file[temp].type = MEDIA_TYPE_NO;
                FreeMemory(&media_file[temp].data);
            }
            sprintf(buf,"%s%s",GetMediaDirectory(), tag);
            if(!(media_file[temp].data = sprite_load_file(buf,0)))
                return ret;

            media_file[temp].type = MEDIA_TYPE_PNG;
            strcpy(media_file[temp].name, tag);
            media_count = temp;
            media_show = media_count;
        }
		return ret;
}

/*
int blt_window_slider(_Sprite *slider, int maxlen, int winlen, int startoff, int len_h, int x, int y)
{
	SDL_Rect box;
	int startpos;

	if (len_h < 0)
		len_h = slider->bitmap->h;

	if(maxlen < winlen)
		maxlen=winlen;
	if(startoff+winlen >maxlen)
		maxlen=startoff+winlen;

	box.x=0;
	box.y=0;
	box.w=slider->bitmap->w;

	box.h = (len_h*winlen)/maxlen;
	startpos = (startoff *len_h)/maxlen;
	if(box.h < 1)
		box.h=1;

	if(startoff+winlen >=maxlen && startpos+box.h<len_h)
		startpos ++;

	sprite_blt(slider,x,y+startpos, &box, NULL);
}
*/
int blt_window_slider(_Sprite *slider, int maxlen, int winlen, int startoff, int len, int x, int y)
{
    SDL_Rect box;
    double temp;
    int startpos, len_h;

	if(len != -1)
		len_h = len;
	else
		len_h = slider->bitmap->h;

    if(maxlen < winlen)
        maxlen=winlen;
    if(startoff+winlen >maxlen)
        maxlen=startoff+winlen;

    box.x=0;
    box.y=0;
    box.w=slider->bitmap->w;

    /* now we have 100% = 1.0 to 0% = 0.0 of the length */
    temp = (double)winlen/(double)maxlen; /* between 0.0 <-> 1.0 */
    startpos = (int)((double)startoff *((double)len_h/(double )maxlen)); /* startpixel */
    temp = (double)len_h*temp;
    box.h = (Uint16) temp;
	if(!box.h)
		box.h=1;

    if(startoff+winlen >=maxlen && startpos+box.h<len_h)
		startpos ++;

   sprite_blt(slider,x,y+startpos, &box, NULL);
   return 0;

}

static int load_anim_tmp(void)
{
	int i, anim_len=0,new_anim = TRUE;
	uint8 faces=0;
	uint16 count=0, face_id;
	FILE *stream;
	char buf[HUGE_BUF];
	unsigned char anim_cmd[2048];


	/* clear both animation tables
	 * this *must* be reloaded every time we connect
	 * - remember that different servers can have different
	 * animations!
	 */
	for(i=0;i<MAXANIM;i++)
	{
		if(animations[i].faces)
			free(animations[i].faces);
		if(anim_table[i].anim_cmd)
			free(anim_table[i].anim_cmd);
	}
	memset(animations, 0, sizeof(animations));

	/* animation #0 is like face id #0 a bug catch - if ever
	 * appear in game flow its a sign of a uninit of simply
	 * buggy operation.
	 */
	anim_cmd[0]= (unsigned char) ((count>>8)&0xff);
	anim_cmd[1] =(unsigned char)( count & 0xff);
	anim_cmd[2]=0; /* flags ... */
	anim_cmd[3]=1;
	anim_cmd[4]=0; /* face id o */
	anim_cmd[5]=0;
	anim_table[count].anim_cmd = malloc(6);
	memcpy(anim_table[count].anim_cmd, anim_cmd, 6);
	anim_table[count].len = 6;
	/* end of dummy animation #0 */

	count++;
    if( (stream = fopen(FILE_ANIMS_TMP, "rt" )) == NULL )
	{
		LOG(LOG_ERROR,"load_anim_tmp: Error reading anim.tmp!");
		SYSTEM_End(); /* fatal */
		exit(0);
	}

	while(fgets(buf, HUGE_BUF-1, stream)!=NULL)
	{
		if(new_anim == TRUE) /* we are outside a anim body ? */
		{
			if(!strncmp(buf, "anim ",5))
			{
				new_anim = FALSE;
				faces = 0;
				anim_cmd[0]= (unsigned char) ((count>>8)&0xff);
				anim_cmd[1] =(unsigned char)( count & 0xff);
				faces = 1;
				anim_len = 4;

			}
			else /* we should never hit this point */
			{
				LOG(LOG_ERROR,"load_anim_tmp:Error parsing anims.tmp - unknown cmd: >%s<!\n", buf);
			}
		}
		else /* no, we are inside! */
		{
			if(!strncmp(buf, "facings ",8))
			{
				faces = atoi(buf+8);
			}
			else if(!strncmp(buf, "mina",4))
			{
				/*LOG(LOG_DEBUG,"LOAD ANIM: #%d - len: %d (%d)\n", count, anim_len, faces);*/
				anim_cmd[2]=0; /* flags ... */
				anim_cmd[3]=faces; /* facings */
				anim_table[count].len = anim_len;
				anim_table[count].anim_cmd = malloc(anim_len);
				memcpy(anim_table[count].anim_cmd, anim_cmd, anim_len);
				count++;
				new_anim = TRUE;
			}
			else
			{
				face_id = (uint16) atoi(buf);
				anim_cmd[anim_len++]= (unsigned char) ((face_id>>8)&0xff);
				anim_cmd[anim_len++] = (unsigned char) (face_id & 0xff);
			}
		}
	}


    fclose( stream );
	return 1;
}


int read_anim_tmp(void)
{
    FILE *stream, *ftmp;
	int i,new_anim=TRUE,count=1;
	char buf[HUGE_BUF],cmd[HUGE_BUF];
    struct stat	stat_bmap, stat_anim, stat_tmp;

	/* if this fails, we have a urgent problem somewhere before */
    if( (stream = fopen(FILE_BMAPS_TMP, "rb" )) == NULL )
	{
		LOG(LOG_ERROR,"read_anim_tmp:Error reading bmap.tmp for anim.tmp!");
		SYSTEM_End(); /* fatal */
		exit(0);
	}
	fstat(fileno(stream), &stat_bmap);
    fclose( stream );

    if( (stream = fopen(FILE_CLIENT_ANIMS, "rb" )) == NULL )
	{
		LOG(LOG_ERROR,"read_anim_tmp:Error reading bmap.tmp for anim.tmp!");
		SYSTEM_End(); /* fatal */
		exit(0);
	}
	fstat(fileno(stream), &stat_anim);
    fclose( stream );

    if( (stream = fopen(FILE_ANIMS_TMP, "rb" )) != NULL )
	{
		fstat(fileno(stream), &stat_tmp);
		fclose( stream );

		/* our anim file must be newer as our default anim file */
		if(difftime(stat_tmp.st_mtime, stat_anim.st_mtime) > 0.0f)
		{

			/* our anim file must be newer as our bmaps.tmp */
			if(difftime(stat_tmp.st_mtime, stat_bmap.st_mtime) > 0.0f)
				return load_anim_tmp(); /* all fine - load file */
		}
	}

	unlink(FILE_ANIMS_TMP); /* for some reason - recreate this file */
    if( (ftmp = fopen(FILE_ANIMS_TMP, "wt" )) == NULL )
	{
		LOG(LOG_ERROR,"read_anim_tmp:Error opening anims.tmp!");
		SYSTEM_End(); /* fatal */
		exit(0);
	}

    if( (stream = fopen(FILE_CLIENT_ANIMS, "rt" )) == NULL )
	{
		LOG(LOG_ERROR,"read_anim_tmp:Error reading client_anims for anims.tmp!");
		SYSTEM_End(); /* fatal */
		exit(0);
	}
	while(fgets(buf, HUGE_BUF-1, stream)!=NULL)
	{
		sscanf(buf,"%s",cmd);
		if(new_anim == TRUE) /* we are outside a anim body ? */
		{
			if(!strncmp(buf, "anim ",5))
			{
				sprintf(cmd, "anim %d -> %s",count++, buf);
				fputs(cmd,ftmp); /* safe this key string! */
				new_anim = FALSE;
			}
			else /* we should never hit this point */
			{
				LOG(LOG_ERROR,"read_anim_tmp:Error parsing client_anim - unknown cmd: >%s<!\n", cmd);
			}
		}
		else /* no, we are inside! */
		{
			if(!strncmp(buf, "facings ",8))
			{
				fputs(buf, ftmp); /* safe this key word! */
			}
			else if(!strncmp(cmd, "mina",4))
			{
				fputs(buf, ftmp); /* safe this key word! */
				new_anim = TRUE;
			}
			else
			{
				/* this is really slow when we have more pictures - we
				 * browsing #anim * #bmaps times the same table -
				 * pretty bad - when we stay to long here, we must create
				 * for bmaps.tmp entries a hash table too.
				 */
				for(i=0;i<bmaptype_table_size;i++)
				{
					if(!strcmp(bmaptype_table[i].name,cmd))
						break;
				}
				if(i>=bmaptype_table_size)
				{
					/* if we are here then we have a picture name in the anims file
					 * which we don't have in our bmaps file! Pretty bad. But because
					 * face #0 is ALWAYS bug.101 - we simply use it here! */
					i=0;
					LOG(LOG_ERROR,"read_anim_tmp: Invalid anim name >%s< - set to #0 (bug.101)!\n", cmd);
				}
				sprintf(cmd, "%d\n",i);
				fputs(cmd, ftmp);
			}
		}

	}

    fclose( stream );
    fclose( ftmp );
	return load_anim_tmp(); /* all fine - load file */
}

void read_anims(void)
{
    FILE *stream;
	char *temp_buf;
	struct stat statbuf;
	int i;

	LOG(LOG_DEBUG,"Loading %s....",FILE_CLIENT_ANIMS);
	srv_client_files[SRV_CLIENT_ANIMS].len = 0;
	srv_client_files[SRV_CLIENT_ANIMS].crc = 0;
    if( (stream = fopen(FILE_CLIENT_ANIMS, "rb" )) != NULL )
    {
		/* temp load the file and get the data we need for compare with server */
		fstat (fileno (stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_ANIMS].len = i;
		temp_buf=malloc(i);
		if (fread(temp_buf, sizeof(char), i, stream))
			srv_client_files[SRV_CLIENT_ANIMS].crc = crc32(1L,temp_buf,i);
		free(temp_buf);
        fclose( stream );
		LOG(LOG_DEBUG," found file!(%d/%x)",srv_client_files[SRV_CLIENT_ANIMS].len,srv_client_files[SRV_CLIENT_ANIMS].crc );
	}
	LOG(LOG_DEBUG,"done.\n");
}

/* after we tested and/or created bmaps.p0 - load the data from it */
static void load_bmaps_p0(void)
{
	char buf[HUGE_BUF];
	char name[HUGE_BUF];
	int len, pos, num;
	unsigned int crc;
	_bmaptype *at;
	FILE *fbmap;

	/* clear bmap hash table */
	memset((void *) bmap_table,0,BMAPTABLE*sizeof(_bmaptype *));

	/* try to open bmaps_p0 file */
    if( (fbmap = fopen(FILE_BMAPS_P0, "rb" )) == NULL )
	{
		LOG(LOG_ERROR,"FATAL: Error loading bmaps.p0!");
		SYSTEM_End(); /* fatal */
		unlink(FILE_BMAPS_P0);
		exit(0);
	}
	while(fgets(buf, HUGE_BUF-1, fbmap)!=NULL)
	{
		sscanf(buf,"%d %d %x %d %s", &num, &pos, &crc, &len, name);

		at = (_bmaptype *) malloc(sizeof(_bmaptype));
		at->name = (char *) malloc(strlen(name)+1);
		strcpy(at->name, name);
		at->crc = crc;
		at->num = num;
		at->len = len;
		at->pos = pos;
		add_bmap(at);
		/*LOG(LOG_DEBUG,"%d %d %d %x >%s<\n", num, pos, len, crc, name);*/
	}
    fclose(fbmap);
}


/* read and/or create the bmaps.p0 file out of the
 * atrinik.p0 file
 */
void read_bmaps_p0(void)
{
	FILE *fbmap, *fpic;
	char *temp_buf, *cp;
	int bufsize, len, num,  pos;
	unsigned int crc;
	char buf[HUGE_BUF];
    struct stat	bmap_stat, pic_stat;

	if( (fpic = fopen(FILE_ATRINIK_P0, "rb" )) == NULL )
	{
		LOG(LOG_ERROR,"FATAL: Can't find atrinik.p0 file!");
		SYSTEM_End(); /* fatal */
		unlink(FILE_BMAPS_P0);
		exit(0);
	}
	/* get time stamp of the file atrinik.p0 */
	fstat(fileno(fpic), &pic_stat);

	/* try to open bmaps_p0 file */
    if( (fbmap = fopen(FILE_BMAPS_P0, "r" )) == NULL )
		goto create_bmaps;

	/* get time stamp of the file */
	fstat(fileno(fbmap), &bmap_stat);
    fclose(fbmap);

	if(difftime(pic_stat.st_mtime, bmap_stat.st_mtime) > 0.0f)
		goto create_bmaps;

    fclose(fpic);
	load_bmaps_p0();
	return;

	create_bmaps: /* if we are here, then we have to (re)create the bmaps.p0 file */
    if( (fbmap = fopen(FILE_BMAPS_P0, "w" )) == NULL )
	{
		LOG(LOG_ERROR,"FATAL: Can't create bmaps.p0 file!");
		SYSTEM_End(); /* fatal */
	    fclose(fbmap);
		unlink(FILE_BMAPS_P0);
		exit(0);
	}
	temp_buf = malloc((bufsize = 24*1024));

	while(fgets(buf, HUGE_BUF-1, fpic)!=NULL)
	{
	    if(strncmp(buf,"IMAGE ",6)!=0)
		{
			LOG(LOG_ERROR,"read_client_images:Bad image line - not IMAGE, instead\n%s",buf);
			SYSTEM_End(); /* fatal */
			fclose(fbmap);
			fclose(fpic);
			unlink(FILE_BMAPS_P0);
			exit(0);
		}

	    num = atoi(buf+6);
	    /* Skip accross the number data */
	    for (cp=buf+6; *cp!=' '; cp++) ;
	    len = atoi(cp);

		strcpy(buf, cp);
		pos = (int) ftell( fpic );

		if(len >bufsize) /* dynamic buffer adjustment */
		{
			free(temp_buf);
			/* we assume thats this is nonsense */
			if(len>128*1024)
			{
				LOG(LOG_ERROR,"read_client_images:Size of picture out of bounds!(len:%d)(pos:%d)",len, pos);
				SYSTEM_End(); /* fatal */
			    fclose(fbmap);
				fclose(fpic);
				unlink(FILE_BMAPS_P0);
				exit(0);
			}
			bufsize=len;
			temp_buf = malloc(bufsize);
		}

		if (!fread(temp_buf, 1, len, fpic))
			return;
		crc = crc32(1L,temp_buf,len);

		/* now we got all we needed! */
		sprintf(temp_buf, "%d %d %x %s",num, pos,crc,buf );
		fputs(temp_buf,fbmap);
/*		LOG(LOG_DEBUG,"FOUND: %s", temp_buf);		*/
	}


	free(temp_buf);
    fclose(fbmap);
    fclose(fpic);
	load_bmaps_p0();
	return;
}

void delete_bmap_tmp(void)
{
	int i;

	bmaptype_table_size = 0;
	for(i=0;i<MAX_BMAPTYPE_TABLE;i++)
	{
		if(bmaptype_table[i].name)
			free(bmaptype_table[i].name);
		bmaptype_table[i].name=NULL;
	}
}

static int load_bmap_tmp(void)
{
	FILE *stream;
	char buf[HUGE_BUF],name[HUGE_BUF];
	int i=0,len, pos;
	unsigned int crc;

	delete_bmap_tmp();
    if( (stream = fopen(FILE_BMAPS_TMP, "rt" )) == NULL )
	{
		LOG(LOG_ERROR,"bmaptype_table(): error open file <bmap.tmp>");
		SYSTEM_End(); /* fatal */
		exit(0);
	}
	while(fgets(buf, HUGE_BUF-1, stream)!=NULL)
	{
		sscanf(buf,"%d %d %x %s\n", &pos, &len, &crc, name);
		bmaptype_table[i].crc = crc;
		bmaptype_table[i].len = len;
		bmaptype_table[i].pos = pos;
		bmaptype_table[i].name =(char*) malloc(strlen(name)+1);
		strcpy(bmaptype_table[i].name,name);
		i++;
	}
	bmaptype_table_size=i;
    fclose( stream );
	return 0;
}


int read_bmap_tmp(void)
{
    FILE *stream, *fbmap0;
	char buf[HUGE_BUF],name[HUGE_BUF];
    struct stat	stat_bmap, stat_tmp, stat_bp0;
	int len;
	unsigned int crc;
	_bmaptype *at;

    if( (stream = fopen(FILE_CLIENT_BMAPS, "rb" )) == NULL )
	{
		/* we can't make bmaps.tmp without this file */
		unlink(FILE_BMAPS_TMP);
		return 1;
	}
	fstat(fileno(stream), &stat_bmap);
    fclose( stream );

    if( (stream = fopen(FILE_BMAPS_P0, "rb" )) == NULL )
	{
		/* we can't make bmaps.tmp without this file */
		unlink(FILE_BMAPS_TMP);
		return 1;
	}
	fstat(fileno(stream), &stat_bp0);
    fclose( stream );

    if( (stream = fopen(FILE_BMAPS_TMP, "rb" )) == NULL )
		goto create_bmap_tmp;
	fstat(fileno(stream), &stat_tmp);
    fclose( stream );

	/* ok - client_bmap & bmaps.p0 are there - now check
	 * our bmap_tmp is newer - is not newer as both, we
	 * create it new - then it is newer.
	 */

	if(difftime(stat_tmp.st_mtime, stat_bmap.st_mtime) > 0.0f)
	{
		if(difftime(stat_tmp.st_mtime, stat_bp0.st_mtime) > 0.0f)
			return load_bmap_tmp(); /* all fine */
	}

	create_bmap_tmp:
	unlink(FILE_BMAPS_TMP);

	/* NOW we are sure... we must create us a new bmaps.tmp */
    if( (stream = fopen(FILE_CLIENT_BMAPS, "rb" )) != NULL )
    {
		/* we can use text mode here, its local */
	    if( (fbmap0 = fopen(FILE_BMAPS_TMP, "wt" )) != NULL )
		{
			/* read in the bmaps from the server, check with the
			 * loaded bmap table (from bmaps.p0) and create with
			 * this information the bmaps.tmp file.
			 */
			while(fgets(buf, HUGE_BUF-1, stream)!=NULL)
			{
				sscanf(buf,"%x %x %s", &len, &crc, name);
				at=find_bmap(name);

				/* now we can check, our local file package has
				 * the right png - if not, we mark this pictures
				 * as "in cache". We don't check it here now -
				 * that will happens at runtime.
				 * That can change when we include later a forbidden
				 * flag in the server (no face send) - then we need
				 * to break and upddate the picture and/or check the cache.
				 */
				/* position -1 mark "not i the atrinik.p0 file */
				if(!at || at->len != len || at->crc != crc) /* is different or not there! */
					sprintf(buf,"-1 %d %x %s\n", len, crc, name);
				else /* we have it */
					sprintf(buf,"%d %d %x %s\n", at->pos, len, crc, name);
				fputs(buf, fbmap0);
			}
		    fclose( fbmap0 );
		}
	    fclose( stream );
	}
	return load_bmap_tmp(); /* all fine */
}


void read_bmaps(void)
{
    FILE *stream;
	char *temp_buf;
	struct stat statbuf;
	int i;

	srv_client_files[SRV_CLIENT_BMAPS].len = 0;
	srv_client_files[SRV_CLIENT_BMAPS].crc = 0;
	LOG(LOG_DEBUG,"Reading %s....",FILE_CLIENT_BMAPS);
    if( (stream = fopen(FILE_CLIENT_BMAPS, "rb" )) != NULL )
    {
		/* temp load the file and get the data we need for compare with server */
		fstat (fileno (stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_BMAPS].len = i;
		temp_buf=malloc(i);
		if (fread(temp_buf, sizeof(char), i, stream))
			srv_client_files[SRV_CLIENT_BMAPS].crc = crc32(1L,temp_buf,i);
		free(temp_buf);
        fclose( stream );
		LOG(LOG_DEBUG," found file!(%d/%x)",srv_client_files[SRV_CLIENT_BMAPS].len,srv_client_files[SRV_CLIENT_BMAPS].crc );

	}
	else
	{
		unlink(FILE_BMAPS_TMP);
		LOG(LOG_DEBUG,"done.\n");
		return;
	}

	LOG(LOG_DEBUG,"done.\n");


}

/* in the setting files we have a list of chars templates
 * for char building. Delete this list here.
 */
void delete_server_chars(void)
{
	_server_char *tmp, *tmp1;

	for(tmp1=tmp=first_server_char;tmp1;tmp=tmp1)
	{
		tmp1 = tmp->next;
		free(tmp->name);
		free(tmp->desc[0]);
		free(tmp->desc[1]);
		free(tmp->desc[2]);
		free(tmp->desc[3]);
		free(tmp->char_arch[0]);
		free(tmp->char_arch[1]);
		free(tmp->char_arch[2]);
		free(tmp->char_arch[3]);
		free(tmp);
	}
	first_server_char=NULL;
}


/* find a face ID by name,
 * request the face (find it, load it or request it)
 * and return the ID
 */
static int get_bmap_id(char *name)
{
	int i;

	for(i=0;i<bmaptype_table_size;i++)
	{
		if(!strcmp(bmaptype_table[i].name,name))
		{
			request_face(i, 0);
			return i;
		}
	}

	return -1;
}

void load_settings(void)
{
    FILE *stream;
	char buf[HUGE_BUF], buf1[HUGE_BUF],buf2[HUGE_BUF];
	char cmd[HUGE_BUF];
	char para[HUGE_BUF];
	int para_count=0, last_cmd=0;
	int tmp_level=0;

	delete_server_chars();
	LOG(LOG_DEBUG,"Loading %s....\n",FILE_CLIENT_SETTINGS);
    if( (stream = fopen(FILE_CLIENT_SETTINGS, "rb" )) != NULL )
    {

		while(fgets(buf, HUGE_BUF-1, stream)!=NULL)
		{
			if(buf[0] == '#' || buf[0] == '\0' )
				continue;

			if(last_cmd == 0)
			{
				sscanf(adjust_string(buf),"%s %s", cmd, para);
				if(!strcmp(cmd, "char") )
				{
					_server_char *serv_char = malloc( sizeof(_server_char));

					memset(serv_char,0,sizeof(_server_char));
					/* copy name */
					serv_char->name = malloc(strlen(para)+1);
					strcpy(serv_char->name, para);

					/* get next legal line */
					while(fgets(buf, HUGE_BUF-1, stream)!=NULL && (buf[0] == '#'|| buf[0] == '\0'))
						;
					sscanf(adjust_string(buf),"%s %d %d %d %d %d %d", buf1,
						&serv_char->bar[0], &serv_char->bar[1],&serv_char->bar[2],
						&serv_char->bar_add[0], &serv_char->bar_add[1],&serv_char->bar_add[2]);

					serv_char->pic_id = get_bmap_id(buf1);

					while(fgets(buf, HUGE_BUF-1, stream)!=NULL && (buf[0] == '#'|| buf[0] == '\0'))
						;
					sscanf(adjust_string(buf),"%d %s %s", &serv_char->gender[0], buf1, buf2);
					serv_char->char_arch[0] = malloc(strlen(buf1)+1);
					strcpy(serv_char->char_arch[0], buf1);
					serv_char->face_id[0] = get_bmap_id(buf2);

					while(fgets(buf, HUGE_BUF-1, stream)!=NULL && (buf[0] == '#'|| buf[0] == '\0'))
						;
					sscanf(adjust_string(buf),"%d %s %s", &serv_char->gender[1], buf1, buf2);
					serv_char->char_arch[1] = malloc(strlen(buf1)+1);
					strcpy(serv_char->char_arch[1], buf1);
					serv_char->face_id[1] = get_bmap_id(buf2);

					while(fgets(buf, HUGE_BUF-1, stream)!=NULL && (buf[0] == '#'|| buf[0] == '\0'))
						;
					sscanf(adjust_string(buf),"%d %s %s", &serv_char->gender[2], buf1, buf2);
					serv_char->char_arch[2] = malloc(strlen(buf1)+1);
					strcpy(serv_char->char_arch[2], buf1);
					serv_char->face_id[2] = get_bmap_id(buf2);

					while(fgets(buf, HUGE_BUF-1, stream)!=NULL && (buf[0] == '#'|| buf[0] == '\0'))
						;
					sscanf(adjust_string(buf),"%d %s %s", &serv_char->gender[3], buf1, buf2);
					serv_char->char_arch[3] = malloc(strlen(buf1)+1);
					strcpy(serv_char->char_arch[3], buf1);
					serv_char->face_id[3] = get_bmap_id(buf2);

					while(fgets(buf, HUGE_BUF-1, stream)!=NULL && (buf[0] == '#'|| buf[0] == '\0'))
						;
					sscanf(adjust_string(buf),"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
						&serv_char->stat_points,
						&serv_char->stats[0],&serv_char->stats_min[0],&serv_char->stats_max[0],
						&serv_char->stats[1],&serv_char->stats_min[1],&serv_char->stats_max[1],
						&serv_char->stats[2],&serv_char->stats_min[2],&serv_char->stats_max[2],
						&serv_char->stats[3],&serv_char->stats_min[3],&serv_char->stats_max[3],
						&serv_char->stats[4],&serv_char->stats_min[4],&serv_char->stats_max[4],
						&serv_char->stats[5],&serv_char->stats_min[5],&serv_char->stats_max[5],
						&serv_char->stats[6],&serv_char->stats_min[6],&serv_char->stats_max[6]
						);

					while(fgets(buf, HUGE_BUF-1, stream)!=NULL && (buf[0] == '#'|| buf[0] == '\0'))
						;
					serv_char->desc[0] = malloc(strlen(adjust_string(buf))+1);
					strcpy(serv_char->desc[0], buf);
					while(fgets(buf, HUGE_BUF-1, stream)!=NULL && (buf[0] == '#'|| buf[0] == '\0'))
						;
					serv_char->desc[1] = malloc(strlen(adjust_string(buf))+1);
					strcpy(serv_char->desc[1], buf);
					while(fgets(buf, HUGE_BUF-1, stream)!=NULL && (buf[0] == '#'|| buf[0] == '\0'))
						;
					serv_char->desc[2] = malloc(strlen(adjust_string(buf))+1);
					strcpy(serv_char->desc[2], buf);
					while(fgets(buf, HUGE_BUF-1, stream)!=NULL && (buf[0] == '#'|| buf[0] == '\0'))
						;
					serv_char->desc[3] = malloc(strlen(adjust_string(buf))+1);
					strcpy(serv_char->desc[3], buf);

					/* add this char template to list */
					if(!first_server_char)
						first_server_char = serv_char;
					else
					{
						_server_char *tmpc;

						for(tmpc=first_server_char;tmpc->next;tmpc=tmpc->next)
							;
						tmpc->next = serv_char;
						serv_char->prev = tmpc;
					}


				}
				else if(!strcmp(cmd, "level") )
				{
					tmp_level = atoi(para);
					if(tmp_level<0 || tmp_level > 450)
					{
					    fclose( stream );
						LOG(LOG_ERROR,"client_settings_: level cmd out of bounds! >%s<\n",buf);
						return;
					}
					server_level.level =tmp_level;
					last_cmd = 1; /* cmd 'level' */
					para_count = 0;
				}
				else /* we close here... better we include later a fallback to login */
				{
					fclose(stream);
					LOG(LOG_ERROR,"Unknown command in client_settings! >%s<\n",buf);
					return;
				}
			}
			else if(last_cmd == 1)
			{
				server_level.exp[para_count++] = strtoul(buf, NULL ,16);
				if(para_count >tmp_level)
					last_cmd = 0;
			}


		}
	    fclose( stream );
	}

	if(first_server_char)
	{
		int g;

		memcpy(&new_character, first_server_char, sizeof(_server_char));

		/* adjust gender */
		for(g=0;g<4;g++)
		{
			if(new_character.gender[g])
			{
				new_character.gender_selected = g;
				break;
			}
		}
	}
}

void read_settings(void)
{
    FILE *stream;
	char *temp_buf;
	struct stat statbuf;
	int i;

	srv_client_files[SRV_CLIENT_SETTINGS].len = 0;
	srv_client_files[SRV_CLIENT_SETTINGS].crc = 0;
	LOG(LOG_DEBUG,"Reading %s....",FILE_CLIENT_SETTINGS);
    if( (stream = fopen(FILE_CLIENT_SETTINGS, "rb" )) != NULL )
    {
		/* temp load the file and get the data we need for compare with server */
		fstat (fileno (stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_SETTINGS].len = i;
		temp_buf=malloc(i);
		if (fread(temp_buf, sizeof(char), i, stream))
			srv_client_files[SRV_CLIENT_SETTINGS].crc = crc32(1L,temp_buf,i);
		free(temp_buf);
	    fclose( stream );
		LOG(LOG_DEBUG," found file!(%d/%x)",srv_client_files[SRV_CLIENT_SETTINGS].len,srv_client_files[SRV_CLIENT_SETTINGS].crc );
	}
	LOG(LOG_DEBUG,"done.\n");
}

void read_spells(void)
{
    int i,ii,panel;
    char type, nchar, *tmp, *tmp2;
	struct stat statbuf;
    FILE *stream;
	char *temp_buf;
    char line[255], name[255], d1[255], d2[255], d3[255], d4[255], icon[128];

	for(i=0;i<SPELL_LIST_MAX;i++)
    {
		for(ii=0;ii<DIALOG_LIST_ENTRY;ii++)
        {
                spell_list[i].entry[0][ii].flag = LIST_ENTRY_UNUSED;
                spell_list[i].entry[1][ii].flag = LIST_ENTRY_UNUSED;
                spell_list[i].entry[0][ii].name[0] = 0;
                spell_list[i].entry[1][ii].name[0] = 0;
		}
	}
    spell_list_set.class_nr = 0;
    spell_list_set.entry_nr = 0;
    spell_list_set.group_nr = 0;

	srv_client_files[SRV_CLIENT_SPELLS].len = 0;
	srv_client_files[SRV_CLIENT_SPELLS].crc = 0;
	LOG(LOG_DEBUG,"Reading %s....",FILE_CLIENT_SPELLS);
    if( (stream = fopen(FILE_CLIENT_SPELLS, "rb" )) != NULL )
    {
		/* temp load the file and get the data we need for compare with server */
		fstat (fileno (stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_SPELLS].len = i;
		temp_buf=malloc(i);
		if (fread(temp_buf, sizeof(char), i, stream))
			srv_client_files[SRV_CLIENT_SPELLS].crc = crc32(1L,temp_buf,i);
		free(temp_buf);
		rewind(stream);

        for(i=0;;i++)
        {
            if( fgets( line, 255, stream ) == NULL)
                    break;
            line[250]=0;
            tmp = strchr(line,'"' );
            tmp2 = strchr(tmp+1,'"' );
            *tmp2=0;
            strcpy(name, tmp+1);
            if( fgets( line, 255, stream ) == NULL)
                break;
            sscanf(line,"%c %c %d %s", &type, &nchar, &panel, icon);
            if( fgets( line, 255, stream ) == NULL)
                break;
            line[250]=0;
            tmp = strchr(line,'"' );
            tmp2 = strchr(tmp+1,'"' );
            *tmp2=0;
            strcpy(d1, tmp+1);
            if( fgets( line, 255, stream ) == NULL)
                break;
            line[250]=0;
            tmp = strchr(line,'"' );
            tmp2 = strchr(tmp+1,'"' );
            *tmp2=0;
            strcpy(d2, tmp+1);
            if( fgets( line, 255, stream ) == NULL)
                break;
            line[250]=0;
            tmp = strchr(line,'"' );
            tmp2 = strchr(tmp+1,'"' );
            *tmp2=0;
            strcpy(d3, tmp+1);
            if( fgets( line, 255, stream ) == NULL)
                break;
            line[250]=0;
            tmp = strchr(line,'"' );
            tmp2 = strchr(tmp+1,'"' );
            *tmp2=0;
            strcpy(d4, tmp+1);
            panel--;
            spell_list[panel].entry[type=='w'?0:1][nchar-'a'].flag = LIST_ENTRY_USED;
            strcpy(spell_list[panel].entry[type=='w'?0:1][nchar-'a'].icon_name, icon);
            sprintf(line,"%s%s", GetIconDirectory(),icon);
            spell_list[panel].entry[type=='w'?0:1][nchar-'a'].icon = sprite_load_file(line,0);

            strcpy(spell_list[panel].entry[type=='w'?0:1][nchar-'a'].name, name);
            strcpy(spell_list[panel].entry[type=='w'?0:1][nchar-'a'].desc[0], d1);
            strcpy(spell_list[panel].entry[type=='w'?0:1][nchar-'a'].desc[1], d2);
            strcpy(spell_list[panel].entry[type=='w'?0:1][nchar-'a'].desc[2], d3);
            strcpy(spell_list[panel].entry[type=='w'?0:1][nchar-'a'].desc[3], d4);
        }
        fclose( stream );
		LOG(LOG_DEBUG," found file!(%d/%x)",srv_client_files[SRV_CLIENT_SPELLS].len,srv_client_files[SRV_CLIENT_SPELLS].crc );
    }
	LOG(LOG_DEBUG,"done.\n");
}

void read_help_files(void)
{
    FILE *stream;
	char *temp_buf;
	struct stat statbuf;
	int i;

	srv_client_files[SRV_CLIENT_HFILES].len = 0;
	srv_client_files[SRV_CLIENT_HFILES].crc = 0;
	LOG(LOG_DEBUG,"Reading %s....",FILE_CLIENT_HFILES);
    if( (stream = fopen(FILE_CLIENT_HFILES, "rb" )) != NULL )
    {
		/* temp load the file and get the data we need for compare with server */
		fstat (fileno (stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_HFILES].len = i;
		temp_buf=malloc(i);
		if (fread(temp_buf, sizeof(char), i, stream))
			srv_client_files[SRV_CLIENT_HFILES].crc = crc32(1L,temp_buf,i);
		free(temp_buf);
	    fclose( stream );
		LOG(LOG_DEBUG," found file!(%d/%x)",srv_client_files[SRV_CLIENT_HFILES].len,srv_client_files[SRV_CLIENT_HFILES].crc );
	}
	LOG(LOG_DEBUG,"done.\n");
}

void read_skills(void)
{
    int i,ii,panel;
	char *temp_buf;
    char nchar, *tmp, *tmp2;
	struct stat statbuf;
    FILE *stream;
    char line[255], name[255], d1[255], d2[255], d3[255], d4[255], icon[128];

	for(i=0;i<SKILL_LIST_MAX;i++)
	{
		for(ii=0;ii<DIALOG_LIST_ENTRY;ii++)
		{
			skill_list[i].entry[ii].flag=LIST_ENTRY_UNUSED;
            skill_list[i].entry[ii].name[0]=0;
        }
	}

	skill_list_set.group_nr = 0;
	skill_list_set.entry_nr = 0;

	srv_client_files[SRV_CLIENT_SKILLS].len = 0;
	srv_client_files[SRV_CLIENT_SKILLS].crc = 0;

	LOG(LOG_DEBUG,"Reading %s....",FILE_CLIENT_SKILLS);
    if( (stream = fopen(FILE_CLIENT_SKILLS, "rb" )) != NULL )
    {

		/* temp load the file and get the data we need for compare with server */
		fstat (fileno (stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_SKILLS].len = i;
		temp_buf=malloc(i);
		if (fread(temp_buf, sizeof(char), i, stream))
			srv_client_files[SRV_CLIENT_SKILLS].crc = crc32(1L,temp_buf,i);
		free(temp_buf);
		rewind(stream);

        for(i=0;;i++)
        {
            if( fgets( line, 255, stream ) == NULL)
                break;
            line[250]=0;
            tmp = strchr(line,'"' );
            tmp2 = strchr(tmp+1,'"' );
            *tmp2=0;
            strcpy(name, tmp+1);
            if( fgets( line, 255, stream ) == NULL)
                break;
            sscanf(line,"%d %c %s", &panel, &nchar, icon);
            if( fgets( line, 255, stream ) == NULL)
                break;
            line[250]=0;
            tmp = strchr(line,'"' );
            tmp2 = strchr(tmp+1,'"' );
            *tmp2=0;
            strcpy(d1, tmp+1);
            if( fgets( line, 255, stream ) == NULL)
                break;
            line[250]=0;
            tmp = strchr(line,'"' );
            tmp2 = strchr(tmp+1,'"' );
            *tmp2=0;
            strcpy(d2, tmp+1);
            if( fgets( line, 255, stream ) == NULL)
                break;
            line[250]=0;
            tmp = strchr(line,'"' );
            tmp2 = strchr(tmp+1,'"' );
            *tmp2=0;
            strcpy(d3, tmp+1);
            if( fgets( line, 255, stream ) == NULL)
                break;
            line[250]=0;
            tmp = strchr(line,'"' );
            tmp2 = strchr(tmp+1,'"' );
            *tmp2=0;
            strcpy(d4, tmp+1);

            skill_list[panel].entry[nchar-'a'].flag = LIST_ENTRY_USED;
            skill_list[panel].entry[nchar-'a'].exp=0;
            skill_list[panel].entry[nchar-'a'].exp_level=0;

            strcpy(skill_list[panel].entry[nchar-'a'].icon_name, icon);
            sprintf(line,"%s%s", GetIconDirectory(),icon);
            skill_list[panel].entry[nchar-'a'].icon = sprite_load_file(line,0);

            strcpy(skill_list[panel].entry[nchar-'a'].name, name);
            strcpy(skill_list[panel].entry[nchar-'a'].desc[0], d1);
            strcpy(skill_list[panel].entry[nchar-'a'].desc[1], d2);
            strcpy(skill_list[panel].entry[nchar-'a'].desc[2], d3);
            strcpy(skill_list[panel].entry[nchar-'a'].desc[3], d4);
        }
        fclose( stream );
		LOG(LOG_DEBUG," found file!(%d/%x)",srv_client_files[SRV_CLIENT_SKILLS].len,srv_client_files[SRV_CLIENT_SKILLS].crc );
    }
	LOG(LOG_DEBUG,"done.\n");
}


int get_quickslot(int x, int y)
{
	int i;

	for(i=0;i<MAX_QUICK_SLOTS;i++)
	{
		if(x>=SKIN_POS_QUICKSLOT_X+quickslots_pos[i][0] &&
					x<=SKIN_POS_QUICKSLOT_X+quickslots_pos[i][0]+32 &&
					y>=SKIN_POS_QUICKSLOT_Y+quickslots_pos[i][1] &&
					y<=SKIN_POS_QUICKSLOT_Y+quickslots_pos[i][1]+32)
			return i;

	}
	return -1;
}

void show_quickslots(int x, int y)
{
   int i, mx, my;
   char buf[16];

   SDL_GetMouseState(&mx, &my);
   update_quickslots(-1);
   sprite_blt(Bitmaps[BITMAP_QUICKSLOTS],x, y, NULL, NULL);

   for(i=MAX_QUICK_SLOTS-1;i>=0;i--)
   {
      if(quick_slots[i].tag != -1)
      {
         /* spell in quickslot */
         if (quick_slots[i].spell == TRUE)
         {
			 sprite_blt(spell_list[quick_slots[i].groupNr].entry[quick_slots[i].classNr][quick_slots[i].tag].icon ,x+quickslots_pos[i][0],y+quickslots_pos[i][1], NULL, NULL);
            if (mx >= x+quickslots_pos[i][0] && mx < x+quickslots_pos[i][0]+33
               && my >= y+quickslots_pos[i][1] && my < y+quickslots_pos[i][1]+33)
            show_tooltip(mx, my, spell_list[quick_slots[i].groupNr].entry[quick_slots[i].classNr][quick_slots[i].tag].name);
         }
         /* item in quickslot */
         else
         {
            item *tmp;
            tmp = locate_item_from_item(cpl.ob , quick_slots[i].tag);
            if(tmp)
            {
               blt_inv_item(tmp , x+quickslots_pos[i][0],y+quickslots_pos[i][1]);
               /* show tooltip */
               if (mx >= x+quickslots_pos[i][0] && mx < x+quickslots_pos[i][0]+33
                  && my >= y+quickslots_pos[i][1] && my < y+quickslots_pos[i][1]+33)
               show_tooltip(mx, my, tmp->s_name);
            }
         }
       }
       sprintf(buf,"F%d", i+1);
       StringBlt(ScreenSurface, &Font6x3Out,buf,x+quickslots_pos[i][0]+12, y+quickslots_pos[i][1]-6,COLOR_DEFAULT, NULL, NULL);
   }
}

void update_quickslots(int del_item)
{
   int i;

   for(i=0;i<MAX_QUICK_SLOTS;i++)
   {
       if(quick_slots[i].tag == del_item)
          quick_slots[i].tag =-1;
       if(quick_slots[i].tag == -1)
          continue;
       /* only items in the *main* inventory can used with quickslot! */
       if(quick_slots[i].spell == FALSE && !locate_item_from_inv(cpl.ob->inv , quick_slots[i].tag))
       quick_slots[i].tag=-1;
       if(quick_slots[i].tag != -1)
		   quick_slots[i].nr = locate_item_nr_from_tag (cpl.ob->inv, quick_slots[i].tag);
   }
}


/******************************************************************
 Restore quickslots from last game.
******************************************************************/
#define QUICKSLOT_FILE "quick.dat"
void load_quickslots_entrys()
{
   int i;
   FILE *stream;

   if(!(stream = fopen( QUICKSLOT_FILE, "rb" )))
      return;
   for(i=0;i<MAX_QUICK_SLOTS;i++)
   {
		if (!fread(&quick_slots[i],1,sizeof(_quickslot),stream))
			continue;

	  if (quick_slots[i].tag == -1)
         continue;
      cpl.win_inv_slot =quick_slots[i].invSlot;
      if (quick_slots[i].spell == FALSE)
		  quick_slots[i].tag = locate_item_tag_from_nr (cpl.ob->inv, quick_slots[i].nr);
   }
   fclose (stream);
   update_quickslots(-1);
}

/******************************************************************
 Save the current quickslots.
******************************************************************/
void save_quickslots_entrys()
{
   FILE *stream;

   if(!(stream = fopen( QUICKSLOT_FILE, "wb" )))
      return;

   fwrite(&quick_slots,sizeof(_quickslot),MAX_QUICK_SLOTS,stream);
   fclose (stream);
}

void show_target(int x, int y)
{
	char *ptr=NULL;
    SDL_Rect box;
	double temp;
	int hp_tmp;

	sprite_blt(Bitmaps[cpl.target_mode?BITMAP_TARGET_ATTACK:BITMAP_TARGET_NORMAL],x, y, NULL, NULL);

	sprite_blt(Bitmaps[BITMAP_TARGET_HP_B],x-1, y+20, NULL, NULL);

	/* redirect target_hp to our hp - server don't send it
	 * because we should now our hp exactly
	 */
	hp_tmp = (int) cpl.target_hp ;
	if(cpl.target_code==0)
		hp_tmp = (int)(((float)cpl.stats.hp / (float)cpl.stats.maxhp)*100.0f);

	if(cpl.target_code==0)
	{
		if(cpl.target_mode)
			ptr = "target self (hold attack)";
		else
			ptr = "target self";
	}
	else if(cpl.target_code==1)
	{
		if(cpl.target_mode)
			ptr = "target and attack enemy";
		else
			ptr = "target enemy";
	}
	else if(cpl.target_code==2)
	{
		if(cpl.target_mode)
			ptr = "target friend (hold attack)";
		else
			ptr = "target friend";
	}
	if(cpl.target_code)
		sprite_blt(Bitmaps[BITMAP_TARGET_TALK],x+270, y+27, NULL, NULL);

	if(options.show_target_self || cpl.target_code!=0)
	{
		if(hp_tmp)
		{
			temp = (double)hp_tmp*0.01;
			box.x =0;
		    box.y = 0;
		    box.h = Bitmaps[BITMAP_TARGET_HP]->bitmap->h;
		    box.w = (int) (Bitmaps[BITMAP_TARGET_HP]->bitmap->w*temp);
		    if(!box.w)
				box.w =1;
		     if(box.w > Bitmaps[BITMAP_TARGET_HP]->bitmap->w)
				box.w=Bitmaps[BITMAP_TARGET_HP]->bitmap->w;
			sprite_blt(Bitmaps[BITMAP_TARGET_HP],x, y+21, &box, NULL);
		}

		if(ptr)
		{
			StringBlt(ScreenSurface, &SystemFont,cpl.target_name ,x+30, y, cpl.target_color, NULL, NULL);
			StringBlt(ScreenSurface, &SystemFont,ptr ,x+30, y+11, cpl.target_color, NULL, NULL);
		}
	}
}
