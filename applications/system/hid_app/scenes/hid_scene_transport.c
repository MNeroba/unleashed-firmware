#include "../hid.h"
#include "../views.h"

enum HidTransportMenuIndex {
    HidTransportMenuIndexWired,
    HidTransportMenuIndexWireless,
    HidTransportMenuIndexError,
};

static void hid_scene_transport_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void hid_scene_transport_error_callback(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, HidTransportMenuIndexError);
}

static void hid_scene_transport_show_error(Hid* app) {
    popup_reset(app->popup);
    popup_set_header(app->popup, "Unable to start", 64, 20, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Check the selected\nconnection", 64, 38, AlignCenter, AlignTop);
    popup_set_timeout(app->popup, 1500);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, hid_scene_transport_error_callback);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
}

void hid_scene_transport_on_enter(void* context) {
    Hid* app = context;

    submenu_set_header(app->submenu, "Jiggler connection");
    submenu_add_item(
        app->submenu,
        "Wired (USB)",
        HidTransportMenuIndexWired,
        hid_scene_transport_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Wireless (Bluetooth)",
        HidTransportMenuIndexWireless,
        hid_scene_transport_submenu_callback,
        app);
    submenu_set_selected_item(app->submenu, app->transport);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool hid_scene_transport_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == HidTransportMenuIndexError) {
            popup_reset(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
        } else {
            HidTransport transport = event.event == HidTransportMenuIndexWired ?
                                         HidTransportWired :
                                         HidTransportWireless;

            app->transport_restore = app->transport;
            if(!app->transport_started || app->transport != transport) {
                hid_transport_stop(app);
                if(!hid_transport_start(app, transport)) {
                    if(!hid_transport_start(app, app->transport_restore)) {
                        FURI_LOG_E("HID", "Failed to restore the previous transport");
                    }
                    hid_scene_transport_show_error(app);
                    return consumed;
                }
            }

            app->transport_restore_pending = true;
            if(scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, HidSceneStart)) {
                scene_manager_set_scene_state(
                    app->scene_manager, HidSceneMain, app->transport_target_view);
                scene_manager_next_scene(app->scene_manager, HidSceneMain);
            } else {
                app->transport_restore_pending = false;
                FURI_LOG_E("HID", "Unable to return to the HID menu");
            }
        }
    }

    return consumed;
}

void hid_scene_transport_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
    popup_reset(app->popup);
}
