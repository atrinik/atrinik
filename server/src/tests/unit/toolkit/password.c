/*************************************************************************
 * Password-record tests.                                               *
 ************************************************************************/

#include <check.h>
#include <global.h>
#include <checkstd.h>
#include <check_utils.h>
#include <toolkit/password.h>

START_TEST(test_password_known_answer) {
    const char *record = "$argon2id$v=19$m=65536,t=3,p=1$000102030405060708090A0B0C0D0E0F$"
                         "3DA7C605979A32436F52FE2F9C7DD240E1A75DCA3CCAA47EB8DD2EBC99ACA0EE";

    ck_assert(password_record_is_valid(record));
    ck_assert_int_eq(password_record_verify("correct-horse", record), PASSWORD_VERIFY_MATCH);
    ck_assert_int_eq(password_record_verify("wrong", record), PASSWORD_VERIFY_MISMATCH);
}
END_TEST

START_TEST(test_password_create_round_trip) {
    char first[PASSWORD_RECORD_SIZE];
    char second[PASSWORD_RECORD_SIZE];

    ck_assert(password_record_create("battery staple", first));
    ck_assert(password_record_create("battery staple", second));
    ck_assert_str_ne(first, second);
    ck_assert(password_record_is_valid(first));
    ck_assert_int_eq(password_record_verify("battery staple", first), PASSWORD_VERIFY_MATCH);
}
END_TEST

START_TEST(test_password_rejects_malformed_and_unbounded_records) {
    static const char *records[] = {
        "",
        "$argon2i$v=19$m=65536,t=3,p=1$000102030405060708090a0b0c0d0e0f$"
        "3da7c605979a32436f52fe2f9c7dd240e1a75dca3ccaa47eb8dd2ebc99aca0ee",
        "$argon2id$v=16$m=65536,t=3,p=1$000102030405060708090a0b0c0d0e0f$"
        "3da7c605979a32436f52fe2f9c7dd240e1a75dca3ccaa47eb8dd2ebc99aca0ee",
        "$argon2id$v=19$m=7,t=3,p=1$000102030405060708090a0b0c0d0e0f$"
        "3da7c605979a32436f52fe2f9c7dd240e1a75dca3ccaa47eb8dd2ebc99aca0ee",
        "$argon2id$v=19$m=262145,t=3,p=1$000102030405060708090a0b0c0d0e0f$"
        "3da7c605979a32436f52fe2f9c7dd240e1a75dca3ccaa47eb8dd2ebc99aca0ee",
        "$argon2id$v=19$m=65536,t=11,p=1$000102030405060708090a0b0c0d0e0f$"
        "3da7c605979a32436f52fe2f9c7dd240e1a75dca3ccaa47eb8dd2ebc99aca0ee",
        "$argon2id$v=19$m=65536,t=3,p=5$000102030405060708090a0b0c0d0e0f$"
        "3da7c605979a32436f52fe2f9c7dd240e1a75dca3ccaa47eb8dd2ebc99aca0ee",
        "$argon2id$v=19$m=65536,t=3,p=1$00$00",
        "$argon2id$v=19$m=65536,t=3,p=1$000102030405060708090A0B0C0D0E0F$"
        "3da7c605979a32436f52fe2f9c7dd240e1a75dca3ccaa47eb8dd2ebc99aca0eejunk",
        "$argon2id$v=19$m=065536,t=3,p=1$000102030405060708090A0B0C0D0E0F$"
        "3DA7C605979A32436F52FE2F9C7DD240E1A75DCA3CCAA47EB8DD2EBC99ACA0EE",
        "$argon2id$v=19$m= 65536,t=3,p=1$000102030405060708090A0B0C0D0E0F$"
        "3DA7C605979A32436F52FE2F9C7DD240E1A75DCA3CCAA47EB8DD2EBC99ACA0EE",
    };

    for (size_t i = 0; i < arraysize(records); i++) {
        ck_assert(!password_record_is_valid(records[i]));
        ck_assert_int_eq(password_record_verify("password", records[i]), PASSWORD_VERIFY_ERROR);
    }
}
END_TEST

START_TEST(test_password_rehash_policy) {
    const char *current = "$argon2id$v=19$m=65536,t=3,p=1$000102030405060708090A0B0C0D0E0F$"
                          "3DA7C605979A32436F52FE2F9C7DD240E1A75DCA3CCAA47EB8DD2EBC99ACA0EE";
    const char *weaker = "$argon2id$v=19$m=8192,t=2,p=1$000102030405060708090A0B0C0D0E0F$"
                         "0000000000000000000000000000000000000000000000000000000000000000";

    ck_assert(!password_record_needs_rehash(current));
    ck_assert(password_record_needs_rehash(weaker));
    ck_assert(!password_record_needs_rehash("malformed"));
}
END_TEST

START_TEST(test_legacy_pbkdf2_known_answer) {
    const unsigned char salt[32] = "salt";
    const unsigned char expected[32] = {
        0x72, 0xd3, 0xad, 0xf1, 0x1c, 0x90, 0x73, 0x6f, 0xf9, 0x7b, 0xbe,
        0x2c, 0x1f, 0xc0, 0x2a, 0xeb, 0xb9, 0xd1, 0x7b, 0xcd, 0x36, 0xb4,
        0x3e, 0x39, 0x0a, 0x7c, 0x40, 0x8c, 0x17, 0xe5, 0x8d, 0x37,
    };

    ck_assert_int_eq(password_legacy_pbkdf2_verify("password", salt, expected),
                     PASSWORD_VERIFY_MATCH);
    ck_assert_int_eq(password_legacy_pbkdf2_verify("wrong", salt, expected),
                     PASSWORD_VERIFY_MISMATCH);
}
END_TEST

static Suite *suite(void) {
    Suite *s = suite_create("password");
    TCase *tc = tcase_create("core");

    tcase_set_timeout(tc, 30);
    tcase_add_test(tc, test_password_known_answer);
    tcase_add_test(tc, test_password_create_round_trip);
    tcase_add_test(tc, test_password_rejects_malformed_and_unbounded_records);
    tcase_add_test(tc, test_password_rehash_policy);
    tcase_add_test(tc, test_legacy_pbkdf2_known_answer);
    suite_add_tcase(s, tc);
    return s;
}

void check_server_password(void) {
    check_run_suite(suite(), __FILE__);
}
