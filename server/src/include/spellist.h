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
 * Spells list. */

#include "sounds.h"
#include "spells.h"

/** Array of all the spells. */
spell spells[NROFREALSPELLS] =
{
	{"firestorm", SPELL_TYPE_WIZARD, 1, 5, 8, 3, 6,0, 1.0,
		4,      5,      4,      4, 9,	SOUND_MAGIC_FIRE,
		SPELL_USE_CAST|SPELL_USE_HORN|SPELL_USE_WAND|
		SPELL_USE_ROD|SPELL_USE_DUST|SPELL_USE_BOOK|SPELL_USE_POTION,
		SPELL_DESC_DIRECTION,
		PATH_FIRE, "firebreath",SPELL_ACTIVE
	},

	{"icestorm",					SPELL_TYPE_WIZARD, 1, 5, 8, 3, 6,0,1.0,
	 4,      5,      4,      4,	9,	SOUND_MAGIC_ICE,
	 SPELL_USE_CAST|SPELL_USE_HORN|SPELL_USE_WAND|SPELL_USE_ROD|
	 SPELL_USE_BOOK|SPELL_USE_POTION|SPELL_USE_DUST,
	 SPELL_DESC_DIRECTION,
	 PATH_FROST, "icestorm",SPELL_ACTIVE
	},

	{"minor healing",				SPELL_TYPE_PRIEST, 1, 4, 8, 3, 6,3,1.0,
	 0,       0,     0,      0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST|SPELL_USE_BALM|SPELL_USE_SCROLL|
	 SPELL_USE_ROD|SPELL_USE_POTION|SPELL_USE_BOOK,
	 SPELL_DESC_SELF|SPELL_DESC_FRIENDLY|SPELL_DESC_WIS|SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green",SPELL_ACTIVE
	},

	{"cure poison",					SPELL_TYPE_PRIEST, 1, 5, 8, 3, 6,4, 1.0,/* potion only */
	 0,       0,     0,      0,0,	SOUND_MAGIC_STAT,
	 SPELL_USE_CAST|SPELL_USE_POTION|SPELL_USE_BOOK,
	 SPELL_DESC_SELF|SPELL_DESC_FRIENDLY|SPELL_DESC_WIS|SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple",SPELL_ACTIVE
	},

	{"cure disease",				SPELL_TYPE_PRIEST, 1, 5, 8, 3, 6,4, 1.0,/* balm only */
	 0,       0,     0,      0,0,	SOUND_MAGIC_STAT,
	 SPELL_USE_CAST|SPELL_USE_BALM|SPELL_USE_BOOK,
	 SPELL_DESC_SELF|SPELL_DESC_FRIENDLY|SPELL_DESC_WIS|SPELL_DESC_TOWN,
	 PATH_RESTORE,"meffect_purple",SPELL_ACTIVE
	},

	{"strength self",				SPELL_TYPE_WIZARD, 1, 5, 8, 3, 6,0,1.0,
	 0,       0,     0,      4, 0,	SOUND_MAGIC_STAT,
	 SPELL_USE_CAST|SPELL_USE_HORN|SPELL_USE_WAND|SPELL_USE_POTION|
	 SPELL_USE_ROD|SPELL_USE_SCROLL|SPELL_USE_BOOK,
	 SPELL_DESC_SELF|SPELL_DESC_TOWN,
	 PATH_SELF, "meffect_yellow",SPELL_ACTIVE
	},

	{"identify",					SPELL_TYPE_WIZARD, 1, 5, 8, 3, 6,2,1.0,
	 0,       0,     0,      0,0,	SOUND_MAGIC_DEFAULT,
	 SPELL_USE_CAST|SPELL_USE_HORN|SPELL_USE_WAND|
	 SPELL_USE_ROD|SPELL_USE_SCROLL|SPELL_USE_BOOK,
	 SPELL_DESC_SELF|SPELL_DESC_TOWN,
	 PATH_INFO, "meffect_pink",SPELL_ACTIVE
	},

	{"detect magic",				SPELL_TYPE_WIZARD, 1, 5, 8, 3, 6,0,1.0,
	 0,       0,     0,      0,0,	SOUND_MAGIC_DEFAULT,
	 SPELL_USE_CAST|SPELL_USE_HORN|SPELL_USE_WAND|
	 SPELL_USE_ROD|SPELL_USE_SCROLL|SPELL_USE_BOOK,
	 SPELL_DESC_SELF|SPELL_DESC_TOWN,
	 PATH_INFO, "meffect_pink",SPELL_ACTIVE
	},

	{"detect curse",				SPELL_TYPE_PRIEST, 1, 5, 8, 3, 6,0,1.0,
	 0,       0,     0,      0,0,	SOUND_MAGIC_DEFAULT,
	 SPELL_USE_CAST|SPELL_USE_WAND|
	 SPELL_USE_ROD|SPELL_USE_SCROLL|SPELL_USE_BOOK,
	 SPELL_DESC_SELF|SPELL_DESC_TOWN|SPELL_DESC_WIS,
	 PATH_INFO, "meffect_pink",SPELL_ACTIVE
	},

	{"remove curse",				SPELL_TYPE_PRIEST, 1, 5, 8, 3, 6,2,1.0,
	 0,       0,     0,      0,0, SOUND_MAGIC_DEFAULT,
	 SPELL_USE_CAST|SPELL_USE_SCROLL|SPELL_USE_BOOK,		/* scroll */
	 SPELL_DESC_SELF|SPELL_DESC_TOWN|SPELL_DESC_FRIENDLY|SPELL_DESC_WIS,
	 PATH_TURNING, "meffect_blue",SPELL_ACTIVE
	},

	{"remove damnation",			SPELL_TYPE_PRIEST, 1, 5, 8, 3, 6,2,1.0,
	 0,       0,     0,      0,0,	SOUND_MAGIC_DEFAULT,
	 SPELL_USE_CAST|SPELL_USE_SCROLL|SPELL_USE_BOOK, /* scroll*/
	 SPELL_DESC_SELF|SPELL_DESC_TOWN|SPELL_DESC_FRIENDLY|SPELL_DESC_WIS,
	 PATH_TURNING, "meffect_blue",SPELL_ACTIVE
	},

	{"cause light wounds",			SPELL_TYPE_PRIEST, 1, 4, 8, 3, 6,0,	1.0,/* scroll*/
	 4,      5,      4,      4,	9,	SOUND_MAGIC_WOUND,
	 SPELL_USE_CAST|SPELL_USE_SCROLL|SPELL_USE_BOOK,
	 SPELL_DESC_DIRECTION|SPELL_DESC_WIS,
	 PATH_WOUNDING,"cause_wounds",SPELL_ACTIVE
	},

	/* NOT ACTIVE */
	{"confuse",						SPELL_TYPE_WIZARD, 1, 5, 8, 3, 6,0,	1.0,/* dust effect */
	 4,      5,      4,      4,	9,	SOUND_MAGIC_CONFUSION,
	 SPELL_USE_CAST|SPELL_USE_DUST|SPELL_USE_BOOK,
	 SPELL_DESC_DIRECTION,
	 PATH_MIND,NULL,SPELL_DEACTIVE
	},

	{"magic bullet",				SPELL_TYPE_WIZARD, 1, 4, 8, 3, 6,0,1.0,
	 4,      5,      4,      4, 9,	SOUND_MAGIC_BULLET1,
	 SPELL_USE_CAST|SPELL_USE_SCROLL|SPELL_USE_HORN|
	 SPELL_USE_WAND| SPELL_USE_ROD|SPELL_USE_BOOK,
	 SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet",SPELL_ACTIVE
	},

	/* NOT ACTIVE */
	{"summon golem",				SPELL_TYPE_WIZARD, 1, 5, 8, 3, 6,0,1.0,
	 4,      5,      4,      4, 9,	SOUND_MAGIC_SUMMON1,
	 SPELL_USE_CAST|SPELL_USE_HORN|SPELL_USE_WAND| SPELL_USE_ROD|SPELL_USE_BOOK,
	 SPELL_DESC_DIRECTION,
	 PATH_SUMMON, "golem",SPELL_DEACTIVE
	},

	{"remove depletion",			SPELL_TYPE_PRIEST, 1, 5, 8, 3, 6,0, 1.0,/* aka potion of restoration/life */
	 0,       0,     0,      0,0,	SOUND_MAGIC_STAT,
	 SPELL_USE_CAST, /* npc/god only atm */
	 SPELL_DESC_SELF|SPELL_DESC_TOWN|SPELL_DESC_WIS,
	 PATH_RESTORE, "meffect_purple",SPELL_ACTIVE
	},

	{"probe",						SPELL_TYPE_WIZARD, 1, 5, 8, 3, 6,0,1.0,
	 4,      5,      4,      4, 9,	SOUND_MAGIC_DEFAULT,
	 SPELL_USE_CAST|SPELL_USE_SCROLL|SPELL_USE_HORN|
	 SPELL_USE_WAND| SPELL_USE_ROD|SPELL_USE_BOOK,
	 SPELL_DESC_TOWN|SPELL_DESC_DIRECTION,
	 PATH_INFO, "probebullet",SPELL_ACTIVE
	},

	{"town portal",	        		SPELL_TYPE_PRIEST, 15, 30, 8, 1, 0, 3, 1.0,
	 0,       0,     0,      0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST|SPELL_USE_SCROLL|SPELL_USE_BOOK,
	 SPELL_DESC_SELF|SPELL_DESC_WIS,
	 PATH_TELE, "perm_magic_portal", SPELL_ACTIVE
	},

	{"create food",	        		SPELL_TYPE_WIZARD, 1, 30, 8, 1, 0, 3, 1.0,
	 25,       0,     0,      0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION|SPELL_DESC_TOWN,
	 PATH_CREATE, NULL, SPELL_ACTIVE
	},

	{"word of recall",	       		SPELL_TYPE_PRIEST, 12, 40, 24, 1, 19, 3, 1.0,
	 25,       3,     3,      4, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST|SPELL_USE_WAND|SPELL_USE_ROD|SPELL_USE_HORN, SPELL_DESC_DIRECTION|SPELL_DESC_TOWN,
	 PATH_TELE, NULL, SPELL_ACTIVE
	},

	{"recharge",                    SPELL_TYPE_WIZARD,
	 16, 50, 100, 2, 10, 0, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL, SPELL_ACTIVE
	},

	{"greater healing",             SPELL_TYPE_PRIEST,
	 30, 8, 16, 3, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_SCROLL | SPELL_USE_ROD | SPELL_USE_POTION,
	 SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_WIS | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", SPELL_ACTIVE
	},

	{"restoration",                 SPELL_TYPE_PRIEST,
	 20, 10, 12, 3, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_WIS | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", SPELL_ACTIVE
	},

	{"protection from cold",        SPELL_TYPE_PRIEST,
	 24, 22, 100, 2, 0, 3, 1.0, 15, 350, 3, 4, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST | SPELL_USE_BALM,
	 SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, SPELL_ACTIVE
	},

	{"protection from fire",        SPELL_TYPE_PRIEST,
	 24, 22, 100, 2, 0, 3, 1.0, 15, 350, 3, 4, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST | SPELL_USE_BALM,
	 SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, SPELL_ACTIVE
	},

	{"protection from electricity", SPELL_TYPE_PRIEST,
	 24, 22, 100, 2, 0, 3, 1.0, 15, 350, 3, 4, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST | SPELL_USE_BALM,
	 SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, SPELL_ACTIVE
	},

	{"protection from poison",      SPELL_TYPE_PRIEST,
	 24, 22, 100, 2, 0, 3, 1.0, 15, 350, 3, 4, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST | SPELL_USE_BALM,
	 SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, SPELL_ACTIVE
	},

	{"consecrate",                  SPELL_TYPE_PRIEST,
	 15, 10, 200, 2, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL, SPELL_ACTIVE
	},

	{"finger of death",             SPELL_TYPE_WIZARD,
	 50, 19, 86, 2, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_DEATH, NULL, SPELL_ACTIVE
	},

	{"cause cold",                  SPELL_TYPE_PRIEST,
	 5, 10, 86, 2, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_DEATH, NULL, SPELL_ACTIVE
	},

	{"cause flu",                   SPELL_TYPE_PRIEST,
	 15, 13, 86, 2, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_DEATH, NULL, SPELL_ACTIVE
	},

	{"cause leprosy",               SPELL_TYPE_PRIEST,
	 15, 14, 86, 2, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_DEATH, NULL, SPELL_ACTIVE
	},

	{"cause smallpox",              SPELL_TYPE_PRIEST,
	 20, 16, 86, 2, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_DEATH, NULL, SPELL_ACTIVE
	},

	{"cause pneumotic plague",      SPELL_TYPE_PRIEST,
	 25, 18, 86, 2, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_DEATH, NULL, SPELL_ACTIVE
	},

	{"meteor",                      SPELL_TYPE_WIZARD,
	 30, 18, 86, 2, 0, 3, 1.0, 4, 5, 4, 4, 9, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_FIRE, NULL, SPELL_ACTIVE
	},

	{"meteor swarm",                SPELL_TYPE_WIZARD,
	 75, 18, 86, 2, 0, 3, 1.0, 4, 5, 4, 4, 9, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_FIRE, "meteor", SPELL_ACTIVE
	},

	{"poison fog",                  SPELL_TYPE_WIZARD,
	 5, 18, 86, 2, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, NULL, SPELL_ACTIVE
	},

	{"bullet swarm",                SPELL_TYPE_WIZARD,
	 25, 18, 86, 2, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_MISSILE, NULL, SPELL_ACTIVE
	},

	{"bullet storm",                SPELL_TYPE_WIZARD,
	 20, 18, 86, 2, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_MISSILE, NULL, SPELL_ACTIVE
	},

	{"destruction",                 SPELL_TYPE_WIZARD,
	 18, 30, 20, 0, 3, 3, 1.0, 2, 1, 5, 4, 9, SOUND_MAGIC_DESTRUCTION,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, "spell_destruction", SPELL_ACTIVE
	},

	{"create bomb",                 SPELL_TYPE_WIZARD,
	 15, 18, 86, 2, 0, 3, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST,
	 SPELL_DESC_DIRECTION,
	 PATH_DETONATE, NULL, SPELL_ACTIVE
	},

	{"cure confusion",              SPELL_TYPE_PRIEST,
	 1, 5, 8., 3, 6,4, 1.0, 0, 0, 0, 0, 0, SOUND_MAGIC_STAT,
	 SPELL_USE_CAST | SPELL_USE_POTION | SPELL_USE_BOOK,
	 SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_WIS | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple", SPELL_ACTIVE
	},
};

/** Spell paths */
char *spellpathnames[NRSPELLPATHS] =
{
	"Protection",
	"Fire",
	"Frost",
	"Electricity",
	"Missiles",
	"Self",
	"Summoning",
	"Abjuration",
	"Restoration",
	"Detonation",
	"Mind",
	"Creation",
	"Teleportation",
	"Information",
	"Transmutation",
	"Transference",
	"Turning",
	"Wounding",
	"Death",
	"Light"
};
