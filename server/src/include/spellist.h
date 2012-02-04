/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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

#ifndef SPELLIST_H
#define SPELLIST_H

/** Array of all the spells. */
spell_struct spells[NROFREALSPELLS] =
{
	{"firestorm",
	 15, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_fire.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_DUST | SPELL_USE_BOOK | SPELL_USE_POTION, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "firebreath", 50000
	},

	{"icestorm",
	 15, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_ice.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK | SPELL_USE_POTION | SPELL_USE_DUST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "icestorm", 50000
	},

	{"minor healing",
	 1, 4, 8, 3, 6, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_SCROLL | SPELL_USE_ROD | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", 0
	},

	{"cure poison",
	 15, 5, 8, 3, 6, 4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple", 25000
	},

	{"cure disease",
	 15, 5, 8, 3, 6, 4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple", 25000
	},

	{"strength self",
	 1, 5, 8, 3, 6, 0, 1.0,
	 0, 0, 0, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_POTION | SPELL_USE_ROD | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_SELF, "meffect_yellow", 0
	},

	{"identify",
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_INFO, "meffect_pink", 0
	},

	{"asteroid",
	 30, 5, 32, 2, 0, 3, 1.0,
	 4, 10, 4, 16, 8, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "asteroid", 75000
	},

	{"frost nova",
	 70, 5, 42, 2, 0, 3, 1.0,
	 4, 10, 4, 8, 6, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "asteroid", 200000
	},

	{"remove curse",
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN | SPELL_DESC_FRIENDLY,
	 PATH_TURNING, "meffect_blue", 0
	},

	{"remove damnation",
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN | SPELL_DESC_FRIENDLY,
	 PATH_TURNING, "meffect_blue", 0
	},

	{"cause light wounds",
	 1, 4, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_wound.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, "cause_wounds", 0
	},

	{"confuse",
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_confusion.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MIND, NULL, 0
	},

	{"magic bullet",
	 1, 4, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_bullet1.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet", 0
	},

	{"summon golem",
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_summon1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_SUMMON, NULL, 0
	},

	{"remove depletion",
	 1, 5, 8, 3, 6,0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple", 0
	},

	{"probe",
	 1, 2, 8, 3, 6, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK, SPELL_DESC_TOWN | SPELL_DESC_DIRECTION,
	 PATH_INFO, "probebullet", 0
	},

	{"town portal",
	 15, 30, 8, 1, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF,
	 PATH_TELE, "perm_magic_portal", 0
	},

	{"create food",
	 1, 30, 8, 1, 0, 3, 1.0,
	 3, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_CREATE, NULL, 0
	},

	{"word of recall",
	 12, 5, 24, 1, 19, 3, 1.0,
	 25, 3, 3, 4, 0, "magic_teleport.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TELE, NULL, 0
	},

	{"recharge",
	 16, 50, 100, 2, 10, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_invisible.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL, 0
	},

	{"greater healing",
	 10, 6, 8, 3, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_SCROLL | SPELL_USE_ROD | SPELL_USE_POTION, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", 20000
	},

	{"restoration",
	 20, 10, 12, 3, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", 0
	},

	{"protection from cold",
	 30, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, 50000
	},

	{"protection from fire",
	 30, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, 50000
	},

	{"protection from electricity",
	 30, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, 50000
	},

	{"protection from poison",
	 30, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, 50000
	},

	{"consecrate",
	 25, 10, 70, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL, 30000
	},

	{"finger of death",
	 20, 7, 12, 2, 0, 5, 1.0,
	 8, 0, 2, 0, 15, "magic_hword.ogg",
	 SPELL_USE_CAST, SPELL_DESC_ENEMY,
	 PATH_DEATH, "spellobject_finger_of_death", 30000
	},

	{"cause cold",
	 5, 10, 50, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "disease_cold", 0
	},

	{"cause flu",
	 10, 12, 50, 2, 0, 3, 1.0,
	 10, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "flu", 0
	},

	{"cause leprosy",
	 15, 14, 58, 2, 0, 3, 1.0,
	 6, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "leprosy", 0
	},

	{"cause smallpox",
	 20, 16, 58, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "smallpox", 0
	},

	{"cause pneumonic plague",
	 25, 18, 58, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "pneumonic_plague", 0
	},

	{"meteor",
	 30, 5, 32, 2, 0, 3, 1.0,
	 4, 10, 4, 16, 8, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "meteor", 75000
	},

	{"meteor swarm",
	 70, 5, 42, 2, 0, 3, 1.0,
	 4, 10, 4, 8, 6, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "meteor", 200000
	},

	{"poison fog",
	 5, 18, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, NULL, 0
	},

	{"bullet swarm",
	 65, 5, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 9, "magic_bullet1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet", 175000
	},

	{"bullet storm",
	 50, 4, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 9, "magic_bullet1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet", 125000
	},

	{"destruction",
	 30, 20, 20, 0, 3, 3, 1.0,
	 4, 1, 3, 20, 24, "magic_destruction.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, "spellobject_destruction", 40000
	},

	{"create bomb",
	 10, 10, 30, 2, 0, 3, 1.0,
	 10, 9, 0, 30, 9, "magic_bomb.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DETONATE, "bomb", 0
	},

	{"cure confusion",
	 1, 5, 8, 3, 6,4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple", 5000
	},

	{"transform wealth",
	 15, 18, 40, 2, 7, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_turn.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL, 30000
	},

	{"magic missile",
	 40, 3, 8, 3, 6, 9, 1.0,
	 4, 5, 4, 4, 9, "magic_missile.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_ENEMY,
	 PATH_MISSILE, "magic_missile", 75000
	},

	{"rain of healing",
	 20, 6, 18, 0, 0, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", 40000
	},

	{"party heal",
	 20, 6, 16, 0, 0, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", 40000
	},

	{"frostbolt",
	 20, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_ice.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_FROST, "frostbolt", 65000
	},

	{"firebolt",
	 20, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_fire.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "firebolt", 65000
	},

	{"lightning",
	 20, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_elec.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_ELEC, "lightning", 65000
	},

	{"forked lightning",
	 25, 5, 8, 3, 6, 0, 1.0,
	 4, 4, 4, 8, 9, "magic_elec.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_ELEC, "forked_lightning", 80000
	},

	{"negative energy bolt",
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 8, 9, "magic_elec.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "negabolt", 0
	},

	{"holy word",
	 35, 6, 12, 0, 0, 0, 1.0,
	 4, 6, 4, 4, 9, "magic_hword.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_TURNING, "holyword", 60000
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

#endif
