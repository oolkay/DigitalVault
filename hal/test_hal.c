/*
 * HAL test suite — simulator socket + optional serial PTY
 *
 *   python3 -m hw_simulator.simulator -v
 *   ./test_hal
 *   ./test_hal serial:/dev/pts/N   # with uart mode: python3 -m hw_simulator.simulator --uart /dev/pts/M
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hal.h"

#define GREEN  "\033[92m"
#define RED    "\033[91m"
#define CYAN   "\033[96m"
#define RESET  "\033[0m"
#define BOLD   "\033[1m"

static int tests_run;
static int tests_passed;

#define TEST(name, expr) do { \
    tests_run++; \
    if (expr) { tests_passed++; printf(GREEN "  [PASS]" RESET " %s\n", name); } \
    else { printf(RED "  [FAIL]" RESET " %s\n", name); } \
} while (0)

#define SECTION(name) printf("\n" BOLD CYAN "── %s ──" RESET "\n", name)

static int run_suite(const char *hal_path)
{
    int ret;
    uint8_t key[HAL_KEY_SIZE];
    int matched_id = 0;
    hal_status_t status;
    hal_finger_info_t fingers[16];
    int finger_count;

    ret = hal_init(hal_path);
    TEST("hal_init()", ret == HAL_OK);
    if (ret != HAL_OK)
        return 1;

    ret = hal_get_status(&status);
    TEST("hal_get_status()", ret == HAL_OK);
    TEST("device connected", status.state == HAL_STATE_CONNECTED);

    ret = hal_enroll_finger(10, "Test Finger");
    TEST("enroll #10", ret == HAL_OK);

    ret = hal_enroll_finger(10, "dup");
    TEST("duplicate enroll rejected", ret != HAL_OK);

    ret = hal_list_fingers(fingers, 16, &finger_count);
    TEST("list fingers", ret == HAL_OK || strstr(hal_path, "serial:") != NULL);

    memset(key, 0, sizeof(key));
    matched_id = 0;
    ret = hal_verify_finger(&matched_id);
    TEST("verify_start", ret == HAL_OK);
    TEST("matched_id valid", matched_id >= 1 && matched_id <= 127);

    if (ret == HAL_OK) {
        ret = hal_get_key(key);
        TEST("get_key", ret == HAL_OK);
        int nz = 0;
        for (int i = 0; i < HAL_KEY_SIZE; i++)
            if (key[i]) { nz = 1; break; }
        TEST("key non-zero", nz);
    }

    ret = hal_delete_finger(10);
    TEST("delete #10", ret == HAL_OK);

    hal_cleanup();
    return (tests_passed == tests_run) ? 0 : 1;
}

static int run_stress(const char *hal_path, int iterations)
{
    int ret = hal_init(hal_path);
    if (ret != HAL_OK) {
        printf(RED "hal_init failed\n" RESET);
        return 1;
    }

    int ok_count = 0;
    for (int i = 0; i < iterations; i++) {
        uint8_t key[HAL_KEY_SIZE];
        int matched = 0;
        ret = hal_verify_finger(&matched);
        if (ret != HAL_OK)
            continue;
        ret = hal_get_key(key);
        if (ret == HAL_OK)
            ok_count++;
    }
    hal_cleanup();

    printf("Stress: %d/%d verify+get_key OK\n", ok_count, iterations);
    TEST("stress all iterations", ok_count == iterations);
    return (ok_count == iterations) ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *path = "simulator:/tmp/biometric_vault.sock";
    int stress = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--stress") == 0 && i + 1 < argc) {
            stress = atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            path = argv[i];
        }
    }

    tests_run = 0;
    tests_passed = 0;

    printf(BOLD "\nHAL Test Suite — %s\n" RESET, path);
    if (stress > 0) {
        SECTION("DoD stress");
        return run_stress(path, stress) == 0 ? 0 : 1;
    }
    run_suite(path);

    printf(BOLD "\n════════════════════════════════════════\n");
    if (tests_passed == tests_run)
        printf(GREEN "  ALL %d TESTS PASSED" RESET "\n", tests_run);
    else
        printf(RED "  %d/%d PASSED" RESET "\n", tests_passed, tests_run);
    printf(BOLD "════════════════════════════════════════\n" RESET);

    return (tests_passed == tests_run) ? 0 : 1;
}
