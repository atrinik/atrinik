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
 * Holds definitions about the plugin hook functions, variables, arrays, etc.
 */

#undef PLUGIN_HOOK_FUNCTION
#undef PLUGIN_HOOK_VARIABLE
#undef PLUGIN_HOOK_ARRAY

#if defined(PLUGIN_HOOK_DEFINITIONS)
#   define PLUGIN_HOOK_FUNCTION(ret, id, ...) id,
#   define PLUGIN_HOOK_VARIABLE(ret, id) &id,
#   define PLUGIN_HOOK_ARRAY(type, id) id,
#elif defined(PLUGIN_HOOK_DECLARATIONS)
#   define PLUGIN_HOOK_FUNCTION(type, id, ...) type(*id)(__VA_ARGS__);
#   define PLUGIN_HOOK_VARIABLE(type, id) type *id;
#   define PLUGIN_HOOK_ARRAY(type, id) type *id;
#else
#   define PLUGIN_HOOK_NONE
#endif

#ifndef PLUGIN_HOOK_NONE

#if defined(PLUGIN_HOOK_DEFINITIONS)
/** The actual hooklist. */
static struct plugin_hooklist hooklist =
#else
/**
 * The plugin hook list.
 *
 * If you need a function or variable from server accessed by a plugin,
 * add it here.
 */
struct plugin_hooklist
#endif
{

PLUGIN_HOOK_FUNCTION(StringBuffer *, object_get_name, const object *, const object *, StringBuffer *)
PLUGIN_HOOK_FUNCTION(const char *, re_cmp, const char *, const char *)
PLUGIN_HOOK_FUNCTION(object *, present_in_ob, unsigned char, object *)
PLUGIN_HOOK_FUNCTION(int , players_on_map, mapstruct *)
PLUGIN_HOOK_FUNCTION(char *, create_pathname, const char *)
PLUGIN_HOOK_FUNCTION(void , free_string_shared, const char *)
PLUGIN_HOOK_FUNCTION(const char *, add_string, const char *)
PLUGIN_HOOK_FUNCTION(void , object_remove, object *, int)
PLUGIN_HOOK_FUNCTION(void , object_destroy, object *)
PLUGIN_HOOK_FUNCTION(int , living_update, object *)
PLUGIN_HOOK_FUNCTION(object *, insert_ob_in_ob, object *, object *)
PLUGIN_HOOK_FUNCTION(void , draw_info_map, uint8_t, const char *, const char *, mapstruct *, int, int, int, object *, object *, const char *)
PLUGIN_HOOK_FUNCTION(void , rune_spring, object *, object *)
PLUGIN_HOOK_FUNCTION(int , cast_spell, object *, object *, int, int, int, int, const char *)
PLUGIN_HOOK_FUNCTION(void , update_ob_speed, object *)
PLUGIN_HOOK_FUNCTION(int , change_skill, object *, int)
PLUGIN_HOOK_FUNCTION(void , pick_up, object *, object *, int)
PLUGIN_HOOK_FUNCTION(mapstruct *, get_map_from_coord, mapstruct *, int *, int *)
PLUGIN_HOOK_FUNCTION(void , esrv_send_item, object *)
PLUGIN_HOOK_FUNCTION(player *, find_player, const char *)
PLUGIN_HOOK_FUNCTION(int , manual_apply, object *, object *, int)
PLUGIN_HOOK_FUNCTION(void , command_drop, object *, const char *, char *)
PLUGIN_HOOK_FUNCTION(int , transfer_ob, object *, int, int, int, object *, object *)
PLUGIN_HOOK_FUNCTION(bool , kill_object, object *, object *)
PLUGIN_HOOK_FUNCTION(void , esrv_send_inventory, object *, object *)
PLUGIN_HOOK_FUNCTION(object *, arch_get, const char *)
PLUGIN_HOOK_FUNCTION(mapstruct *, ready_map_name, const char *, mapstruct *, int)
PLUGIN_HOOK_FUNCTION(int64_t, add_exp, object *, int64_t, int, int)
PLUGIN_HOOK_FUNCTION(const char *, determine_god, object *)
PLUGIN_HOOK_FUNCTION(object *, find_god, const char *)
PLUGIN_HOOK_FUNCTION(void , register_global_event, const char *, int)
PLUGIN_HOOK_FUNCTION(void , unregister_global_event, const char *, int)
PLUGIN_HOOK_FUNCTION(object *, load_object_str, const char *)
PLUGIN_HOOK_FUNCTION(int64_t, shop_get_cost, object *, int)
PLUGIN_HOOK_FUNCTION(int64_t, shop_get_money, object *)
PLUGIN_HOOK_FUNCTION(bool, shop_pay, object *, int64_t)
PLUGIN_HOOK_FUNCTION(object *, object_create_clone, object *)
PLUGIN_HOOK_FUNCTION(object *, get_object, void)
PLUGIN_HOOK_FUNCTION(void , copy_object, object *, object *, int)
PLUGIN_HOOK_FUNCTION(int , object_enter_map, object *, object *, mapstruct *, int, int, uint8_t)
PLUGIN_HOOK_FUNCTION(void , play_sound_map, mapstruct *, int, const char *, int, int, int, int)
PLUGIN_HOOK_FUNCTION(object *, find_marked_object, object *)
PLUGIN_HOOK_FUNCTION(int , cast_identify, object *, int, object *, int)
PLUGIN_HOOK_FUNCTION(struct archetype *, arch_find, const char *)
PLUGIN_HOOK_FUNCTION(object *, arch_to_object, struct archetype *)
PLUGIN_HOOK_FUNCTION(object *, insert_ob_in_map, object *, mapstruct *, object *, int)
PLUGIN_HOOK_FUNCTION(char *, shop_get_cost_string, int64_t)
PLUGIN_HOOK_FUNCTION(int , bank_deposit, object *, const char *, int64_t *value)
PLUGIN_HOOK_FUNCTION(int , bank_withdraw, object *, const char *, int64_t *value)
PLUGIN_HOOK_FUNCTION(int64_t, bank_get_balance, object *)
PLUGIN_HOOK_FUNCTION(int , swap_apartments, const char *, const char *, int, int, object *)
PLUGIN_HOOK_FUNCTION(int , player_exists, const char *)
PLUGIN_HOOK_FUNCTION(void , get_tod, timeofday_t *)
PLUGIN_HOOK_FUNCTION(const char *, object_get_value, const object *, const char *const)
PLUGIN_HOOK_FUNCTION(int , object_set_value, object *, const char *, const char *, int)
PLUGIN_HOOK_FUNCTION(void , drop, object *, object *, int)
PLUGIN_HOOK_FUNCTION(StringBuffer *, object_get_short_name, const object *, const object *, StringBuffer *)
PLUGIN_HOOK_FUNCTION(object *, beacon_locate, shstr *)
PLUGIN_HOOK_FUNCTION(void , player_cleanup_name, char *)
PLUGIN_HOOK_FUNCTION(party_struct *, find_party, const char *)
PLUGIN_HOOK_FUNCTION(void , add_party_member, party_struct *, object *)
PLUGIN_HOOK_FUNCTION(void , remove_party_member, party_struct *, object *)
PLUGIN_HOOK_FUNCTION(void , send_party_message, party_struct *, const char *, int, object *, object *)
PLUGIN_HOOK_FUNCTION(void , dump_object, object *, StringBuffer *)
PLUGIN_HOOK_FUNCTION(StringBuffer *, stringbuffer_new, void)
PLUGIN_HOOK_FUNCTION(void , stringbuffer_append_string, StringBuffer *, const char *)
PLUGIN_HOOK_FUNCTION(void , stringbuffer_append_printf, StringBuffer *, const char *, ...)
PLUGIN_HOOK_FUNCTION(char *, stringbuffer_finish, StringBuffer *)
PLUGIN_HOOK_FUNCTION(int , find_face, const char *, int)
PLUGIN_HOOK_FUNCTION(int , find_animation, const char *)
PLUGIN_HOOK_FUNCTION(void , play_sound_player_only, player *, int, const char *, int, int, int, int)
PLUGIN_HOOK_FUNCTION(int , was_destroyed, object *, tag_t)
PLUGIN_HOOK_FUNCTION(int , object_get_gender, object *)
PLUGIN_HOOK_FUNCTION(object *, decrease_ob_nr, object *, uint32_t)
PLUGIN_HOOK_FUNCTION(int , wall, mapstruct *, int, int)
PLUGIN_HOOK_FUNCTION(int , blocked, object *, mapstruct *, int, int, int)
PLUGIN_HOOK_FUNCTION(int , get_rangevector, object *, object *, rv_vector *, int)
PLUGIN_HOOK_FUNCTION(int , get_rangevector_from_mapcoords, mapstruct *, int, int, mapstruct *, int, int, rv_vector *, int)
PLUGIN_HOOK_FUNCTION(int , player_can_carry, object *, uint32_t)
PLUGIN_HOOK_FUNCTION(cache_struct *, cache_find, shstr *)
PLUGIN_HOOK_FUNCTION(int , cache_add, const char *, void *, uint32_t)
PLUGIN_HOOK_FUNCTION(int , cache_remove, shstr *)
PLUGIN_HOOK_FUNCTION(void , cache_remove_by_flags, uint32_t)
PLUGIN_HOOK_FUNCTION(shstr *, find_string, const char *)
PLUGIN_HOOK_FUNCTION(void , command_take, object *, const char *, char *)
PLUGIN_HOOK_FUNCTION(void , esrv_update_item, int, object *)
PLUGIN_HOOK_FUNCTION(void , commands_handle, object *, char *)
PLUGIN_HOOK_FUNCTION(treasurelist *, find_treasurelist, const char *)
PLUGIN_HOOK_FUNCTION(void , create_treasure, treasurelist *, object *, int, int, int, int, int, struct _change_arch *)
PLUGIN_HOOK_FUNCTION(void , dump_object_rec, object *, StringBuffer *)
PLUGIN_HOOK_FUNCTION(int , hit_player, object *, int, object *)
PLUGIN_HOOK_FUNCTION(int , move_ob, object *, int, object *)
PLUGIN_HOOK_FUNCTION(mapstruct *, get_empty_map, int, int)
PLUGIN_HOOK_FUNCTION(void , set_map_darkness, mapstruct *, int)
PLUGIN_HOOK_FUNCTION(int , find_free_spot, struct archetype *, object *, mapstruct *, int, int, int, int)
PLUGIN_HOOK_FUNCTION(void , send_target_command, player *)
PLUGIN_HOOK_FUNCTION(void , examine, object *, object *, StringBuffer *sb_capture)
PLUGIN_HOOK_FUNCTION(void , draw_info, const char *, object *, const char *)
PLUGIN_HOOK_FUNCTION(void , draw_info_format, const char *, object *, const char *, ...)
PLUGIN_HOOK_FUNCTION(void , draw_info_type, uint8_t, const char *, const char *, object *, const char *)
PLUGIN_HOOK_FUNCTION(void , draw_info_type_format, uint8_t, const char *, const char *, object *, const char *, ...)
PLUGIN_HOOK_FUNCTION(struct artifact_list *, artifact_list_find, uint8_t)
PLUGIN_HOOK_FUNCTION(void , artifact_change_object, struct artifact *, object *)
PLUGIN_HOOK_FUNCTION(int , connection_object_get_value, object *)
PLUGIN_HOOK_FUNCTION(void , connection_object_add, object *, mapstruct *, int)
PLUGIN_HOOK_FUNCTION(void , connection_trigger, object *, int)
PLUGIN_HOOK_FUNCTION(void , connection_trigger_button, object *, int)
PLUGIN_HOOK_FUNCTION(struct packet_struct *, packet_new, uint8_t, size_t, size_t)
PLUGIN_HOOK_FUNCTION(void , packet_free, struct packet_struct *)
PLUGIN_HOOK_FUNCTION(void , packet_compress, struct packet_struct *)
PLUGIN_HOOK_FUNCTION(void , packet_enable_ndelay, struct packet_struct *)
PLUGIN_HOOK_FUNCTION(void , packet_set_pos, struct packet_struct *, size_t)
PLUGIN_HOOK_FUNCTION(size_t, packet_get_pos, struct packet_struct *)
PLUGIN_HOOK_FUNCTION(void , packet_append_uint8, struct packet_struct *, uint8_t)
PLUGIN_HOOK_FUNCTION(void , packet_append_int8, struct packet_struct *, int8_t)
PLUGIN_HOOK_FUNCTION(void , packet_append_uint16, struct packet_struct *, uint16_t)
PLUGIN_HOOK_FUNCTION(void , packet_append_int16, struct packet_struct *, int16_t)
PLUGIN_HOOK_FUNCTION(void , packet_append_uint32, struct packet_struct *, uint32_t)
PLUGIN_HOOK_FUNCTION(void , packet_append_int32, struct packet_struct *, int32_t)
PLUGIN_HOOK_FUNCTION(void , packet_append_uint64, struct packet_struct *, uint64_t)
PLUGIN_HOOK_FUNCTION(void , packet_append_int64, struct packet_struct *, int64_t)
PLUGIN_HOOK_FUNCTION(void , packet_append_data_len, struct packet_struct *, const uint8_t *, size_t)
PLUGIN_HOOK_FUNCTION(void , packet_append_string, struct packet_struct *, const char *)
PLUGIN_HOOK_FUNCTION(void , packet_append_string_len_terminated, struct packet_struct *, const char *, size_t)
PLUGIN_HOOK_FUNCTION(void , packet_append_string_terminated, struct packet_struct *, const char *)
PLUGIN_HOOK_FUNCTION(void , packet_append_map_name, struct packet_struct *, object *, object *)
PLUGIN_HOOK_FUNCTION(void , packet_append_map_music, struct packet_struct *, object *, object *)
PLUGIN_HOOK_FUNCTION(void , packet_append_map_weather, struct packet_struct *, object *, object *)
PLUGIN_HOOK_FUNCTION(void , socket_send_packet, socket_struct *, struct packet_struct *)
PLUGIN_HOOK_FUNCTION(void , logger_print, logger_level, const char *, uint64_t, const char *, ...)
PLUGIN_HOOK_FUNCTION(logger_level, logger_get_level, const char *)
PLUGIN_HOOK_FUNCTION(void , commands_add, const char *, command_func, double, uint64_t)
PLUGIN_HOOK_FUNCTION(int , map_get_darkness, mapstruct *, int, int, object **)
PLUGIN_HOOK_FUNCTION(char *, map_get_path, mapstruct *, const char *, uint8_t, const char *)
PLUGIN_HOOK_FUNCTION(int , map_path_isabs, const char *)
PLUGIN_HOOK_FUNCTION(char *, path_dirname, const char *)
PLUGIN_HOOK_FUNCTION(char *, path_basename, const char *)
PLUGIN_HOOK_FUNCTION(char *, string_join, const char *delim, ...)
PLUGIN_HOOK_FUNCTION(object *, get_env_recursive, object *)
PLUGIN_HOOK_FUNCTION(int , set_variable, object *, const char *)
PLUGIN_HOOK_FUNCTION(uint64_t, level_exp, int, double)
PLUGIN_HOOK_FUNCTION(int , string_endswith, const char *, const char *)
PLUGIN_HOOK_FUNCTION(char *, string_sub, const char *, ssize_t, ssize_t MEMORY_DEBUG_PROTO)
PLUGIN_HOOK_FUNCTION(char *, path_join, const char *, const char *)
PLUGIN_HOOK_FUNCTION(void , monster_enemy_signal, object *, object *)
PLUGIN_HOOK_FUNCTION(void , map_redraw, mapstruct *, int, int, int, int)
#ifndef NDEBUG
PLUGIN_HOOK_FUNCTION(void *, memory_emalloc, size_t, const char *, uint32_t)
PLUGIN_HOOK_FUNCTION(void , memory_efree, void *, const char *, uint32_t)
PLUGIN_HOOK_FUNCTION(void *, memory_ecalloc, size_t, size_t, const char *, uint32_t)
PLUGIN_HOOK_FUNCTION(void *, memory_erealloc, void *, size_t, const char *, uint32_t)
PLUGIN_HOOK_FUNCTION(void *, memory_reallocz, void *, size_t, size_t, const char *, uint32_t)
PLUGIN_HOOK_FUNCTION(char *, string_estrdup, const char *, const char *, uint32_t)
PLUGIN_HOOK_FUNCTION(char *, string_estrndup, const char *, size_t , const char *, uint32_t)
#else
PLUGIN_HOOK_FUNCTION(void *, memory_emalloc, size_t)
PLUGIN_HOOK_FUNCTION(void , memory_efree, void *)
PLUGIN_HOOK_FUNCTION(void *, memory_ecalloc, size_t, size_t)
PLUGIN_HOOK_FUNCTION(void *, memory_erealloc, void *, size_t)
PLUGIN_HOOK_FUNCTION(void *, memory_reallocz, void *, size_t, size_t)
PLUGIN_HOOK_FUNCTION(char *, string_estrdup, const char *)
PLUGIN_HOOK_FUNCTION(char *, string_estrndup, const char *, size_t)
#endif
PLUGIN_HOOK_FUNCTION(player_faction_t *, player_faction_create, player *, shstr *)
PLUGIN_HOOK_FUNCTION(void , player_faction_free, player *, player_faction_t *)
PLUGIN_HOOK_FUNCTION(player_faction_t *, player_faction_find, player *, shstr *)
PLUGIN_HOOK_FUNCTION(struct faction *, faction_find, shstr *)
PLUGIN_HOOK_FUNCTION(double, faction_get_bounty, struct faction *, player *)
PLUGIN_HOOK_FUNCTION(void, faction_clear_bounty, struct faction *, player *)
PLUGIN_HOOK_FUNCTION(void, shop_insert_coins, object *, int64_t)
PLUGIN_HOOK_FUNCTION(void, add_object_to_packet, struct packet_struct *, object *, object *, uint8_t, uint32_t, int)
PLUGIN_HOOK_FUNCTION(void, player_save, object *)
PLUGIN_HOOK_FUNCTION(int, new_save_map, mapstruct *, int)
PLUGIN_HOOK_FUNCTION(void, main_process, void)
PLUGIN_HOOK_FUNCTION(char *, socket_get_addr, socket_t *)
PLUGIN_HOOK_FUNCTION(char *, socket_get_str, socket_t *)
PLUGIN_HOOK_FUNCTION(bool, faction_is_friend, struct faction *, object *)

PLUGIN_HOOK_ARRAY(const char *, season_name)
PLUGIN_HOOK_ARRAY(const char *, weekdays)
PLUGIN_HOOK_ARRAY(const char *, month_name)
PLUGIN_HOOK_ARRAY(const char *, periodsofday)
PLUGIN_HOOK_ARRAY(const char *, gender_noun)
PLUGIN_HOOK_ARRAY(const char *, gender_subjective)
PLUGIN_HOOK_ARRAY(const char *, gender_subjective_upper)
PLUGIN_HOOK_ARRAY(const char *, gender_objective)
PLUGIN_HOOK_ARRAY(const char *, gender_possessive)
PLUGIN_HOOK_ARRAY(const char *, gender_reflexive)
PLUGIN_HOOK_ARRAY(const char *, object_flag_names)
PLUGIN_HOOK_ARRAY(int, freearr_x)
PLUGIN_HOOK_ARRAY(int, freearr_y)
PLUGIN_HOOK_ARRAY(spell_struct, spells)
PLUGIN_HOOK_ARRAY(skill_struct, skills)

PLUGIN_HOOK_VARIABLE(struct shstr_constants, shstr_cons)
PLUGIN_HOOK_VARIABLE(player *, first_player)
PLUGIN_HOOK_VARIABLE(New_Face *, new_faces)
PLUGIN_HOOK_VARIABLE(int, nrofpixmaps)
PLUGIN_HOOK_VARIABLE(Animations *, animations)
PLUGIN_HOOK_VARIABLE(int, num_animations)
PLUGIN_HOOK_VARIABLE(mapstruct *, first_map)
PLUGIN_HOOK_VARIABLE(party_struct *, first_party)
PLUGIN_HOOK_VARIABLE(region_struct *, first_region)
PLUGIN_HOOK_VARIABLE(long, pticks)
PLUGIN_HOOK_VARIABLE(settings_struct, settings)
PLUGIN_HOOK_VARIABLE(long, max_time)

};

#endif
