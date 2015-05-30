/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Header file dealing with objects. */

#ifndef ITEM_H
#define ITEM_H

/** How many objects are initially reserved for the objects pool. */
#define NROF_ITEMS 50

/** Maximum length of a name. */
#define NAME_LEN 128

/**
 * Item structure keeps all information what player (= client) knows
 * about items in its inventory. */
typedef struct obj {
    /** Next item in inventory. */
    struct obj *next;

    /* Everything below will be cleared by memset() in
     * object_remove(). */

    /** Previous item in inventory. */
    struct obj *prev;

    /** Which item's inventory is this item. */
    struct obj *env;

    /** Item's inventory. */
    struct obj *inv;

    /** Item's singular name as sent to us. */
    char s_name[NAME_LEN];

    /** Item identifier (0 = free). */
    int32_t tag;

    /** Number of items. */
    int32_t nrof;

    /** How much item weights. */
    double weight;

    /** Index for face array. */
    int16_t face;

    /** Index into animation array. */
    uint16_t animation_id;

    /** How often to animate. */
    uint16_t anim_speed;

    /** Last face in sequence drawn. */
    uint16_t anim_state;

    /** How many ticks have passed since we last animated. */
    uint16_t last_anim;

    /** Unmodified flags value as sent from the server. */
    uint32_t flags;

    /** Item type. */
    uint8_t itype;
    uint8_t stype;

    /** Item's quality. */
    uint8_t item_qua;

    /** Item's condition. */
    uint8_t item_con;

    /** UID of the required skill. */
    uint32_t item_skill_tag;

    /** Requires level. */
    uint8_t item_level;

    /** The item's direction. */
    uint8_t direction;
} object;

#define TYPE_PLAYER                 1
#define TYPE_BULLET                 2
#define TYPE_ROD                    3
#define TYPE_TREASURE               4
#define TYPE_POTION                 5
#define TYPE_FOOD                   6
#define TYPE_REGION_MAP             7
#define TYPE_BOOK                   8
#define TYPE_CLOCK                  9
#define TYPE_FBULLET                10
#define TYPE_FBALL                  11
#define TYPE_LIGHTNING              12
#define TYPE_ARROW                  13
#define TYPE_BOW                    14
#define TYPE_WEAPON                 15
#define TYPE_ARMOUR                 16
#define TYPE_PEDESTAL               17
#define TYPE_ALTAR                  18
#define TYPE_CONFUSION              19
#define TYPE_LOCKED_DOOR            20
#define TYPE_SPECIAL_KEY            21
#define TYPE_MAP                    22
#define TYPE_DOOR                   23
#define TYPE_KEY                    24
#define TYPE_MMISSILE               25
#define TYPE_TIMED_GATE             26
#define TYPE_TRIGGER                27
#define TYPE_GRIMREAPER             28
#define TYPE_SPELL                  29
#define TYPE_TRIGGER_BUTTON         30
#define TYPE_TRIGGER_ALTAR          31
#define TYPE_TRIGGER_PEDESTAL       32
#define TYPE_SHIELD                 33
#define TYPE_HELMET                 34
#define TYPE_PANTS                  35
#define TYPE_MONEY                  36
#define TYPE_CLASS                  37
#define TYPE_GRAVESTONE             38
#define TYPE_AMULET                 39
#define TYPE_PLAYERMOVER            40
#define TYPE_TELEPORTER             41
#define TYPE_CREATOR                42
#define TYPE_SKILL                  43
#define TYPE_EXPERIENCE             44
#define TYPE_EARTHWALL              45
#define TYPE_GOLEM                  46
#define TYPE_BOMB                   47
#define TYPE_THROWN_OBJ             48
#define TYPE_BLINDNESS              49
#define TYPE_GOD                    50
#define TYPE_DETECTOR               51
#define TYPE_SKILL_ITEM             52
#define TYPE_DEAD_OBJECT            53
#define TYPE_DRINK                  54
#define TYPE_MARKER                 55
#define TYPE_HOLY_ALTAR             56
#define TYPE_PLAYER_CHANGER         57
#define TYPE_BATTLEGROUND           58
#define TYPE_PEACEMAKER             59
#define TYPE_GEM                    60
#define TYPE_FIRECHEST              61
#define TYPE_FIREWALL               62
#define TYPE_ANVIL                  63
#define TYPE_CHECK_INV              64
#define TYPE_MOOD_FLOOR             65
#define TYPE_EXIT                   66
#define TYPE_ENCOUNTER              67
#define TYPE_SHOP_FLOOR             68
#define TYPE_SHOP_MAT               69
#define TYPE_RING                   70
#define TYPE_FLOOR                  71
#define TYPE_FLESH                  72
#define TYPE_INORGANIC              73
#define TYPE_LIGHT_APPLY            74
#define TYPE_LIGHTER                75
#define TYPE_TRAP_PART              76
#define TYPE_WALL                   77
#define TYPE_LIGHT_SOURCE           78
#define TYPE_MISC_OBJECT            79
#define TYPE_MONSTER                80
#define TYPE_SPAWN_GENERATOR        81
#define TYPE_SPELLBOOK              85
#define TYPE_CLOAK                  87
#define TYPE_CONE                   88
#define TYPE_AURA                   89
#define TYPE_SPINNER                90
#define TYPE_GATE                   91
#define TYPE_BUTTON                 92
#define TYPE_CF_HANDLE              93
#define TYPE_HOLE                   94
#define TYPE_TRAPDOOR               95
#define TYPE_WORD_OF_RECALL         96
#define TYPE_PARAIMAGE              97
#define TYPE_SIGN                   98
#define TYPE_BOOTS                  99
#define TYPE_GLOVES                 100
#define TYPE_CONVERTER              103
#define TYPE_BRACERS                104
#define TYPE_POISONING              105
#define TYPE_SAVEBED                106
#define TYPE_POISONCLOUD            107
#define TYPE_FIREHOLES              108
#define TYPE_WAND                   109
#define TYPE_ABILITY                110
#define TYPE_SCROLL                 111
#define TYPE_DIRECTOR               112
#define TYPE_GIRDLE                 113
#define TYPE_FORCE                  114
#define TYPE_POTION_EFFECT          115
#define TYPE_CLOSE_CON              121
#define TYPE_CONTAINER              122
#define TYPE_ARMOUR_IMPROVER        123
#define TYPE_WEAPON_IMPROVER        124
#define TYPE_SKILLSCROLL            130
#define TYPE_DEEP_SWAMP             138
#define TYPE_IDENTIFY_ALTAR         139
#define TYPE_CANCELLATION           141
#define TYPE_MENU                   150
#define TYPE_BALL_LIGHTNING         151
#define TYPE_SWARM_SPELL            153
#define TYPE_RUNE                   154
#define TYPE_POWER_CRYSTAL          156
#define TYPE_CORPSE                 157
#define TYPE_DISEASE                158
#define TYPE_SYMPTOM                159

#define F_ETHEREAL 0x0080
#define F_INVISIBLE 0x0100

/** Delete item by tag. */
#define delete_object(tag) object_remove(object_find(tag))

#endif
