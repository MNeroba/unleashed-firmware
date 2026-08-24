#include "hid.h"
#include <extra_profiles/hid_profile.h>
#include <profiles/serial_profile.h>
#include "views.h"
#include <notification/notification_messages.h>
#include <dolphin/dolphin.h>
#include <flipper_format/flipper_format.h>

#define TAG "HidApp"

#define HID_BT_CFG_PATH      APP_DATA_PATH(".bt_hid.cfg")
#define HID_BT_CFG_FILE_TYPE "Flipper BT Remote Settings File"
#define HID_BT_CFG_VERSION   1
#define HID_BT_DEFAULT_NAME  "Wireless Mouse"

bool hid_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Hid* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

bool hid_back_event_callback(void* context) {
    furi_assert(context);
    Hid* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void bt_hid_remove_pairing(Hid* app) {
    Bt* bt = app->bt;
    bt_disconnect(bt);

    // Wait 2nd core to update nvm storage
    furi_delay_ms(200);

    furi_hal_bt_stop_advertising();

    bt_forget_bonded_devices(bt);

    furi_hal_bt_start_advertising();
}

static void bt_hid_load_cfg(Hid* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);

    FuriString* temp_str = furi_string_alloc();
    uint32_t temp_uint = 0;

    strlcpy(app->ble_hid_cfg.name, HID_BT_DEFAULT_NAME, sizeof(app->ble_hid_cfg.name));

    do {
        if(!flipper_format_file_open_existing(fff, HID_BT_CFG_PATH)) break;

        if(!flipper_format_read_header(fff, temp_str, &temp_uint)) break;
        if((strcmp(furi_string_get_cstr(temp_str), HID_BT_CFG_FILE_TYPE) != 0) ||
           (temp_uint != HID_BT_CFG_VERSION))
            break;

        if(flipper_format_read_string(fff, "name", temp_str)) {
            strlcpy(
                app->ble_hid_cfg.name,
                furi_string_get_cstr(temp_str),
                sizeof(app->ble_hid_cfg.name));
        } else {
            flipper_format_rewind(fff);
        }

    } while(0);

    furi_string_free(temp_str);

    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);
}

void bt_hid_save_cfg(Hid* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_always(fff, HID_BT_CFG_PATH)) {
        do {
            if(!flipper_format_write_header_cstr(fff, HID_BT_CFG_FILE_TYPE, HID_BT_CFG_VERSION))
                break;
            if(!flipper_format_write_string_cstr(fff, "name", app->ble_hid_cfg.name)) break;
        } while(0);
    }

    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);
}

static void hid_set_transport_status(Hid* hid, bool wireless, bool connected) {
    if(wireless) {
        notification_internal_message(
            hid->notifications, connected ? &sequence_set_blue_255 : &sequence_reset_blue);
    }
    hid_keynote_set_connected_status(hid->hid_keynote, connected, wireless);
    hid_keyboard_set_connected_status(hid->hid_keyboard, connected, wireless);
    hid_numpad_set_connected_status(hid->hid_numpad, connected, wireless);
    hid_media_set_connected_status(hid->hid_media, connected, wireless);
    hid_music_macos_set_connected_status(hid->hid_music_macos, connected, wireless);
    hid_movie_set_connected_status(hid->hid_movie, connected, wireless);
    hid_mouse_set_connected_status(hid->hid_mouse, connected, wireless);
    hid_mouse_clicker_set_connected_status(hid->hid_mouse_clicker, connected, wireless);
    hid_mouse_jiggler_set_connected_status(hid->hid_mouse_jiggler, connected, wireless);
    hid_mouse_jiggler_stealth_set_connected_status(
        hid->hid_mouse_jiggler_stealth, connected, wireless);
    hid_ptt_set_connected_status(hid->hid_ptt, connected, wireless);
    hid_tiktok_set_connected_status(hid->hid_tiktok, connected, wireless);
}

static void bt_hid_connection_status_changed_callback(BtStatus status, void* context) {
    furi_assert(context);
    Hid* hid = context;
    hid_set_transport_status(hid, true, status == BtStatusConnected);
}

static uint32_t hid_ptt_menu_view(void* context) {
    UNUSED(context);
    return HidViewPushToTalkMenu;
}

Hid* hid_alloc() {
    Hid* app = malloc(sizeof(Hid));
    app->ble_hid_profile = NULL;
    app->transport = HidTransportWired;
    app->transport_restore = HidTransportWired;
    app->transport_target_view = HidViewSubmenu;
    app->usb_mode_prev = NULL;
    app->transport_started = false;
    app->transport_restore_pending = false;
    app->transport_restore_deferred = false;

    // Gui
    app->gui = furi_record_open(RECORD_GUI);

    // Bt
    app->bt = furi_record_open(RECORD_BT);

    // Notifications
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // View dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, hid_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, hid_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Scene Manager
    app->scene_manager = scene_manager_alloc(&hid_scene_handlers, app);

    // Device Type Submenu view
    app->submenu = submenu_alloc();

    view_dispatcher_add_view(app->view_dispatcher, HidViewSubmenu, submenu_get_view(app->submenu));

    // Dialog view
    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(app->view_dispatcher, HidViewDialog, dialog_ex_get_view(app->dialog));

    // Text input
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewTextInput, text_input_get_view(app->text_input));

    // Popup view
    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, HidViewPopup, popup_get_view(app->popup));

    // Keynote view
    app->hid_keynote = hid_keynote_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewKeynote, hid_keynote_get_view(app->hid_keynote));

    // Keyboard view
    app->hid_keyboard = hid_keyboard_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewKeyboard, hid_keyboard_get_view(app->hid_keyboard));

    //Numpad keyboard view
    app->hid_numpad = hid_numpad_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewNumpad, hid_numpad_get_view(app->hid_numpad));

    // Media view
    app->hid_media = hid_media_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewMedia, hid_media_get_view(app->hid_media));

    // Music MacOs view
    app->hid_music_macos = hid_music_macos_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewMusicMacOs, hid_music_macos_get_view(app->hid_music_macos));

    // Movie view
    app->hid_movie = hid_movie_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewMovie, hid_movie_get_view(app->hid_movie));

    // TikTok view
    app->hid_tiktok = hid_tiktok_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, BtHidViewTikTok, hid_tiktok_get_view(app->hid_tiktok));

    // Mouse view
    app->hid_mouse = hid_mouse_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewMouse, hid_mouse_get_view(app->hid_mouse));

    // Mouse clicker view
    app->hid_mouse_clicker = hid_mouse_clicker_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewMouseClicker,
        hid_mouse_clicker_get_view(app->hid_mouse_clicker));

    // Mouse jiggler view
    app->hid_mouse_jiggler = hid_mouse_jiggler_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewMouseJiggler,
        hid_mouse_jiggler_get_view(app->hid_mouse_jiggler));
    // Mouse jiggler stealth view
    app->hid_mouse_jiggler_stealth = hid_mouse_jiggler_stealth_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewMouseJigglerStealth,
        hid_mouse_jiggler_stealth_get_view(app->hid_mouse_jiggler_stealth));

    // PushToTalk view
    app->hid_ptt_menu = hid_ptt_menu_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewPushToTalkMenu, hid_ptt_menu_get_view(app->hid_ptt_menu));
    app->hid_ptt = hid_ptt_alloc(app);
    view_set_previous_callback(hid_ptt_get_view(app->hid_ptt), hid_ptt_menu_view);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewPushToTalk, hid_ptt_get_view(app->hid_ptt));

    return app;
}

void hid_free(Hid* app) {
    furi_assert(app);

    // Reset notification
    if(app->transport == HidTransportWireless) {
        notification_internal_message(app->notifications, &sequence_reset_blue);
    }
    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, HidViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewDialog);
    dialog_ex_free(app->dialog);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewTextInput);
    text_input_free(app->text_input);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewPopup);
    popup_free(app->popup);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewKeynote);
    hid_keynote_free(app->hid_keynote);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewKeyboard);
    hid_keyboard_free(app->hid_keyboard);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewNumpad);
    hid_numpad_free(app->hid_numpad);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMedia);
    hid_media_free(app->hid_media);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMusicMacOs);
    hid_music_macos_free(app->hid_music_macos);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMovie);
    hid_movie_free(app->hid_movie);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMouse);
    hid_mouse_free(app->hid_mouse);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMouseClicker);
    hid_mouse_clicker_free(app->hid_mouse_clicker);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMouseJiggler);
    hid_mouse_jiggler_free(app->hid_mouse_jiggler);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMouseJigglerStealth);
    hid_mouse_jiggler_stealth_free(app->hid_mouse_jiggler_stealth);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewPushToTalkMenu);
    hid_ptt_menu_free(app->hid_ptt_menu);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewPushToTalk);
    hid_ptt_free(app->hid_ptt);
    view_dispatcher_remove_view(app->view_dispatcher, BtHidViewTikTok);
    hid_tiktok_free(app->hid_tiktok);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    // Close records
    furi_record_close(RECORD_GUI);
    app->gui = NULL;
    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;
    furi_record_close(RECORD_BT);
    app->bt = NULL;

    // Free rest
    free(app);
}

bool hid_transport_start(Hid* app, HidTransport transport) {
    furi_assert(app);
    furi_check(!app->transport_started);

    if(transport == HidTransportWired) {
        app->usb_mode_prev = furi_hal_usb_get_config();
        furi_hal_usb_unlock();
        if(!furi_hal_usb_set_config(&usb_hid, NULL)) {
            app->usb_mode_prev = NULL;
            return false;
        }

        app->transport = HidTransportWired;
        app->transport_started = true;
        hid_set_transport_status(app, false, true);
        FURI_LOG_D("HID", "Starting as USB transport");
    } else {
        bt_disconnect(app->bt);

        // Wait 2nd core to update nvm storage
        furi_delay_ms(200);

        // Migrate data from old sd-card folder
        Storage* storage = furi_record_open(RECORD_STORAGE);

        storage_common_migrate(
            storage,
            EXT_PATH("apps/Tools/" HID_BT_KEYS_STORAGE_NAME),
            APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME));

        bt_keys_storage_set_storage_path(app->bt, APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME));

        furi_record_close(RECORD_STORAGE);

        bt_hid_load_cfg(app);

        app->ble_hid_profile = bt_profile_start(app->bt, ble_profile_hid_ext, &app->ble_hid_cfg);
        if(!app->ble_hid_profile) {
            bt_keys_storage_set_default_path(app->bt);
            bt_profile_restore_default(app->bt);
            return false;
        }

        app->transport = HidTransportWireless;
        app->transport_started = true;
        hid_set_transport_status(app, true, false);
        bt_set_status_changed_callback(app->bt, bt_hid_connection_status_changed_callback, app);
        furi_hal_bt_start_advertising();
        FURI_LOG_D("HID", "Starting as Bluetooth transport");
    }

    dolphin_deed(DolphinDeedPluginStart);
    return true;
}

void hid_transport_stop(Hid* app) {
    furi_assert(app);
    if(!app->transport_started) return;

    if(app->transport == HidTransportWireless) {
        bt_set_status_changed_callback(app->bt, NULL, NULL);
        bt_disconnect(app->bt);

        // Wait 2nd core to update nvm storage
        furi_delay_ms(200);

        bt_keys_storage_set_default_path(app->bt);

        furi_check(bt_profile_restore_default(app->bt));
        app->ble_hid_profile = NULL;
    } else {
        furi_hal_usb_set_config(app->usb_mode_prev, NULL);
        app->usb_mode_prev = NULL;
    }

    app->transport_started = false;
}

static int32_t hid_app(void* p, HidTransport initial_transport) {
    UNUSED(p);
    Hid* app = hid_alloc();

    furi_check(hid_transport_start(app, initial_transport));
    scene_manager_next_scene(app->scene_manager, HidSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    hid_transport_stop(app);
    hid_free(app);

    return 0;
}

int32_t hid_usb_app(void* p) {
    return hid_app(p, HidTransportWired);
}

int32_t hid_ble_app(void* p) {
    return hid_app(p, HidTransportWireless);
}
