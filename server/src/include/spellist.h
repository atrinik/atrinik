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
 * Spells list. */

#include "sounds.h"
#include "spells.h"

/** Array of all the spells. */
spell spells[NROFREALSPELLS] =
{
	{"firestorm", SPELL_TYPE_WIZARD,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_fire.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_DUST | SPELL_USE_BOOK | SPELL_USE_POTION, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "firebreath"
	},

	{"icestorm", SPELL_TYPE_WIZARD,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_ice.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK | SPELL_USE_POTION | SPELL_USE_DUST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "icestorm"
	},

	{"minor healing", SPELL_TYPE_PRIEST,
	 1, 4, 8, 3, 6, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_SCROLL | SPELL_USE_ROD | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green"
	},

	{"cure poison", SPELL_TYPE_PRIEST,
	 1, 5, 8, 3, 6, 4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple"
	},

	{"cure disease", SPELL_TYPE_PRIEST,
	 1, 5, 8, 3, 6, 4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple"
	},

	{"strength self", SPELL_TYPE_WIZARD,
	 1, 5, 8, 3, 6, 0, 1.0,
	 0, 0, 0, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_POTION | SPELL_USE_ROD | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_SELF, "meffect_yellow"
	},

	{"identify", SPELL_TYPE_WIZARD,
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_INFO, "meffect_pink"
	},

	{"asteroid", SPELL_TYPE_WIZARD,
	 30, 20, 32, 2, 0, 3, 1.0,
	 4, 10, 4, 16, 20, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "asteroid"
	},

	{"frost nova", SPELL_TYPE_WIZARD,
	 80, 70, 60, 2, 0, 3, 1.0,
	 4, 10, 4, 8, 35, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "asteroid"
	},

	{"remove curse", SPELL_TYPE_PRIEST,
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN | SPELL_DESC_FRIENDLY,
	 PATH_TURNING, "meffect_blue"
	},

	{"remove damnation", SPELL_TYPE_PRIEST,
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN | SPELL_DESC_FRIENDLY,
	 PATH_TURNING, "meffect_blue"
	},

	{"cause light wounds", SPELL_TYPE_PRIEST,
	 1, 4, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_wound.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, "cause_wounds"
	},

	{"confuse", SPELL_TYPE_WIZARD,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_confusion.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MIND, NULL
	},

	{"magic bullet", SPELL_TYPE_WIZARD,
	 1, 4, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_bullet1.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet"
	},

	{"summon golem", SPELL_TYPE_WIZARD,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_summon1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_SUMMON, NULL
	},

	{"remove depletion", SPELL_TYPE_PRIEST,
	 1, 5, 8, 3, 6,0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple"
	},

	{"probe", SPELL_TYPE_WIZARD,
	 1, 2, 8, 3, 6, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK, SPELL_DESC_TOWN | SPELL_DESC_DIRECTION,
	 PATH_INFO, "probebullet"
	},

	{"town portal", SPELL_TYPE_PRIEST,
	 15, 30, 8, 1, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF,
	 PATH_TELE, "perm_magic_portal"
	},

	{"create food", SPELL_TYPE_WIZARD,
	 1, 30, 8, 1, 0, 3, 1.0,
	 3, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_CREATE, NULL
	},

	{"word of recall", SPELL_TYPE_PRIEST,
	 12, 5, 24, 1, 19, 3, 1.0,
	 25, 3, 3, 4, 0, "magic_teleport.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_HORN, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TELE, NULL
	},

	{"recharge", SPELL_TYPE_WIZARD,
	 16, 50, 100, 2, 10, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_invisible.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL
	},

	{"greater healing", SPELL_TYPE_PRIEST,
	 10, 6, 12, 3, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_SCROLL | SPELL_USE_ROD | SPELL_USE_POTION, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green"
	},

	{"restoration", SPELL_TYPE_PRIEST,
	 20, 10, 12, 3, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green"
	},

	{"protection from cold", SPELL_TYPE_PRIEST,
	 24, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL
	},

	{"protection from fire", SPELL_TYPE_PRIEST,
	 24, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL
	},

	{"protection from electricity", SPELL_TYPE_PRIEST,
	 24, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL
	},

	{"protection from poison", SPELL_TYPE_PRIEST,
	 24, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL
	},

	{"consecrate", SPELL_TYPE_PRIEST,
	 15, 10, 200, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL
	},

	{"finger of death", SPELL_TYPE_PRIEST,
	 18, 7, 18, 2, 0, 5, 1.0,
	 8, 0, 2, 0, 15, "magic_hword.ogg",
	 SPELL_USE_CAST, SPELL_DESC_ENEMY,
	 PATH_DEATH, "spellobject_finger_of_death"
	},

	{"cause cold", SPELL_TYPE_PRIEST,
	 5, 10, 50, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "disease_cold"
	},

	{"cause flu", SPELL_TYPE_PRIEST,
	 10, 12, 50, 2, 0, 3, 1.0,
	 10, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "flu"
	},

	{"cause leprosy", SPELL_TYPE_PRIEST,
	 15, 14, 58, 2, 0, 3, 1.0,
	 6, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "leprosy"
	},

	{"cause smallpox", SPELL_TYPE_PRIEST,
	 20, 16, 58, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "smallpox"
	},

	{"cause pneumonic plague", SPELL_TYPE_PRIEST,
	 25, 18, 58, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "pneumonic_plague"
	},

	{"meteor", SPELL_TYPE_WIZARD,
	 30, 20, 32, 2, 0, 3, 1.0,
	 4, 10, 4, 16, 20, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "meteor"
	},

	{"meteor swarm", SPELL_TYPE_WIZARD,
	 80, 70, 60, 2, 0, 3, 1.0,
	 4, 10, 4, 8, 35, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "meteor"
	},

	{"poison fog", SPELL_TYPE_WIZARD,
	 5, 18, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, NULL
	},

	{"bullet swarm", SPELL_TYPE_WIZARD,
	 25, 18, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_bullet1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet"
	},

	{"bullet storm",SPELL_TYPE_WIZARD,
	 20, 18, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_bullet1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet"
	},

	{"destruction", SPELL_TYPE_WIZARD,
	 18, 20, 20, 0, 3, 3, 1.0,
	 2, 1, 3, 20, 20, "magic_destruction.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, "spellobject_destruction"
	},

	{"create bomb", SPELL_TYPE_WIZARD,
	 10, 10, 30, 2, 0, 3, 1.0,
	 10, 9, 0, 30, 9, "magic_bomb.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DETONATE, "bomb"
	},

	{"cure confusion", SPELL_TYPE_PRIEST,
	 1, 5, 8, 3, 6,4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple"
	},

	{"transform wealth", SPELL_TYPE_WIZARD,
	 15, 18, 40, 2, 7, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_turn.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL
	},

	{"magic missile", SPELL_TYPE_WIZARD,
	 1, 3, 8, 3, 6, 9, 1.0,
	 4, 5, 4, 4, 9, "magic_missile.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_ENEMY,
	 PATH_MISSILE, "magic_missile"
	},

	{"rain of healing", SPELL_TYPE_PRIEST,
	 20, 6, 22, 0, 0, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green"
	},

	{"party heal", SPELL_TYPE_PRIEST,
	 20, 6, 30, 0, 0, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green"
	},

	{"frostbolt", SPELL_TYPE_WIZARD,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_ice.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_FROST, "frostbolt"
	},

	{"firebolt", SPELL_TYPE_WIZARD,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_fire.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "firebolt"
	},

	{"lightning", SPELL_TYPE_WIZARD,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_elec.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_ELEC, "lightning"
	},

	{"forked lightning", SPELL_TYPE_WIZARD,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 4, 4, 8, 9, "magic_elec.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_ELEC, "forked_lightning"
	},

	{"negative energy bolt", SPELL_TYPE_WIZARD,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 8, 9, "magic_elec.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "negabolt"
	}
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
