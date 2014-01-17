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
     5, 8, 3, 6, 0, 1.0,
     4, 5, 4, 4, 9, "magic_fire.ogg",
     SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_DUST | SPELL_USE_BOOK | SPELL_USE_POTION, SPELL_DESC_DIRECTION,
     PATH_FIRE, "firebreath", NULL},

    {"icestorm",
     5, 8, 3, 6, 0, 1.0,
     4, 5, 4, 4, 9, "magic_ice.ogg",
     SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK | SPELL_USE_POTION | SPELL_USE_DUST, SPELL_DESC_DIRECTION,
     PATH_FROST, "icestorm", NULL},

    {"minor healing",
     4, 8, 3, 6, 3, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_SCROLL | SPELL_USE_ROD | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
     PATH_RESTORE, "meffect_green", NULL},

    {"cure poison",
     5, 8, 3, 6, 4, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
     PATH_RESTORE, "meffect_purple", NULL},

    {"cure disease",
     5, 8, 3, 6, 4, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
     PATH_RESTORE, "meffect_purple", NULL},

    {"strength self",
     5, 8, 3, 6, 0, 1.0,
     0, 0, 0, 4, 0, "magic_stat.ogg",
     SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_POTION | SPELL_USE_ROD | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN,
     PATH_SELF, "meffect_yellow", NULL},

    {"identify",
     5, 8, 3, 6, 2, 1.0,
     0, 0, 0, 0, 0, "magic_default.ogg",
     SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN,
     PATH_INFO, "meffect_pink", NULL},

    {"asteroid",
     5, 32, 2, 0, 3, 1.0,
     4, 10, 4, 16, 8, "magic_rstrike.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_FROST, "asteroid", NULL},

    {"frost nova",
     5, 42, 2, 0, 3, 1.0,
     4, 10, 4, 8, 6, "magic_rstrike.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_FROST, "asteroid", NULL},

    {"remove curse",
     5, 8, 3, 6, 2, 1.0,
     0, 0, 0, 0, 0, "magic_default.ogg",
     SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN | SPELL_DESC_FRIENDLY,
     PATH_TURNING, "meffect_blue", NULL},

    {"remove damnation",
     5, 8, 3, 6, 2, 1.0,
     0, 0, 0, 0, 0, "magic_default.ogg",
     SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN | SPELL_DESC_FRIENDLY,
     PATH_TURNING, "meffect_blue", NULL},

    {"cause light wounds",
     4, 8, 3, 6, 0, 1.0,
     4, 5, 4, 4, 9, "magic_wound.ogg",
     SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_DIRECTION,
     PATH_WOUNDING, "cause_wounds", NULL},

    {"confuse",
     5, 8, 3, 6, 0, 1.0,
     4, 5, 4, 4, 9, "magic_confusion.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_MIND, NULL, NULL},

    {"magic bullet",
     4, 8, 3, 6, 0, 1.0,
     4, 5, 4, 4, 9, "magic_bullet1.ogg",
     SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK, SPELL_DESC_DIRECTION,
     PATH_MISSILE, "bullet", NULL},

    {"summon golem",
     5, 8, 3, 6, 0, 1.0,
     4, 5, 4, 4, 9, "magic_summon1.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_SUMMON, NULL, NULL},

    {"remove depletion",
     5, 8, 3, 6,0, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_SELF | SPELL_DESC_TOWN,
     PATH_RESTORE, "meffect_purple", NULL},

    {"probe",
     2, 8, 3, 6, 0, 1.0,
     0, 0, 0, 0, 0, "magic_default.ogg",
     SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK, SPELL_DESC_TOWN | SPELL_DESC_DIRECTION,
     PATH_INFO, "probebullet", NULL},

    {"town portal",
     30, 8, 1, 0, 3, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF,
     PATH_TELE, "perm_magic_portal", NULL},

    {"create food",
     30, 8, 1, 0, 3, 1.0,
     3, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
     PATH_CREATE, NULL, NULL},

    {"word of recall",
     5, 24, 1, 19, 3, 1.0,
     25, 3, 3, 4, 0, "magic_teleport.ogg",
     SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
     PATH_TELE, NULL, NULL},

    {"recharge",
     50, 100, 2, 10, 0, 1.0,
     0, 0, 0, 0, 0, "magic_invisible.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
     PATH_TRANSMUTE, NULL, NULL},

    {"greater healing",
     6, 8, 3, 0, 3, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_SCROLL | SPELL_USE_ROD | SPELL_USE_POTION, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
     PATH_RESTORE, "meffect_green", NULL},

    {"restoration",
     10, 12, 3, 0, 3, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
     PATH_RESTORE, "meffect_green", NULL},

    {"protection from cold",
     22, 100, 2, 0, 3, 1.0,
     1, 350, 3, 4, 0, "magic_stat.ogg",
     SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
     PATH_PROT, NULL, NULL},

    {"protection from fire",
     22, 100, 2, 0, 3, 1.0,
     1, 350, 3, 4, 0, "magic_stat.ogg",
     SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
     PATH_PROT, NULL, NULL},

    {"protection from electricity",
     22, 100, 2, 0, 3, 1.0,
     1, 350, 3, 4, 0, "magic_stat.ogg",
     SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
     PATH_PROT, NULL, NULL},

    {"protection from poison",
     22, 100, 2, 0, 3, 1.0,
     1, 350, 3, 4, 0, "magic_stat.ogg",
     SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
     PATH_PROT, NULL, NULL},

    {"consecrate",
     10, 70, 2, 0, 3, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
     PATH_TRANSMUTE, NULL, NULL},

    {"finger of death",
     7, 12, 2, 0, 5, 1.0,
     8, 0, 2, 0, 15, "magic_hword.ogg",
     SPELL_USE_CAST, SPELL_DESC_ENEMY,
     PATH_DEATH, "spellobject_finger_of_death", NULL},

    {"cause cold",
     10, 50, 2, 0, 3, 1.0,
     8, 0, 4, 2, 7, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_DEATH, "disease_cold", NULL},

    {"cause flu",
     12, 50, 2, 0, 3, 1.0,
     10, 0, 4, 2, 7, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_DEATH, "flu", NULL},

    {"cause leprosy",
     14, 58, 2, 0, 3, 1.0,
     6, 0, 4, 2, 7, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_DEATH, "leprosy", NULL},

    {"cause smallpox",
     16, 58, 2, 0, 3, 1.0,
     8, 0, 4, 2, 7, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_DEATH, "smallpox", NULL},

    {"cause pneumonic plague",
     18, 58, 2, 0, 3, 1.0,
     8, 0, 4, 2, 7, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_DEATH, "pneumonic_plague", NULL},

    {"meteor",
     5, 32, 2, 0, 3, 1.0,
     4, 10, 4, 16, 8, "magic_rstrike.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_FIRE, "meteor", NULL},

    {"meteor swarm",
     5, 42, 2, 0, 3, 1.0,
     4, 10, 4, 8, 6, "magic_rstrike.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_FIRE, "meteor", NULL},

    {"poison fog",
     18, 86, 2, 0, 3, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_WOUNDING, NULL, NULL},

    {"bullet swarm",
     5, 86, 2, 0, 3, 1.0,
     0, 0, 0, 0, 9, "magic_bullet1.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_MISSILE, "bullet", NULL},

    {"bullet storm",
     4, 86, 2, 0, 3, 1.0,
     0, 0, 0, 0, 9, "magic_bullet1.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_MISSILE, "bullet", NULL},

    {"destruction",
     20, 20, 0, 3, 3, 1.0,
     4, 1, 3, 20, 24, "magic_destruction.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_WOUNDING, "spellobject_destruction", NULL},

    {"create bomb",
     10, 30, 2, 0, 3, 1.0,
     10, 9, 0, 30, 9, "magic_bomb.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_DETONATE, "bomb", NULL},

    {"cure confusion",
     5, 8, 3, 6,4, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
     PATH_RESTORE, "meffect_purple", NULL},

    {"transform wealth",
     18, 40, 2, 7, 0, 1.0,
     0, 0, 0, 0, 0, "magic_turn.ogg",
     SPELL_USE_CAST | SPELL_USE_WAND, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
     PATH_TRANSMUTE, NULL, NULL},

    {"magic missile",
     3, 8, 3, 6, 9, 1.0,
     4, 5, 4, 4, 9, "magic_missile.ogg",
     SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_ENEMY,
     PATH_MISSILE, "magic_missile", NULL},

    {"rain of healing",
     6, 18, 0, 0, 0, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
     PATH_RESTORE, "meffect_green", NULL},

    {"party heal",
     6, 16, 0, 0, 0, 1.0,
     0, 0, 0, 0, 0, "magic_stat.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
     PATH_RESTORE, "meffect_green", NULL},

    {"frostbolt",
     5, 8, 3, 6, 0, 1.0,
     4, 7, 4, 0, 9, "magic_ice.ogg",
     SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
     PATH_FROST, "frostbolt", NULL},

    {"firebolt",
     5, 8, 3, 6, 0, 1.0,
     4, 7, 4, 0, 9, "magic_fire.ogg",
     SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
     PATH_FIRE, "firebolt", NULL},

    {"lightning",
     5, 8, 3, 6, 0, 1.0,
     4, 7, 4, 0, 9, "magic_elec.ogg",
     SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
     PATH_ELEC, "lightning", NULL},

    {"forked lightning",
     5, 8, 3, 6, 0, 1.0,
     4, 4, 4, 8, 9, "magic_elec.ogg",
     SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
     PATH_ELEC, "forked_lightning", NULL},

    {"negative energy bolt",
     5, 8, 3, 6, 0, 1.0,
     4, 5, 4, 8, 9, "magic_elec.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_DEATH, "negabolt", NULL},

    {"holy word",
     6, 12, 0, 0, 0, 1.0,
     4, 6, 4, 4, 9, "magic_hword.ogg",
     SPELL_USE_CAST, SPELL_DESC_DIRECTION,
     PATH_TURNING, "holyword", NULL}
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
