/*
 * HAL test — simulator (automated) or serial device (interactive by default)
 *
 *   cd hw_simulator && python3 simulator.py -v
 *   ./build/bin/test_hal
 *
 *   ./build/bin/test_hal serial:/dev/ttyUSB0          # interactive menu
 *   ./build/bin/test_hal serial:/dev/ttyUSB0 --auto   # fixed DoD script (slot 10)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hal.h"
#include "hal_protocol.h"

#define GREEN  "\033[92m"
#define RED    "\033[91m"
#define YELLOW "\033[93m"
#define CYAN   "\033[96m"
#define DIM    "\033[2m"
#define RESET  "\033[0m"
#define BOLD   "\033[1m"

static int tests_run;
static int tests_passed;
static int tests_skipped;

static void step(const char *msg)
{
    printf("\n  " CYAN "→" RESET " %s\n", msg);
    fflush(stdout);
}

static void step_note(const char *msg)
{
    printf("     " DIM "%s" RESET "\n", msg);
    fflush(stdout);
}

static void pass_line(const char *name)
{
    tests_passed++;
    printf(GREEN "  [PASS]" RESET " %s\n", name);
}

static void fail_line(const char *name, int err)
{
    printf(RED "  [FAIL]" RESET " %s", name);
    if (err != HAL_OK)
        printf(" — %s", hal_strerror(err));
    printf("\n");
}

static void skip_line(const char *name, const char *why)
{
    tests_skipped++;
    printf(YELLOW "  [SKIP]" RESET " %s", name);
    if (why && *why)
        printf(" — %s", why);
    printf("\n");
}

#define TEST(name, ok, err) do { \
    tests_run++; \
    if (ok) \
        pass_line(name); \
    else \
        fail_line(name, err); \
} while (0)

#define SECTION(name) printf("\n" BOLD CYAN "── %s ──" RESET "\n", name)

static void print_peer_hint(const char *hal_path)
{
    if (strstr(hal_path, "simulator:") != NULL) {
        step_note("Simulator not reachable. Start it in another terminal:");
        step_note("  cd hw_simulator && python3 simulator.py -v");
    } else if (strstr(hal_path, "serial:") != NULL) {
        step_note("Serial peer not responding. Check cable, port, baud, and firmware.");
    } else {
        step_note("Peer not reachable — check HAL path and backend.");
    }
}

static int run_suite(const char *hal_path, int already_open);

static void on_sensor_event(uint8_t evt_code, void *user_data)
{
    (void)user_data;
    const char *msg = NULL;

    switch (evt_code) {
    case HAL_EVT_WAITING_FINGER:
        msg = "Scan your finger on the sensor…";
        break;
    case HAL_EVT_FINGER_DETECTED:
        msg = "Finger detected.";
        break;
    case HAL_EVT_REMOVE_FINGER:
        msg = "Remove your finger.";
        break;
    case HAL_EVT_PLACE_AGAIN:
        msg = "Place the same finger again…";
        break;
    case HAL_EVT_PROCESSING:
        msg = "Processing fingerprint…";
        break;
    default:
        return;
    }

    printf(CYAN "     >> %s\n" RESET, msg);
    fflush(stdout);
}

static char parse_interactive_cmd(const char *line)
{
    if (!line)
        return '\0';
    while (*line == ' ' || *line == '\t')
        line++;
    if (*line == '-')
        line++;
    return line[0] ? line[0] : '\0';
}

static int read_slot(const char *prompt, int allow_auto)
{
    char line[32];
    if (allow_auto)
        printf("%s [0=auto, 1-127]: ", prompt);
    else
        printf("%s [1-127]: ", prompt);
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin))
        return -1;
    int id = atoi(line);
    if (allow_auto && id == 0)
        return 0;
    if (id < 1 || id > 127)
        return -1;
    return id;
}

static void print_verify_error(int err)
{
    if (err == HAL_ERR_TIMEOUT)
        printf(RED "  Verify failed: süre doldu — parmak algılanmadı (30 sn).\n" RESET);
    else if (err == HAL_ERR_NO_MATCH)
        printf(RED "  Verify failed: parmak okundu ama eşleşme yok (önce enroll?).\n" RESET);
    else
        printf(RED "  Verify failed: %s\n" RESET, hal_strerror(err));
}

static void print_key_hex(const uint8_t *key)
{
    printf("     Key: ");
    for (int i = 0; i < HAL_KEY_SIZE; i++)
        printf("%02x", key[i]);
    printf("\n");
}

static void print_finger_list(void)
{
    hal_finger_info_t fingers[64];
    int count = 0;

    printf(CYAN "\n  → Listing templates (127 slot scan, ~10–60 sn)…\n" RESET);
    printf(DIM "     MCU yanıt bekleniyor (Ctrl+C sonrası: c veya STM32 reset)\n" RESET);
    fflush(stdout);
    int ret = hal_list_fingers(fingers, 64, &count);
    if (ret != HAL_OK) {
        printf(RED "  Error: %s\n" RESET, hal_strerror(ret));
        return;
    }
    if (count == 0) {
        printf("  No enrolled templates.\n");
        return;
    }
    printf("  %d template(s):\n", count);
    for (int i = 0; i < count; i++)
        printf("    slot %3d  %s\n", fingers[i].finger_id, fingers[i].name);
}

static int run_interactive(const char *hal_path)
{
    int ret;

    printf(BOLD "\nHAL Interactive — %s\n" RESET, hal_path);
    printf(DIM "  Commands: p=ping  s=status  l=list  e=enroll  v=verify  k=get_key\n"
           "            d=delete  c=cancel  a=auto suite  q=quit\n" RESET);

    ret = hal_init(hal_path);
    if (ret != HAL_OK) {
        printf(RED "hal_init: %s\n" RESET, hal_strerror(ret));
        print_peer_hint(hal_path);
        return 1;
    }
    hal_set_event_callback(on_sensor_event, NULL);

    hal_status_t status;
    ret = hal_get_status(&status);
    if (ret != HAL_OK || status.state != HAL_STATE_CONNECTED) {
        printf(RED "Device not ready: %s\n" RESET,
               ret != HAL_OK ? hal_strerror(ret) : "disconnected");
        print_peer_hint(hal_path);
        hal_cleanup();
        return 1;
    }
    printf(GREEN "Connected.\n" RESET);

    for (;;) {
        printf(BOLD "\n> " RESET);
        fflush(stdout);
        char line[32];
        if (!fgets(line, sizeof(line), stdin))
            break;
        char cmd = parse_interactive_cmd(line);
        if (cmd == '\n' || cmd == '\0')
            continue;

        switch (cmd) {
        case 'q':
        case 'Q':
            hal_cleanup();
            printf("Bye.\n");
            return 0;

        case 'p':
        case 'P': {
            int tpl = 0;
            ret = hal_ping(&tpl);
            if (ret == HAL_OK)
                printf(GREEN "  Ping OK — MCU link up, ~%d template(s) on sensor.\n" RESET, tpl);
            else
                printf(RED "  Ping failed: %s\n" RESET, hal_strerror(ret));
            break;
        }

        case 's':
        case 'S':
            ret = hal_get_status(&status);
            printf("  status: %s\n", ret == HAL_OK ? "connected" : hal_strerror(ret));
            break;

        case 'l':
        case 'L':
            print_finger_list();
            break;

        case 'e':
        case 'E': {
            int slot = read_slot("Enroll slot", 1);
            if (slot < 0) {
                printf("  Invalid slot (use 0 for auto).\n");
                break;
            }
            int assigned = 0;
            if (slot == 0)
                printf(CYAN "  Auto enroll — first free slot, watch prompts below.\n" RESET);
            else
                printf(CYAN "  Enrolling slot %d — watch prompts below.\n" RESET, slot);
            ret = hal_enroll_finger(slot, "interactive", &assigned);
            if (ret == HAL_OK)
                printf(GREEN "  Enrolled slot %d.\n" RESET, assigned);
            else
                printf(RED "  Enroll failed: %s\n" RESET, hal_strerror(ret));
            break;
        }

        case 'v':
        case 'V': {
            int matched = 0;
            printf(CYAN "  Place enrolled finger on sensor (30s) — wait for >> Scan…\n" RESET);
            fflush(stdout);
            ret = hal_verify_finger(&matched);
            if (ret == HAL_OK)
                printf(GREEN "  Match: slot %d\n" RESET, matched);
            else
                print_verify_error(ret);
            break;
        }

        case 'k':
        case 'K': {
            uint8_t key[HAL_KEY_SIZE];
            memset(key, 0, sizeof(key));
            ret = hal_get_key(key);
            if (ret == HAL_OK) {
                printf(GREEN "  GET_KEY OK\n" RESET);
                print_key_hex(key);
            } else {
                printf(RED "  GET_KEY failed: %s\n" RESET, hal_strerror(ret));
                step_note("Run verify first; key window ~10s.");
            }
            break;
        }

        case 'd':
        case 'D': {
            int slot = read_slot("Delete slot", 0);
            if (slot < 0) {
                printf("  Invalid slot.\n");
                break;
            }
            ret = hal_delete_finger(slot);
            if (ret == HAL_OK)
                printf(GREEN "  Deleted slot %d.\n" RESET, slot);
            else if (ret == HAL_ERR_NO_MATCH)
                printf(YELLOW "  Slot %d: kayıtlı parmak yok.\n" RESET, slot);
            else
                printf(RED "  Delete failed: %s\n" RESET, hal_strerror(ret));
            break;
        }

        case 'c':
        case 'C':
            ret = hal_cancel_operation();
            printf("  Cancel: %s\n", ret == HAL_OK ? "sent" : hal_strerror(ret));
            break;

        case 'a':
        case 'A':
            printf(DIM "  Running automated DoD suite (slot 10)…\n" RESET);
            run_suite(hal_path, 1);
            printf(DIM "  Back to interactive menu.\n" RESET);
            break;

        default:
            printf("  Unknown command '%c'\n", cmd);
            break;
        }
    }

    hal_cleanup();
    return 0;
}

static int run_suite(const char *hal_path, int already_open)
{
    int ret;
    uint8_t key[HAL_KEY_SIZE];
    int matched_id = 0;
    hal_status_t status;
    hal_finger_info_t fingers[16];
    int finger_count;
    int enroll_ok = 0;
    int verify_ok = 0;
    const int on_serial = (strstr(hal_path, "serial:") != NULL);

    if (!already_open) {
        step("Opening HAL connection…");
        step_note(hal_path);
        ret = hal_init(hal_path);
        TEST("hal_init()", ret == HAL_OK, ret);
        if (ret != HAL_OK)
            return 1;
        if (on_serial)
            hal_set_event_callback(on_sensor_event, NULL);
    }

    step("Querying device status…");
    ret = hal_get_status(&status);
    if (ret == HAL_OK && status.state == HAL_STATE_CONNECTED) {
        TEST("hal_get_status()", 1, HAL_OK);
        TEST("device connected", 1, HAL_OK);
        step_note("Device reports connected.");
    } else if (ret == HAL_OK) {
        TEST("hal_get_status()", 1, HAL_OK);
        TEST("device connected", 0, HAL_ERR_DISCONNECTED);
        step_note("Device reports disconnected.");
    } else {
        TEST("hal_get_status()", 0, ret);
        TEST("device connected", 0, ret);
        print_peer_hint(hal_path);
        step("Remaining tests skipped (no peer).");
        if (!already_open)
            hal_cleanup();
        step_note("Done.");
        return 1;
    }

    step("Preparing slot 10 (delete if leftover from earlier run)…");
    (void)hal_delete_finger(10);

    step("Enrolling fingerprint in slot 10 (up to 60s)…");
    step_note("Follow the sensor prompts below.");
    int assigned_slot = 0;
    ret = hal_enroll_finger(10, "Test Finger", &assigned_slot);
    TEST("enroll #10", ret == HAL_OK, ret);
    if (ret == HAL_OK) {
        enroll_ok = 1;
        step_note("Slot 10 enrolled successfully.");
    }

    step("Enrolling slot 10 again (expect duplicate error)…");
    if (!enroll_ok) {
        skip_line("duplicate enroll rejected", "first enroll did not succeed");
    } else {
        ret = hal_enroll_finger(10, "dup", NULL);
        if (ret == HAL_ERR_EXISTS) {
            TEST("duplicate enroll rejected", 1, HAL_OK);
            step_note(hal_strerror(ret));
        } else {
            TEST("duplicate enroll rejected", 0, ret == HAL_OK ? HAL_ERR_UNKNOWN : ret);
        }
    }

    step("Listing enrolled fingerprints on sensor…");
    finger_count = 0;
    ret = hal_list_fingers(fingers, 16, &finger_count);
    TEST("list fingers", ret == HAL_OK, ret);
    if (ret == HAL_OK) {
        if (finger_count == 0)
            step_note("No templates reported.");
        else {
            char buf[128];
            int pos = 0;
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%d slot(s): ", finger_count);
            for (int i = 0; i < finger_count && pos < (int)sizeof(buf) - 8; i++) {
                if (i > 0)
                    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, ", ");
                pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%d", fingers[i].finger_id);
            }
            step_note(buf);
        }
    }

    step("Verifying fingerprint (1:N match, up to 30s)…");
    step_note("Scan the enrolled finger.");
    memset(key, 0, sizeof(key));
    matched_id = 0;
    ret = hal_verify_finger(&matched_id);
    TEST("verify_start", ret == HAL_OK, ret);
    if (ret == HAL_OK) {
        if (matched_id >= 1 && matched_id <= 127) {
            TEST("matched_id valid", 1, HAL_OK);
            verify_ok = 1;
            char note[48];
            snprintf(note, sizeof(note), "Matched slot %d.", matched_id);
            step_note(note);
        } else {
            TEST("matched_id valid", 0, HAL_ERR_PROTOCOL);
        }
    } else {
        TEST("matched_id valid", 0, ret);
    }

    if (verify_ok) {
        step("Requesting vault key (GET_KEY)…");
        step_note("Must be within ~10s after successful verify.");
        ret = hal_get_key(key);
        TEST("get_key", ret == HAL_OK, ret);
        if (ret == HAL_OK) {
            int nz = 0;
            for (int i = 0; i < HAL_KEY_SIZE; i++)
                if (key[i])
                    nz = 1;
            TEST("key non-zero", nz, HAL_ERR_PROTOCOL);
            if (nz)
                step_note("Received 32-byte vault key.");
        } else {
            tests_run++;
            fail_line("key non-zero", ret);
        }
    } else {
        step("Skipping GET_KEY (verify did not succeed).");
        step_note("get_key and key non-zero tests not run.");
        skip_line("get_key", "verify did not succeed");
        skip_line("key non-zero", "verify did not succeed");
    }

    step("Deleting fingerprint slot 10…");
    ret = hal_delete_finger(10);
    TEST("delete #10", ret == HAL_OK, ret);
    if (ret == HAL_OK)
        step_note("Slot 10 removed from sensor.");

    if (!already_open) {
        step("Closing HAL connection…");
        hal_cleanup();
        step_note("Done.");
    }

    return (tests_passed == tests_run) ? 0 : 1;
}

static int run_stress(const char *hal_path, int iterations)
{
    int ret;

    step("Opening HAL connection (stress mode)…");
    ret = hal_init(hal_path);
    if (ret != HAL_OK) {
        printf(RED "  hal_init failed: %s\n" RESET, hal_strerror(ret));
        print_peer_hint(hal_path);
        return 1;
    }
    TEST("hal_init()", 1, HAL_OK);

    if (strstr(hal_path, "serial:") != NULL)
        hal_set_event_callback(on_sensor_event, NULL);

    hal_status_t status;
    ret = hal_get_status(&status);
    if (ret != HAL_OK || status.state != HAL_STATE_CONNECTED) {
        printf(RED "  Peer not ready: %s\n" RESET,
               ret != HAL_OK ? hal_strerror(ret) : "device disconnected");
        print_peer_hint(hal_path);
        hal_cleanup();
        return 1;
    }

    int ok_count = 0;
    for (int i = 0; i < iterations; i++) {
        char label[64];
        snprintf(label, sizeof(label), "Stress iteration %d/%d — verify + get_key", i + 1,
                 iterations);
        step(label);
        uint8_t key[HAL_KEY_SIZE];
        int matched = 0;
        ret = hal_verify_finger(&matched);
        if (ret != HAL_OK) {
            step_note(hal_strerror(ret));
            continue;
        }
        char note[48];
        snprintf(note, sizeof(note), "Matched slot %d, fetching key…", matched);
        step_note(note);
        ret = hal_get_key(key);
        if (ret == HAL_OK)
            ok_count++;
        else
            step_note(hal_strerror(ret));
    }

    step("Closing HAL connection…");
    hal_cleanup();

    printf("\n  Stress summary: %d/%d verify+get_key OK\n", ok_count, iterations);
    TEST("stress all iterations", ok_count == iterations, HAL_ERR_UNKNOWN);

    return (ok_count == iterations) ? 0 : 1;
}

/** List enrolled slots and delete each (non-interactive factory reset). */
static int run_wipe_sensor(const char *hal_path)
{
    hal_finger_info_t fingers[64];
    int count = 0;
    int tpl = 0;

    printf(BOLD "\nHAL wipe-sensor — %s\n" RESET, hal_path);

    int ret = hal_init(hal_path);
    if (ret != HAL_OK) {
        printf(RED "hal_init: %s\n" RESET, hal_strerror(ret));
        print_peer_hint(hal_path);
        return 1;
    }

    printf(CYAN "  Listing enrolled templates (may take up to ~2 min)…\n" RESET);
    fflush(stdout);
    ret = hal_list_fingers(fingers, 64, &count);
    if (ret != HAL_OK) {
        printf(RED "  List failed: %s\n" RESET, hal_strerror(ret));
        hal_cleanup();
        return 1;
    }

    if (count == 0) {
        printf(GREEN "  No templates on sensor.\n" RESET);
    } else {
        printf("  Deleting %d template(s)…\n", count);
        for (int i = 0; i < count; i++) {
            const int slot = fingers[i].finger_id;
            printf("    slot %d … ", slot);
            fflush(stdout);
            ret = hal_delete_finger(slot);
            if (ret == HAL_OK)
                printf(GREEN "OK\n" RESET);
            else if (ret == HAL_ERR_NO_MATCH)
                printf(YELLOW "empty\n" RESET);
            else
                printf(RED "%s\n" RESET, hal_strerror(ret));
        }
    }

    ret = hal_ping(&tpl);
    if (ret == HAL_OK)
        printf(GREEN "  Ping: ~%d template(s) remaining.\n" RESET, tpl);
    else
        printf(YELLOW "  Ping: %s\n" RESET, hal_strerror(ret));

    hal_cleanup();
    return (ret == HAL_OK && tpl == 0) ? 0 : (count == 0 ? 0 : 1);
}

int main(int argc, char **argv)
{
    const char *path = "simulator:/tmp/biometric_vault.sock";
    int stress = 0;
    int force_auto = 0;
    int force_interactive = 0;
    int wipe_sensor = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--stress") == 0 && i + 1 < argc) {
            stress = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--auto") == 0 || strcmp(argv[i], "--suite") == 0) {
            force_auto = 1;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
            force_interactive = 1;
        } else if (strcmp(argv[i], "--wipe-sensor") == 0) {
            wipe_sensor = 1;
        } else if (argv[i][0] != '-') {
            path = argv[i];
        }
    }

    if (wipe_sensor)
        return run_wipe_sensor(path);

    const int is_serial = (strstr(path, "serial:") != NULL);
    const int interactive = force_interactive || (is_serial && !force_auto && stress == 0);

    if (interactive) {
        return run_interactive(path);
    }

    tests_run = 0;
    tests_passed = 0;
    tests_skipped = 0;

    printf(BOLD "\nHAL Test Suite — %s\n" RESET, path);
    if (stress > 0) {
        SECTION("DoD stress");
        return run_stress(path, stress) == 0 ? 0 : 1;
    }
    run_suite(path, 0);

    printf(BOLD "\n════════════════════════════════════════\n");
    if (tests_passed == tests_run)
        printf(GREEN "  ALL %d TESTS PASSED" RESET, tests_run);
    else
        printf(RED "  %d/%d PASSED" RESET, tests_passed, tests_run);
    if (tests_skipped > 0)
        printf(", " YELLOW "%d SKIPPED" RESET, tests_skipped);
    printf("\n");
    printf(BOLD "════════════════════════════════════════\n" RESET);

    return (tests_passed == tests_run) ? 0 : 1;
}
