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

static packet_struct *queued_command_find(socket_struct *cs, uint8_t type) {
    for (packet_struct *packet = cs->packets; packet != NULL; packet = packet->next) {
        if (packet->type == type) {
            return packet;
        }
    }

    return NULL;
}

static void request_version(socket_struct *cs, player *pl, uint32_t version) {
    packet_struct *request = packet_new(0, 4, 0);
    packet_writer_write_uint32(request, version);
    socket_command_version(cs, pl, request->data, request->len, 0);
    packet_free(request);
}

START_TEST(test_target_packet_includes_current_level_and_plain_name) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    pl->level = 42;
    CONTR(pl)->tgm = 1;
    socket_buffer_clear(CONTR(pl)->cs);

    send_target_command(CONTR(pl));

    packet_struct *packet = queued_command_find(CONTR(pl)->cs, CLIENT_CMD_TARGET);
    ck_assert_ptr_nonnull(packet);

    packet_reader_t reader;
    packet_reader_init(&reader, packet->data, packet->len);
    char color[MAX_BUF];
    char name[MAX_BUF];
    ck_assert_uint_eq(packet_reader_read_uint8(&reader), CMD_TARGET_SELF);
    ck_assert(packet_reader_read_string(&reader, VS(color)));
    ck_assert(packet_reader_read_string(&reader, VS(name)));
    ck_assert_str_eq(name, pl->name);
    ck_assert_uint_eq(packet_reader_read_uint8(&reader), 42);
    ck_assert_uint_eq(packet_reader_read_uint8(&reader), CONTR(pl)->combat);
    ck_assert_uint_eq(packet_reader_read_uint8(&reader), CONTR(pl)->combat_force);
    ck_assert(packet_reader_finish(&reader));
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

START_TEST(test_setup_round_trip_uses_current_option_ids) {
    mapstruct *map;
    object *pl;

    ck_assert_uint_eq(CMD_SETUP_SOUND, 0);
    ck_assert_uint_eq(CMD_SETUP_MAPSIZE, 1);
    ck_assert_uint_eq(CMD_SETUP_DATA_URL, 2);
    ck_assert_uint_eq(CMD_SETUP_JOIN_PASSWORD, 3);
    ck_assert_uint_eq(CMD_SETUP_ASSET_TRANSPORT, 4);
    ck_assert_uint_eq(CMD_SETUP_CONNECTION_MODE, 5);

    check_setup_env_pl(&map, &pl);
    socket_struct *cs = CONTR(pl)->cs;
    uint8_t request[] = {CMD_SETUP_SOUND, 1, CMD_SETUP_MAPSIZE, 13, 15};
    socket_buffer_clear(cs);

    socket_command_setup(cs, CONTR(pl), request, sizeof(request), 0);

    packet_struct *response = queued_command_find(cs, CLIENT_CMD_SETUP);
    ck_assert_ptr_nonnull(response);

    packet_reader_t reader;
    packet_reader_init(&reader, response->data, response->len);
    ck_assert_uint_eq(packet_reader_read_uint8(&reader), CMD_SETUP_SOUND);
    ck_assert_uint_eq(packet_reader_read_uint8(&reader), 1);
    ck_assert_uint_eq(packet_reader_read_uint8(&reader), CMD_SETUP_MAPSIZE);
    ck_assert_uint_eq(packet_reader_read_uint8(&reader), 13);
    ck_assert_uint_eq(packet_reader_read_uint8(&reader), 15);
    ck_assert(packet_reader_finish(&reader));
    ck_assert_uint_eq(cs->sound, 1);
    ck_assert_int_eq(cs->mapx, 13);
    ck_assert_int_eq(cs->mapy, 15);
}
END_TEST

START_TEST(test_setup_rejects_unknown_option) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    socket_struct *cs = CONTR(pl)->cs;
    uint8_t request[] = {UINT8_MAX};
    socket_buffer_clear(cs);

    socket_command_setup(cs, CONTR(pl), request, sizeof(request), 0);

    ck_assert_int_eq(cs->state, ST_ZOMBIE);
    ck_assert_ptr_null(queued_command_find(cs, CLIENT_CMD_SETUP));
}
END_TEST

START_TEST(test_version_requires_exact_match) {
    mapstruct *map;
    object *pl;

    check_setup_env_pl(&map, &pl);
    socket_struct *cs = CONTR(pl)->cs;

    socket_buffer_clear(cs);
    cs->state = ST_LOGIN;
    cs->socket_version = 0;
    request_version(cs, CONTR(pl), SOCKET_VERSION - 1);
    ck_assert_int_eq(cs->state, ST_ZOMBIE);
    ck_assert_uint_eq(cs->socket_version, 0);
    ck_assert_ptr_null(queued_command_find(cs, CLIENT_CMD_VERSION));

    socket_buffer_clear(cs);
    cs->state = ST_LOGIN;
    cs->socket_version = 0;
    request_version(cs, CONTR(pl), SOCKET_VERSION + 1);
    ck_assert_int_eq(cs->state, ST_ZOMBIE);
    ck_assert_uint_eq(cs->socket_version, 0);
    ck_assert_ptr_null(queued_command_find(cs, CLIENT_CMD_VERSION));

    socket_buffer_clear(cs);
    cs->state = ST_LOGIN;
    cs->socket_version = 0;
    request_version(cs, CONTR(pl), SOCKET_VERSION);
    ck_assert_int_eq(cs->state, ST_LOGIN);
    ck_assert_uint_eq(cs->socket_version, SOCKET_VERSION);

    packet_struct *response = queued_command_find(cs, CLIENT_CMD_VERSION);
    ck_assert_ptr_nonnull(response);
    packet_reader_t reader;
    packet_reader_init(&reader, response->data, response->len);
    ck_assert_uint_eq(packet_reader_read_uint32(&reader), SOCKET_VERSION);
    ck_assert(packet_reader_finish(&reader));
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
    tcase_add_test(tc_core, test_setup_round_trip_uses_current_option_ids);
    tcase_add_test(tc_core, test_setup_rejects_unknown_option);
    tcase_add_test(tc_core, test_version_requires_exact_match);
    return s;
}

void check_server_request(void) {
    check_run_suite(suite(), __FILE__);
}
