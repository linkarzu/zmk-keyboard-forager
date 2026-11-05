/*
 * Lets the Mac read whether the Windows layer is on, which ZMK doesn't tell a
 * host. Exposes it as a Bluetooth characteristic, 1 on Windows and 0 on macOS,
 * read by keyboard_battery.swift for the sketchybar keyboard widget in
 * dotfiles-latest. It only checks the Windows layer because layer 0 is always
 * active: this way holding other layers, like Symb or Apps, sends nothing over
 * the radio.
 */

#include <sys/types.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

// LAYER_Windows in the keymap.
#define WINDOWS_LAYER 1

// The second generated attribute is the one used to send GATT notifications.
#define LAYER_REPORT_ATTR_NOTIFY_IDX 1

static struct bt_uuid_128 layer_report_service_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x1300d6b0, 0x6b7b, 0x4845, 0xbc0e, 0xf39462875a3e));
static struct bt_uuid_128 layer_report_value_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x557a1190, 0xe35b, 0x448e, 0xa7cb, 0xfe6352d6021a));

static uint8_t last_value;

static ssize_t read_layer_report(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                                 uint16_t len, uint16_t offset) {
    uint8_t value = zmk_keymap_layer_active(WINDOWS_LAYER) ? 1 : 0;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, &value, sizeof(value));
}

BT_GATT_SERVICE_DEFINE(layer_report, BT_GATT_PRIMARY_SERVICE(&layer_report_service_uuid),
                       BT_GATT_CHARACTERISTIC(&layer_report_value_uuid.uuid,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ, read_layer_report, NULL, NULL),
                       BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

static int layer_report_listener(const zmk_event_t *eh) {
    uint8_t value = zmk_keymap_layer_active(WINDOWS_LAYER) ? 1 : 0;

    if (value == last_value) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    last_value = value;

    int rc = bt_gatt_notify(NULL, &layer_report.attrs[LAYER_REPORT_ATTR_NOTIFY_IDX], &value,
                            sizeof(value));
    if (rc < 0 && rc != -ENOTCONN) {
        LOG_WRN("Failed to notify hosts of the layer: %d", rc);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(layer_report, layer_report_listener);
ZMK_SUBSCRIPTION(layer_report, zmk_layer_state_changed);
