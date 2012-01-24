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
	{"firestorm", SPELL_TYPE_WIZARD,
	 "icon_firestorm.101", "A cone of fire you project in front of you. The spell gains strength as you grow in level, so it remains one of your best spells even at high level.",
	 15, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_fire.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_DUST | SPELL_USE_BOOK | SPELL_USE_POTION, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "firebreath", 50000
	},

	{"icestorm", SPELL_TYPE_WIZARD,
	 "icon_icestorm.101", "A cone of ice which freezes monsters facing the caster. This spell gains power with level, so it remains useful even at high level.",
	 15, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_ice.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK | SPELL_USE_POTION | SPELL_USE_DUST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "icestorm", 50000
	},

	{"minor healing", SPELL_TYPE_PRIEST,
	 "icon_minor_healing.101", "This prayer heals minor wounds on either the caster or the target.",
	 1, 4, 8, 3, 6, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_SCROLL | SPELL_USE_ROD | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", 0
	},

	{"cure poison", SPELL_TYPE_PRIEST,
	 "icon_cure_poison.101", "This prayer cures all poison from your character.",
	 15, 5, 8, 3, 6, 4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple", 25000
	},

	{"cure disease", SPELL_TYPE_PRIEST,
	 "icon_cure_disease.101", "This prayer cures all diseases from your character.",
	 15, 5, 8, 3, 6, 4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple", 25000
	},

	{"strength self", SPELL_TYPE_WIZARD,
	 "icon_strength_self.101", "This spell will increase your strength by some amount. Recasting while in effect will refresh the wear out counter.",
	 1, 5, 8, 3, 6, 0, 1.0,
	 0, 0, 0, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_POTION | SPELL_USE_ROD | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_SELF, "meffect_yellow", 0
	},

	{"identify", SPELL_TYPE_WIZARD,
	 "icon_identify.101", "This spell identifies some items in your inventory. Number of items identified is your literacy level and your Intelligence stat combined.",
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_INFO, "meffect_pink", 0
	},

	{"asteroid", SPELL_TYPE_WIZARD,
	 "icon_asteroid.101", "Fires an asteroid in front of you, which explodes into a cone of ice if it hits an object.",
	 30, 5, 32, 2, 0, 3, 1.0,
	 4, 10, 4, 16, 8, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "asteroid", 75000
	},

	{"frost nova", SPELL_TYPE_WIZARD,
	 "icon_frost_nova.101", "Similar to the spell of asteroid, but this spell fires a bunch of asteroids in front of you, which then explode into cones of ice if they hit something.",
	 70, 5, 42, 2, 0, 3, 1.0,
	 4, 10, 4, 8, 6, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FROST, "asteroid", 200000
	},

	{"remove curse", SPELL_TYPE_PRIEST,
	 "icon_remove_curse.101", "This prayer removes any curse from applied items. Unapplied items are not affected.",
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN | SPELL_DESC_FRIENDLY,
	 PATH_TURNING, "meffect_blue", 0
	},

	{"remove damnation", SPELL_TYPE_PRIEST,
	 "icon_remove_damnation.101", "This prayer removes any curse or damnation from applied items. Unapplied items are not affected.",
	 1, 5, 8, 3, 6, 2, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_TOWN | SPELL_DESC_FRIENDLY,
	 PATH_TURNING, "meffect_blue", 0
	},

	{"cause light wounds", SPELL_TYPE_PRIEST,
	 "icon_cause_light_wounds.101", "This prayer creates a missile which inflicts damage to your enemies.",
	 1, 4, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_wound.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, "cause_wounds", 0
	},

	{"confuse", SPELL_TYPE_WIZARD,
	 NULL, NULL,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_confusion.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MIND, NULL, 0
	},

	{"magic bullet", SPELL_TYPE_WIZARD,
	 "icon_magic_bullet.101", "This spell fires a magical bullet which does not track but instead flies in one direction until it hits something.",
	 1, 4, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_bullet1.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet", 0
	},

	{"summon golem", SPELL_TYPE_WIZARD,
	 NULL, NULL,
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 4, 9, "magic_summon1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_SUMMON, NULL, 0
	},

	{"remove depletion", SPELL_TYPE_PRIEST,
	 "icon_remove_depletion.101", "This prayer restores depleted stats of your character.",
	 1, 5, 8, 3, 6,0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_SELF | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple", 0
	},

	{"probe", SPELL_TYPE_WIZARD,
	 "icon_probe.101", "Probe will invoke a magic ball which will give you information about the monster it hits. Probe tells you about the monster's level, the damage it does, and so on. A probe will be invisible to the monster and will invoke no aggression.",
	 1, 2, 8, 3, 6, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_default.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_WAND | SPELL_USE_ROD | SPELL_USE_BOOK, SPELL_DESC_TOWN | SPELL_DESC_DIRECTION,
	 PATH_INFO, "probebullet", 0
	},

	{"town portal", SPELL_TYPE_PRIEST,
	 "icon_town_portal.101", "This spell allows you to set up magic portals from one place to another.",
	 15, 30, 8, 1, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_SCROLL | SPELL_USE_BOOK, SPELL_DESC_SELF,
	 PATH_TELE, "perm_magic_portal", 0
	},

	{"create food", SPELL_TYPE_WIZARD,
	 "icon_create_food.101", "Creates food in your inventory which you may eat, but will vanish if dropped.",
	 1, 30, 8, 1, 0, 3, 1.0,
	 3, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_CREATE, NULL, 0
	},

	{"word of recall", SPELL_TYPE_PRIEST,
	 "icon_default.101", "Teleports you to your house, apartment, or wherever you applied a bed to reality. This spell takes a while to complete, but is faster the higher level you are.",
	 12, 5, 24, 1, 19, 3, 1.0,
	 25, 3, 3, 4, 0, "magic_teleport.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TELE, NULL, 0
	},

	{"recharge", SPELL_TYPE_WIZARD,
	 "icon_default.101", "Recharges marked wand in your inventory. There is a slight chance that the wand will be destroyed when using this spell.",
	 16, 50, 100, 2, 10, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_invisible.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL, 0
	},

	{"greater healing", SPELL_TYPE_PRIEST,
	 "icon_greater_healing.101", "An improved version of minor healing, this prayer heals wounds on either the caster or the target.",
	 10, 6, 8, 3, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM | SPELL_USE_SCROLL | SPELL_USE_ROD | SPELL_USE_POTION, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", 20000
	},

	{"restoration", SPELL_TYPE_PRIEST,
	 "icon_default.101", "Heals the target or the caster, restores food and removes any disease, confusion and poison.",
	 20, 10, 12, 3, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", 0
	},

	{"protection from cold", SPELL_TYPE_PRIEST,
	 "icon_prot_cold.101", "Raises your protection to cold. The protection raised by this spell depends on the caster's level.",
	 30, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, 50000
	},

	{"protection from fire", SPELL_TYPE_PRIEST,
	 "icon_prot_fire.101", "Raises your protection to fire. The protection raised by this spell depends on the caster's level.",
	 30, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, 50000
	},

	{"protection from electricity", SPELL_TYPE_PRIEST,
	 "icon_prot_elec.101", "Raises your protection to electricity. The protection raised by this spell depends on the caster's level.",
	 30, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, 50000
	},

	{"protection from poison", SPELL_TYPE_PRIEST,
	 "icon_prot_poison.101", "Raises your protection to poison. The protection raised by this spell depends on the caster's level.",
	 30, 22, 100, 2, 0, 3, 1.0,
	 1, 350, 3, 4, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_BALM, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_PROT, NULL, 50000
	},

	{"consecrate", SPELL_TYPE_PRIEST,
	 "icon_consecrate.101", "Consecrates the altar you are standing on to your god. The prayer may fail if the altar's level is higher than yours.",
	 25, 10, 70, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL, 30000
	},

	{"finger of death", SPELL_TYPE_PRIEST,
	 "icon_finger_of_death.101", "You point your finger at targeted creature, and it gets struck by the power of your god.",
	 20, 7, 12, 2, 0, 5, 1.0,
	 8, 0, 2, 0, 15, "magic_hword.ogg",
	 SPELL_USE_CAST, SPELL_DESC_ENEMY,
	 PATH_DEATH, "spellobject_finger_of_death", 30000
	},

	{"cause cold", SPELL_TYPE_PRIEST,
	 NULL, NULL,
	 5, 10, 50, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "disease_cold", 0
	},

	{"cause flu", SPELL_TYPE_PRIEST,
	 NULL, NULL,
	 10, 12, 50, 2, 0, 3, 1.0,
	 10, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "flu", 0
	},

	{"cause leprosy", SPELL_TYPE_PRIEST,
	 NULL, NULL,
	 15, 14, 58, 2, 0, 3, 1.0,
	 6, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "leprosy", 0
	},

	{"cause smallpox", SPELL_TYPE_PRIEST,
	 NULL, NULL,
	 20, 16, 58, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "smallpox", 0
	},

	{"cause pneumonic plague", SPELL_TYPE_PRIEST,
	 NULL, NULL,
	 25, 18, 58, 2, 0, 3, 1.0,
	 8, 0, 4, 2, 7, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "pneumonic_plague", 0
	},

	{"meteor", SPELL_TYPE_WIZARD,
	 "icon_meteor.101", "Fires a meteor in front of you, which explodes into a cone of fire if it hits an object.",
	 30, 5, 32, 2, 0, 3, 1.0,
	 4, 10, 4, 16, 8, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "meteor", 75000
	},

	{"meteor swarm", SPELL_TYPE_WIZARD,
	 "icon_meteor_swarm.101", "Similar to the spell of meteor, but this spell fires a bunch of meteors in front of you, which then explode into cones of fire if they hit something.",
	 70, 5, 42, 2, 0, 3, 1.0,
	 4, 10, 4, 8, 6, "magic_rstrike.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "meteor", 200000
	},

	{"poison fog", SPELL_TYPE_WIZARD,
	 NULL, NULL,
	 5, 18, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, NULL, 0
	},

	{"bullet swarm", SPELL_TYPE_WIZARD,
	 "icon_bullet_swarm.101", "Fires 5 magic bullets in front of you at once.",
	 65, 5, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 9, "magic_bullet1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet", 175000
	},

	{"bullet storm",SPELL_TYPE_WIZARD,
	 "icon_bullet_storm.101", "Fires 3 magic bullets in front of you at once.",
	 50, 4, 86, 2, 0, 3, 1.0,
	 0, 0, 0, 0, 9, "magic_bullet1.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_MISSILE, "bullet", 125000
	},

	{"destruction", SPELL_TYPE_WIZARD,
	 "icon_destruction.101", "Creatures around you get struck by a magical power.",
	 30, 20, 20, 0, 3, 3, 1.0,
	 4, 1, 3, 20, 24, "magic_destruction.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_WOUNDING, "spellobject_destruction", 40000
	},

	{"create bomb", SPELL_TYPE_WIZARD,
	 NULL, NULL,
	 10, 10, 30, 2, 0, 3, 1.0,
	 10, 9, 0, 30, 9, "magic_bomb.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DETONATE, "bomb", 0
	},

	{"cure confusion", SPELL_TYPE_PRIEST,
	 "icon_default.101", "Cures any confusion your target or yourself may have.",
	 1, 5, 8, 3, 6,4, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST | SPELL_USE_POTION | SPELL_USE_BOOK, SPELL_DESC_SELF | SPELL_DESC_FRIENDLY | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_purple", 5000
	},

	{"transform wealth", SPELL_TYPE_WIZARD,
	 "icon_transform_wealth.101", "Mark wealth object (money, coppers for example) and cast this spell. The coppers will be transformed into silvers at regular money rate (you must have enough copper coins). There is 20% of the money sacrifice to cast it, so 100 coppers will become 80 coppers.",
	 15, 18, 40, 2, 7, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_turn.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_TRANSMUTE, NULL, 30000
	},

	{"magic missile", SPELL_TYPE_WIZARD,
	 "icon_magic_missile.101", "Fires a missile at currently selected target, following the target wherever they go.",
	 40, 3, 8, 3, 6, 9, 1.0,
	 4, 5, 4, 4, 9, "magic_missile.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_ENEMY,
	 PATH_MISSILE, "magic_missile", 75000
	},

	{"rain of healing", SPELL_TYPE_PRIEST,
	 "icon_rain_of_healing.101", "This prayer heals all friends around and below the caster, excluding the caster.",
	 20, 6, 18, 0, 0, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", 40000
	},

	{"party heal", SPELL_TYPE_PRIEST,
	 "icon_party_heal.101", "Heals all nearby party members, including the caster.",
	 20, 6, 16, 0, 0, 0, 1.0,
	 0, 0, 0, 0, 0, "magic_stat.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION | SPELL_DESC_TOWN,
	 PATH_RESTORE, "meffect_green", 40000
	},

	{"frostbolt", SPELL_TYPE_WIZARD,
	 "icon_frostbolt.101", "A blast of cold is fired in straight line. Each part of it can hit enemies only once.",
	 20, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_ice.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_FROST, "frostbolt", 65000
	},

	{"firebolt", SPELL_TYPE_WIZARD,
	 "icon_firebolt.101", "A blast of fire is fired in straight line. Each part of it can hit enemies only once.",
	 20, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_fire.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_FIRE, "firebolt", 65000
	},

	{"lightning", SPELL_TYPE_WIZARD,
	 "icon_lightning.101", "Lightning is fired in straight line. Each part of it can hit enemies only once and it will bounce off of walls.",
	 20, 5, 8, 3, 6, 0, 1.0,
	 4, 7, 4, 0, 9, "magic_elec.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_ELEC, "lightning", 65000
	},

	{"forked lightning", SPELL_TYPE_WIZARD,
	 "icon_forked_lightning.101", "Lightning is fired in straight line. Each part of it can hit enemies only once and it will bounce off of walls. The main line of lightning may create forks of lightning, which will do less damage.",
	 25, 5, 8, 3, 6, 0, 1.0,
	 4, 4, 4, 8, 9, "magic_elec.ogg",
	 SPELL_USE_CAST | SPELL_USE_WAND | SPELL_USE_ROD, SPELL_DESC_DIRECTION,
	 PATH_ELEC, "forked_lightning", 80000
	},

	{"negative energy bolt", SPELL_TYPE_WIZARD,
	 "icon_negabolt.101", "A blast of negative energy is fired in straight line. Each part of it can hit enemies only once and it will bounce off of walls.",
	 1, 5, 8, 3, 6, 0, 1.0,
	 4, 5, 4, 8, 9, "magic_elec.ogg",
	 SPELL_USE_CAST, SPELL_DESC_DIRECTION,
	 PATH_DEATH, "negabolt", 0
	},

	{"holy word", SPELL_TYPE_PRIEST,
	 "icon_holy_word.101", "Cone of holy power that will damage all undead creatures it hits.",
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
