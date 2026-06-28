#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../src/cli.h"

START_TEST(test_buffer_reads_never_exceed_declared_length)
{
    // Invariant: Buffer reads never exceed the declared length
    const char *payloads[] = {
        "normal",                    // Valid input
        "A",                         // Boundary: single char
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  // 100 chars - exceeds typical buffer
        "\x00\x01\x02\x03\x04\x05",  // Binary data
        "A;B;DROP TABLE users;--"    // SQL injection attempt
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        // Test the actual parse_command function from cli.c
        char buffer[32];  // Small buffer to test overflow
        memset(buffer, 0xCC, sizeof(buffer));  // Fill with sentinel value
        
        // Call the actual production function
        int result = parse_command(payloads[i], buffer, sizeof(buffer));
        
        // Security invariant: buffer must be null-terminated within bounds
        ck_assert_msg(buffer[sizeof(buffer)-1] == 0xCC || buffer[sizeof(buffer)-1] == '\0',
                     "Buffer overflow detected for payload: %s", payloads[i]);
        
        // If function succeeded, verify string is properly terminated
        if (result == 0) {
            ck_assert_msg(strnlen(buffer, sizeof(buffer)) < sizeof(buffer),
                         "String length exceeds buffer size for payload: %s", payloads[i]);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_buffer_reads_never_exceed_declared_length);
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