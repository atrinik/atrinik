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
 * Atrinik Python plugin object related code. */

/* First let's include the header file needed */
#include <atrinik_object.h>
#include <plugin_common.h>
#include <inline.h>

/** Available Python methods for the AtrinikObject object */
static PyMethodDef ObjectMethods[] =
{
	{"SetSaveBed",                   (PyCFunction) Atrinik_Object_SetSaveBed,             METH_VARARGS, 0},
	{"SwapApartments",               (PyCFunction) Atrinik_Object_SwapApartments,         METH_VARARGS, 0},
	{"GetSkill",                     (PyCFunction) Atrinik_Object_GetSkill,               METH_VARARGS, 0},
	{"SetSkill",                     (PyCFunction) Atrinik_Object_SetSkill,               METH_VARARGS, 0},
	{"ActivateRune",                 (PyCFunction) Atrinik_Object_ActivateRune,           METH_VARARGS, 0},
	{"CastAbility",                  (PyCFunction) Atrinik_Object_CastAbility,            METH_VARARGS, 0},
	{"InsertInside",                 (PyCFunction) Atrinik_Object_InsertInside,           METH_VARARGS, 0},
	{"GetGod",                       (PyCFunction) Atrinik_Object_GetGod,                 METH_VARARGS, 0},
	{"SetGod",                       (PyCFunction) Atrinik_Object_SetGod,                 METH_VARARGS, 0},
	{"TeleportTo",                   (PyCFunction) Atrinik_Object_TeleportTo,             METH_VARARGS, 0},
	{"Apply",                        (PyCFunction) Atrinik_Object_Apply,                  METH_VARARGS, 0},
	{"PickUp",                       (PyCFunction) Atrinik_Object_PickUp,                 METH_VARARGS, 0},
	{"Drop",                         (PyCFunction) Atrinik_Object_Drop,                   METH_VARARGS, 0},
	{"Fix",                          (PyCFunction) Atrinik_Object_Fix,                    METH_VARARGS, 0},
	{"Kill",                         (PyCFunction) Atrinik_Object_Kill,                   METH_VARARGS, 0},
	{"DoKnowSpell",                  (PyCFunction) Atrinik_Object_DoKnowSpell,            METH_VARARGS, 0},
	{"AcquireSpell",                 (PyCFunction) Atrinik_Object_AcquireSpell,           METH_VARARGS, 0},
	{"DoKnowSkill",                  (PyCFunction) Atrinik_Object_DoKnowSkill,            METH_VARARGS, 0},
	{"AcquireSkill",                 (PyCFunction) Atrinik_Object_AcquireSkill,           METH_VARARGS, 0},
	{"FindMarkedObject",             (PyCFunction) Atrinik_Object_FindMarkedObject,       METH_VARARGS, 0},
	{"GetQuestObject",               (PyCFunction) Atrinik_Object_GetQuestObject,         METH_VARARGS, 0},
	{"StartQuest",                   (PyCFunction) Atrinik_Object_StartQuest,             METH_VARARGS, 0},
	{"CreatePlayerForce",            (PyCFunction) Atrinik_Object_CreatePlayerForce,      METH_VARARGS, 0},
	{"CreatePlayerInfo",             (PyCFunction) Atrinik_Object_CreatePlayerInfo,       METH_VARARGS, 0},
	{"GetPlayerInfo",                (PyCFunction) Atrinik_Object_GetPlayerInfo,          METH_VARARGS, 0},
	{"GetNextPlayerInfo",            (PyCFunction) Atrinik_Object_GetNextPlayerInfo,      METH_VARARGS, 0},
	{"CheckInvisibleObjectInside",   (PyCFunction) Atrinik_Object_CheckInvisibleInside,   METH_VARARGS, 0},
	{"CreateInvisibleObjectInside",  (PyCFunction) Atrinik_Object_CreateInvisibleInside,  METH_VARARGS, 0},
	{"CreateObjectInside",           (PyCFunction) Atrinik_Object_CreateObjectInside,     METH_VARARGS, 0},
	{"CheckInventory",               (PyCFunction) Atrinik_Object_CheckInventory,         METH_VARARGS, 0},
	{"Remove",                       (PyCFunction) Atrinik_Object_Remove,                 METH_VARARGS, 0},
	{"SetPosition",                  (PyCFunction) Atrinik_Object_SetPosition,            METH_VARARGS, 0},
	{"IdentifyItem",                 (PyCFunction) Atrinik_Object_IdentifyItem,           METH_VARARGS, 0},
	{"Deposit",                      (PyCFunction) Atrinik_Object_Deposit,                METH_VARARGS, 0},
	{"Withdraw",                     (PyCFunction) Atrinik_Object_Withdraw,               METH_VARARGS, 0},
	{"Communicate",                  (PyCFunction) Atrinik_Object_Communicate,            METH_VARARGS, 0},
	{"Say",                          (PyCFunction) Atrinik_Object_Say,                    METH_VARARGS, 0},
	{"SayTo",                        (PyCFunction) Atrinik_Object_SayTo,                  METH_VARARGS, 0},
	{"Write",                        (PyCFunction) Atrinik_Object_Write,                  METH_VARARGS, 0},
	{"SetGender",                    (PyCFunction) Atrinik_Object_SetGender,              METH_VARARGS, 0},
	{"SetRank",                      (PyCFunction) Atrinik_Object_SetRank,                METH_VARARGS, 0},
	{"SetAlignment",                 (PyCFunction) Atrinik_Object_SetAlignment,           METH_VARARGS, 0},
	{"GetAlignmentForce",            (PyCFunction) Atrinik_Object_GetAlignmentForce,      METH_VARARGS, 0},
	{"SetGuildForce",                (PyCFunction) Atrinik_Object_SetGuildForce,          METH_VARARGS, 0},
	{"GetGuildForce",                (PyCFunction) Atrinik_Object_GetGuildForce,          METH_VARARGS, 0},
	{"IsOfType",                     (PyCFunction) Atrinik_Object_IsOfType,               METH_VARARGS, 0},
	{"Save",                         (PyCFunction) Atrinik_Object_Save,                   METH_VARARGS, 0},
	{"GetIP",                        (PyCFunction) Atrinik_Object_GetIP,                  METH_VARARGS, 0},
	{"GetArchName",                  (PyCFunction) Atrinik_Object_GetArchName,            METH_VARARGS, 0},
	{"ShowCost",                     (PyCFunction) Atrinik_Object_ShowCost,               METH_VARARGS, 0},
	{"GetItemCost",                  (PyCFunction) Atrinik_Object_GetItemCost,            METH_VARARGS, 0},
	{"GetMoney",                     (PyCFunction) Atrinik_Object_GetMoney,               METH_VARARGS, 0},
	{"PayForItem",                   (PyCFunction) Atrinik_Object_PayForItem,             METH_VARARGS, 0},
	{"PayAmount",                    (PyCFunction) Atrinik_Object_PayAmount,              METH_VARARGS, 0},
	{"GetUnmodifiedAttribute",       (PyCFunction) Atrinik_Object_GetUnmodifiedAttribute, METH_VARARGS, 0},
	{"SendCustomCommand",            (PyCFunction) Atrinik_Object_SendCustomCommand,      METH_VARARGS, 0},
	{"CheckTrigger",                 (PyCFunction) Atrinik_Object_CheckTrigger,           METH_VARARGS, 0},
	{"Clone",                        (PyCFunction) Atrinik_Object_Clone,                  METH_VARARGS, 0},
	{"GetSaveBed",                   (PyCFunction) Atrinik_Object_GetSaveBed,             METH_VARARGS, 0},
	{NULL, NULL, 0, 0}
};

/** Object fields structure */
typedef struct
{
	/** The field type */
	char *name;

	/** Type of the field */
	field_type type;

	/** Offset in object structure */
	uint32 offset;

	/** Flags for special handling */
	uint32 flags;

	/** Extra data for some special fields */
	uint32 extra_data;
} obj_fields_struct;

/**
 * @anchor plugin_python_object_fields
 *
 * All the possible fields of an object.
 *
 * List of the fields and their meaning:
 *
 * - <b>below</b>: @copydoc object::below
 * - <b>above</b>: @copydoc object::above
 * - <b>inventory</b>: @copydoc object::inv
 * - <b>map</b>: @copydoc object::map
 * - <b>name</b>: @copydoc object::name
 * - <b>title</b>: @copydoc object::title
 * - <b>race</b>: @copydoc object::race
 * - <b>slaying</b>: @copydoc object::slaying
 * - <b>message</b>: @copydoc object::msg
 * - <b>weight</b>: @copydoc object::weight
 * - <b>weight_limit</b>: @copydoc object::weight_limit
 * - <b>carrying</b>: @copydoc object::carrying
 * - <b>path_attuned</b>: @copydoc object::path_attuned
 * - <b>path_repelled</b>: @copydoc object::path_repelled
 * - <b>path_denied</b>: @copydoc object::path_denied
 * - <b>value</b>: @copydoc object::value
 * - <b>quantity</b>: @copydoc object::nrof
 * - <b>damage_round_tag</b>: @copydoc object::damage_round_tag
 * - <b>update_tag</b>: @copydoc object::update_tag
 * - <b>enemy</b>: @copydoc object::enemy
 * - <b>attacked_by</b>: @copydoc object::attacked_by
 * - <b>owner</b>: @copydoc object::owner
 * - <b>x</b>: @copydoc object::x
 * - <b>y</b>: @copydoc object::y
 * - <b>attacked_by_distance</b>: @copydoc object::attacked_by_distance
 * - <b>last_damage</b>: @copydoc object::last_damage
 * - <b>terrain_type</b>: @copydoc object::terrain_type
 * - <b>terrain_flag</b>: @copydoc object::terrain_flag
 * - <b>material</b>: @copydoc object::material
 * - <b>material_real</b>: @copydoc object::material_real
 * - <b>last_heal</b>: @copydoc object::last_heal
 * - <b>last_sp</b>: @copydoc object::last_sp
 * - <b>last_grace</b>: @copydoc object::last_grace
 * - <b>last_eat</b>: @copydoc object::last_eat
 * - <b>animation_id</b>: @copydoc object::animation_id
 * - <b>inv_animation_id</b>: @copydoc object::inv_animation_id
 * - <b>magic</b>: @copydoc object::magic
 * - <b>state</b>: @copydoc object::state
 * - <b>level</b>: @copydoc object::level
 * - <b>direction</b>: @copydoc object::direction
 * - <b>facing</b>: @copydoc object::facing
 * - <b>quick_pos</b>: @copydoc object::quick_pos
 * - <b>type</b>: @copydoc object::type
 * - <b>sub_type_1</b>: @copydoc object::sub_type1
 * - <b>item_quality</b>: @copydoc object::item_quality
 * - <b>item_condition</b>: @copydoc object::item_condition
 * - <b>item_race</b>: @copydoc object::item_race
 * - <b>item_level</b>: @copydoc object::item_level
 * - <b>item_skill</b>: @copydoc object::item_skill
 * - <b>glow_radius</b>: @copydoc object::glow_radius
 * - <b>move_status</b>: @copydoc object::move_status
 * - <b>move_type</b>: @copydoc object::move_type
 * - <b>anim_enemy_dir</b>: @copydoc object::anim_enemy_dir
 * - <b>anim_moving_dir</b>: @copydoc object::anim_moving_dir
 * - <b>anim_enemy_dir_last</b>: @copydoc object::anim_enemy_dir_last
 * - <b>anim_moving_dir_last</b>: @copydoc object::anim_moving_dir_last
 * - <b>anim_last_facing</b>: @copydoc object::anim_last_facing
 * - <b>anim_last_facing_last</b>: @copydoc object::anim_last_facing_last
 * - <b>anim_speed</b>: @copydoc object::anim_speed
 * - <b>last_anim</b>: @copydoc object::last_anim
 * - <b>will_apply</b>: @copydoc object::will_apply
 * - <b>run_away</b>: @copydoc object::run_away
 * - <b>pick_up</b>: @copydoc object::pick_up
 * - <b>hide</b>: @copydoc object::hide
 * - <b>layer</b>: @copydoc object::layer
 * - <b>speed</b>: @copydoc object::speed
 * - <b>speed_left</b>: @copydoc object::speed_left
 * - <b>weapon_speed</b>: @copydoc object::weapon_speed
 * - <b>weapon_speed_left</b>: @copydoc object::weapon_speed_left
 * - <b>weapon_speed_add</b>: @copydoc object::weapon_speed_add
 * - <b>experience</b>: @copydoc living::exp
 * - <b>hitpoints</b>: @copydoc living::hp
 * - <b>max_hitpoints</b>: @copydoc living::maxhp
 * - <b>spellpoints</b>: @copydoc living::sp
 * - <b>max_spellpoints</b>: @copydoc living::maxsp
 * - <b>grace</b>: @copydoc living::grace
 * - <b>max_grace</b>: @copydoc living::maxgrace
 * - <b>food</b>: @copydoc living::food
 * - <b>damage</b>: @copydoc living::dam
 * - <b>weapon_class</b>: @copydoc living::wc
 * - <b>armour_class</b>: @copydoc living::ac
 * - <b>weapon_class_range</b>: @copydoc living::wc_range
 * - <b>strength</b>: @copydoc living::Str
 * - <b>dexterity</b>: @copydoc living::Dex
 * - <b>constitution</b>: @copydoc living::Con
 * - <b>wisdom</b>: @copydoc living::Wis
 * - <b>charisma</b>: @copydoc living::Cha
 * - <b>intelligence</b>: @copydoc living::Int
 * - <b>power</b>: @copydoc living::Pow
 * - <b>luck</b>: @copydoc living::luck
 *
 * @todo Message field needs special handling (check for endmsg, limit to
 * 4096 characters.
 * @todo Limit weight to >= 0
 * @todo Maximum 100000 quantity
 * @todo Make enemy and owner settable (requires HOOK to set_npc_enemy()
 * and set_owner().
 * @todo Limit last_sp and last_grace to max 16000?
 * @todo Animation_id will require animation lookup function. How about
 * face, is that a special anim?
 * @todo -10.0 \< speed \< 10.0, also might want to call
 * update_object_speed()
 * @todo Limit food to max 999 (at least to players)?
 * @todo Damage: limit to 0 <= dam <= 120?
 * @todo Limit hitpoints, spellpoints and grace to +/- 16000?
 * @todo Limit weapon_class and armour_class to +/- 120.
 * @todo Limit all player stats to +/- 30. */
obj_fields_struct obj_fields[] =
{
	{"below",                  FIELDTYPE_OBJECT,     offsetof(object, below),                  FIELDFLAG_READONLY,          0},
	{"above",                  FIELDTYPE_OBJECT,     offsetof(object, above),                  FIELDFLAG_READONLY,          0},
	{"inventory",              FIELDTYPE_OBJECT,     offsetof(object, inv),                    FIELDFLAG_READONLY,          0},
	{"map",                    FIELDTYPE_MAP,        offsetof(object, map),                    FIELDFLAG_READONLY,          0},
	{"name",                   FIELDTYPE_SHSTR,      offsetof(object, name),                   FIELDFLAG_PLAYER_READONLY,   0},
	{"title",                  FIELDTYPE_SHSTR,      offsetof(object, title),                  0,                           0},
	{"race",                   FIELDTYPE_SHSTR,      offsetof(object, race),                   0,                           0},
	{"slaying",                FIELDTYPE_SHSTR,      offsetof(object, slaying),                0,                           0},
	{"message",                FIELDTYPE_SHSTR,      offsetof(object, msg),                    0,                           0},
	{"weight",                 FIELDTYPE_SINT32,     offsetof(object, weight),                 0,                           0},

	{"weight_limit",           FIELDTYPE_UINT32,     offsetof(object, weight_limit),           0,                           0},
	{"carrying",               FIELDTYPE_SINT32,     offsetof(object, carrying),               0,                           0},
	{"path_attuned",           FIELDTYPE_UINT32,     offsetof(object, path_attuned),           0,                           0},
	{"path_repelled",          FIELDTYPE_UINT32,     offsetof(object, path_repelled),          0,                           0},
	{"path_denied",            FIELDTYPE_UINT32,     offsetof(object, path_denied),            0,                           0},
	{"value",                  FIELDTYPE_SINT32,     offsetof(object, value),                  0,                           0},
	{"quantity",               FIELDTYPE_UINT32,     offsetof(object, nrof),                   0,                           0},
	{"enemy",                  FIELDTYPE_OBJECTREF,  offsetof(object, enemy),                  FIELDFLAG_READONLY,          offsetof(object, enemy_count)},
	{"attacked_by",            FIELDTYPE_OBJECTREF,  offsetof(object, attacked_by),            FIELDFLAG_READONLY,          offsetof(object, attacked_by_count)},
	{"owner",                  FIELDTYPE_OBJECTREF,  offsetof(object, owner),                  FIELDFLAG_READONLY,          offsetof(object, ownercount)},

	{"x",                      FIELDTYPE_SINT16,     offsetof(object, x),                      FIELDFLAG_READONLY,          0},
	{"y",                      FIELDTYPE_SINT16,     offsetof(object, y),                      FIELDFLAG_READONLY,          0},
	{"attacked_by_distance",   FIELDTYPE_SINT16,     offsetof(object, attacked_by_distance),   0,                           0},
	{"last_damage",            FIELDTYPE_UINT16,     offsetof(object, last_damage),            0,                           0},
	{"terrain_type",           FIELDTYPE_UINT16,     offsetof(object, terrain_type),           0,                           0},
	{"terrain_flag",           FIELDTYPE_UINT16,     offsetof(object, terrain_flag),           0,                           0},
	{"material",               FIELDTYPE_UINT16,     offsetof(object, material),               0,                           0},
	{"material_real",          FIELDTYPE_SINT16,     offsetof(object, material_real),          0,                           0},
	{"last_heal",              FIELDTYPE_SINT16,     offsetof(object, last_heal),              0,                           0},
	{"last_sp",                FIELDTYPE_SINT16,     offsetof(object, last_sp),                0,                           0},

	{"last_grace",             FIELDTYPE_SINT16,     offsetof(object, last_grace),             0,                           0},
	{"last_eat",               FIELDTYPE_SINT16,     offsetof(object, last_eat),               0,                           0},
	{"animation_id",           FIELDTYPE_UINT16,     offsetof(object, animation_id),           0,                           0},
	{"inv_animation_id",       FIELDTYPE_UINT16,     offsetof(object, inv_animation_id),       0,                           0},
	{"magic",                  FIELDTYPE_SINT8,      offsetof(object, magic),                  0,                           0},
	{"state",                  FIELDTYPE_UINT8,      offsetof(object, state),                  0,                           0},
	{"level",                  FIELDTYPE_SINT8,      offsetof(object, level),                  FIELDFLAG_PLAYER_READONLY,   0},
	{"direction",              FIELDTYPE_SINT8,      offsetof(object, direction),              0,                           0},
	{"facing",                 FIELDTYPE_SINT8,      offsetof(object, facing),                 0,                           0},
	{"quick_pos",              FIELDTYPE_UINT8,      offsetof(object, quick_pos),              0,                           0},

	{"type",                   FIELDTYPE_UINT8,      offsetof(object, type),                   FIELDFLAG_READONLY,          0},
	{"sub_type_1",             FIELDTYPE_UINT8,      offsetof(object, sub_type1),              0,                           0},
	{"item_quality",           FIELDTYPE_UINT8,      offsetof(object, item_quality),           0,                           0},
	{"item_condition",         FIELDTYPE_UINT8,      offsetof(object, item_condition),         0,                           0},
	{"item_race",              FIELDTYPE_UINT8,      offsetof(object, item_race),              0,                           0},
	{"item_level",             FIELDTYPE_UINT8,      offsetof(object, item_level),             0,                           0},
	{"item_skill",             FIELDTYPE_UINT8,      offsetof(object, item_skill),             0,                           0},
	{"glow_radius",            FIELDTYPE_SINT8,      offsetof(object, glow_radius),            0,                           0},
	{"move_status",            FIELDTYPE_SINT8,      offsetof(object, move_status),            0,                           0},
	{"move_type",              FIELDTYPE_UINT8,      offsetof(object, move_type),              0,                           0},

	{"anim_enemy_dir",         FIELDTYPE_SINT8,      offsetof(object, anim_enemy_dir),         0,                           0},
	{"anim_moving_dir",        FIELDTYPE_SINT8,      offsetof(object, anim_moving_dir),        0,                           0},
	{"anim_enemy_dir_last",    FIELDTYPE_SINT8,      offsetof(object, anim_enemy_dir_last),    0,                           0},
	{"anim_moving_dir_last",   FIELDTYPE_SINT8,      offsetof(object, anim_moving_dir_last),   0,                           0},
	{"anim_last_facing",       FIELDTYPE_SINT8,      offsetof(object, anim_last_facing),       0,                           0},
	{"anim_last_facing_last",  FIELDTYPE_SINT8,      offsetof(object, anim_last_facing_last),  0,                           0},
	{"anim_speed",             FIELDTYPE_UINT8,      offsetof(object, anim_speed),             0,                           0},
	{"last_anim",              FIELDTYPE_UINT8,      offsetof(object, last_anim),              0,                           0},
	{"will_apply",             FIELDTYPE_UINT8,      offsetof(object, will_apply),             0,                           0},
	{"run_away",               FIELDTYPE_UINT8,      offsetof(object, run_away),               0,                           0},

	{"pick_up",                FIELDTYPE_UINT8,      offsetof(object, pick_up),                0,                           0},
	{"hide",                   FIELDTYPE_UINT8,      offsetof(object, hide),                   0,                           0},
	{"layer",                  FIELDTYPE_UINT8,      offsetof(object, layer),                  0,                           0},
	{"speed",                  FIELDTYPE_FLOAT,      offsetof(object, speed),                  FIELDFLAG_PLAYER_READONLY,   0},
	{"speed_left",             FIELDTYPE_FLOAT,      offsetof(object, speed_left),             0,                           0},
	{"weapon_speed",           FIELDTYPE_FLOAT,      offsetof(object, weapon_speed),           0,                           0},
	{"weapon_speed_left",      FIELDTYPE_FLOAT,      offsetof(object, weapon_speed_left),      0,                           0},
	{"weapon_speed_add",       FIELDTYPE_FLOAT,      offsetof(object, weapon_speed_add),       0,                           0},
	{"experience",             FIELDTYPE_SINT32,     offsetof(object, stats.exp),              0,                           0},
	{"hitpoints",              FIELDTYPE_SINT32,     offsetof(object, stats.hp),               0,                           0},

	{"max_hitpoints",          FIELDTYPE_SINT32,     offsetof(object, stats.maxhp),            FIELDFLAG_PLAYER_READONLY,   0},
	{"spellpoints",            FIELDTYPE_SINT16,     offsetof(object, stats.sp),               0,                           0},
	{"max_spellpoints",        FIELDTYPE_SINT16,     offsetof(object, stats.maxsp),            FIELDFLAG_PLAYER_READONLY,   0},
	{"grace",                  FIELDTYPE_SINT16,     offsetof(object, stats.grace),            0,                           0},
	{"max_grace",              FIELDTYPE_SINT16,     offsetof(object, stats.maxgrace),         FIELDFLAG_PLAYER_READONLY,   0},
	{"food",                   FIELDTYPE_SINT16,     offsetof(object, stats.food),             0,                           0},
	{"damage",                 FIELDTYPE_SINT16,     offsetof(object, stats.dam),              FIELDFLAG_PLAYER_READONLY,   0},
	{"weapon_class",           FIELDTYPE_SINT16,     offsetof(object, stats.wc),               FIELDFLAG_PLAYER_READONLY,   0},
	{"armour_class",           FIELDTYPE_SINT16,     offsetof(object, stats.ac),               FIELDFLAG_PLAYER_READONLY,   0},
	{"weapon_class_range",     FIELDTYPE_UINT8,      offsetof(object, stats.wc_range),         0,                           0},

	{"strength",               FIELDTYPE_SINT8,      offsetof(object, stats.Str),              FIELDFLAG_PLAYER_FIX,        0},
	{"dexterity",              FIELDTYPE_SINT8,      offsetof(object, stats.Dex),              FIELDFLAG_PLAYER_FIX,        0},
	{"constitution",           FIELDTYPE_SINT8,      offsetof(object, stats.Con),              FIELDFLAG_PLAYER_FIX,        0},
	{"wisdom",                 FIELDTYPE_SINT8,      offsetof(object, stats.Wis),              FIELDFLAG_PLAYER_FIX,        0},
	{"charisma",               FIELDTYPE_SINT8,      offsetof(object, stats.Cha),              FIELDFLAG_PLAYER_FIX,        0},
	{"intelligence",           FIELDTYPE_SINT8,      offsetof(object, stats.Int),              FIELDFLAG_PLAYER_FIX,        0},
	{"power",                  FIELDTYPE_SINT8,      offsetof(object, stats.Pow),              FIELDFLAG_PLAYER_FIX,        0},
	{"luck",                   FIELDTYPE_SINT8,      offsetof(object, stats.luck),             FIELDFLAG_PLAYER_READONLY,   0}
};

/** Number of object fields. */
#define NUM_OBJFIELDS (sizeof(obj_fields) / sizeof(obj_fields[0]))

/**
 * @anchor plugin_python_object_flags
 * This is a list of strings that correspond to the FLAG_... values.
 *
 * This is a simple 1:1 mapping - if FLAG_FRIENDLY is 15, then the 15th
 * element of this array should match that name.
 *
 * If an entry is NULL, that flag cannot be set/read from scripts
 *
 * Yes, this is almost exactly a repeat from loader.l.
 *
 * List of the flags and their meaning:
 *
 * - <b>f_sleep</b>: @copydoc FLAG_SLEEP
 * - <b>f_confused</b>: @copydoc FLAG_CONFUSED
 * - <b>f_paralyzed</b>: @copydoc FLAG_PARALYZED
 * - <b>f_scared</b>: @copydoc FLAG_SCARED
 * - <b>f_is_blind</b>: @copydoc FLAG_BLIND
 * - <b>f_is_invisible</b>: @copydoc FLAG_IS_INVISIBLE
 * - <b>f_is_ethereal</b>: @copydoc FLAG_IS_ETHEREAL
 * - <b>f_is_good</b>: @copydoc FLAG_IS_GOOD
 * - <b>f_no_pick</b>: @copydoc FLAG_NO_PICK
 * - <b>f_walk_on</b>: @copydoc FLAG_WALK_ON
 * - <b>f_no_pass</b>: @copydoc FLAG_NO_PASS
 * - <b>f_is_animated</b>: @copydoc FLAG_ANIMATE
 * - <b>f_slow_move</b>: @copydoc FLAG_SLOW_MOVE
 * - <b>f_flying</b>: @copydoc FLAG_FLYING
 * - <b>f_monster</b>: @copydoc FLAG_MONSTER
 * - <b>f_friendly</b>: @copydoc FLAG_FRIENDLY
 * - <b>f_been_applied</b>: @copydoc FLAG_BEEN_APPLIED
 * - <b>f_auto_apply</b>: @copydoc FLAG_AUTO_APPLY
 * - <b>f_treasure</b>: @copydoc FLAG_TREASURE
 * - <b>f_is_neutral</b>: @copydoc FLAG_IS_NEUTRAL
 * - <b>f_see_invisible</b>: @copydoc FLAG_SEE_INVISIBLE
 * - <b>f_can_roll</b>: @copydoc FLAG_CAN_ROLL
 * - <b>f_generator</b>: @copydoc FLAG_GENERATOR
 * - <b>f_is_turnable</b>: @copydoc FLAG_IS_TURNABLE
 * - <b>f_walk_off</b>: @copydoc FLAG_WALK_OFF
 * - <b>f_fly_on</b>: @copydoc FLAG_FLY_ON
 * - <b>f_fly_off</b>: @copydoc FLAG_FLY_OFF
 * - <b>f_is_used_up</b>: @copydoc FLAG_IS_USED_UP
 * - <b>f_identified</b>: @copydoc FLAG_IDENTIFIED
 * - <b>f_reflecting</b>: @copydoc FLAG_REFLECTING
 * - <b>f_changing</b>: @copydoc FLAG_CHANGING
 * - <b>f_splitting</b>: @copydoc FLAG_SPLITTING
 * - <b>f_hitback</b>: @copydoc FLAG_HITBACK
 * - <b>f_startequip</b>: @copydoc FLAG_STARTEQUIP
 * - <b>f_blocksview</b>: @copydoc FLAG_BLOCKSVIEW
 * - <b>f_undead</b>: @copydoc FLAG_UNDEAD
 * - <b>f_unaggressive</b>: @copydoc FLAG_UNAGGRESSIVE
 * - <b>f_reflect_missile</b>: @copydoc FLAG_REFL_MISSILE
 * - <b>f_reflect_spell</b>: @copydoc FLAG_REFL_SPELL
 * - <b>f_no_magic</b>: @copydoc FLAG_NO_MAGIC
 * - <b>f_no_fix_player</b>: @copydoc FLAG_NO_FIX_PLAYER
 * - <b>f_is_evil</b>: @copydoc FLAG_IS_EVIL
 * - <b>f_tear_down</b>: @copydoc FLAG_TEAR_DOWN
 * - <b>f_run_away</b>: @copydoc FLAG_RUN_AWAY
 * - <b>f_pass_thru</b>: @copydoc FLAG_PASS_THRU
 * - <b>f_can_pass_thru</b>: @copydoc FLAG_CAN_PASS_THRU
 * - <b>f_pick_up</b>: @copydoc FLAG_PICK_UP
 * - <b>f_unique</b>: @copydoc FLAG_UNIQUE
 * - <b>f_no_drop</b>: @copydoc FLAG_NO_DROP
 * - <b>f_is_indestructible</b>: @copydoc FLAG_INDESTRUCTIBLE
 * - <b>f_can_cast_spell</b>: @copydoc FLAG_CAST_SPELL
 * - <b>f_can_use_scroll</b>: @copydoc FLAG_USE_SCROLL
 * - <b>f_can_use_range</b>: @copydoc FLAG_USE_RANGE
 * - <b>f_can_use_bow</b>: @copydoc FLAG_USE_BOW
 * - <b>f_can_use_armour</b>: @copydoc FLAG_USE_ARMOUR
 * - <b>f_can_use_weapon</b>: @copydoc FLAG_USE_WEAPON
 * - <b>f_can_use_ring</b>: @copydoc FLAG_USE_RING
 * - <b>f_has_ready_range</b>: @copydoc FLAG_READY_RANGE
 * - <b>f_has_ready_bow</b>: @copydoc FLAG_READY_BOW
 * - <b>f_xrays</b>: @copydoc FLAG_XRAYS
 * - <b>f_no_apply</b>: @copydoc FLAG_NO_APPLY
 * - <b>f_is_floor</b>: @copydoc FLAG_IS_FLOOR
 * - <b>f_lifesave</b>: @copydoc FLAG_LIFESAVE
 * - <b>f_is_magical</b>: @copydoc FLAG_IS_MAGICAL
 * - <b>f_alive</b>: @copydoc FLAG_ALIVE
 * - <b>f_stand_still</b>: @copydoc FLAG_STAND_STILL
 * - <b>f_random_move</b>: @copydoc FLAG_RANDOM_MOVE
 * - <b>f_only_attack</b>: @copydoc FLAG_ONLY_ATTACK
 * - <b>f_wiz</b>: @copydoc FLAG_WIZ
 * - <b>f_stealth</b>: @copydoc FLAG_STEALTH
 * - <b>f_cursed</b>: @copydoc FLAG_CURSED
 * - <b>f_damned</b>: @copydoc FLAG_DAMNED
 * - <b>f_see_anywhere</b>: @copydoc FLAG_SEE_ANYWHERE
 * - <b>f_known_magical</b>: @copydoc FLAG_KNOWN_MAGICAL
 * - <b>f_known_cursed</b>: @copydoc FLAG_KNOWN_CURSED
 * - <b>f_can_use_skill</b>: @copydoc FLAG_CAN_USE_SKILL
 * - <b>f_is_thrown</b>: @copydoc FLAG_IS_THROWN
 * - <b>f_is_vul_sphere</b>: @copydoc FLAG_VUL_SPHERE
 * - <b>f_is_proof_sphere</b>: @copydoc FLAG_PROOF_SPHERE
 * - <b>f_is_male</b>: @copydoc FLAG_IS_MALE
 * - <b>f_is_female</b>: @copydoc FLAG_IS_FEMALE
 * - <b>f_applied</b>: @copydoc FLAG_APPLIED
 * - <b>f_inv_locked</b>: @copydoc FLAG_INV_LOCKED
 * - <b>f_is_wooded</b>: @copydoc FLAG_IS_WOODED
 * - <b>f_is_hilly</b>: @copydoc FLAG_IS_HILLY
 * - <b>f_has_ready_skill</b>: @copydoc FLAG_READY_SKILL
 * - <b>f_has_ready_weapon</b>: @copydoc FLAG_READY_WEAPON
 * - <b>f_no_skill_ident</b>: @copydoc FLAG_NO_SKILL_IDENT
 * - <b>f_can_see_in_dark</b>: @copydoc FLAG_SEE_IN_DARK
 * - <b>f_is_cauldron</b>: @copydoc FLAG_IS_CAULDRON
 * - <b>f_is_dust</b>: @copydoc FLAG_DUST
 * - <b>f_no_steal</b>: @copydoc FLAG_NO_STEAL
 * - <b>f_one_hit</b>: @copydoc FLAG_ONE_HIT
 * - <b>f_berserk</b>: @copydoc FLAG_BERSERK
 * - <b>f_no_attack</b>: @copydoc FLAG_NO_ATTACK
 * - <b>f_invulnerable</b>: @copydoc FLAG_INVULNERABLE
 * - <b>f_quest_item</b>: @copydoc FLAG_QUEST_ITEM
 * - <b>f_is_vul_elemental</b>: @copydoc FLAG_VUL_ELEMENTAL
 * - <b>f_is_proof_elemental</b>: @copydoc FLAG_PROOF_ELEMENTAL
 * - <b>f_is_vul_magic</b>: @copydoc FLAG_VUL_MAGIC
 * - <b>f_is_proof_magic</b>: @copydoc FLAG_PROOF_MAGIC
 * - <b>f_is_vul_physical</b>: @copydoc FLAG_VUL_PHYSICAL
 * - <b>f_is_proof_physical</b>: @copydoc FLAG_PROOF_PHYSICAL
 * - <b>f_sys_object</b>: @copydoc FLAG_SYS_OBJECT
 * - <b>f_use_fix_pos</b>: @copydoc FLAG_USE_FIX_POS
 * - <b>f_unpaid</b>: @copydoc FLAG_UNPAID
 * - <b>f_is_aged</b>: @copydoc FLAG_IS_AGED
 * - <b>f_make_invisible</b>: @copydoc FLAG_MAKE_INVISIBLE
 * - <b>f_make_ethereal</b>: @copydoc FLAG_MAKE_ETHEREAL
 * - <b>f_is_named</b>: @copydoc FLAG_IS_NAMED
 * - <b>f_no_teleport</b>: @copydoc FLAG_NO_TELEPORT
 * - <b>f_corpse</b>: @copydoc FLAG_CORPSE
 * - <b>f_corpse_forced</b>: @copydoc FLAG_CORPSE_FORCED
 * - <b>f_player_only</b>: @copydoc FLAG_PLAYER_ONLY
 * - <b>f_no_cleric</b>: @copydoc FLAG_NO_CLERIC
 * - <b>f_one_drop</b>: @copydoc FLAG_ONE_DROP
 * - <b>f_cursed_perm</b>: @copydoc FLAG_PERM_CURSED
 * - <b>f_damned_perm</b>: @copydoc FLAG_PERM_DAMNED
 * - <b>f_door_closed</b>: @copydoc FLAG_DOOR_CLOSED
 * - <b>f_is_missile</b>: @copydoc FLAG_IS_MISSILE
 * - <b>f_can_reflect_missile</b>: @copydoc FLAG_CAN_REFL_MISSILE
 * - <b>f_can_reflect_spell</b>: @copydoc FLAG_CAN_REFL_SPELL
 * - <b>flag_is_assassination</b>: @copydoc FLAG_IS_ASSASSINATION */
static char *flag_names[NUM_FLAGS + 1] =
{
	"f_sleep",               "f_confused",          "f_paralyzed",           "f_scared",          "f_is_blind",
	"f_is_invisible",        "f_is_ethereal",       "f_is_good",             "f_no_pick",         "f_walk_on",
	"f_no_pass",             "f_is_animated",       "f_slow_move",           "f_flying",          "f_monster",
	"f_friendly",            NULL,                  "f_been_applied",        "f_auto_apply",      "f_treasure",
	"f_is_neutral",          "f_see_invisible",     "f_can_roll",            "f_generator",       "f_is_turnable",
	"f_walk_off",            "f_fly_on",            "f_fly_off",             "f_is_used_up",      "f_identified",
	"f_reflecting",          "f_changing",          "f_splitting",           "f_hitback",         "f_startequip",
	"f_blocksview",          "f_undead",            NULL,                    "f_unaggressive",    "f_reflect_missile",
	"f_reflect_spell",       "f_no_magic",          "f_no_fix_player",       "f_is_evil",         "f_tear_down",
	"f_run_away",            "f_pass_thru",         "f_can_pass_thru",       "f_pick_up",         "f_unique",
	"f_no_drop",             "f_is_indestructible", "f_can_cast_spell",      "f_can_use_scroll",  "f_can_use_range",
	"f_can_use_bow",         "f_can_use_armour",    "f_can_use_weapon",      "f_can_use_ring",    "f_has_ready_range",
	"f_has_ready_bow",       "f_xrays",             "f_no_apply",            "f_is_floor",        "f_lifesave",
	"f_is_magical",          "f_alive",             "f_stand_still",         "f_random_move",     "f_only_attack",
	"f_wiz",                 "f_stealth",           NULL,                    NULL,                "f_cursed",
	"f_damned",              "f_see_anywhere",      "f_known_magical",       "f_known_cursed",    "f_can_use_skill",
	"f_is_thrown",           "f_is_vul_sphere",     "f_is_proof_sphere",     "f_is_male",         "f_is_female",
	"f_applied",             "f_inv_locked",        "f_is_wooded",           "f_is_hilly",        "f_has_ready_skill",
	"f_has_ready_weapon",    "f_no_skill_ident",    NULL,                    "f_can_see_in_dark", "f_is_cauldron",
	"f_is_dust",             "f_no_steal",          "f_one_hit",             NULL,                "f_berserk",
	"f_no_attack",           "f_invulnerable",      "f_quest_item",          NULL,                "f_is_vul_elemental",
	"f_is_proof_elemental",  "f_is_vul_magic",      "f_is_proof_magic",      "f_is_vul_physical", "f_is_proof_physical",
	"f_sys_object",          "f_use_fix_pos",       "f_unpaid",              "f_is_aged",         "f_make_invisible",
	"f_make_ethereal",       NULL,                  "f_is_named",            NULL,                "f_no_teleport",
	"f_corpse",              "f_corpse_forced",     "f_player_only",         "f_no_cleric",       "f_one_drop",
	"f_cursed_perm",         "f_damned_perm",       "f_door_closed",         NULL,                "f_is_missile",
	"f_can_reflect_missile", "f_can_reflect_spell", "flag_is_assassination", NULL,                NULL,
	NULL
};

/** This is filled in when we initialize our object type */
static PyGetSetDef Object_getseters[NUM_OBJFIELDS + NUM_FLAGS + 1];

/* Our actual Python ObjectType */
PyTypeObject Atrinik_ObjectType =
{
	PyObject_HEAD_INIT(NULL)
	0,                         /* ob_size*/
	"Atrinik.Object",         /* tp_name*/
	sizeof(Atrinik_Object),   /* tp_basicsize*/
	0,                         /* tp_itemsize*/
	(destructor)Atrinik_Object_dealloc, /* tp_dealloc*/
	0,                         /* tp_print*/
	0,                         /* tp_getattr*/
	0,                         /* tp_setattr*/
	0,                         /* tp_compare*/
	0,                         /* tp_repr*/
	0,                         /* tp_as_number*/
	0,                         /* tp_as_sequence*/
	0,                         /* tp_as_mapping*/
	0,                         /* tp_hash */
	0,                         /* tp_call*/
	(reprfunc)Atrinik_Object_str,/* tp_str*/
	0,                         /* tp_getattro*/
	0,                         /* tp_setattro*/
	0,                         /* tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,        /* tp_flags*/
	"Atrinik objects",        /* tp_doc */
	0,		                   /* tp_traverse */
	0,		                   /* tp_clear */
	0,		                   /* tp_richcompare */
	0,		                   /* tp_weaklistoffset */
	0,		                   /* tp_iter */
	0,		                   /* tp_iternext */
	ObjectMethods,             /* tp_methods */
	0,                         /* tp_members */
	Object_getseters,          /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	0,                         /* tp_init */
	0,                         /* tp_alloc */
	Atrinik_Object_new,       /* tp_new */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};


/**
 * @anchor plugin_python_object_constants
 * Object constants. */
static Atrinik_Constant object_constants[] =
{
	{"MAP_INFO_NORMAL",              MAP_INFO_NORMAL},
	{"MAP_INFO_ALL",                 MAP_INFO_ALL},

	{"COST_TRUE",                    F_TRUE},
	{"COST_BUY",                     F_BUY},
	{"COST_SELL",                    F_SELL},

	{"APPLY_TOGGLE",                 0},
	{"APPLY_ALWAYS",                 AP_APPLY},
	{"UNAPPLY_ALWAYS",               AP_UNAPPLY},
	{"UNAPPLY_NO_MERGE",             AP_NO_MERGE},
	{"UNAPPLY_IGNORE_CURSE",         AP_IGNORE_CURSE},

	{"NEUTER",                       0},
	{"MALE",                         1},
	{"FEMALE",                       2},
	{"HERMAPHRODITE",                3},

	{"CAST_NORMAL",                  0},
	{"CAST_POTION",                  1},

	{"LEARN",                        0},
	{"UNLEARN",                      1},

	{"UNIDENTIFIED",                 0},
	{"IDENTIFIED",                   1},

	{"IDENTIFY_NORMAL",              0},
	{"IDENTIFY_ALL",                 1},
	{"IDENTIFY_MARKED",              2},

	{"CLONE_WITH_INVENTORY",         0},
	{"CLONE_WITHOUT_INVENTORY",      1},

	{"EXP_AGILITY",                  1},
	{"EXP_MENTAL",                   2},
	{"EXP_MAGICAL",                  3},
	{"EXP_PERSONAL",                 4},
	{"EXP_PHYSICAL",                 5},
	{"EXP_WISDOM",                   6},

	{"COLOR_ORANGE",                 NDI_ORANGE},
	{"COLOR_WHITE",                  NDI_WHITE},
	{"COLOR_NAVY",                   NDI_NAVY},
	{"COLOR_YELLOW",                 NDI_YELLOW},
	{"COLOR_BLUE",                   NDI_BLUE},
	{"COLOR_RED",                    NDI_RED},

	{"NDI_PLAYER",                   NDI_PLAYER},
	{"NDI_ANIM",                     NDI_ANIM},

	/* Argh, the object types. Make sure to keep up-to date if any are added/removed */
	{"TYPE_PLAYER",                  PLAYER},
	{"TYPE_BULLET",                  BULLET},
	{"TYPE_ROD",                     ROD},
	{"TYPE_TREASURE",                TREASURE},
	{"TYPE_POTION",                  POTION},
	{"TYPE_FOOD",                    FOOD},
	{"TYPE_POISON",                  POISON},
	{"TYPE_BOOK",                    BOOK},
	{"TYPE_CLOCK",                   CLOCK},
	{"TYPE_FBULLET",                 FBULLET},
	{"TYPE_FBALL",                   FBALL},
	{"TYPE_LIGHTNING",               LIGHTNING},
	{"TYPE_ARROW",                   ARROW},
	{"TYPE_BOW",                     BOW},
	{"TYPE_WEAPON",                  WEAPON},
	{"TYPE_ARMOUR",                  ARMOUR},
	{"TYPE_PEDESTAL",                PEDESTAL},
	{"TYPE_ALTAR",                   ALTAR},
	{"TYPE_CONFUSION",               CONFUSION},
	{"TYPE_LOCKED_DOOR",             LOCKED_DOOR},
	{"TYPE_SPECIAL_KEY",             SPECIAL_KEY},
	{"TYPE_MAP",                     MAP},
	{"TYPE_DOOR",                    DOOR},
	{"TYPE_KEY",                     KEY},
	{"TYPE_MMISSILE",                MMISSILE},
	{"TYPE_TIMED_GATE",              TIMED_GATE},
	{"TYPE_TRIGGER",                 TRIGGER},
	{"TYPE_GRIMREAPER",              GRIMREAPER},
	{"TYPE_MAGIC_EAR",               MAGIC_EAR},
	{"TYPE_TRIGGER_BUTTON",          TRIGGER_BUTTON},
	{"TYPE_TRIGGER_ALTAR",           TRIGGER_ALTAR},
	{"TYPE_TRIGGER_PEDESTAL",        TRIGGER_PEDESTAL},
	{"TYPE_SHIELD",                  SHIELD},
	{"TYPE_HELMET",                  HELMET},
	{"TYPE_HORN",                    HORN},
	{"TYPE_MONEY",                   MONEY},
	{"TYPE_CLASS",                   CLASS},
	{"TYPE_GRAVESTONE",              GRAVESTONE},
	{"TYPE_AMULET",                  AMULET},
	{"TYPE_PLAYERMOVER",             PLAYERMOVER},
	{"TYPE_TELEPORTER",              TELEPORTER},
	{"TYPE_CREATOR",                 CREATOR},
	{"TYPE_SKILL",                   SKILL},
	{"TYPE_EXPERIENCE",              EXPERIENCE},
	{"TYPE_EARTHWALL",               EARTHWALL},
	{"TYPE_GOLEM",                   GOLEM},
	{"TYPE_BOMB",                    BOMB},
	{"TYPE_THROWN_OBJ",              THROWN_OBJ},
	{"TYPE_BLINDNESS",               BLINDNESS},
	{"TYPE_GOD",                     GOD},
	{"TYPE_DETECTOR",                DETECTOR},
	{"TYPE_SPEEDBALL",               SPEEDBALL},
	{"TYPE_DEAD_OBJECT",             DEAD_OBJECT},
	{"TYPE_DRINK",                   DRINK},
	{"TYPE_MARKER",                  MARKER},
	{"TYPE_HOLY_ALTAR",              HOLY_ALTAR},
	{"TYPE_PLAYER_CHANGER",          PLAYER_CHANGER},
	{"TYPE_PEACEMAKER",              PEACEMAKER},
	{"TYPE_GEM",                     GEM},
	{"TYPE_FIRECHEST",               FIRECHEST},
	{"TYPE_FIREWALL",                FIREWALL},
	{"TYPE_ANVIL",                   ANVIL},
	{"TYPE_CHECK_INV",               CHECK_INV},
	{"TYPE_MOOD_FLOOR",              MOOD_FLOOR},
	{"TYPE_EXIT",                    EXIT},
	{"TYPE_AGE_FORCE",               TYPE_AGE_FORCE},
	{"TYPE_SHOP_FLOOR",              SHOP_FLOOR},
	{"TYPE_SHOP_MAT",                SHOP_MAT},
	{"TYPE_RING",                    RING},
	{"TYPE_FLOOR",                   FLOOR},
	{"TYPE_FLESH",                   FLESH},
	{"TYPE_INORGANIC",               INORGANIC},
	{"TYPE_LIGHT_APPLY",             TYPE_LIGHT_APPLY},
	{"TYPE_LIGHTER",                 LIGHTER},
	{"TYPE_TRAP_PART",               TRAP_PART},
	{"TYPE_WALL",                    WALL},
	{"TYPE_LIGHT_SOURCE",            LIGHT_SOURCE},
	{"TYPE_MISC_OBJECT",             MISC_OBJECT},
	{"TYPE_MONSTER",                 MONSTER},
	{"TYPE_SPAWN_POINT",             SPAWN_POINT},
	{"TYPE_LIGHT_REFILL",            TYPE_LIGHT_REFILL},
	{"TYPE_SPAWN_POINT_MOB",         SPAWN_POINT_MOB},
	{"TYPE_SPAWN_POINT_INFO",        SPAWN_POINT_INFO},
	{"TYPE_SPELLBOOK",               SPELLBOOK},
	{"TYPE_ORGANIC",                 ORGANIC},
	{"TYPE_CLOAK",                   CLOAK},
	{"TYPE_CONE",                    CONE},
	{"TYPE_AURA",                    AURA},
	{"TYPE_SPINNER",                 SPINNER},
	{"TYPE_GATE",                    GATE},
	{"TYPE_BUTTON",                  BUTTON},
	{"TYPE_CF_HANDLE",               CF_HANDLE},
	{"TYPE_PIT",                     PIT},
	{"TYPE_TRAPDOOR",                TRAPDOOR},
	{"TYPE_WORD_OF_RECALL",          WORD_OF_RECALL},
	{"TYPE_PARAIMAGE",               PARAIMAGE},
	{"TYPE_SIGN",                    SIGN},
	{"TYPE_BOOTS",                   BOOTS},
	{"TYPE_GLOVES",                  GLOVES},
	{"TYPE_BASE_INFO",               TYPE_BASE_INFO},
	{"TYPE_RANDOM_DROP",             TYPE_RANDOM_DROP},
	{"TYPE_CONVERTER",               CONVERTER},
	{"TYPE_BRACERS",                 BRACERS},
	{"TYPE_POISONING",               POISONING},
	{"TYPE_SAVEBED",                 SAVEBED},
	{"TYPE_POISONCLOUD",             POISONCLOUD},
	{"TYPE_FIREHOLES",               FIREHOLES},
	{"TYPE_WAND",                    WAND},
	{"TYPE_ABILITY",                 ABILITY},
	{"TYPE_SCROLL",                  SCROLL},
	{"TYPE_DIRECTOR",                DIRECTOR},
	{"TYPE_GIRDLE",                  GIRDLE},
	{"TYPE_FORCE",                   FORCE},
	{"TYPE_POTION_EFFECT",           POTION_EFFECT},
	{"TYPE_JEWEL",                   TYPE_JEWEL},
	{"TYPE_NUGGET",                  TYPE_NUGGET},
	{"TYPE_EVENT_OBJECT",            TYPE_EVENT_OBJECT},
	{"TYPE_WAYPOINT_OBJECT",         TYPE_WAYPOINT_OBJECT},
	{"TYPE_QUEST_CONTAINER",         TYPE_QUEST_CONTAINER},
	{"TYPE_CLOSE_CON",               CLOSE_CON},
	{"TYPE_CONTAINER",               CONTAINER},
	{"TYPE_ARMOUR_IMPROVER",         ARMOUR_IMPROVER},
	{"TYPE_WEAPON_IMPROVER",         WEAPON_IMPROVER},
	{"TYPE_WEALTH",                  TYPE_WEALTH},
	{"TYPE_SKILLSCROLL",             SKILLSCROLL},
	{"TYPE_DEEP_SWAMP",              DEEP_SWAMP},
	{"TYPE_IDENTIFY_ALTAR",          IDENTIFY_ALTAR},
	{"TYPE_CANCELLATION",            CANCELLATION},
	{"TYPE_MENU",                    MENU},
	{"TYPE_BALL_LIGHTNING",          BALL_LIGHTNING},
	{"TYPE_SWARM_SPELL",             SWARM_SPELL},
	{"TYPE_RUNE",                    RUNE},
	{"TYPE_POWER_CRYSTAL",           POWER_CRYSTAL},
	{"TYPE_CORPSE",                  CORPSE},
	{"TYPE_DISEASE",                 DISEASE},
	{"TYPE_SYMPTOM",                 SYMPTOM},

	{NULL, 0}
};

/**
 * @defgroup plugin_python_object_functions Python plugin object functions
 * Object related functions used in Atrinik Python plugin.
 *@{*/

/**
 * <h1>object.GetSkill(<i>\<int\></i> type, <i>\<int\></i> id)</h1>
 *
 * Fetch a skill or exp_skill object from the specified object.
 * @param type Type of the object to look for. Unused.
 * @param id ID of the skill or experience
 * @return The object if found.
 * @todo Remove the type parameter? */
static PyObject *Atrinik_Object_GetSkill(Atrinik_Object *whoptr, PyObject *args)
{
	object *tmp;
	int type, id;

	if (!PyArg_ParseTuple(args, "ii", &type, &id))
	{
		return NULL;
	}

	/* Browse the inventory of object to find a matching skill or exp_obj. */
	for (tmp = WHO->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == SKILL && tmp->stats.sp == id)
		{
			return wrap_object(tmp);
		}

		if (tmp->type == EXPERIENCE && tmp->sub_type1 == id)
		{
			return wrap_object(tmp);
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SetSkill(<i>\<int\></i> type, <i>\<int\></i> skillid,
 * <i>\<long\></i> level, <i>\<long\></i> value)</h1>
 *
 * Set object's experience in the skill to a new value.
 * Also can change the level of a skill.
 * @param type Type of the skill, should be TYPE_SKILL
 * @param skillid ID of the skill
 * @param level Level to set for the skill. Must be non zero to set,
 * otherwise experience is set instead.
 * @param value Experience to set for the skill. Only set if level is
 * lower than 1.
 * @todo Overall experience is not changed (should it be?) */
static PyObject *Atrinik_Object_SetSkill(Atrinik_Object *whoptr, PyObject *args)
{
	object *tmp;
	int type, skill, currentxp;
	long level, value;

	if (!PyArg_ParseTuple(args, "iill", &type, &skill, &level, &value))
	{
		return NULL;
	}

	/* We don't set anything in exp_obj types */
	if (type != SKILL)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	/* Browse the inventory of object to find a matching skill. */
	for (tmp = WHO->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == type && tmp->stats.sp == skill)
		{
			/*plugin_log(-1,"LEVEL1 %d (->%d) :: %s (exp %d)\n",tmp->level,level,query_name(tmp), tmp->stats.exp);*/

			/* This is a bit tricky: some skills are marked with exp -1
			 * or -2 as special used skills (level but no exp):
			 * if we have here a level > 0, we set level but NEVER exp.
			 * if we have level == 0, we only set exp - the
			 * addexp */
			if (level > 0)
			{
				tmp->level = level;
			}
			else
			{
				currentxp = tmp->stats.exp;
				value = value - currentxp;

				GCFP.Value[0] = (void *) (WHO);
				GCFP.Value[1] = (void *) (&value);
				GCFP.Value[2] = (void *) (&skill);
				(PlugHooks[HOOK_ADDEXP])(&GCFP);
			}

			/*plugin_log(-1,"LEVEL2 %d (->%d) :: %s (exp %d)\n",tmp->level,level,query_name(tmp), tmp->stats.exp);*/

			/* We will sure change skill exp, mark for update */
			if (WHO->type == PLAYER && CONTR(WHO))
			{
				CONTR(WHO)->update_skills = 1;
			}

			Py_INCREF(Py_None);
			return Py_None;
		}
	}

	RAISE("Unknown skill");
}

/**
 * <h1>object.ActivateRune(<i>\<object\></i> who)</h1>
 *
 * Activate a rune.
 * @param who Who should be affected by the effects of the rune.
 * @warning Untested. */
static PyObject *Atrinik_Object_ActivateRune(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &whatptr))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHAT);
	GCFP.Value[1] = (void *) (WHO);
	(PlugHooks[HOOK_SPRINGTRAP])(&GCFP);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.CheckTrigger(<i>\<object\></i> who)</h1>
 *
 * Check trigger of an object.
 * @param who Who is triggering the trigger object.
 * @warning Unfinished, do not use.
 * @todo Create a hook for check_trigger() to make this work. */
static PyObject *Atrinik_Object_CheckTrigger(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;

	(void) whoptr;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &whatptr))
	{
		return NULL;
	}

	/* check_trigger(WHAT,WHO); should be hook too! */

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.GetGod()</h1>
 *
 * Determine who is the object follower of (who the god is)
 * @return Returns a string of the god's name. */
static PyObject *Atrinik_Object_GetGod(Atrinik_Object *whoptr, PyObject *args)
{
	CFParm *CFR;
	static char *value;

	if (!PyArg_ParseTuple(args, ""))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	CFR = (PlugHooks[HOOK_DETERMINEGOD])(&GCFP);
	value = (char *) (CFR->Value[0]);
	free(CFR);

	return Py_BuildValue("s", value);
}

/**
 * <h1>object.SetGod(<i>\<string\></i> godname)</h1>
 *
 * Make an object become follower of a different god.
 * @param godname Name of the god
 * @warning Unfinished, do not use.
 * @todo Finish this. */
static PyObject *Atrinik_Object_SetGod(Atrinik_Object *whoptr, PyObject *args)
{
	char *txt;
	const char *prayname;
	object *tmp;
	CFParm *CFR0, *CFR;
	int value;

	if (!PyArg_ParseTuple(args, "s", &txt))
	{
		return NULL;
	}

	prayname = add_string_hook("praying");

	GCFP1.Value[0] = (void *) (WHO);
	GCFP1.Value[1] = (void *) (prayname);

	GCFP2.Value[0] = (void *) (WHO);
	GCFP0.Value[0] = (char *) (txt);
	CFR0 = (PlugHooks[HOOK_FINDGOD])(&GCFP0);
	tmp = (object *) (CFR0->Value[0]);
	free(CFR0);
	GCFP2.Value[1] = (void *) (tmp);

	CFR = (PlugHooks[HOOK_CMDRSKILL])(&GCFP1);
	value = *(int *) (CFR->Value[0]);

	if (value)
	{
		(PlugHooks[HOOK_BECOMEFOLLOWER])(&GCFP2);
	}

	free(CFR);
	FREE_STRING_HOOK(prayname);
	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.TeleportTo(<i>\<string\></i> map, <i>\<int\></i> x,
 * <i>\<int\></i> y, <i>\<int\></i> unique)</h1>
 *
 * Teleport object to the given position of map.
 * @param map Map name to teleport the object to
 * @param x X position on the map
 * @param y Y position on the map
 * @param unique If non-zero, the destination will be unique map for the
 * player. Optional, defaults to 0. */
static PyObject *Atrinik_Object_TeleportTo(Atrinik_Object *whoptr, PyObject *args)
{
	char *mapname, *msg = NULL;
	int x, y, u = 0;

	if (!PyArg_ParseTuple(args, "sii|i", &mapname, &x, &y, &u))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (char *) (mapname);
	GCFP.Value[2] = (void *) (&x);
	GCFP.Value[3] = (void *) (&y);
	GCFP.Value[4] = (void *) (&u);
	GCFP.Value[5] = (char *) (msg);
	(PlugHooks[HOOK_TELEPORTOBJECT])(&GCFP);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.InsertInside(<i>\<object\></i> where)</h1>
 *
 * Insert object into where.
 * @param where Where to insert the object. */
static PyObject *Atrinik_Object_InsertInside(Atrinik_Object *whatptr, PyObject *args)
{
	Atrinik_Object *whereptr;
	object *myob, *obenv, *tmp;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &whereptr))
	{
		return NULL;
	}

	myob = WHAT;
	obenv = myob->env;

	if (!QUERY_FLAG(myob, FLAG_REMOVED))
	{
		GCFP.Value[0] = (void *) (myob);
		(PlugHooks[HOOK_REMOVEOBJECT])(&GCFP);
	}

	myob = insert_ob_in_ob_hook(myob, WHERE);

	/* Make sure the inventory image/text is updated */
	for (tmp = WHERE; tmp != NULL; tmp = tmp->env)
	{
		if (tmp->type == PLAYER)
		{
			GCFP.Value[0] = (void *) (tmp);
			GCFP.Value[1] = (void *) (myob);
			(PlugHooks[HOOK_ESRVSENDITEM])(&GCFP);

			break;
		}
	}

	/* If we're taking from player. */
	for (tmp = obenv; tmp != NULL; tmp = tmp->env)
	{
		if (tmp->type == PLAYER)
		{
			GCFP.Value[0] = (void *) (tmp);
			GCFP.Value[1] = (void *) (tmp);
			(PlugHooks[HOOK_ESRVSENDINVENTORY])(&GCFP);

			break;
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Apply(<i>\<object\></i> what, <i>\<int\></i> flags)</h1>
 *
 * Forces object to apply what.
 * @param what What object to apply.
 * @param flas Reasonable combination of the following:
 * - <b>Atrinik.APPLY_TOGGLE</b>: Normal apply (toggle)
 * - <b>Atrinik.APPLY_ALWAYS</b>: Always apply (never unapply)
 * - <b>Atrinik.UNAPPLY_ALWAYS</b>: Always unapply (never apply)
 * - <b>Atrinik.UNAPPLY_NOMERGE</b>: Don't merge unapplied items
 * - <b>Atrinik.UNAPPLY_IGNORE_CURSE</b>: Unapply cursed items
 * @return Return values:
 * - <b>0</b>: Object cannot apply objects of that type.
 * - <b>1</b>: Object was applied, or not...
 * - <b>2</b>: Object must be in inventory to be applied */
static PyObject *Atrinik_Object_Apply(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;
	int flags, retval;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "O!i", &Atrinik_ObjectType, &whatptr, &flags))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (WHAT);
	GCFP.Value[2] = (void *) (&flags);
	CFR = (PlugHooks[HOOK_MANUALAPPLY])(&GCFP);
	retval = *(int *) (CFR->Value[0]);
	free(CFR);

	return Py_BuildValue("i", retval);
}

/**
 * <h1>object.PickUp(<i>\<object\></i> what)</h1>
 *
 * Force the object to pick up what.
 * @param what The object to pick up */
static PyObject *Atrinik_Object_PickUp(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &whatptr))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (WHAT);
	(PlugHooks[HOOK_PICKUP])(&GCFP);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Drop(<i>\<string\></i> what)</h1>
 *
 * Equivalent to the player command "/drop".
 *
 * @param what Object name, "all", "unpaid", "cursed", "unlocked" or a
 * count + object name:\n
 * "\<nnn\> \<object name\>", or a base name, or a short name... */
static PyObject *Atrinik_Object_Drop(Atrinik_Object *whoptr, PyObject *args)
{
	char *name;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "s", &name))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (name);
	CFR = (PlugHooks[HOOK_CMDDROP])(&GCFP);
	free(CFR);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Deposit(<i>\<object\></i> deposit_object, <i>\<string\></i>
 * string)</h1>
 *
 * Deposit value or string money from object in deposit_object.
 *
 * Control first object has that amount of money, then remove it
 * from object and add it in ->value of deposit_object.
 *
 * @param deposit_object The deposit object
 * @param string How much money to deposit, in string representation.
 * @return The value deposited. */
static PyObject *Atrinik_Object_Deposit(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *obptr;
	char *text;
	CFParm *CFR;
	int value;

	if (!PyArg_ParseTuple(args, "O!s", &Atrinik_ObjectType, &obptr, &text))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (obptr->obj);
	GCFP.Value[2] = (void *) (text);

	CFR = (PlugHooks[HOOK_DEPOSIT])(&GCFP);

	value = *(int *) (CFR->Value[0]);

	return Py_BuildValue("i", value);
}

/**
 * <h1>object.Withdraw(<i>\<object\></i> deposit_object, <i>\<string\>
 * </i> string)</h1>
 *
 * Withdraw value or string money from object in deposit_object.
 *
 * @param deposit_object The withdraw object
 * @param string How much money to withdraw, in string representation.
 * @return The value withdrawn. */
static PyObject *Atrinik_Object_Withdraw(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *obptr;
	char *text;
	CFParm *CFR;
	int value;

	if (!PyArg_ParseTuple(args, "O!s", &Atrinik_ObjectType, &obptr, &text))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (obptr->obj);
	GCFP.Value[2] = (void *) (text);

	CFR = (PlugHooks[HOOK_WITHDRAW])(&GCFP);

	value = *(int *) (CFR->Value[0]);

	return Py_BuildValue("i", value);
}

/**
 * <h1>object.Communicate(<i>\<string\></i> message)</h1>
 *
 * Object says message to everybody on its map.
 *
 * @param message The message to say. */
static PyObject *Atrinik_Object_Communicate(Atrinik_Object *whoptr, PyObject *args)
{
	char *message;

	if (!PyArg_ParseTuple(args, "s", &message))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (message);

	(PlugHooks[HOOK_COMMUNICATE])(&GCFP);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Say(<i>\<string\></i> message, <i>\<int\></i> mode)</h1>
 *
 * Object says message to everybody on its map.
 *
 * @param message The message to say.
 * @param mode If set to non-zero, message is not prefixed with
 * "object.name says: ". Optional, defaults to 0. */
static PyObject *Atrinik_Object_Say(Atrinik_Object *whoptr, PyObject *args)
{
	char *message;
	static char buf[HUGE_BUF];
	int val, d = MAP_INFO_NORMAL, x, y, mode = 0;

	if (!PyArg_ParseTuple(args, "s|i", &message, &mode))
	{
		return NULL;
	}

	val = NDI_NAVY | NDI_UNIQUE;
	x = WHO->x;
	y = WHO->y;

	if (mode)
	{
		GCFP.Value[5] = (void *) (message);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s says: %s", query_name(WHO, NULL), message);
		GCFP.Value[5] = (void *) (buf);
	}

	GCFP.Value[0] = (void *) (&val);
	GCFP.Value[1] = (void *) (WHO->map);
	GCFP.Value[2] = (void *) (&x);
	GCFP.Value[3] = (void *) (&y);
	GCFP.Value[4] = (void *) (&d);

	(PlugHooks[HOOK_NEWINFOMAP])(&GCFP);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SayTo(<i>\<object\></i> target, <i>\<string\></i> message,
 * <i>\<int\></i> mode)</h1>
 *
 * NPC talks only to player but map gets "xxx talks to yyy" msg too.
 *
 * @param target Target object the NPC is talking to.
 * @param message The message to say.
 * @param mode If set to non-zero, there is no "xxx talks to yyy" map
 * message. The message is not prefixed with "xxx says: " either.
 * Optional, defaults to 0. */
static PyObject *Atrinik_Object_SayTo(Atrinik_Object *whoptr, PyObject *args)
{
	object *target;
	Atrinik_Object *obptr2;
	char *message;
	static char buf[HUGE_BUF];
	int val, d = MAP_INFO_NORMAL, x, y, zero = 0, mode = 0;

	if (!PyArg_ParseTuple(args, "O!s|i", &Atrinik_ObjectType, &obptr2, &message, &mode))
	{
		return NULL;
	}

	target = obptr2->obj;

	if (mode)
	{
		GCFP.Value[3] = (void *) (message);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s talks to %s.", query_name(WHO, NULL), query_name(target, NULL));
		val = NDI_UNIQUE;
		x = WHO->x;
		y = WHO->y;

		GCFP.Value[0] = (void *) (&val);
		GCFP.Value[1] = (void *) (WHO->map);
		GCFP.Value[2] = (void *) (&x);
		GCFP.Value[3] = (void *) (&y);
		GCFP.Value[4] = (void *) (&d);
		GCFP.Value[5] = (void *) (WHO);
		GCFP.Value[6] = (void *) (target);
		GCFP.Value[7] = (void *) (buf);
		(PlugHooks[HOOK_NEWINFOMAPEXCEPT])(&GCFP);

		snprintf(buf, sizeof(buf), "\n%s says: %s", query_name(WHO, NULL), message);
		GCFP.Value[3] = (void *) (buf);
	}

	val = NDI_NAVY | NDI_UNIQUE;
	GCFP.Value[0] = (void *) (&val);
	GCFP.Value[1] = (void *) (&zero);
	GCFP.Value[2] = (void *) (target);
	(PlugHooks[HOOK_NEWDRAWINFO])(&GCFP);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Write(<i>\<string\></i> message, <i>\<int\></i> color)</h1>
 *
 * Writes a message to a specific player.
 *
 * @param message The message to write
 * @param color Color to write the message in. Defaults to orange. */
static PyObject *Atrinik_Object_Write(Atrinik_Object *whoptr, PyObject *args)
{
	int zero = 0, color = NDI_UNIQUE | NDI_ORANGE;
	char *message;

	if (!PyArg_ParseTuple(args, "s|i", &message, &color))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (&color);
	GCFP.Value[1] = (void *) (&zero);
	GCFP.Value[2] = (void *) (WHO);
	GCFP.Value[3] = (void *) (message);

	(PlugHooks[HOOK_NEWDRAWINFO])(&GCFP);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SetGender(<i>\<int\></i> gender)</h1>
 *
 * Changes the gender of object.
 *
 * @param gender The new gender to set. One of:
 * - <b>Atrinik.NEUTER</b>: No gender
 * - <b>Atrinik.MALE</b>: Male gender
 * - <b>Atrinik.GENDER_FEMALE</b>: Female gender
 * - <b>Atrinik.HERMAPHRODITE</b>: Both male and female */
static PyObject *Atrinik_Object_SetGender(Atrinik_Object *whoptr, PyObject *args)
{
	int gender;

	if (!PyArg_ParseTuple(args, "i", &gender))
	{
		return NULL;
	}

	/* Set object to neuter */
	CLEAR_FLAG(WHO, FLAG_IS_MALE);
	CLEAR_FLAG(WHO, FLAG_IS_FEMALE);

	/* Reset to male or female */
	if (gender & 1)
	{
		SET_FLAG(WHO, FLAG_IS_MALE);
	}

	if (gender & 2)
	{
		SET_FLAG(WHO, FLAG_IS_FEMALE);
	}

	/* Update the players client if object was a player */
	if (WHO->type == PLAYER)
	{
		CONTR(WHO)->socket.ext_title_flag = 1;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SetRank(<i>\<string\></i> rank_string)</h1>
 *
 * Set the rank of an object to rank_string.
 *
 * @param rank_string The new rank string to set. If "Mr", then clear the
 * rank.
 * @return If successfully set the rank, return the rank force object. */
static PyObject *Atrinik_Object_SetRank(Atrinik_Object *whoptr, PyObject *args)
{
	object *walk;
	char *rank;

	if (!PyArg_ParseTuple(args, "s", &rank))
	{
		return NULL;
	}

	if (WHO->type != PLAYER)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	for (walk = WHO->inv; walk != NULL; walk = walk->below)
	{
		if (walk->name && !strcmp(walk->name, "RANK_FORCE") && !strcmp(walk->arch->name, "rank_force"))
		{
			/* We find the rank of the player, now change it to new one */
			if (walk->title)
			{
				FREE_STRING_HOOK(walk->title);
			}

			/* "Mr" is keyword to clear title and not add it as rank */
			if (strcmp(rank, "Mr"))
			{
				walk->title = add_string_hook(rank);
			}

			/* Demand update to client */
			CONTR(WHO)->socket.ext_title_flag = 1;
			return wrap_object(walk);
		}
	}

	plugin_log(llevDebug, "Python Warning:: SetRank: Object %s has no rank_force!\n", query_name(WHO, NULL));

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SetAlignment(<i>\<string\></i> alignment_string)</h1>
 *
 * Set the alignment of an object to alignment_string.
 *
 * @param alignment_string The new alignment string to set.
 * @return If successfully set the alignment, return the alignment force
 * object. */
static PyObject *Atrinik_Object_SetAlignment(Atrinik_Object *whoptr, PyObject *args)
{
	object *walk;
	char *align;

	if (!PyArg_ParseTuple(args, "s", &align))
	{
		return NULL;
	}

	if (WHO->type != PLAYER)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	for (walk = WHO->inv; walk != NULL; walk = walk->below)
	{
		if (walk->name && !strcmp(walk->name, "ALIGNMENT_FORCE")  && !strcmp(walk->arch->name, "alignment_force"))
		{
			/* We find the alignment of the player, now change it to new one */
			if (walk->title)
			{
				FREE_STRING_HOOK(walk->title);
			}

			walk->title = add_string_hook(align);

			/* Demand update to client */
			CONTR(WHO)->socket.ext_title_flag = 1;

			return wrap_object(walk);
		}
	}

	plugin_log(llevDebug, "Python Warning:: SetAlignment: Object %s has no alignment_force!\n", query_name(WHO, NULL));

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.GetAlignmentForce()</h1>
 *
 * Get the alignment_force from object's inventory.
 *
 * @return The alignment force if found. */
static PyObject *Atrinik_Object_GetAlignmentForce(Atrinik_Object *whoptr, PyObject *args)
{
	object *walk;

	if (!PyArg_ParseTuple(args, ""))
	{
		return NULL;
	}

	if (WHO->type != PLAYER)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	for (walk = WHO->inv; walk != NULL; walk = walk->below)
	{
		if (walk->name && !strcmp(walk->name, "ALIGNMENT_FORCE") && !strcmp(walk->arch->name, "alignment_force"))
		{
			return wrap_object(walk);
		}
	}

	plugin_log(llevDebug, "Python Warning:: GetAlignmentForce: Object %s has no aligment_force!\n", query_name(WHO, NULL));

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SetGuildForce(<i>\<string\></i> rank_string)</h1>
 *
 * Set the current rank of object to rank_string.
 *
 * This only sets the title. The guild tag is in slaying field of the
 * force object.
 *
 * To test for guild force, use
 * @ref Atrinik_Object_GetGuildForce "Atrinik.GetGuildForce()".
 *
 * To set the guild tag you can use this function, because it returns the
 * guild force object after setting the title.
 *
 * @param rank_string The rank string in the guild to set.
 * @return The guild force object if found. */
static PyObject *Atrinik_Object_SetGuildForce(Atrinik_Object *whoptr, PyObject *args)
{
	object *walk;
	char *guild;

	if (!PyArg_ParseTuple(args, "s", &guild))
	{
		return NULL;
	}

	if (WHO->type != PLAYER)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	for (walk = WHO->inv; walk != NULL; walk = walk->below)
	{
		if (walk->name && !strcmp(walk->name, "GUILD_FORCE") && !strcmp(walk->arch->name, "guild_force"))
		{
			/* We find the rank of the player, now change it to new one */
			if (walk->title)
			{
				FREE_STRING_HOOK(walk->title);
			}

			if (guild && strcmp(guild, ""))
			{
				walk->title = add_string_hook(guild);
			}

			/* Demand update to client */
			CONTR(WHO)->socket.ext_title_flag = 1;

			return wrap_object(walk);
		}
	}

	plugin_log(llevDebug, "Python Warning:: SetGuildForce: Object %s has no guild_force!\n", query_name(WHO, NULL));

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.GetGuildForce()</h1>
 *
 * Get the guild force from player's inventory.
 *
 * @return The guild force object if found. */
static PyObject *Atrinik_Object_GetGuildForce(Atrinik_Object *whoptr, PyObject *args)
{
	object *walk;

	if (!PyArg_ParseTuple(args, ""))
	{
		return NULL;
	}

	if (WHO->type != PLAYER)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	for (walk = WHO->inv; walk != NULL; walk = walk->below)
	{
		if (walk->name && !strcmp(walk->name, "GUILD_FORCE") && !strcmp(walk->arch->name, "guild_force"))
		{
			return wrap_object(walk);
		}
	}

	plugin_log(llevDebug, "Python Warning:: GetGuildForce: Object %s has no guild_force!\n", query_name(WHO, NULL));

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Fix()</h1>
 *
 * Recalculate player's or monster's stats depending on equipment, forces
 * skills, etc.
 *
 * @warning Untested. */
static PyObject *Atrinik_Object_Fix(Atrinik_Object *whoptr, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
	{
		return NULL;
	}

	fix_player_hook(WHO);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Kill(<i>\<object\></i> what, <i>\<int\></i> how)</h1>
 *
 * Kill an object.
 *
 * @param what The object to kill
 * @param how How to kill the object.
 *
 * @warning Untested. */
static PyObject *Atrinik_Object_Kill(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;
	int ktype, k = 1;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "O!i", &Atrinik_ObjectType, &whatptr, &ktype))
	{
		return NULL;
	}

	WHAT->speed = 0;
	WHAT->speed_left = 0.0;
	GCFP.Value[0] = (void *) (WHAT);
	(PlugHooks[HOOK_UPDATESPEED])(&GCFP);

	if (QUERY_FLAG(WHAT, FLAG_REMOVED))
	{
		plugin_log(llevDebug, "Warning (from KillObject): Trying to remove removed object\n");
		RAISE("Trying to remove removed object");
	}
	else
	{
		WHAT->stats.hp = -1;
		GCFP.Value[0] = (void *) (WHAT);
		GCFP.Value[1] = (void *) (&k);
		GCFP.Value[2] = (void *) (WHO);
		GCFP.Value[3] = (void *) (&ktype);

		CFR = (PlugHooks[HOOK_KILLOBJECT])(&GCFP);
		free(CFR);
	}

	/* This is to avoid the attack routine to continue after we called
	 * killObject, since the attacked object no longer exists.
	 * By fixing guile_current_other to NULL, guile_use_weapon_script will
	 * return -1, meaning the attack function must be immediately terminated. */
	if (WHAT == StackOther[StackPosition])
	{
		StackOther[StackPosition] = NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.CastAbility(<i>\<object\></i> target, <i>\<int\></i>
 * spellno, <i>\<int\></i> mode, <i>\<int\></i> direction, <i>\<string\>
 * </i> option)</h1>
 *
 * Object casts the ability numbered spellno on target.
 * @param target The target object
 * @param spellno ID of the spell to cast
 * @param mode Atrinik.CAST_NORMAL or Atrinik.CAST_POTION
 * @param direction The direction to cast the ability in
 * @param option Additional string option(s) */
static PyObject *Atrinik_Object_CastAbility(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *target;
	int spell, dir, mode, parm = 1, parm2, typeoffire = FIRE_DIRECTIONAL;
	CFParm *CFR;
	char *op;

	if (!PyArg_ParseTuple(args, "O!iiis", &Atrinik_ObjectType, &target, &spell, &mode, &dir, &op))
	{
		return NULL;
	}

	if (WHO->type != PLAYER)
	{
		parm2 = spellNPC;
	}
	else
	{
		if (!mode)
		{
			parm2 = spellNormal;
		}
		else
		{
			parm2 = spellPotion;
		}
	}

	GCFP.Value[0] = (void *) (target->obj);
	GCFP.Value[1] = (void *) (WHO);
	GCFP.Value[2] = (void *) (&dir);
	GCFP.Value[3] = (void *) (&spell);
	GCFP.Value[4] = (void *) (&parm);
	GCFP.Value[5] = (void *) (&parm2);
	GCFP.Value[6] = (void *) (op);
	GCFP.Value[7] = (void *) (&typeoffire);

	CFR = (PlugHooks[HOOK_CASTSPELL])(&GCFP);

	free(CFR);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.DoKnowSpell(<i>\<int\></i> spell)</h1>
 *
 * Check if object knowns a given spell.
 * @param spell ID of the spell to check for
 * @return 1 if the object knows the spell, 0 otherwise */
static PyObject *Atrinik_Object_DoKnowSpell(Atrinik_Object *whoptr, PyObject *args)
{
	int spell, value;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "i", &spell))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (&spell);
	CFR = (PlugHooks[HOOK_CHECKFORSPELL])(&GCFP);
	value = *(int *) (CFR->Value[0]);
	free(CFR);

	return Py_BuildValue("i", value);
}

/**
 * <h1>object.AcquireSpell(<i>\<int\></i> spell, <i>\<int\></i> mode)
 * </h1>
 *
 * Object will learn or unlearn spell.
 * @param spell ID of the spell to learn/unlearn for
 * @param mode Possible modes:
 * - <b>Atrinik.LEARN</b>: Learn the spell
 * - <b>Atrinik.UNLEARN</b>: Unlearn the spell
 * @return 1 if the object knows the spell, 0 otherwise */
static PyObject *Atrinik_Object_AcquireSpell(Atrinik_Object *whoptr, PyObject *args)
{
	int spell, mode;

	if (!PyArg_ParseTuple(args, "ii", &spell, &mode))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (&spell);
	GCFP.Value[2] = (void *) (&mode);

	(PlugHooks[HOOK_LEARNSPELL])(&GCFP);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.DoKnowSkill(<i>\<int\></i> skill)</h1>
 *
 * Check if object knowns a given skill.
 * @param skill ID of the skill to check for
 * @return 1 if the object knows the skill, 0 otherwise */
static PyObject *Atrinik_Object_DoKnowSkill(Atrinik_Object *whoptr, PyObject *args)
{
	int skill, value;
	CFParm * CFR;

	if (!PyArg_ParseTuple(args, "i", &skill))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (&skill);
	CFR = (PlugHooks[HOOK_CHECKFORSKILLKNOWN])(&GCFP);
	value = *(int *) (CFR->Value[0]);
	free(CFR);

	return Py_BuildValue("i", value);
}

/**
 * <h1>object.AcquireSkill(<i>\<int\></i> skillno)</h1>
 *
 * Object will learn or unlearn skill.
 * @param skillno ID of the skill to learn/unlearn for
 * @return 1 if the object knows the skill, 0 otherwise */
static PyObject *Atrinik_Object_AcquireSkill(Atrinik_Object *whoptr, PyObject *args)
{
	int skill;

	if (!PyArg_ParseTuple(args, "i", &skill))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (&skill);
	(PlugHooks[HOOK_LEARNSKILL])(&GCFP);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.FindMarkedObject()</h1>
 *
 * Find marked object in object's inventory.
 * @return The marked object, or None if no object is marked. */
static PyObject *Atrinik_Object_FindMarkedObject(Atrinik_Object *whoptr, PyObject *args)
{
	object *value;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, ""))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	CFR = (PlugHooks[HOOK_FINDMARKEDOBJECT])(&GCFP);

	value = (object *) (CFR->Value[0]);

	return wrap_object(value);
}

/**
 * <h1>object.CheckInvisibleInside(<i>\<string\></i> id)</h1>
 *
 * Find a force inside object's inventory.
 *
 * @param id Slaying field of the force must match this ID.
 * @return The force object is found with matching ID.
 * @warning Untested. */
static PyObject *Atrinik_Object_CheckInvisibleInside(Atrinik_Object *whoptr, PyObject *args)
{
	char *id;
	object *tmp2;

	if (!PyArg_ParseTuple(args, "s", &id))
	{
		return NULL;
	}

	for (tmp2 = WHO->inv; tmp2 !=NULL; tmp2 = tmp2->below)
	{
		if (tmp2->type == FORCE && tmp2->slaying && !strcmp(tmp2->slaying, id))
		{
			break;
		}
	}

	return wrap_object(tmp2);
}

/**
 * <h1>object.CreatePlayerForce(<i>\<string\></i> force_name, <i>\<int\>
 * </i> time)</h1>
 *
 * Creates and inserts an invisible player force in object.
 *
 * The values of the force will affect the object it is in, which should
 * usually be player.
 *
 * @param force_name Name of the player force
 * @param time If non-zero, the force will be removed again after
 * time / 0.02 ticks. Optional, defaults to 0.
 * @return The new player force object. */
static PyObject *Atrinik_Object_CreatePlayerForce(Atrinik_Object *whereptr, PyObject *args)
{
	char *txt, txt2[16] = "player_force";
	object *myob;
	CFParm *CFR;
	int time = 0;

	if (!PyArg_ParseTuple(args, "s|i", &txt, &time))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (txt2);
	CFR = (PlugHooks[HOOK_GETARCHETYPE])(&GCFP);
	myob = (object *)(CFR->Value[0]);
	free(CFR);

	if (!myob)
	{
		plugin_log(llevDebug, "Python WARNING:: CreatePlayerForce: Can't find archetype 'player_force'\n");
		RAISE("Can't find archetype 'player_force'");
	}

	/* For temporary forces */
	if (time > 0)
	{
		SET_FLAG(myob, FLAG_IS_USED_UP);
		myob->stats.food = time;
		myob->speed = 0.02f;
		GCFP.Value[0] = (void *)(myob);
		(PlugHooks[HOOK_UPDATESPEED])(&GCFP);
	}

	/* Setup the force and put it in activator */
	if (myob->name)
	{
		FREE_STRING_HOOK(myob->name);
	}

	myob->name = add_string_hook(txt);
	myob = insert_ob_in_ob_hook(myob, WHERE);

	GCFP.Value[0] = (void *)(WHERE);
	GCFP.Value[1] = (void *)(myob);
	(PlugHooks[HOOK_ESRVSENDITEM])(&GCFP);

	return wrap_object(myob);
}

/**
 * <h1>object.GetQuestObject(<i>\<string\></i> quest_name)</h1>
 *
 * Get a quest object for specified quest.
 * @param quest_name Name of the quest to look for.
 * @return The quest object if found. */
static PyObject *Atrinik_Object_GetQuestObject(Atrinik_Object *whoptr, PyObject *args)
{
	char *quest_name;
	object *walk;

	if (!PyArg_ParseTuple(args, "s", &quest_name))
	{
		return NULL;
	}

	/* Let's first check the inventory for the quest_container object */
	for (walk = WHO->inv; walk != NULL; walk = walk->below)
	{
		if (walk->type == TYPE_QUEST_CONTAINER)
		{
			for (walk = walk->inv; walk != NULL; walk = walk->below)
			{
				if (!strcmp(walk->name, quest_name))
				{
					return wrap_object(walk);
				}
			}

			break;
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.StartQuest(<i>\<string\></i> quest_name)</h1>
 *
 * Create a quest object inside the specified object, starting a new
 * quest.
 * @param quest_name Name of the quest.
 * @return The newly created quest object. */
static PyObject *Atrinik_Object_StartQuest(Atrinik_Object *whoptr, PyObject *args)
{
	object *quest_container, *quest_object, *myob;
	char *quest_name, buf[MAX_BUF];
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "s", &quest_name))
	{
		return NULL;
	}

	if (!(quest_container = present_in_ob(TYPE_QUEST_CONTAINER, WHO)))
	{
		quest_container = create_quest_container(WHO);
	}

	quest_object = get_object();

	strncpy(buf, QUEST_CONTAINER_ARCHETYPE, sizeof(buf) - 1);

	GCFP.Value[0] = (void *) (buf);
	CFR = (PlugHooks[HOOK_GETARCHETYPE])(&GCFP);
	myob = (object *) (CFR->Value[0]);
	free(CFR);

	copy_object(myob, quest_object);

	quest_object->magic = 0;
	quest_object->name = add_string_hook(quest_name);
	insert_ob_in_ob_hook(quest_object, quest_container);

	return wrap_object(quest_object);
}

/**
 * <h1>object.CreatePlayerInfo(<i>\<string\></i> name)</h1>
 *
 * Creates a player info object of specified name in object's inventory.
 *
 * The values of a player info  object will NOT affect the object it is
 * in.
 *
 * @param name Name of the player info
 * @return The new player info object */
static PyObject *Atrinik_Object_CreatePlayerInfo(Atrinik_Object *whereptr, PyObject *args)
{
	char *txt, txt2[16];
	object *myob;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "s", &txt))
	{
		return NULL;
	}

	strcpy(txt2, "player_info");

	GCFP.Value[0] = (void *) (txt2);
	CFR = (PlugHooks[HOOK_GETARCHETYPE])(&GCFP);
	myob = (object *) (CFR->Value[0]);
	free(CFR);

	if (!myob)
	{
		plugin_log(llevDebug, "Python WARNING:: CreatePlayerInfo: Cant't find archtype 'player_info'\n");
		RAISE("Cant't find archtype 'player_info'");
	}

	/* Setup the info and put it in activator */
	if (myob->name)
	{
		FREE_STRING_HOOK(myob->name);
	}

	myob->name = add_string_hook(txt);
	myob = insert_ob_in_ob_hook(myob, WHERE);

	GCFP.Value[0] = (void *)(WHERE);
	GCFP.Value[1] = (void *)(myob);
	(PlugHooks[HOOK_ESRVSENDITEM])(&GCFP);

	return wrap_object(myob);
}

/**
 * <h1>object.GetPlayerInfo(<i>\<string\></i> name)</h1>
 *
 * Get the first player info object with the specified name in object's
 * inventory.
 *
 * @param name Name of the player info
 * @return The player info object if found, None otherwise. */
static PyObject *Atrinik_Object_GetPlayerInfo(Atrinik_Object *whoptr, PyObject *args)
{
	char *name;
	object *walk;

	if (!PyArg_ParseTuple(args, "s", &name))
	{
		return NULL;
	}

	/* Get the first linked player info arch in this inventory */
	for (walk = WHO->inv; walk != NULL; walk = walk->below)
	{
		if (walk->name && !strcmp(walk->arch->name, "player_info") && !strcmp(walk->name, name))
		{
			return wrap_object(walk);
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.GetNextPlayerInfo(<i>\<object\></i> player_info)</h1>
 *
 * Get next player info object in object's inventory with same name as
 * player_info.
 *
 * @param player_info Previously found player info object.
 * @return The next player info object if found, None otherwise. */
static PyObject *Atrinik_Object_GetNextPlayerInfo(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *myob;
	char name[128];
	object *walk;

	(void) whoptr;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &myob))
	{
		return NULL;
	}

	/* Our check paramters: arch "force_info", name of this arch */
	strncpy(name, STRING_OBJ_NAME(myob->obj), 127);
	name[63] = '\0';

	/* Get the next linked player_info arch in this inventory */
	for (walk = myob->obj->below; walk != NULL; walk = walk->below)
	{
		if (walk->name && !strcmp(walk->arch->name, "player_info") && !strcmp(walk->name, name))
		{
			return wrap_object(walk);
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.CreateInvisibleInside(<i>\<string\></i> id)</h1>
 *
 * Create an invisible force object in object's inventory.
 *
 * @param id String ID of the force object.
 * @return The created invisible force object. */
static PyObject *Atrinik_Object_CreateInvisibleInside(Atrinik_Object *whereptr, PyObject *args)
{
	char *txt, txt2[6];
	object *myob;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "s", &txt))
	{
		return NULL;
	}

	strcpy(txt2, "force");

	GCFP.Value[0] = (void *) (txt2);
	CFR = (PlugHooks[HOOK_GETARCHETYPE])(&GCFP);

	myob = (object *)(CFR->Value[0]);
	free(CFR);

	if (!myob)
	{
		plugin_log(llevDebug, "Python WARNING:: CFCreateInvisibleInside: Can't find archtype 'force'\n");
		RAISE("Cant't find archtype 'force'");
	}

	myob->speed = 0.0;
	GCFP.Value[0] = (void *)(myob);
	(PlugHooks[HOOK_UPDATESPEED])(&GCFP);

	if (myob->slaying)
	{
		FREE_STRING_HOOK(myob->slaying);
	}

	myob->slaying = add_string_hook(txt);
	myob = insert_ob_in_ob_hook(myob, WHERE);

	GCFP.Value[0] = (void *) (WHERE);
	GCFP.Value[1] = (void *) (myob);
	(PlugHooks[HOOK_ESRVSENDITEM])(&GCFP);

	return wrap_object(myob);
}

/**
 * <h1>object.CreateObjectInside(<i>\<string\></i> archname, <i>\<long\>
 * </i> identified, <i>\<long\></i> number, <i>\<long\></i> value)</h1>
 *
 * Creates an object from archname and inserts into object.
 *
 * @param archname Name of the arch
 * @param identifed Either Atrinik.IDENTIFIED or Atrinik.UNIDENTIFIED
 * @param number Number of objects to create
 * @param value If higher than -1, will be used as the new object's value
 * instead of the arch's default. Optional, defaults to -1.
 * @return The created invisible force object. */
static PyObject *Atrinik_Object_CreateObjectInside(Atrinik_Object *whereptr, PyObject *args)
{
	object *myob, *tmp;
	long value = -1, id, nrof = 1;
	char *txt;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "sll|l", &txt, &id, &nrof, &value))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (txt);
	CFR = (PlugHooks[HOOK_GETARCHETYPE])(&GCFP);
	myob = (object *) (CFR->Value[0]);
	free(CFR);

	if (!myob)
	{
		plugin_log(llevDebug, "BUG python_CFCreateObjectInside(): ob:>%s< = NULL!\n", query_name(myob, NULL));
		RAISE("Failed to create the object. Did you use an existing arch?");
	}

	/* -1 means, we use original value */
	if (value != -1)
	{
		myob->value = (sint32) value;
	}

	if (id)
	{
		SET_FLAG(myob, FLAG_IDENTIFIED);
		SET_FLAG(myob, FLAG_KNOWN_MAGICAL);
		SET_FLAG(myob, FLAG_KNOWN_CURSED);
	}

	if (nrof > 1)
	{
		myob->nrof = nrof;
	}

	myob = insert_ob_in_ob_hook(myob, WHERE);

	/* Make sure inventory image/text is updated */
	for (tmp = WHERE; tmp != NULL; tmp = tmp->env)
	{
		if (tmp->type == PLAYER)
		{
			GCFP.Value[0] = (void *) (tmp);
			GCFP.Value[1] = (void *) (myob);
			(PlugHooks[HOOK_ESRVSENDITEM])(&GCFP);
		}
	}

	return wrap_object(myob);
}


/**
 * Helper function for Atrinik_Object_CheckInventory() to recursively
 * check inventories. */
static object *object_check_inventory_rec(object *tmp, int mode, char *arch_name, char *name, char *title, int type)
{
	object *tmp2;

	while (tmp)
	{
		if ((!name || (tmp->name && !strcmp(tmp->name, name))) && (!title || (tmp->title && !strcmp(tmp->title, title))) && (!arch_name || (tmp->arch && tmp->arch->name && !strcmp(tmp->arch->name, arch_name))) && (type == -1 || tmp->type == type))
		{
			return tmp;
		}

		if (mode == 2 || (mode && tmp->type == CONTAINER))
		{
			if ((tmp2 = object_check_inventory_rec(tmp->inv, mode, arch_name, name, title, type)))
			{
				return tmp2;
			}
		}

		tmp = tmp->below;
	}

	return NULL;
}

/**
 * <h1>object.CheckInventory(<i>\<int\></i> mode, <i>\<string|None\></i>
 * arch, <i>\<string|None\></i> name, <i>\<string|None\></i> title, <i>
 * \<int\></i> type)</h1>
 *
 * Looks for a certain arch object in object's inventory.
 *
 * @param mode How to search the inventory. Possible modes:
 * - <b>0</b>: Only inventory
 * - <b>1</b>: Inventory and containers
 * - <b>2</b>: All inventory
 * @param arch Arch name of the object to search for
 * @param name Name of the object. Optional, defaults to ignore name.
 * @param title Title of the object. Optional, defaults to ignore title.
 * @param type Type of the object. Optional, defaults to ignore type.
 * @return The object we wanted if found, None otherwise. */
static PyObject *Atrinik_Object_CheckInventory(Atrinik_Object *whoptr, PyObject *args)
{
	int type = -1, mode = 0;
	char *name = NULL, *title = NULL, *arch_name = NULL;
	object *tmp, *tmp2;

	if (!PyArg_ParseTuple(args, "iz|zzi", &mode, &arch_name, &name, &title, &type))
	{
		return NULL;
	}

	tmp = WHO->inv;

	while (tmp)
	{
		if ( (!name || (tmp->name && !strcmp(tmp->name, name))) && (!title || (tmp->title && !strcmp(tmp->title, title))) && (!arch_name || (tmp->arch && tmp->arch->name && !strcmp(tmp->arch->name, arch_name))) && (type == -1 || tmp->type == type))
		{
			return wrap_object(tmp);
		}

		if (mode == 2 || (mode == 1 && tmp->type == CONTAINER))
		{
			if ((tmp2 = object_check_inventory_rec(tmp->inv, mode, arch_name, name, title, type)))
			{
				return wrap_object(tmp2);
			}
		}

		tmp = tmp->below;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SetSaveBed(<i>\<map\></i> map, <i>\<int\></i> x, <i>\<int\>
 * </i> y)</h1>
 *
 * Sets new save bed position for object.
 *
 * @param map Map of the new save bed
 * @param x X position of the new save bed
 * @param y Y position of the new save bed */
static PyObject *Atrinik_Object_SetSaveBed(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Map *map;
	int x, y;

	if (!PyArg_ParseTuple(args, "O!ii", &Atrinik_MapType, &map, &x, &y))
	{
		return NULL;
	}

	if (WHO->type == PLAYER)
	{
		strcpy(CONTR(WHO)->savebed_map, map->map->path);
		CONTR(WHO)->bed_x = x;
		CONTR(WHO)->bed_y = y;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.Remove()</h1>
 *
 * Takes the object out of whatever map or inventory it is in. The object
 * can then be inserted or teleported somewhere else, or just left alone
 * for the garbage collection to take care of. */
static PyObject *Atrinik_Object_Remove(Atrinik_Object *whoptr, PyObject *args)
{
	object *myob;
	object *obenv;

	if (!PyArg_ParseTuple(args, ""))
	{
		return NULL;
	}

	myob = WHO;
	obenv = myob->env;

	/* Don't allow removing any of the involved objects. Messes things up... */
	if (StackActivator[StackPosition] == myob || StackWho[StackPosition] == myob || StackOther[StackPosition] == myob)
	{
		RAISE("You are not allowed to remove one of the active objects. Workaround using CFTeleport or some other solution.");
	}

	GCFP.Value[0] = (void *) (myob);
	(PlugHooks[HOOK_REMOVEOBJECT])(&GCFP);

	/* Player inventory can be removed even if the activator is not a player */
	if (obenv != NULL && obenv->type == PLAYER)
	{
		GCFP.Value[0] = (void *) (obenv);
		GCFP.Value[1] = (void *) (obenv);
		(PlugHooks[HOOK_ESRVSENDINVENTORY])(&GCFP);
	}

	/* Handle removing any of the active objects (e.g. the activator) */
	if (StackActivator[StackPosition] == myob)
	{
		StackActivator[StackPosition] = NULL;
	}

	if (StackWho[StackPosition] == myob)
	{
		StackWho[StackPosition] = NULL;
	}

	if (StackOther[StackPosition] == myob)
	{
		StackOther[StackPosition] = NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.SetPosition(<i>\<int\></i> x, <i>\<int\></i> y)</h1>
 *
 * Sets new position coordinates for the object.
 *
 * Cannot be used to move objects out of containers, use Drop() or
 * TeleportTo() for that.
 * @param x New X position on the same map
 * @param y New Y position on the same map */
static PyObject *Atrinik_Object_SetPosition(Atrinik_Object *whoptr, PyObject *args)
{
	int x, y, k = 0;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "ii", &x, &y))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (&x);
	GCFP.Value[2] = (void *) (&y);
	GCFP.Value[3] = (void *) (&k);
	GCFP.Value[4] = (void *) (NULL);
	GCFP.Value[5] = (void *) (NULL);

	CFR = (PlugHooks[HOOK_TRANSFEROBJECT])(&GCFP);

	free(CFR);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.IdentifyItem(<i>\<object\></i> target, <i>\<object\></i>
 * marked, <i>\<long\></i> mode)</h1>
 *
 * Object identifies object(s) in target's inventory.
 *
 * @param target The target object
 * @param marked Marked object. Only use if mode is
 * Atrinik.IDENTIFY_MARKED, otherwise use None.
 * @param mode Possible modes:
 * - <b>Atrinik.IDENTIFY_NORMAL</b>: Normal identify
 * - <b>Atrinik.IDENTIFY_ALL</b>: Identify all items
 * - <b>Atrinik.IDENTIFY_MARKED</b>: Identify only marked item */
static PyObject *Atrinik_Object_IdentifyItem(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *target;
	PyObject *ob;
	object *marked = NULL;
	long mode;

	if (!PyArg_ParseTuple(args, "O!Ol", &Atrinik_ObjectType, &target, &ob, &mode))
	{
		return NULL;
	}

	if (mode == 2)
	{
		if (!PyObject_TypeCheck(ob, &Atrinik_ObjectType))
		{
			RAISE("Parameter 2 must be a Atrinik.Object for mode IDENTIFY_MARKED");
		}

		marked = ((Atrinik_Object *) ob)->obj;
	}
	else if (mode == 0 || mode == 1)
	{
		if (ob != Py_None)
		{
			RAISE("Parameter 2 must be None for modes IDENTIFY_NORMAL and IDENTIFY_ALL");
		}
	}
	else
	{
		RAISE("Mode must be IDENTIFY_NORMAL, IDENTIFY_ALL or IDENTIFY_MARKED");
	}

	GCFP.Value[0] = (void *) WHO;
	GCFP.Value[1] = (void *) target->obj;
	GCFP.Value[2] = (void *) marked;
	GCFP.Value[3] = (void *) &mode;
	(PlugHooks[HOOK_IDENTIFYOBJECT])(&GCFP);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.IsOfType(<i>\<int\></i> type)</h1>
 *
 * Check if specified object is of certain type.
 *
 * @param type The type to check for
 * @return 1 if the object is of specifie type, 0 otherwise */
static PyObject *Atrinik_Object_IsOfType(Atrinik_Object *whoptr, PyObject *args)
{
	int type;

	if (!PyArg_ParseTuple(args, "i", &type))
	{
		return NULL;
	}

	return Py_BuildValue("i", WHO->type == type ? 1 : 0);
}

/**
 * <h1>object.Save()</h1>
 *
 * Check if specified object is of certain type.
 *
 * @warning Untested. */
static PyObject *Atrinik_Object_Save(Atrinik_Object *whoptr, PyObject *args)
{
	static char *result;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, ""))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	CFR = (PlugHooks[HOOK_DUMPOBJECT])(&GCFP);
	result = (char *) (CFR->Value[0]);
	free(CFR);

	return Py_BuildValue("s", result);
}

/**
 * <h1>object.GetIP()</h1>
 *
 * Get IP of a specified player object.
 *
 * @param type The type to check for
 * @return The IP address of player */
static PyObject *Atrinik_Object_GetIP(Atrinik_Object *whoptr, PyObject *args)
{
	static char *result;

	if (!PyArg_ParseTuple(args, ""))
	{
		return NULL;
	}

	if (WHO->type != PLAYER)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	if (CONTR(WHO))
	{
		result = CONTR(WHO)->socket.host;
		return Py_BuildValue("s", result);
	}
	else
	{
		plugin_log(llevDebug, "PYTHON:: Error - This object has no controller\n");
		return Py_BuildValue("s", "");
	}
}

/**
 * <h1>object.GetArchName()</h1>
 *
 * Get arch name of an object.
 *
 * @return The arch name of the object. */
static PyObject *Atrinik_Object_GetArchName(Atrinik_Object *whoptr, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
	{
		return NULL;
	}

	return Py_BuildValue("s", WHO->arch->name);
}

/**
 * <h1>object.ShowCost(<i>\<int\></i> value)</h1>
 *
 * Show cost of value.
 * @param value Value to show cost from
 * @return string describing value as X gold, X silver, X copper. */
static PyObject *Atrinik_Object_ShowCost(Atrinik_Object *whoptr, PyObject *args)
{
	int value;
	char *cost_string;
	CFParm *CFR;

	(void) whoptr;

	if (!PyArg_ParseTuple(args, "i", &value))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (&value);
	CFR = (PlugHooks[HOOK_SHOWCOST])(&GCFP);
	cost_string = (char *) (CFR->Value[0]);

	return Py_BuildValue("s", cost_string);
}

/**
 * <h1>object.GetItemCost(<i>\<object\></i> object, <i>\<int\></i> type)
 * </h1>
 *
 * Get cost of an object in integer value.
 *
 * @param object The object to query cost for
 * @param type Possible types:
 * - <b>Atrinik.COST_TRUE</b>
 * - <b>Atrinik.COST_BUY</b>
 * - <b>Atrinik.COST_SELL</b>
 * @return The cost of the item as integer.
 * @warning Untested. */
static PyObject *Atrinik_Object_GetItemCost(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;
	int flag, cost;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "O!i", &Atrinik_ObjectType, &whatptr, &flag))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHAT);
	GCFP.Value[1] = (void *) (WHO);
	GCFP.Value[2] = (void *) (&flag);
	CFR = (PlugHooks[HOOK_QUERYCOST])(&GCFP);

	cost = *(int *)(CFR->Value[0]);
	free (CFR);

	return Py_BuildValue("i", cost);
}

/**
 * <h1>object.GetMoney()</h1>
 *
 * Get all the money the object is carrying as integer.
 *
 * @return The amount of money the object is carrying in copper (as
 * integer). */
static PyObject *Atrinik_Object_GetMoney(Atrinik_Object *whoptr, PyObject *args)
{
	int amount;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, ""))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	CFR = (PlugHooks[HOOK_QUERYMONEY])(&GCFP);

	amount = *(int *) (CFR->Value[0]);
	free (CFR);

	return Py_BuildValue("i", amount);
}

/**
 * <h1>object.PayForItem(<i>\<object\></i> object)</h1>
 *
 * @warning Untested. */
static PyObject *Atrinik_Object_PayForItem(Atrinik_Object *whoptr, PyObject *args)
{
	Atrinik_Object *whatptr;
	int val;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "O!", &Atrinik_ObjectType, &whatptr))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHAT);
	GCFP.Value[1] = (void *) (WHO);
	CFR = (PlugHooks[HOOK_PAYFORITEM])(&GCFP);

	val = *(int *) (CFR->Value[0]);
	free (CFR);

	return Py_BuildValue("i", val);
}

/**
 * <h1>object.PayAmount(<i>\<int\></i> value)</h1>
 *
 * Get the object to pay a specified amount of money in copper.
 *
 * @param value The amount of money in copper to pay for
 * @return 1 if the object paid the money (the object had enough money in
 * inventory), 0 otherwise. */
static PyObject *Atrinik_Object_PayAmount(Atrinik_Object *whoptr, PyObject *args)
{
	int to_pay, val;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "i", &to_pay))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (&to_pay);
	GCFP.Value[1] = (void *) (WHO);
	CFR = (PlugHooks[HOOK_PAYFORAMOUNT])(&GCFP);

	val = *(int *) (CFR->Value[0]);
	free (CFR);

	return Py_BuildValue("i", val);
}

/**
 * <h1>player.SendCustomCommand(<i>\<int\></i>command_id, <i>\<string\></i> command_data)</h1>
 *
 * @warning Untested. */
static PyObject *Atrinik_Object_SendCustomCommand(Atrinik_Object *whoptr, PyObject *args)
{
	char *customcmd;
	char cmd[MAX_BUF];

	if (!PyArg_ParseTuple(args, "bs", cmd, &customcmd))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (cmd);
	GCFP.Value[2] = (void *) (customcmd);
	(PlugHooks[HOOK_SENDCUSTOMCOMMAND])(&GCFP);

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * <h1>object.object.Clone(<i>\<int\></i> mode)</h1>
 *
 * Clone an object.
 *
 * You should do something with the clone.
 * @ref Atrinik_Object_TeleportTo "TeleportTo()" or
 * @ref Atrinik_Object_InsertInside "InsertInside()" are useful functions
 * for that.
 *
 * @param mode Optional mode, one of:
 * - <b>Atrinik.CLONE_WITH_INVENTORY</b> (default)
 * - <b>Atrinik.CLONE_WITHOUT_INVENTORY</b>
 * @return The cloned object. */
static PyObject *Atrinik_Object_Clone(Atrinik_Object *whoptr, PyObject *args)
{
	CFParm *CFR;
	int mode = 0;
	object *clone;

	if (!PyArg_ParseTuple(args, "|i", &mode))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (WHO);
	GCFP.Value[1] = (void *) (&mode);

	CFR = (PlugHooks[HOOK_CLONEOBJECT])(&GCFP);

	clone = (object *) (CFR->Value[0]);
	free(CFR);

	if (clone->type == PLAYER || QUERY_FLAG(clone, FLAG_IS_PLAYER))
	{
		clone->type = MONSTER;
		CLEAR_FLAG(clone, FLAG_IS_PLAYER);
	}

	return wrap_object(clone);
}

/**
 * <h1>object.SwapApartments(<i>\<string\></i> oldmap, <i>\<string\></i>
 * newmap, <i>\<int\></i> x, <i>\<int\></i> y)</h1>
 *
 * Swaps oldmap apartment with newmap one.
 * Copies old items from oldmap to newmap at x, y and saves the map.
 *
 * @param oldmap The old apartment map
 * @param oldmap The new apartment map
 * @param x X position to copy the items to
 * @param y Y position to copy the items to */
static PyObject *Atrinik_Object_SwapApartments(Atrinik_Object *whoptr, PyObject *args)
{
	char *mapold, *mapnew;
	int x, y, val;
	CFParm *CFR;

	if (!PyArg_ParseTuple(args, "ss|ii", &mapold, &mapnew, &x, &y))
	{
		return NULL;
	}

	GCFP.Value[0] = (void *) (mapold);
	GCFP.Value[1] = (void *) (mapnew);
	GCFP.Value[2] = (void *) (&x);
	GCFP.Value[3] = (void *) (&y);
	GCFP.Value[4] = (void *) (WHO);

	CFR = (PlugHooks[HOOK_SWAPAPARTMENTS])(&GCFP);
	val = *(int *) (CFR->Value[0]);
	free(CFR);

	return Py_BuildValue("i", val);
}

/**
 * <h1>object.GetUnmodifiedAttribute(<i>\<int\></i> attribute_id)</h1>
 *
 * @warning Unfinished.
 * @todo Finish. */
static PyObject *Atrinik_Object_GetUnmodifiedAttribute(Atrinik_Object *whoptr, PyObject *args)
{
	int fieldno;

	if (!PyArg_ParseTuple(args, "i", &fieldno))
	{
		return NULL;
	}

	if (fieldno < 0 || fieldno >= (int) NUM_OBJFIELDS)
	{
		RAISE("Illegal field ID");
	}

	if (WHO->type != PLAYER)
	{
		RAISE("Can only be used on players");
	}

	RAISE("Not implemented");

#if 0
	switch (fieldno)
	{
		case OBJFIELD_STAT_INT:
			return Py_BuildValue("i", CONTR(WHO)->orig_stats.Int);
		case OBJFIELD_STAT_STR:
			return Py_BuildValue("i", CONTR(WHO)->orig_stats.Str);
		case OBJFIELD_STAT_CHA:
			return Py_BuildValue("i", CONTR(WHO)->orig_stats.Cha);
		case OBJFIELD_STAT_WIS:
			return Py_BuildValue("i", CONTR(WHO)->orig_stats.Wis);
		case OBJFIELD_STAT_DEX:
			return Py_BuildValue("i", CONTR(WHO)->orig_stats.Dex);
		case OBJFIELD_STAT_CON:
			return Py_BuildValue("i", CONTR(WHO)->orig_stats.Con);
		case OBJFIELD_STAT_POW:
			return Py_BuildValue("i", CONTR(WHO)->orig_stats.Pow);

		default:
			RAISE("No unmodified version of attribute available");
	}
#endif
}

/**
 * <h1>object.GetSaveBed()</h1>
 * Get a player's save bed location.
 * @note Can only be used on player objects.
 * @return A dictionary containing the information about the player's
 * save bed:
 * - <b>map</b>: Map path of the save bed.
 * - <b>x</b>: X location of the save bed.
 * - <b>y</b>: Y location of the save bed. */
static PyObject *Atrinik_Object_GetSaveBed(Atrinik_Object *whoptr, PyObject *args)
{
	PyObject *dict;

	(void) args;

	if (WHO->type != PLAYER)
	{
		RAISE("Can only be used on player objects.");
	}

	dict = PyDict_New();

	PyDict_SetItemString(dict, "map", Py_BuildValue("s", CONTR(WHO)->savebed_map));
	PyDict_SetItemString(dict, "x", Py_BuildValue("i", CONTR(WHO)->bed_x));
	PyDict_SetItemString(dict, "y", Py_BuildValue("i", CONTR(WHO)->bed_y));

	return dict;
}

/*@}*/

/* Attribute getter */
static PyObject *Object_GetAttribute(Atrinik_Object* whoptr, int fieldno)
{
	void *field_ptr, *field_ptr2;
	tag_t tag;
	object *obj;
	char *str;

	if (fieldno < 0 || fieldno >= (int) NUM_OBJFIELDS)
		RAISE("Illegal field ID");

	field_ptr = (void *)((char *)WHO + obj_fields[fieldno].offset);

	/* TODO: better handling of types, signs, and overflows */
	switch (obj_fields[fieldno].type)
	{
		case FIELDTYPE_SHSTR:
		case FIELDTYPE_CSTR:
			str =  *(char **)field_ptr;
			return Py_BuildValue("s", str ? str : "");

		case FIELDTYPE_UINT8:
			return Py_BuildValue("b", *(uint8 *)field_ptr);

		case FIELDTYPE_SINT8:
			return Py_BuildValue("b", *(sint8 *)field_ptr);

		case FIELDTYPE_UINT16:
			return Py_BuildValue("i", *(uint16 *)field_ptr);

		case FIELDTYPE_SINT16:
			return Py_BuildValue("i", *(sint16 *)field_ptr);

		case FIELDTYPE_UINT32:
			return Py_BuildValue("l", *(uint32 *)field_ptr);

		case FIELDTYPE_SINT32:
			return Py_BuildValue("l", *(sint32 *)field_ptr);

		case FIELDTYPE_FLOAT:
			return Py_BuildValue("f", *(float *)field_ptr);

		case FIELDTYPE_MAP:
			return wrap_map(*(mapstruct **)field_ptr);

		case FIELDTYPE_OBJECT:
			return wrap_object(*(object **)field_ptr);

		case FIELDTYPE_OBJECTREF:
			field_ptr2 = (void *)((char *)WHO + obj_fields[fieldno].extra_data);
			obj = *(object **)field_ptr;
			tag = *(tag_t *)field_ptr2;
			return wrap_object(OBJECT_VALID(obj, tag) ? obj : NULL);

		default:
			RAISE("BUG: unknown field type");
	}
}

/* Object attribute setter */
static int Object_SetAttribute(Atrinik_Object* whoptr, PyObject *value, int fieldno)
{
	void *field_ptr;
	object *tmp;
	uint32 flags, offset;

	if (fieldno < 0 || fieldno >= (int) NUM_OBJFIELDS)
		INTRAISE("Illegal field ID");

	flags = obj_fields[fieldno].flags;
	if ((flags & FIELDFLAG_READONLY) || ((flags & FIELDFLAG_PLAYER_READONLY) && WHO->type == PLAYER))
		INTRAISE("Trying to modify readonly field");

	offset = obj_fields[fieldno].offset;
	field_ptr = (void *)((char *)WHO + offset);

	switch (obj_fields[fieldno].type)
	{
		case FIELDTYPE_SHSTR:
			if (PyString_Check(value))
			{
				const char *str = PyString_AsString(value);

				if (*(char **)field_ptr != NULL)
					FREE_STRING_HOOK(*(char **)field_ptr);

				if (str && strcmp(str, ""))
					*(const char **)field_ptr = add_string_hook(str);
			}
			else
				INTRAISE("Illegal value for text field");

			break;

			/* TODO: better handling of types, signs, and overflows */
		case FIELDTYPE_UINT8:
			if (PyInt_Check(value))
				*(uint8 *)field_ptr = (uint8) PyInt_AsLong(value);
			else
				INTRAISE("Illegal value for int field");

			break;

		case FIELDTYPE_SINT8:
			if (PyInt_Check(value))
				*(sint8 *)field_ptr = (sint8)PyInt_AsLong(value);
			else
				INTRAISE("Illegal value for int field");

			break;

		case FIELDTYPE_UINT16:
			if (PyInt_Check(value))
				*(uint16 *)field_ptr = (uint16)PyInt_AsLong(value);
			else
				INTRAISE("Illegal value for int field");

			break;

		case FIELDTYPE_SINT16:
			if (PyInt_Check(value))
				*(sint16 *)field_ptr = (sint16)PyInt_AsLong(value);
			else
				INTRAISE("Illegal value for int field");

			break;

		case FIELDTYPE_UINT32:
			if (PyInt_Check(value))
				*(uint32 *)field_ptr = (uint32)PyInt_AsLong(value);
			else
				INTRAISE("Illegal value for int field");

			break;

		case FIELDTYPE_SINT32:
			if (PyInt_Check(value))
				*(sint32 *)field_ptr = (sint32)PyInt_AsLong(value);
			else
				INTRAISE("Illegal value for int field");

			break;

		case FIELDTYPE_FLOAT:
			if (PyFloat_Check(value))
				*(float *)field_ptr = (float)PyFloat_AsDouble(value);
			else if (PyInt_Check(value))
				*(float *)field_ptr = (float)PyInt_AsLong(value);
			else
				INTRAISE("Illegal value for float field");

			break;

		default:
			INTRAISE("BUG: unknown field type");
	}

	/* Make sure the inventory image/text is updated */
	/* FIXME: what if object was not carried by player ? */
	for (tmp = WHO->env; tmp != NULL; tmp = tmp->env)
	{
		if (tmp->type == PLAYER)
		{
			GCFP.Value[0] = (void *)(tmp);
			GCFP.Value[1] = (void *)(WHO);
			(PlugHooks[HOOK_ESRVSENDITEM])(&GCFP);
		}
	}

	/* Special handling for some player stuff */
	if (WHO->type == PLAYER)
	{
		/* VC gives a error for the offsetof() because case:
		 * must be followed by a constant value.
		switch (offset)
		{
		    case offsetof(object, stats.Int): CONTR(WHO)->orig_stats.Int = PyInt_AsLong(value); break;
		    case offsetof(object, stats.Str): CONTR(WHO)->orig_stats.Str = PyInt_AsLong(value); break;
		    case offsetof(object, stats.Cha): CONTR(WHO)->orig_stats.Cha = PyInt_AsLong(value); break;
		    case offsetof(object, stats.Wis): CONTR(WHO)->orig_stats.Wis = PyInt_AsLong(value); break;
		    case offsetof(object, stats.Dex): CONTR(WHO)->orig_stats.Dex = PyInt_AsLong(value); break;
		    case offsetof(object, stats.Con): CONTR(WHO)->orig_stats.Con = PyInt_AsLong(value); break;
		    case offsetof(object, stats.Pow): CONTR(WHO)->orig_stats.Pow = PyInt_AsLong(value); break;
		    default:
		}
		*/

		/* replacing the switch struct above */
		if (offset == offsetof(object, stats.Int))
			CONTR(WHO)->orig_stats.Int = (sint8)PyInt_AsLong(value);
		else if (offset == offsetof(object, stats.Str))
			CONTR(WHO)->orig_stats.Str = (sint8)PyInt_AsLong(value);
		else if (offset == offsetof(object, stats.Cha))
			CONTR(WHO)->orig_stats.Cha = (sint8)PyInt_AsLong(value);
		else if (offset == offsetof(object, stats.Wis))
			CONTR(WHO)->orig_stats.Wis = (sint8)PyInt_AsLong(value);
		else if (offset == offsetof(object, stats.Dex))
			CONTR(WHO)->orig_stats.Dex = (sint8)PyInt_AsLong(value);
		else if (offset == offsetof(object, stats.Con))
			CONTR(WHO)->orig_stats.Con = (sint8)PyInt_AsLong(value);
		else if (offset == offsetof(object, stats.Pow))
			CONTR(WHO)->orig_stats.Pow = (sint8)PyInt_AsLong(value);

		if (flags & FIELDFLAG_PLAYER_FIX)
			fix_player_hook(WHO);
	}

	return 0;
}

/* Object flag getter */
static PyObject *Object_GetFlag(Atrinik_Object* whoptr, int flagno)
{
	if (flagno < 0 || flagno >= NUM_FLAGS)
		RAISE("Unknown flag");

	return Py_BuildValue("i", QUERY_FLAG(WHO,flagno) ? 1 : 0);
}

/* Object flag setter */
int Object_SetFlag(Atrinik_Object* whoptr, PyObject *val, int flagno)
{
	int value;
	object *tmp;

	if (flagno < 0 || flagno >= NUM_FLAGS)
		INTRAISE("Unknown flag");

	if (!PyInt_Check(val))
		INTRAISE("Value must be 0 or 1");

	value = PyInt_AsLong(val);

	if (value <0 || value > 1)
		INTRAISE("Value must be 0 or 1");

	if (value)
		SET_FLAG(WHO, flagno);
	else
		CLEAR_FLAG(WHO, flagno);

	/* Make sure the inventory image/text is updated */
	/* FIXME: what if object was not carried by player ? */
	for (tmp = WHO->env; tmp != NULL; tmp = tmp->env)
	{
		if (tmp->type == PLAYER)
		{
			GCFP.Value[0] = (void *)(tmp);
			GCFP.Value[1] = (void *)(WHO);
			(PlugHooks[HOOK_ESRVSENDITEM])(&GCFP);
		}
	}

	/* TODO: if gender changed:
	if()
	   CONTR(WHO)->socket.ext_title_flag = 1; * demand update to client */

	return 0;
}

/****************************************************************************/
/* Python Object management code                                            */
/****************************************************************************/

/* Initialize our CF object wrapper */
int Atrinik_Object_init(PyObject *module)
{
	int i, flagno;

	/* field getseters */
	for (i = 0; i < (int) NUM_OBJFIELDS; i++)
	{
		PyGetSetDef *def = &Object_getseters[i];
		def->name = obj_fields[i].name;
		def->get = (getter)Object_GetAttribute;
		def->set = (setter)Object_SetAttribute;
		def->doc = NULL;
		def->closure = (void *)i;
	}

	/* flag getseters */
	for (flagno = 0; flagno < NUM_FLAGS; flagno++)
	{
		if (flag_names[flagno])
		{
			PyGetSetDef *def = &Object_getseters[i++];
			def->name = flag_names[flagno];
			def->get = (getter)Object_GetFlag;
			def->set = (setter)Object_SetFlag;
			def->doc = NULL;
			def->closure = (void *)flagno;
		}
	}

	Object_getseters[i].name = NULL;

	/* Add constants */
	for (i = 0; object_constants[i].name; i++)
	{
		if (PyModule_AddIntConstant(module, object_constants[i].name, object_constants[i].value))
			return -1;
	}

	Atrinik_ObjectType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&Atrinik_ObjectType) < 0)
		return -1;

#if 0
	Py_INCREF(&Atrinik_ObjectType);
#endif
	return 0;
}

/* Create a new Object wrapper (uninitialized) */
static PyObject *Atrinik_Object_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	Atrinik_Object *self;

	(void) args;
	(void) kwds;

	self = (Atrinik_Object *)type->tp_alloc(type, 0);

	if (self)
		self->obj = NULL;

	return (PyObject *)self;
}

/* Free an Object wrapper */
static void Atrinik_Object_dealloc(Atrinik_Object* self)
{
	/* Clean up "dangling" objects
	 * i.e. objects with no environment (from obj.Clone()) or removed objects */

	/* With the semiautomatic gargbage collection this is no longer needed */
#if 0
	if (self->obj && !OBJECT_FREE(self->obj))
	{
		if (QUERY_FLAG(self->obj, FLAG_REMOVED))
		{
			plugin_log(llevDebug, "PYTHON - Freeing removed object %s \"%s\"\n", STRING_OBJ_ARCH_NAME(self->obj), STRING_OBJ_NAME(self->obj));
			GCFP.Value[0] = (void *)(self->obj);
			(PlugHooks[HOOK_FREEOBJECT])(&GCFP);
		}
		else if (self->obj->env == NULL && self->obj->map == NULL)
		{
			/* This shouldn't really happen, but I added it just for safety */
			plugin_log(llevDebug, "PYTHON - Freeing object in limbo %s \"%s\"\n", STRING_OBJ_ARCH_NAME(self->obj), STRING_OBJ_NAME(self->obj));
			GCFP.Value[0] = (void *)(self->obj);
			(PlugHooks[HOOK_REMOVEOBJECT])(&GCFP);
			GCFP.Value[0] = (void *)(self->obj);
			(PlugHooks[HOOK_FREEOBJECT])(&GCFP);
		}
	}
#endif

	self->obj = NULL;
	self->ob_type->tp_free((PyObject*)self);
}

/** Return a string representation of this object (useful for debugging) */
static PyObject *Atrinik_Object_str(Atrinik_Object *self)
{
	return PyString_FromFormat("[%s \"%s\"]", STRING_OBJ_ARCH_NAME(self->obj), STRING_OBJ_NAME(self->obj));
}

/** Utility method to wrap an object. */
PyObject * wrap_object(object *what)
{
	Atrinik_Object *wrapper;

	/* return None if no object was to be wrapped */
	if (what == NULL)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	wrapper = PyObject_NEW(Atrinik_Object, &Atrinik_ObjectType);

	if (wrapper != NULL)
	{
		wrapper->obj = what;
	}

	return (PyObject *)wrapper;
}
