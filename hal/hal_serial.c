/*
 * HAL — Binary UART backend (simulator PTY or /dev/ttyUSB0)
 */

#include "hal_internal.h"
#include "hal_protocol.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define VERIFY_TIMEOUT_MS  15000
#define ENROLL_TIMEOUT_MS  60000
#define IO_TIMEOUT_MS      2000
#define GET_KEY_WINDOW_MS  10000

static int g_fd = -1;
static int g_initialized = 0;
static int g_key_window_open = 0;

static int serial_configure(int fd)
{
    struct termios tio;
    if (tcgetattr(fd, &tio) < 0)
        return -1;
    cfmakeraw(&tio);
    /* PTY ignores baud; cfsetispeed can fail on Linux pseudo-ttys */
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;
    if (tcsetattr(fd, TCSANOW, &tio) < 0)
        return -1;
    return 0;
}

static int read_byte_timeout(int fd, uint8_t *byte, int timeout_ms)
{
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int pr = poll(&pfd, 1, timeout_ms);
    if (pr < 0)
        return -1;
    if (pr == 0)
        return 0;
    ssize_t n = read(fd, byte, 1);
    if (n == 1)
        return 1;
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return 0;
    return -1;
}

static int recv_packet(hal_packet_t *pkt, int overall_timeout_ms)
{
    hal_rx_t rx;
    hal_rx_reset(&rx);

    int elapsed = 0;
    while (elapsed < overall_timeout_ms) {
        uint8_t b;
        int step = (overall_timeout_ms - elapsed) > IO_TIMEOUT_MS
                       ? IO_TIMEOUT_MS
                       : (overall_timeout_ms - elapsed);
        int r = read_byte_timeout(g_fd, &b, step);
        if (r < 0)
            return HAL_ERR_RECV;
        if (r == 0) {
            elapsed += step;
            continue;
        }
        int complete = hal_rx_feed(&rx, b, pkt);
        if (complete < 0)
            return HAL_ERR_PROTOCOL;
        if (complete == 1)
            return HAL_OK;
    }
    return HAL_ERR_TIMEOUT;
}

static int send_cmd(uint8_t code, const uint8_t *payload, uint8_t plen)
{
    uint8_t buf[HAL_PKT_MAX_SIZE];
    size_t len = 0;
    int ret = hal_pkt_build(code, payload, plen, buf, sizeof(buf), &len);
    if (ret != HAL_OK)
        return ret;
    ssize_t w = write(g_fd, buf, len);
    if (w != (ssize_t)len)
        return HAL_ERR_SEND;
    return HAL_OK;
}

static int wait_result(uint8_t *matched_id, int timeout_ms)
{
    hal_packet_t pkt;
    int deadline = timeout_ms;

    while (deadline > 0) {
        int chunk = deadline > IO_TIMEOUT_MS ? IO_TIMEOUT_MS : deadline;
        int ret = recv_packet(&pkt, chunk);
        if (ret == HAL_ERR_TIMEOUT) {
            deadline -= chunk;
            continue;
        }
        if (ret != HAL_OK)
            return ret;

        if (pkt.code >= HAL_EVT_WAITING_FINGER && pkt.code <= HAL_EVT_PROCESSING) {
            hal_emit_event(pkt.code);
            continue;
        }

        if (pkt.code == HAL_RES_KEY_DATA)
            continue;

        if (pkt.code == HAL_RES_SUCCESS) {
            if (matched_id && pkt.len >= 1)
                *matched_id = pkt.payload[0];
            g_key_window_open = 1;
            return HAL_OK;
        }

        return hal_res_to_error(pkt.code);
    }
    (void)send_cmd(HAL_CMD_CANCEL, NULL, 0);
    return HAL_ERR_TIMEOUT;
}

static int serial_init(const char *path)
{
    if (!path || !*path)
        return HAL_ERR_INVALID_ARG;

    int fd = open(path, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("[HAL serial] open");
        return HAL_ERR_CONNECT;
    }
    if (serial_configure(fd) < 0) {
        close(fd);
        return HAL_ERR_CONNECT;
    }

    g_fd = fd;
    g_initialized = 1;
    g_key_window_open = 0;
    fprintf(stderr, "[HAL] Serial backend: %s\n", path);
    return HAL_OK;
}

static void serial_cleanup(void)
{
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
    g_initialized = 0;
    g_key_window_open = 0;
}

static int serial_verify(int *matched_id_out)
{
    if (!g_initialized)
        return HAL_ERR_CONNECT;
    g_key_window_open = 0;

    int ret = send_cmd(HAL_CMD_VERIFY_START, NULL, 0);
    if (ret != HAL_OK)
        return ret;

    uint8_t mid = 0;
    ret = wait_result(&mid, VERIFY_TIMEOUT_MS);
    if (ret != HAL_OK)
        return ret;
    *matched_id_out = (int)mid;
    return HAL_OK;
}

static int serial_get_key(uint8_t *key_out)
{
    if (!g_initialized)
        return HAL_ERR_CONNECT;
    if (!g_key_window_open)
        return HAL_ERR_PROTOCOL;

    int ret = send_cmd(HAL_CMD_GET_KEY, NULL, 0);
    if (ret != HAL_OK)
        return ret;

    hal_packet_t pkt;
    ret = recv_packet(&pkt, GET_KEY_WINDOW_MS);
    g_key_window_open = 0;
    if (ret != HAL_OK)
        return ret;

    if (pkt.code != HAL_RES_KEY_DATA || pkt.len != HAL_KEY_SIZE)
        return HAL_ERR_PROTOCOL;

    memcpy(key_out, pkt.payload, HAL_KEY_SIZE);
    return HAL_OK;
}

static int serial_cancel(void)
{
    if (!g_initialized)
        return HAL_ERR_CONNECT;
    (void)send_cmd(HAL_CMD_CANCEL, NULL, 0);
    g_key_window_open = 0;
    return HAL_OK;
}

static int serial_enroll(int finger_id, const char *name)
{
    (void)name;
    if (!g_initialized)
        return HAL_ERR_CONNECT;
    if (finger_id < 1 || finger_id > HAL_MAX_FINGERS)
        return HAL_ERR_INVALID_ARG;

    uint8_t pl[1] = { (uint8_t)finger_id };
    int ret = send_cmd(HAL_CMD_ENROLL_START, pl, 1);
    if (ret != HAL_OK)
        return ret;

    uint8_t mid = 0;
    ret = wait_result(&mid, ENROLL_TIMEOUT_MS);
    if (ret == HAL_OK && mid != (uint8_t)finger_id)
        return HAL_ERR_PROTOCOL;
    return ret;
}

static int serial_delete(int finger_id)
{
    if (!g_initialized)
        return HAL_ERR_CONNECT;
    if (finger_id < 1 || finger_id > HAL_MAX_FINGERS)
        return HAL_ERR_INVALID_ARG;

    uint8_t pl[1] = { (uint8_t)finger_id };
    int ret = send_cmd(HAL_CMD_DELETE, pl, 1);
    if (ret != HAL_OK)
        return ret;
    uint8_t mid = 0;
    return wait_result(&mid, IO_TIMEOUT_MS * 5);
}

static int serial_list(hal_finger_info_t *fingers, int max_count, int *out_count)
{
    if (!out_count)
        return HAL_ERR_INVALID_ARG;
    *out_count = 0;
    if (!fingers || max_count <= 0)
        return HAL_OK;

    int ret = send_cmd(HAL_CMD_LIST_ENROLLED, NULL, 0);
    if (ret != HAL_OK)
        return ret;

    hal_packet_t pkt;
    ret = recv_packet(&pkt, VERIFY_TIMEOUT_MS);
    if (ret != HAL_OK)
        return ret;

    if (pkt.code != HAL_RES_SUCCESS || pkt.len < 1)
        return HAL_ERR_PROTOCOL;

    const uint8_t count = pkt.payload[0];
    int n = 0;
    for (uint8_t i = 0; i < count && (size_t)(1 + i) < pkt.len; i++) {
        if (n >= max_count)
            break;
        const int fid = (int)pkt.payload[1 + i];
        if (fid < 1 || fid > HAL_MAX_FINGERS)
            continue;
        fingers[n].finger_id = fid;
        snprintf(fingers[n].name, sizeof(fingers[n].name), "Finger_%d", fid);
        n++;
    }
    *out_count = n;
    return HAL_OK;
}

static int serial_status(hal_status_t *status)
{
    if (!status)
        return HAL_ERR_INVALID_ARG;
    memset(status, 0, sizeof(*status));
    status->state = (g_initialized && g_fd >= 0) ? HAL_STATE_CONNECTED : HAL_STATE_DISCONNECTED;
    return HAL_OK;
}

const hal_backend_t hal_serial_backend = {
    .init = serial_init,
    .cleanup = serial_cleanup,
    .verify = serial_verify,
    .get_key = serial_get_key,
    .cancel = serial_cancel,
    .enroll = serial_enroll,
    .delete_finger = serial_delete,
    .list = serial_list,
    .status = serial_status,
};
