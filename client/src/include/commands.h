/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 2 of the License, or     *
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

/**
 * @file
 * Commands header file. */

#ifndef COMMANDS_H
#define COMMANDS_H

/** Data packed command. */
#define DATA_PACKED_CMD 0x80

/** All the data commands. */
enum
{
	DATA_CMD_NO,
	DATA_CMD_SKILL_LIST,
	DATA_CMD_SPELL_LIST,
	DATA_CMD_SETTINGS_LIST,
	DATA_CMD_ANIM_LIST,
	DATA_CMD_BMAP_LIST,
	DATA_CMD_HFILES_LIST
};

/**
 * @defgroup SPLIST_MODE_xxx SPLIST_MODE_xxx
 * Spell list commands for client's spell list.
 *@{*/
/** Remove existing spell. */
#define SPLIST_MODE_REMOVE  1
/*@}*/

extern void BookCmd(unsigned char *data, int len);
extern void PartyCmd(unsigned char *data, int len);
extern void QuickSlotCmd(char *data);
extern void SoundCmd(unsigned char *data, int len);
extern void SetupCmd(char *buf, int len);
extern void FaceCmd(unsigned char *data, int len);
extern void Face1Cmd(unsigned char *data, int len);
extern void AddMeFail();
extern void AddMeSuccess();
extern void GoodbyeCmd();
extern void AnimCmd(unsigned char *data, int len);
extern void ImageCmd(unsigned char *data, int len);
extern void DrawInfoCmd(unsigned char *data);
extern void DrawInfoCmd2(unsigned char *data, int len);
extern void StatsCmd(unsigned char *data, int len);
extern void PreParseInfoStat(char *cmd);
extern void handle_query(char *data);
extern void send_reply(char *text);
extern void PlayerCmd(unsigned char *data, int len);
extern void Item1Cmd(unsigned char *data, int len);
extern void UpdateItemCmd(unsigned char *data, int len);
extern void DeleteItem(unsigned char *data, int len);
extern void DeleteInventory(unsigned char *data);
extern void Map2Cmd(unsigned char *data, int len );
extern void map_scrollCmd(char *data);
extern void MagicMapCmd();
extern void VersionCmd(char *data);
extern void SendVersion(ClientSocket csock);
extern void SendAddMe(ClientSocket csock);
extern void RequestFile(ClientSocket csock, int index);
extern void SendSetFaceMode(ClientSocket csock, int mode);
extern void MapstatsCmd(unsigned char *data);
extern void SpelllistCmd(char *data);
extern void SkilllistCmd(char *data);
extern void SkillRdyCmd(char *data, int len);
extern void GolemCmd(unsigned char *data);
extern void ItemXCmd(unsigned char *data, int len);
extern void ItemYCmd(unsigned char *data, int len);
extern void TargetObject(unsigned char *data, int len);
extern void DataCmd(unsigned char *data, int len);
extern void NewCharCmd();
extern void ShopCmd(unsigned char *data, int len);
extern void QuestListCmd(unsigned char *data, int len);
#endif
