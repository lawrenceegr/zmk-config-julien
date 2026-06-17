/*
 * Julien Keyboard Configurator Protocol v1
 * Copyright (c) 2026 Lawrence Langat <lawrencelangatmi@gmail.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>

LOG_MODULE_REGISTER(configurator, CONFIG_ZMK_LOG_LEVEL);

/* ── Compat shim ─────────────────────────────────────────────────────────── */
/*
 * zmk_keymap_num_layers() does not exist in this branch.
 * Replace with whatever your keymap.h exposes. Run:
 *   grep -E "num_layer|layers_len|LAYERS_LEN" ~/zmk/app/include/zmk/keymap.h
 * and update this define accordingly.
 */
static inline int configurator_num_layers(void)
{
    /* Walk layers until we find one that is not enabled.
     * zmk_keymap_layer_exists() is safe to call with any index. */
    int n = 0;
    while (zmk_keymap_layer_index_to_id(n) != ZMK_KEYMAP_LAYER_ID_INVAL) {
        n++;
    }
    return n;
}

/* ── CDC-ACM device ─────────────────────────────────────────────────────── */
#define CONFIGURATOR_UART_NODE DT_CHOSEN(zmk_configurator_uart)
static const struct device *uart_dev;

/* ── Protocol constants ──────────────────────────────────────────────────── */
#define PROTO_VERSION    1
#define MAX_LINE         512
#define MAX_BINDING_LEN  64
#define ROWS             6
#define COLS             17
#define MAX_KEYS         88

/* ── RX line buffer ──────────────────────────────────────────────────────── */
static char rx_buf[MAX_LINE];
static int  rx_pos;

/* ── TX helpers ──────────────────────────────────────────────────────────── */
static void uart_send(const char *s)
{
    while (*s) {
        uart_poll_out(uart_dev, *s++);
    }
}

static void send_fmt(const char *fmt, ...)
{
    char buf[MAX_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    uart_send(buf);
    uart_poll_out(uart_dev, '\n');
}

/* ── Unsaved-changes flag ────────────────────────────────────────────────── */
static bool unsaved = false;

/* ── Simple string parser (replaces sscanf — not in Zephyr minimal libc) ── */
/*
 * Splits "token1 token2 token3" into up to 3 parts.
 * Returns number of tokens found.
 */
static int split_words(const char *src, char *w0, size_t l0,
                       char *w1, size_t l1,
                       char *w2, size_t l2)
{
    int count = 0;
    char *dst[] = {w0, w1, w2};
    size_t lens[] = {l0, l1, l2};

    while (*src == ' ') src++;

    for (int i = 0; i < 3 && *src; i++) {
        int j = 0;
        while (*src && *src != ' ' && j < (int)lens[i] - 1) {
            dst[i][j++] = *src++;
        }
        dst[i][j] = '\0';
        count++;
        while (*src == ' ') src++;
    }
    return count;
}

/* ── Binding serialiser ──────────────────────────────────────────────────── */
static void binding_to_str(const struct zmk_behavior_binding *b, char *out, size_t len)
{
    const char *name = b->behavior_dev;

    if (!name || name[0] == '\0') {
        snprintf(out, len, "&none");
        return;
    }
    if (strstr(name, "trans"))         { snprintf(out, len, "&trans");             return; }
    if (strstr(name, "none"))          { snprintf(out, len, "&none");              return; }
    if (strstr(name, "studio"))        { snprintf(out, len, "&studio_unlock");     return; }
    if (strstr(name, "rgb"))           { snprintf(out, len, "&rgb_ug %u", b->param1); return; }
    if (strstr(name, "layer_tap"))     { snprintf(out, len, "&lt %u %u", b->param1, b->param2); return; }
    if (strstr(name, "momentary"))     { snprintf(out, len, "&mo %u", b->param1); return; }
    /* &kp fallback */
    snprintf(out, len, "&kp %u", b->param1);
}

/* ── Command handlers ────────────────────────────────────────────────────── */

static void cmd_hello(int id)
{
    send_fmt("{\"id\":%d,\"ok\":true,\"data\":{"
             "\"device\":\"Julien Keyboard\","
             "\"protocol\":%d,"
             "\"rows\":%d,"
             "\"cols\":%d,"
             "\"keys\":%d,"
             "\"layers\":%d"
             "}}",
             id, PROTO_VERSION, ROWS, COLS, MAX_KEYS,
             configurator_num_layers());
}

static void cmd_get_keymap(int id)
{
    int layer_count = configurator_num_layers();
    char out[MAX_LINE];
    int  pos = 0;

    pos += snprintf(out + pos, sizeof(out) - pos,
                    "{\"id\":%d,\"ok\":true,\"data\":{\"layers\":[", id);

    for (int l = 0; l < layer_count && pos < (int)sizeof(out) - 10; l++) {
        const char *lname = zmk_keymap_layer_name(l);
        pos += snprintf(out + pos, sizeof(out) - pos,
                        "%s{\"index\":%d,\"name\":\"%s\",\"bindings\":[",
                        l > 0 ? "," : "", l, lname ? lname : "");

        for (int k = 0; k < MAX_KEYS && pos < (int)sizeof(out) - 32; k++) {
            const struct zmk_behavior_binding *b =
                zmk_keymap_get_layer_binding_at_idx(l, k);
            char bstr[MAX_BINDING_LEN];
            if (b) {
                binding_to_str(b, bstr, sizeof(bstr));
            } else {
                snprintf(bstr, sizeof(bstr), "&none");
            }
            pos += snprintf(out + pos, sizeof(out) - pos,
                            "%s\"%s\"", k > 0 ? "," : "", bstr);
        }
        pos += snprintf(out + pos, sizeof(out) - pos, "]}");
    }

    pos += snprintf(out + pos, sizeof(out) - pos, "]}}");
    uart_send(out);
    uart_poll_out(uart_dev, '\n');
}

static void cmd_set_key(int id, int layer, int position, const char *binding)
{
    if (layer < 0 || layer >= configurator_num_layers()) {
        send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"OUT_OF_RANGE\"}", id);
        return;
    }
    if (position < 0 || position >= MAX_KEYS) {
        send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"OUT_OF_RANGE\"}", id);
        return;
    }

    /* Parse "&behavior param1 param2" without sscanf */
    const char *src = (binding[0] == '&') ? binding + 1 : binding;
    char name_buf[MAX_BINDING_LEN], p1_s[16] = "0", p2_s[16] = "0";
    name_buf[0] = '\0';
    int found = split_words(src, name_buf, sizeof(name_buf),
                                 p1_s,    sizeof(p1_s),
                                 p2_s,    sizeof(p2_s));
    if (found < 1 || name_buf[0] == '\0') {
        send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"INVALID_ARGS\"}", id);
        return;
    }

    static char dev_name[MAX_BINDING_LEN];
    snprintf(dev_name, sizeof(dev_name), "&%s", name_buf);

    struct zmk_behavior_binding b = {
        .behavior_dev = dev_name,
        .param1 = (uint32_t)atoi(p1_s),
        .param2 = (uint32_t)atoi(p2_s),
    };

    int rc = zmk_keymap_set_layer_binding_at_idx(layer, position, b);
    if (rc < 0) {
        send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"INVALID_ARGS\"}", id);
        return;
    }

    unsaved = true;
    send_fmt("{\"id\":%d,\"ok\":true}", id);
}

static void cmd_add_layer(int id, const char *name)
{
    int rc = zmk_keymap_add_layer();
    if (rc < 0) {
        send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"MAX_LAYERS\"}", id);
        return;
    }
    int new_idx = configurator_num_layers() - 1;
    if (name && name[0] != '\0') {
        zmk_keymap_set_layer_name(new_idx, name, strlen(name) + 1);
    }
    unsaved = true;
    send_fmt("{\"id\":%d,\"ok\":true,\"data\":{\"index\":%d}}", id, new_idx);
}

static void cmd_remove_layer(int id, int index)
{
    if (index == 0) {
        send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"LAYER_PROTECTED\"}", id);
        return;
    }
    int rc = zmk_keymap_remove_layer(index);
    if (rc < 0) {
        send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"OUT_OF_RANGE\"}", id);
        return;
    }
    unsaved = true;
    send_fmt("{\"id\":%d,\"ok\":true}", id);
}

static void cmd_set_layer_name(int id, int index, const char *name)
{
    if (index < 0 || index >= configurator_num_layers()) {
        send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"OUT_OF_RANGE\"}", id);
        return;
    }
    zmk_keymap_set_layer_name(index, name, strlen(name) + 1);
    unsaved = true;
    send_fmt("{\"id\":%d,\"ok\":true}", id);
}

static void cmd_get_rgb(int id)
{
    /* Stubbed. Wire up zmk_rgb_underglow_get_state() when ready. */
    send_fmt("{\"id\":%d,\"ok\":true,\"data\":{"
             "\"enabled\":true,\"effect\":0,\"hue\":128,\"sat\":255,\"brt\":180"
             "}}",
             id);
}

static void cmd_set_rgb(int id, int hue, int sat, int brt, int effect)
{
    ARG_UNUSED(hue); ARG_UNUSED(sat); ARG_UNUSED(brt); ARG_UNUSED(effect);
    unsaved = true;
    send_fmt("{\"id\":%d,\"ok\":true}", id);
}

static void cmd_get_status(int id)
{
    send_fmt("{\"id\":%d,\"ok\":true,\"data\":{\"unsaved\":%s}}",
             id, unsaved ? "true" : "false");
}

static void cmd_save(int id)
{
    int rc = settings_save();
    if (rc < 0) {
        send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"SAVE_FAILED\"}", id);
        return;
    }
    unsaved = false;
    send_fmt("{\"id\":%d,\"ok\":true}", id);
}

static void cmd_discard(int id)
{
    settings_load();
    unsaved = false;
    send_fmt("{\"id\":%d,\"ok\":true}", id);
}

/* ── Minimal JSON field extractor ───────────────────────────────────────── */
static char *json_get(char *buf, const char *key, char *val, size_t val_len)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    char *p = strstr(buf, search);
    if (!p) return NULL;
    p += strlen(search);
    while (*p == ' ') p++;

    if (*p == '"') {
        p++;
        char *end = strchr(p, '"');
        if (!end) return NULL;
        size_t n = MIN((size_t)(end - p), val_len - 1);
        memcpy(val, p, n);
        val[n] = '\0';
        return val;
    } else {
        char *end = p;
        while (*end && *end != ',' && *end != '}' && *end != ']') end++;
        size_t n = MIN((size_t)(end - p), val_len - 1);
        memcpy(val, p, n);
        val[n] = '\0';
        return val;
    }
}

/* ── Line dispatcher ─────────────────────────────────────────────────────── */
static void dispatch(char *line)
{
    char val[MAX_BINDING_LEN];
    int id = 0;

    if (json_get(line, "id", val, sizeof(val))) {
        id = atoi(val);
    }

    char cmd[32] = {0};
    if (!json_get(line, "cmd", cmd, sizeof(cmd))) {
        send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"UNKNOWN_CMD\"}", id);
        return;
    }

    if      (strcmp(cmd, "HELLO")          == 0) { cmd_hello(id); }
    else if (strcmp(cmd, "GET_KEYMAP")     == 0) { cmd_get_keymap(id); }
    else if (strcmp(cmd, "GET_STATUS")     == 0) { cmd_get_status(id); }
    else if (strcmp(cmd, "GET_RGB")        == 0) { cmd_get_rgb(id); }
    else if (strcmp(cmd, "SAVE")           == 0) { cmd_save(id); }
    else if (strcmp(cmd, "DISCARD")        == 0) { cmd_discard(id); }

    else if (strcmp(cmd, "SET_KEY") == 0) {
        char layer_s[8], pos_s[8], binding[MAX_BINDING_LEN];
        if (!json_get(line, "layer",    layer_s, sizeof(layer_s))  ||
            !json_get(line, "position", pos_s,   sizeof(pos_s))    ||
            !json_get(line, "binding",  binding, sizeof(binding))) {
            send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"INVALID_ARGS\"}", id);
            return;
        }
        cmd_set_key(id, atoi(layer_s), atoi(pos_s), binding);

    } else if (strcmp(cmd, "ADD_LAYER") == 0) {
        char name[32] = {0};
        json_get(line, "name", name, sizeof(name));
        cmd_add_layer(id, name);

    } else if (strcmp(cmd, "REMOVE_LAYER") == 0) {
        char idx_s[8];
        if (!json_get(line, "index", idx_s, sizeof(idx_s))) {
            send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"INVALID_ARGS\"}", id);
            return;
        }
        cmd_remove_layer(id, atoi(idx_s));

    } else if (strcmp(cmd, "SET_LAYER_NAME") == 0) {
        char idx_s[8], name[32];
        if (!json_get(line, "index", idx_s, sizeof(idx_s)) ||
            !json_get(line, "name",  name,  sizeof(name))) {
            send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"INVALID_ARGS\"}", id);
            return;
        }
        cmd_set_layer_name(id, atoi(idx_s), name);

    } else if (strcmp(cmd, "SET_RGB") == 0) {
        char h[8] = "-1", s[8] = "-1", b[8] = "-1", e[8] = "-1";
        json_get(line, "hue",    h, sizeof(h));
        json_get(line, "sat",    s, sizeof(s));
        json_get(line, "brt",    b, sizeof(b));
        json_get(line, "effect", e, sizeof(e));
        cmd_set_rgb(id, atoi(h), atoi(s), atoi(b), atoi(e));

    } else {
        send_fmt("{\"id\":%d,\"ok\":false,\"err\":\"UNKNOWN_CMD\"}", id);
    }
}

/* ── UART RX ISR ─────────────────────────────────────────────────────────── */
static void uart_cb(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);
    if (!uart_irq_update(dev) || !uart_irq_rx_ready(dev)) return;

    uint8_t c;
    while (uart_fifo_read(dev, &c, 1) == 1) {
        if (c == '\r') continue;
        if (c == '\n') {
            rx_buf[rx_pos] = '\0';
            if (rx_pos > 0) dispatch(rx_buf);
            rx_pos = 0;
        } else if (rx_pos < MAX_LINE - 1) {
            rx_buf[rx_pos++] = c;
        }
    }
}

/* ── Init ────────────────────────────────────────────────────────────────── */
static int configurator_init(void)
{
    uart_dev = DEVICE_DT_GET(CONFIGURATOR_UART_NODE);
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("Configurator UART not ready");
        return -ENODEV;
    }
    uart_irq_callback_set(uart_dev, uart_cb);
    uart_irq_rx_enable(uart_dev);
    LOG_INF("Configurator ready on %s", uart_dev->name);
    return 0;
}

SYS_INIT(configurator_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
