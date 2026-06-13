#include <check.h>
#include <stdlib.h>
#include <string.h>

/* Include the header that declares PlayerEntity and related functions */
#include "src/playerentity.h"

/* Assume these are the actual function signatures from playerentity.c */
extern PlayerEntity* create_player_entity(void);
extern void set_player_name(PlayerEntity* entity, const char* name);
extern void free_player_entity(PlayerEntity* entity);

/* Assume NAME_MAX_LEN is defined in the header, or define expected size */
#ifndef NAME_MAX_LEN
#define NAME_MAX_LEN 32
#endif

START_TEST(test_player_name_buffer_bounds)
{
    /* Invariant: Player name must never exceed buffer bounds, preventing memory corruption */
    const char *payloads[] = {
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* 64 chars - overflow */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* 33 chars - boundary +1 */
        "ValidName", /* Valid short name */
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        PlayerEntity *entity = create_player_entity();
        ck_assert_ptr_nonnull(entity);
        
        set_player_name(entity, payloads[i]);
        
        /* Security invariant: name length must never exceed buffer capacity */
        size_t name_len = strlen(entity->name);
        ck_assert_msg(name_len < NAME_MAX_LEN,
            "Name length %zu exceeds buffer size %d for payload %d",
            name_len, NAME_MAX_LEN, i);
        
        free_player_entity(entity);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_player_name_buffer_bounds);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}