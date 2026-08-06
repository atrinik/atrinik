/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * Player related header file.
 */

#ifndef PLAYER_H
#define PLAYER_H

/**
 * @defgroup GENDER_xxx Gender IDs.
 * IDs of the various genders.
 *@{*/
/** Neuter: no gender. */
#define GENDER_NEUTER 0
/** Male. */
#define GENDER_MALE 1
/** Female. */
#define GENDER_FEMALE 2
/** Hermaphrodite: both genders. */
#define GENDER_HERMAPHRODITE 3
/** Total number of genders. */
#define GENDER_MAX 4
/*@}*/

#define PLAYER_DOLL_SLOT_COLOR "353734"

#define EXP_PROGRESS_BUBBLES 10

typedef struct Stat_struct {
    int8_t Str, Dex, Con, Int, Pow;

    /** Weapon class. */
    int16_t wc;

    /** Armour class. */
    int16_t ac;

    /** Level. */
    uint32_t level;

    /** Hit points. */
    int32_t hp;

    /** Max hit points */
    int32_t maxhp;

    /** Spell points. */
    int32_t sp;

    /** Max spell points. */
    int32_t maxsp;

    /** Total experience. */
    int64_t exp;

    /** How much food in stomach. */
    int16_t food;

    /** How much damage the player does when hitting. */
    int16_t dam;

    /** Player's speed. */
    float speed;

    /** Weapon speed. */
    float weapon_speed;

    /** Contains fire on/run on flags. */
    uint16_t flags;

    /** Protections. */
    int8_t protection[CS_STAT_PROT_END - CS_STAT_PROT_START + 1];

    /** Ranged weapon damage. */
    int16_t ranged_dam;

    /** Ranged weapon wc. */
    int16_t ranged_wc;

    /** Ranged weapon speed. */
    float ranged_ws;
} Stats;

/** The player structure. */
typedef struct Player_Struct {
    /** Player object. */
    object *ob;

    /** Items below the player (pl.below->inv). */
    object *below;

    /** Inventory of an open container. */
    object *sack;

    /** Objects in the interface GUI. */
    object *interface;

    /** Tag of the open container. */
    tag_t container_tag;

    /** Player's weight limit. */
    float weight_limit;

    /** Are we a DM? */
    int dm;

    /** Target. */
    int target_code;

    /** Target's color. */
    char target_color[COLOR_BUF];

    /** Target name. */
    char target_name[MAX_BUF];

    /** Target level. */
    uint8_t target_level;

    int warn_hp;

    /** Currently marked item. */
    tag_t mark_count;

    /** HP regeneration. */
    float gen_hp;

    /** Mana regeneration. */
    float gen_sp;

    /** Skill cooldown time. */
    float action_timer;

    /** 1 if fire key is pressed. */
    uint8_t fire_on;

    /** 1 if run key is on. */
    uint8_t run_on;

    /** Player's carrying weight. */
    float real_weight;

    int warn_statdown;
    int warn_statup;

    /** Player stats. */
    Stats stats;

    /** HP of our target in percent. */
    char target_hp;

    /** Player's name. */
    char name[40];

    /** Party name this player is member of. */
    char partyname[MAX_BUF];

    /**
     * Buffer for party name the player is joining, but has to enter
     * password first.
     */
    char partyjoin[MAX_BUF];

    /**
     * Which item is being dragged.
     */
    tag_t dragging_tag;

    /**
     * X position where the item was dragged from.
     */
    int dragging_startx;

    /**
     * Y position where the item was dragged from.
     */
    int dragging_starty;

    /** Which inventory widget has the focus. */
    widgetdata *inventory_focus;

    /** Version of the server's socket. */
    int server_socket_version;

    size_t target_object_index;

    uint8_t target_is_friend;

    /**
     * Player's gender.
     */
    uint8_t gender;

    tag_t equipment[PLAYER_EQUIP_MAX];

    uint32_t path_attuned;

    uint32_t path_repelled;

    uint32_t path_denied;

    player_state_t state;

    /**
     * Account name that we are logged into.
     */
    char account[MAX_BUF];

    /**
     * Password that was used to log in.
     */
    char password[MAX_BUF];

    /** Current connection diagnostic ID. */
    char connection_id[SOCKET_CONNECTION_ID_SIZE];

    /** Previous connection diagnostic ID used by the account. */
    char last_connection_id[SOCKET_CONNECTION_ID_SIZE];

    /**
     * Last time the account was used.
     */
    time_t last_time;

    /**
     * HTTP data URL.
     */
    char http_url[MAX_BUF];

    /**
     * Whether this connection can transfer cached assets in-band over QUIC.
     */
    bool asset_transport;

    /**
     * If 1, the player is ready to engage in combat and will swing their
     * weapon at targeted enemies.
     */
    uint8_t combat;

    /**
     * If 1, the player will swing their weapon at their target, be it friend
     * or foe.
     */
    uint8_t combat_force;
} Client_Player;

/** Public API implemented in src/client/player.c. */

extern const char *gender_noun[4];

extern const char *gender_subjective[4];

extern const char *gender_subjective_upper[4];

extern const char *gender_objective[4];

extern const char *gender_possessive[4];

extern const char *gender_reflexive[4];

extern void clear_player(void);

extern void new_player(tag_t tag, long weight, uint16_t face);

extern void client_send_apply(object *op);

extern void client_send_examine(tag_t tag);

extern void client_send_move(tag_t loc, tag_t tag, uint32_t nrof);

extern void send_command(const char *command);

extern void init_player_data(void);

extern int gender_to_id(const char *gender);

extern void telemetry_reset(void);

extern void telemetry_exp_update(uint64_t exp);

extern void telemetry_exp_tracker_reset(void);

extern uint64_t telemetry_exp_gained(void);

extern uint64_t telemetry_exp_per_hour(void);

extern uint64_t telemetry_exp_elapsed_seconds(void);

extern void telemetry_game_time_sync(uint64_t game_seconds, uint32_t millis_per_game_minute);

extern bool telemetry_game_time_get(uint64_t *game_minutes, uint32_t *millis_per_game_minute);

extern void player_draw_exp_progress(SDL_Surface *surface, int x, int y, int64_t xp, uint8_t level);

/** Public API implemented in src/gui/widgets/playerdoll.c. */

extern object *playerdoll_get_equipment(int i, int *xpos, int *ypos);

extern void widget_playerdoll_init(widgetdata *widget);

/** Public API implemented in src/gui/widgets/playerinfo.c. */

extern void widget_playerinfo_init(widgetdata *widget);

/** Public API implemented in src/gui/widgets/protections.c. */

extern void widget_protections_init(widgetdata *widget);

/** Public API implemented in src/gui/widgets/skills.c. */

extern void skills_init(void);

extern void skills_deinit(void);

extern int skill_find(const char *name, size_t *id);

extern int skill_find_object(object *op, size_t *id);

extern skill_entry_struct *skill_get(size_t id);

extern void skills_update(object *op, uint8_t level, int64_t xp, const char *msg);

extern void skills_remove(object *op);

extern void widget_skills_init(widgetdata *widget);

/** Public API implemented in src/gui/widgets/spells.c. */

extern void spells_init(void);

extern void spells_deinit(void);

extern int spell_find(const char *name, size_t *spell_path, size_t *spell_id);

extern int spell_find_object(object *op, size_t *spell_path, size_t *spell_id);

extern spell_entry_struct *spell_get(size_t spell_path, size_t spell_id);

extern void
spells_update(object *op, uint16_t cost, uint32_t path, uint32_t flags, const char *msg);

extern void spells_remove(object *op);

extern void widget_spells_init(widgetdata *widget);

/** Public API implemented in src/gui/widgets/stat.c. */

extern void widget_stat_init(widgetdata *widget);

/** Public API implemented in src/gui/widgets/target.c. */

extern void widget_target_init(widgetdata *widget);

#endif
