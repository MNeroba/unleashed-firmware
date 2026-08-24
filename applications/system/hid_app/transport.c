#include "hid.h"

static bool hid_transport_is_wireless(const Hid* instance) {
    return instance->transport == HidTransportWireless;
}

void hid_hal_keyboard_press(Hid* instance, uint16_t event) {
    furi_assert(instance);
    if(hid_transport_is_wireless(instance)) {
        hid_hal_ble_keyboard_press(instance, event);
    } else {
        hid_hal_usb_keyboard_press(instance, event);
    }
}

void hid_hal_keyboard_release(Hid* instance, uint16_t event) {
    furi_assert(instance);
    if(hid_transport_is_wireless(instance)) {
        hid_hal_ble_keyboard_release(instance, event);
    } else {
        hid_hal_usb_keyboard_release(instance, event);
    }
}

void hid_hal_keyboard_release_all(Hid* instance) {
    furi_assert(instance);
    if(hid_transport_is_wireless(instance)) {
        hid_hal_ble_keyboard_release_all(instance);
    } else {
        hid_hal_usb_keyboard_release_all(instance);
    }
}

void hid_hal_consumer_key_press(Hid* instance, uint16_t event) {
    furi_assert(instance);
    if(hid_transport_is_wireless(instance)) {
        hid_hal_ble_consumer_key_press(instance, event);
    } else {
        hid_hal_usb_consumer_key_press(instance, event);
    }
}

void hid_hal_consumer_key_release(Hid* instance, uint16_t event) {
    furi_assert(instance);
    if(hid_transport_is_wireless(instance)) {
        hid_hal_ble_consumer_key_release(instance, event);
    } else {
        hid_hal_usb_consumer_key_release(instance, event);
    }
}

void hid_hal_consumer_key_release_all(Hid* instance) {
    furi_assert(instance);
    if(hid_transport_is_wireless(instance)) {
        hid_hal_ble_consumer_key_release_all(instance);
    } else {
        hid_hal_usb_consumer_key_release_all(instance);
    }
}

void hid_hal_mouse_move(Hid* instance, int8_t dx, int8_t dy) {
    furi_assert(instance);
    if(hid_transport_is_wireless(instance)) {
        hid_hal_ble_mouse_move(instance, dx, dy);
    } else {
        hid_hal_usb_mouse_move(instance, dx, dy);
    }
}

void hid_hal_mouse_scroll(Hid* instance, int8_t delta) {
    furi_assert(instance);
    if(hid_transport_is_wireless(instance)) {
        hid_hal_ble_mouse_scroll(instance, delta);
    } else {
        hid_hal_usb_mouse_scroll(instance, delta);
    }
}

void hid_hal_mouse_press(Hid* instance, uint16_t event) {
    furi_assert(instance);
    if(hid_transport_is_wireless(instance)) {
        hid_hal_ble_mouse_press(instance, event);
    } else {
        hid_hal_usb_mouse_press(instance, event);
    }
}

void hid_hal_mouse_release(Hid* instance, uint16_t event) {
    furi_assert(instance);
    if(hid_transport_is_wireless(instance)) {
        hid_hal_ble_mouse_release(instance, event);
    } else {
        hid_hal_usb_mouse_release(instance, event);
    }
}

void hid_hal_mouse_release_all(Hid* instance) {
    furi_assert(instance);
    if(hid_transport_is_wireless(instance)) {
        hid_hal_ble_mouse_release_all(instance);
    } else {
        hid_hal_usb_mouse_release_all(instance);
    }
}
