/*
 * Biometric Vault — HAL Test Program
 *
 * HAL katmanının simulator backend'i ile düzgün çalıştığını test eder.
 *
 * Kullanım:
 *   1. Simulator'ı başlat:  cd ../hw_simulator && python3 simulator.py -v
 *   2. Bu programı çalıştır: ./test_hal
 *
 * Beklenen çıktı:
 *   Her test adımında [PASS] veya [FAIL] gösterir.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hal.h"

/* ─── ANSI Colors ────────────────────────────────────────────────────────── */

#define RESET  "\033[0m"
#define BOLD   "\033[1m"
#define GREEN  "\033[92m"
#define RED    "\033[91m"
#define YELLOW "\033[93m"
#define CYAN   "\033[96m"
#define DIM    "\033[2m"

/* ─── Test Helpers ───────────────────────────────────────────────────────── */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name, expr)                                                  \
    do {                                                                  \
        tests_run++;                                                      \
        if (expr) {                                                       \
            tests_passed++;                                               \
            printf(GREEN "  [PASS]" RESET " %s\n", name);                \
        } else {                                                          \
            printf(RED "  [FAIL]" RESET " %s\n", name);                  \
        }                                                                 \
    } while (0)

#define SECTION(name)                                                     \
    printf("\n" BOLD CYAN "── %s ──" RESET "\n", name)

/* ─── Main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    int ret;
    uint8_t key[HAL_KEY_SIZE];
    hal_status_t status;
    hal_finger_info_t fingers[16];
    int finger_count;

    printf(BOLD "\n════════════════════════════════════════\n");
    printf("  Biometric Vault — HAL Test Suite\n");
    printf("════════════════════════════════════════\n" RESET);

    /* ── Init ── */
    SECTION("Initialization");

    ret = hal_init("simulator:/tmp/biometric_vault.sock");
    TEST("hal_init() succeeds", ret == HAL_OK);

    /* ── Status ── */
    SECTION("Device Status");

    ret = hal_get_status(&status);
    TEST("hal_get_status() succeeds", ret == HAL_OK);
    TEST("Device is connected", status.state == HAL_STATE_CONNECTED);

    /* ── Enroll ── */
    SECTION("Fingerprint Enrollment");

    ret = hal_enroll_finger(10, "Test Finger");
    TEST("Enroll finger #10", ret == HAL_OK);

    ret = hal_enroll_finger(10, "Duplicate");
    TEST("Duplicate enroll rejected", ret != HAL_OK);

    ret = hal_enroll_finger(0, "Invalid");
    TEST("Invalid ID (0) rejected", ret == HAL_ERR_INVALID_ARG);

    ret = hal_enroll_finger(200, "Invalid");
    TEST("Invalid ID (200) rejected", ret == HAL_ERR_INVALID_ARG);

    /* ── List ── */
    SECTION("List Fingerprints");

    ret = hal_list_fingers(fingers, 16, &finger_count);
    TEST("hal_list_fingers() succeeds", ret == HAL_OK);
    TEST("At least 1 finger enrolled", finger_count >= 1);

    if (finger_count > 0) {
        printf(DIM "  Enrolled fingers:\n");
        for (int i = 0; i < finger_count; i++) {
            printf("    #%d: %s (verified %d times)\n",
                   fingers[i].finger_id, fingers[i].name, fingers[i].verify_count);
        }
        printf(RESET);
    }

    /* ── Verify ── */
    SECTION("Fingerprint Verification");

    memset(key, 0, HAL_KEY_SIZE);
    ret = hal_verify_finger(10, key);
    TEST("Verify finger #10 succeeds", ret == HAL_OK);

    /* Key'in gerçekten dolu olduğunu kontrol et */
    int key_nonzero = 0;
    for (int i = 0; i < HAL_KEY_SIZE; i++) {
        if (key[i] != 0) { key_nonzero = 1; break; }
    }
    TEST("Key is non-zero (received AES-256 key)", key_nonzero);

    printf(DIM "  Key (first 8 bytes): ");
    for (int i = 0; i < 8; i++) printf("%02x ", key[i]);
    printf("..." RESET "\n");

    ret = hal_verify_finger(99, key);
    TEST("Verify unregistered finger #99 fails", ret == HAL_ERR_NO_MATCH);

    /* ── Delete ── */
    SECTION("Delete Fingerprint");

    ret = hal_delete_finger(10);
    TEST("Delete finger #10", ret == HAL_OK);

    ret = hal_delete_finger(10);
    TEST("Delete non-existent finger fails", ret == HAL_ERR_NOT_FOUND);

    /* ── Error Messages ── */
    SECTION("Error Strings");

    TEST("HAL_OK string", strcmp(hal_strerror(HAL_OK), "Success") == 0);
    TEST("HAL_ERR_NO_MATCH string", strcmp(hal_strerror(HAL_ERR_NO_MATCH), "Fingerprint no match") == 0);
    TEST("Unknown error string", strcmp(hal_strerror(-999), "Unknown error") == 0);

    /* ── Cleanup ── */
    SECTION("Cleanup");

    hal_cleanup();
    TEST("hal_cleanup() completes", 1);

    /* ── Summary ── */
    printf(BOLD "\n════════════════════════════════════════\n");
    if (tests_passed == tests_run) {
        printf(GREEN "  ALL %d TESTS PASSED ✓" RESET "\n", tests_run);
    } else {
        printf(RED "  %d/%d TESTS PASSED" RESET "\n", tests_passed, tests_run);
    }
    printf(BOLD "════════════════════════════════════════\n" RESET "\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
