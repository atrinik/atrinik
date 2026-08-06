/*************************************************************************
 * Atrinik server presentation synchronization regression tests.
 ************************************************************************/

#include <global.h>
#include <check.h>
#include <checkstd.h>
#include <check_proto.h>
#include <toolkit/packet.h>
#include <arch.h>
#include <object.h>
#include <player.h>

static size_t queued_command_count(socket_struct *cs, uint8_t type) {
    size_t count = 0;

    for (packet_struct *packet = cs->packets; packet != NULL; packet = packet->next) {
        if (packet->type == type) {
            count++;
        }
    }

    return count;
}

START_TEST(test_target_packet_includes_current_level_and_plain_name) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    pl->level = 42;
    CONTR(pl)->tgm = 1;
    socket_buffer_clear(CONTR(pl)->cs);

    send_target_command(CONTR(pl));

    packet_struct *packet = CONTR(pl)->cs->packets;
    while (packet != NULL && packet->type != CLIENT_CMD_TARGET) {
        packet = packet->next;
    }
    ck_assert_ptr_nonnull(packet);

    size_t pos = 0;
    char color[MAX_BUF];
    char name[MAX_BUF];
    ck_assert_uint_eq(packet_to_uint8(packet->data, packet->len, &pos), CMD_TARGET_SELF);
    packet_to_string(packet->data, packet->len, &pos, VS(color));
    packet_to_string(packet->data, packet->len, &pos, VS(name));
    ck_assert_str_eq(name, pl->name);
    ck_assert_uint_eq(packet_to_uint8(packet->data, packet->len, &pos), 42);
    ck_assert_uint_eq(packet_to_uint8(packet->data, packet->len, &pos), CONTR(pl)->combat);
    ck_assert_uint_eq(packet_to_uint8(packet->data, packet->len, &pos), CONTR(pl)->combat_force);
    ck_assert_uint_eq(pos, packet->len);
}
END_TEST

START_TEST(test_wizardry_level_change_refreshes_spell_cost_once) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    object *spell = object_insert_into(arch_get("spell_frostbolt"), pl, 0);
    ck_assert_ptr_nonnull(spell);
    ck_assert_int_eq(spell->type, SPELL);

    CONTR(pl)->last_spell_cost_level = CONTR(pl)->skill_ptr[SK_WIZARDRY_SPELLS]->level;
    socket_buffer_clear(CONTR(pl)->cs);
    CONTR(pl)->skill_ptr[SK_WIZARDRY_SPELLS]->level++;

    esrv_update_stats(CONTR(pl));
    ck_assert_uint_eq(queued_command_count(CONTR(pl)->cs, CLIENT_CMD_ITEM_UPDATE), 1);

    socket_buffer_clear(CONTR(pl)->cs);
    esrv_update_stats(CONTR(pl));
    ck_assert_uint_eq(queued_command_count(CONTR(pl)->cs, CLIENT_CMD_ITEM_UPDATE), 0);
}
END_TEST

START_TEST(test_who_escapes_bot_marker_for_client_markup) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    CONTR(pl)->cs->is_bot = 1;
    socket_buffer_clear(CONTR(pl)->cs);

    command_who(pl, "who", NULL);

    bool found_marker = false;
    for (packet_struct *packet = CONTR(pl)->cs->packets; packet != NULL; packet = packet->next) {
        if (packet->type != CLIENT_CMD_DRAWINFO) {
            continue;
        }

        size_t pos = 0;
        char color[MAX_BUF];
        char message[MAX_BUF];
        packet_to_uint8(packet->data, packet->len, &pos);
        packet_to_string(packet->data, packet->len, &pos, VS(color));
        packet_to_string(packet->data, packet->len, &pos, VS(message));
        ck_assert_ptr_null(strstr(message, " [BOT]"));
        found_marker |= strstr(message, " &lsqb;BOT&rsqb;") != NULL;
    }

    ck_assert(found_marker);
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("request");
    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, check_setup, check_teardown);
    tcase_add_checked_fixture(tc_core, check_test_setup, check_test_teardown);
    suite_add_tcase(s, tc_core);
    tcase_add_test(tc_core, test_target_packet_includes_current_level_and_plain_name);
    tcase_add_test(tc_core, test_wizardry_level_change_refreshes_spell_cost_once);
    tcase_add_test(tc_core, test_who_escapes_bot_marker_for_client_markup);

    return s;
}

void check_server_request(void) {
    check_run_suite(suite(), __FILE__);
}
