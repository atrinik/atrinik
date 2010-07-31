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
	 "icon_firestorm.101", "A cone of fire you project in front of you. The spell gains strength as you grow in level, so it remains one of your best spells even at high level.",
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_fire.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_DUST | SPELL_USE_BOOK | SPELL_USE_POTION, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "firebreath"
	},

	{"icestorm", SPELL_TYPE_WIZARD,
	 "icon_icestorm.101", "A cone of ice which freezes monsters facing the caster. This spell gains power with level, so it remains useful even at high level.",
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_ice.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK | SPELL_USE_POTION | SPELL_USE_DUST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "icestorm"
	},

	{"minor healing", SPELL_TYPE_PRIEST,
	 "icon_minor_healing.101", "This prayer heals minor wounds on either the caster or the target.",
	 1, 4, 8, 3, 6, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_SCROLL | SPELL_USE_ROD | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green"
	},

	{"cure poison", SPELL_TYPE_PRIEST,
	 "icon_cure_poison.101", "This prayer cures all poison from your character.",
	 1, 5, 8, 3, 6, 4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple"
	},

	{"cure disease", SPELL_TYPE_PRIEST,
	 "icon_cure_disease.101", "This prayer cures all diseases from your character.",
	 1, 5, 8, 3, 6, 4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple"
	},

	{"strength self", SPELL_TYPE_WIZARD,
	 "icon_strength_self.101", "This spell will increase your strength by some amount. Recasting while in effect will refresh the wear out counter.",
	 1, 5, 8, 3, 6, 0, 1.0,
	 0, 0, 0, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_POTION | SPELL_USE_ROD | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_SELF, "meffect_yellow"
	},

	{"identify", SPELL_TYPE_WIZARD,
	 "icon_identify.101", "This spell identifies some items in your inventory. Number of items identified is your literacy level and your Intelligence stat combined.",
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_INFO, "meffect_pink"
	},

	{"asteroid", SPELL_TYPE_WIZARD,
	 "icon_default.101", "Fires an asteroid in front of you, which explodes into a cone of ice if it hits an object.",
	 30, 20, 32, 2, 0, 3, 1.0,
	 4, 10, 4, 16, 20, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "asteroid"
	},

	{"frost nova", SPELL_TYPE_WIZARD,
	 "icon_default.101", "Similar to the spell of asteroid, but this spell fires a bunch of asteroids in front of you, which then explode into cones of ice if they hit something.",
	 80, 70, 60, 2, 0, 3, 1.0,
	 4, 10, 4, 8, 35, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "asteroid"
	},

	{"remove curse", SPELL_TYPE_PRIEST,
	 "icon_remove_curse.101", "This prayer removes any curse from applied items. Unapplied items are not affected.",
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN | SPELL_DESC_FRIENDLY,
	 PATH_TURNING, "meffect_blue"
	},

	{"remove damnation", SPELL_TYPE_PRIEST,
	 "icon_remove_damnation.101", "This prayer removes any curse or damnation from applied items. Unapplied items are not affected.",
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN | SPELL_DESC_FRIENDLY,
	 PATH_TURNING, "meffect_blue"
	},

	{"cause light wounds", SPELL_TYPE_PRIEST,
	 "icon_cause_light_wounds.101", "This prayer creates a missile which inflicts damage to your enemies.",
	 1, 4, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_wound.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, "cause_wounds"
	},

	{"confuse", SPELL_TYPE_WIZARD,
	 NULL, NULL,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_confusion.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MIND, NULL
	},

	{"magic bullet", SPELL_TYPE_WIZARD,
	 "icon_magic_bullet.101", "This spell fires a magical bullet which does not track but instead flies in one direction until it hits something.",
	 1, 4, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_bullet1.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet"
	},

	{"summon golem", SPELL_TYPE_WIZARD,
	 NULL, NULL,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_summon1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_SUMMON, NULL
	},

	{"remove depletion", SPELL_TYPE_PRIEST,
	 "icon_remove_depletion.101", "This prayer restores depleted stats of your character.",
	 1, 5, 8, 3, 6,0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple"
	},

	{"probe", SPELL_TYPE_WIZARD,
	 "icon_probe.101", "Probe will invoke a magic ball which will give you information about the monster it hits. Probe tells you about the monster's level, the damage it does, and so on. A probe will be invisible to the monster and will invoke no aggression.",
	 1, 2, 8, 3, 6, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK, SPELL_DESC_TOWN | SPELL_DESC_DIRECTION,
	 PATH_INFO, "probebullet"
	},

	{"town portal", SPELL_TYPE_PRIEST,
	 "icon_town_portal.101", "This spell allows you to set up magic portals from one place to another.",
	 15, 30, 8, 1, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF,
	 PATH_TELE, "perm_magic_portal"
	},

	{"create food", SPELL_TYPE_WIZARD,
	 "icon_default.101", "Creates food in your inventory which you may eat, but will vanish if dropped.",
	 1, 30, 8, 1, 0, 3, 1.0,
	 3, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_CREATE, NULL
	},

	{"word of recall", SPELL_TYPE_PRIEST,
	 "icon_default.101", "Teleports you to your house, apartment, or wherever you applied a bed to reality. This spell takes a while to complete, but is faster the higher level you are.",
	 12, 5, 24, 1, 19, 3, 1.0,
	 25, 3, 3, 4, 0, "magic_teleport.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_HORN, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TELE, NULL
	},

	{"recharge", SPELL_TYPE_WIZARD,
	 "icon_default.101", "Recharges marked wand in your inventory. There is a slight chance that the wand will be destroyed when using this spell.",
	 16, 50, 100, 2, 10, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_invisible.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL
	},

	{"greater healing", SPELL_TYPE_PRIEST,
	 "icon_greater_healing.101", "An improved version of minor healing, this prayer heals wounds on either the caster or the target.",
	 10, 6, 12, 3, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_SCROLL | SPELL_USE_ROD | SPELL_USE_POTION, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green"
	},

	{"restoration", SPELL_TYPE_PRIEST,
	 "icon_default.101", "Heals the target or the caster, restores food and removes any disease, confusion and poison.",
	 20, 10, 12, 3, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green"
	},

	{"protection from cold", SPELL_TYPE_PRIEST,
	 "icon_default.101", "Raises your protection to cold. The protection raised by this spell depends on the caster's level.",
	 24, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL
	},

	{"protection from fire", SPELL_TYPE_PRIEST,
	 "icon_default.101", "Raises your protection to fire. The protection raised by this spell depends on the caster's level.",
	 24, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL
	},

	{"protection from electricity", SPELL_TYPE_PRIEST,
	 "icon_default.101", "Raises your protection to electricity. The protection raised by this spell depends on the caster's level.",
	 24, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL
	},

	{"protection from poison", SPELL_TYPE_PRIEST,
	 "icon_default.101", "Raises your protection to poison. The protection raised by this spell depends on the caster's level.",
	 24, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL
	},

	{"consecrate", SPELL_TYPE_PRIEST,
	 "icon_default.101", "Consecrates the altar you are standing on to your god. The prayer may fail if the altar's level is higher than yours.",
	 15, 10, 200, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL
	},

	{"finger of death", SPELL_TYPE_PRIEST,
	 "icon_finger_of_death.101", "You point your finger at targeted creature, and it gets struck by the power of your god.",
	 18, 7, 18, 2, 0, 5, 1.0,
	 8, 0, 2, 0, 15, "magic_hword.ogg",
	 SPELL_USE_CAST, SPELL_DESC_ENEMY,
	 PATH_DEATH, "spellobject_finger_of_death"
	},

	{"cause cold", SPELL_TYPE_PRIEST,
	 NULL, NULL,
	 5, 10, 50, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "disease_cold"
	},

	{"cause flu", SPELL_TYPE_PRIEST,
	 NULL, NULL,
	 10, 12, 50, 2, 0, 3, 1.0,
	 10, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "flu"
	},

	{"cause leprosy", SPELL_TYPE_PRIEST,
	 NULL, NULL,
	 15, 14, 58, 2, 0, 3, 1.0,
	 6, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "leprosy"
	},

	{"cause smallpox", SPELL_TYPE_PRIEST,
	 NULL, NULL,
	 20, 16, 58, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "smallpox"
	},

	{"cause pneumonic plague", SPELL_TYPE_PRIEST,
	 NULL, NULL,
	 25, 18, 58, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "pneumonic_plague"
	},

	{"meteor", SPELL_TYPE_WIZARD,
	 "icon_meteor.101", "Fires a meteor in front of you, which explodes into a cone of fire if it hits an object.",
	 30, 20, 32, 2, 0, 3, 1.0,
	 4, 10, 4, 16, 20, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "meteor"
	},

	{"meteor swarm", SPELL_TYPE_WIZARD,
	 "icon_meteor_swarm.101", "Similar to the spell of meteor, but this spell fires a bunch of meteors in front of you, which then explode into cones of fire if they hit something.",
	 80, 70, 60, 2, 0, 3, 1.0,
	 4, 10, 4, 8, 35, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "meteor"
	},

	{"poison fog", SPELL_TYPE_WIZARD,
	 NULL, NULL,
	 5, 18, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, NULL
	},

	{"bullet swarm", SPELL_TYPE_WIZARD,
	 "icon_default.101", "Fires 5 magic bullets in front of you at once.",
	 25, 18, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_bullet1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet"
	},

	{"bullet storm",SPELL_TYPE_WIZARD,
	 "icon_default.101", "Fires 3 magic bullets in front of you at once.",
	 20, 18, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_bullet1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet"
	},

	{"destruction", SPELL_TYPE_WIZARD,
	 "icon_destruction.101", "Creatures around you get struck by a magical power.",
	 18, 20, 20, 0, 3, 3, 1.0,
	 2, 1, 3, 20, 20, "magic_destruction.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, "spellobject_destruction"
	},

	{"create bomb", SPELL_TYPE_WIZARD,
	 "icon_create_bomb.101", "Creates a bomb in front of you, which then explodes after a short while.",
	 10, 10, 30, 2, 0, 3, 1.0,
	 10, 9, 0, 30, 9, "magic_bomb.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DETONATE, "bomb"
	},

	{"cure confusion", SPELL_TYPE_PRIEST,
	 "icon_default.101", "Cures any confusion your target or yourself may have.",
	 1, 5, 8, 3, 6,4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple"
	},

	{"transform wealth", SPELL_TYPE_WIZARD,
	 "icon_transform_wealth.101", "Mark wealth object (money, coppers for example) and cast this spell. The coppers will be transformed into silvers at regular money rate (you must have enough copper coins). There is 20% of the money sacrifice to cast it, so 100 coppers will become 80 coppers.",
	 15, 18, 40, 2, 7, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_turn.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL
	},

	{"magic missile", SPELL_TYPE_WIZARD,
	 "icon_magic_missile.101", "Fires a missile at currently selected target, following the target wherever they go.",
	 1, 3, 8, 3, 6, 9, 1.0,
	 4, 5, 4, 4, 9, "magic_missile.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_ENEMY,
	 PATH_MISSILE, "magic_missile"
	},

	{"rain of healing", SPELL_TYPE_PRIEST,
	 "icon_rain_of_healing.101", "This prayer heals all friends around and below the caster, excluding the caster.",
	 20, 6, 22, 0, 0, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green"
	},

	{"party heal", SPELL_TYPE_PRIEST,
	 "icon_party_heal.101", "Heals all nearby party members, including the caster.",
	 20, 6, 30, 0, 0, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green"
	},

	{"frostbolt", SPELL_TYPE_WIZARD,
	 "icon_default.101", "A blast of cold is fired in straight line. Each part of it can hit enemies only once.",
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_ice.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_FROST, "frostbolt"
	},

	{"firebolt", SPELL_TYPE_WIZARD,
	 "icon_firebolt.101", "A blast of fire is fired in straight line. Each part of it can hit enemies only once.",
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_fire.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "firebolt"
	},

	{"lightning", SPELL_TYPE_WIZARD,
	 "icon_default.101", "Lightning is fired in straight line. Each part of it can hit enemies only once and it will bounce off of walls.",
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_elec.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_ELEC, "lightning"
	},

	{"forked lightning", SPELL_TYPE_WIZARD,
	 "icon_default.101", "Lightning is fired in straight line. Each part of it can hit enemies only once and it will bounce off of walls. The main line of lightning may create forks of lightning, which will do less damage.",
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 4, 4, 8, 9, "magic_elec.ogg",
	 SPELL_USE_CAST | SPELL_USE_HORN | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_ELEC, "forked_lightning"
	},

	{"negative energy bolt", SPELL_TYPE_WIZARD,
	 "icon_default.101", "A blast of negative energy is fired in straight line. Each part of it can hit enemies only once and it will bounce off of walls.",
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
