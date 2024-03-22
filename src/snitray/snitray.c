// SPDX-FileCopyrightText: © 2024 vetu104
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "snitray.h"

#include <stdint.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>
#include <libdbusmenu-glib/dbusmenu-glib.h>


typedef struct StatusNotifierItem_s StatusNotifierItem;

struct StatusNotifierItem_s {
    GDBusConnection* connection;
    char* bus_name;
    unsigned int owner_id;
    unsigned int registration_id;
    unsigned int watcher_id;
    DbusmenuMenuitem* menuroot;
    GDBusInterfaceInfo* interface_info;
    GDBusNodeInfo* nodeinfo;
    void* client;
};


void trayitem_registration_cb(GDBusConnection *connection, GAsyncResult *res,
        StatusNotifierItem *trayitem);

void watcher_appeared_handler(GDBusConnection *conn, const char *busname,
        const char *uniqname, StatusNotifierItem *trayitem);

void name_acquired_handler(GDBusConnection *connection, const char *name,
        StatusNotifierItem *trayitem);

void trayitem_method_call(GDBusConnection *connection, const char *sender,
        const char *object_path, const char *interface_name,
        const char *method_name, GVariant *parameters,
        GDBusMethodInvocation *invocation, StatusNotifierItem *trayitem);

GVariant* trayitem_get_property(GDBusConnection *connection,
        const char *sender,
        const char *object_path, const char *interface_name,
        const char *property_name, GError **error, void *user_data);

GDBusInterfaceInfo* get_interface_info(char *interface,
        StatusNotifierItem* trayitem);

void exit_signal(DbusmenuMenuitem *item, const char *event, GVariant *info,
        uint32_t time, StatusNotifierItem *trayitem);

void show_signal(DbusmenuMenuitem *item, const char *event, GVariant *info,
        uint32_t time, StatusNotifierItem *trayitem);

void build_menu(StatusNotifierItem *trayitem);
void trayitem_teardown(StatusNotifierItem *trayitem);

GDBusInterfaceVTable trayitem_interface_vtable = {
    (GDBusInterfaceMethodCallFunc) trayitem_method_call,
    (GDBusInterfaceGetPropertyFunc) trayitem_get_property,
    NULL
};


void
trayitem_method_call(GDBusConnection *connection, const char *sender,
        const char *object_path, const char *interface_name,
        const char *method_name, GVariant *parameters,
        GDBusMethodInvocation *invocation, StatusNotifierItem *trayitem)
{
    if (strcmp(method_name, "Activate") == 0) {
        SnitrayAppShow(trayitem->client);
    }
    else {
        g_dbus_method_invocation_return_dbus_error(invocation,
                "org.freedesktop.DBus.Error.UnknownMethod", "Unknown method");
    }
}


GVariant*
trayitem_get_property(GDBusConnection *conn, const char *sender,
        const char *object_path, const char *interface_name,
        const char *property_name, GError **err, void *user_data)
{
    if (strcmp(property_name, "Category") == 0) {
        GVariant *category = g_variant_new("s", "ApplicationStatus");
        return category;
    }
    else if (strcmp(property_name, "IconName") == 0) {
        GVariant *iconname = g_variant_new("s", BOLT_SNI_META_NAME);
        return iconname;
    }
    else if (strcmp(property_name, "Id") == 0) {
        GVariant *id = g_variant_new("s", "bolt");
        return id;
    }
    else if (strcmp(property_name, "ItemIsMenu") == 0) {
        GVariant *ismenu = g_variant_new("b", "FALSE");
        return ismenu;
    }
    else if (strcmp(property_name, "Menu") == 0) {
        GVariant *menu = g_variant_new("o", "/MenuBar");
        return menu;
    }
    else if (strcmp(property_name, "Status") == 0) {
        GVariant *status = g_variant_new("s", "Active");
        return status;
    }
    else if (strcmp(property_name, "Title") == 0) {
        GVariant *title = g_variant_new("s", BOLT_SNI_META_NAME);
        return title;
    }
    else if (strcmp(property_name, "WindowId") == 0) {
        GVariant *windowid = g_variant_new("i", 0);
        return windowid;
    }
    else {
        g_set_error(err, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
                "Property '%s' does not exist.", property_name);
        g_error_free(*err);
        return NULL;
    }
}


GDBusInterfaceInfo*
get_interface_info(char *interface, StatusNotifierItem* trayitem)
{
    char *contents;
    GError *err = NULL;

    g_file_get_contents(BOLT_SNI_XML_PATH, &contents, NULL, &err);
    if (err != NULL) {
        g_warning("%s", err->message);
        g_error_free(err);
    }
    else {
        GDBusNodeInfo *node = g_dbus_node_info_new_for_xml(contents, &err);
        g_free(contents);
        trayitem->nodeinfo = node;
        if (err != NULL) {
            g_warning("Malformed XML: %s", err->message);
            g_error_free(err);
        }
        else {
            const char *iface_name = "org.kde.StatusNotifierItem";
            GDBusInterfaceInfo *info = g_dbus_node_info_lookup_interface(node, iface_name);
            trayitem->interface_info = info;
            return info;
        }
    }
    return NULL;
}


void
exit_signal(DbusmenuMenuitem *item, const char *event, GVariant *info,
        uint32_t time, StatusNotifierItem *trayitem)
{
    SnitrayAppExit(trayitem->client);
    trayitem_teardown(trayitem);

}


void
show_signal(DbusmenuMenuitem *item, const char *event, GVariant *info,
        uint32_t time, StatusNotifierItem *trayitem)
{
    SnitrayAppShow(trayitem->client);
}


void
build_menu(StatusNotifierItem *trayitem)
{
    DbusmenuMenuitem *menuroot = dbusmenu_menuitem_new();
    DbusmenuMenuitem *open = dbusmenu_menuitem_new();
    DbusmenuMenuitem *exit = dbusmenu_menuitem_new();

    g_signal_connect(G_OBJECT(open), DBUSMENU_MENUITEM_SIGNAL_EVENT, G_CALLBACK(show_signal), trayitem);
    dbusmenu_menuitem_property_set(open, "label", "Open");
    dbusmenu_menuitem_child_append(menuroot, open);

    g_signal_connect(G_OBJECT(exit), DBUSMENU_MENUITEM_SIGNAL_EVENT, G_CALLBACK(exit_signal), trayitem);
    dbusmenu_menuitem_property_set(exit, "label", "Exit");
    dbusmenu_menuitem_child_append(menuroot, exit);

    DbusmenuServer *menu = dbusmenu_server_new("/MenuBar");
    dbusmenu_server_set_root(menu, menuroot);
    trayitem->menuroot = menuroot;
}


void
trayitem_teardown(StatusNotifierItem *trayitem) {
    g_bus_unwatch_name(trayitem->watcher_id);
    g_bus_unown_name(trayitem->owner_id);
    g_dbus_connection_unregister_object(trayitem->connection, trayitem->registration_id);
    g_dbus_connection_close_sync(trayitem->connection, NULL, NULL);
    g_free(trayitem);
}


void
watcher_disappeared_handler(GDBusConnection *conn, const char *theirname,
        const char *theiruniqname, StatusNotifierItem *trayitem)
{
    g_warning("%s", "Could not find system tray.");
}


void
watcher_appeared_handler(GDBusConnection *conn, const char *theirname,
        const char *theiruniqname, StatusNotifierItem *trayitem)
{
    const char *objpath = "/StatusNotifierWatcher";
    const char *method = "RegisterStatusNotifierItem";
    GVariant *ourname = g_variant_new("(s)", trayitem->bus_name);

    g_dbus_connection_call(conn, theirname, objpath, theirname,
            method, ourname, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
            (GAsyncReadyCallback)trayitem_registration_cb, trayitem);
}


void
trayitem_registration_cb(GDBusConnection *conn, GAsyncResult *res,
        StatusNotifierItem *trayitem)
{
    GVariant *ret;
    GError *err;

    err = NULL;
    ret = g_dbus_connection_call_finish(conn, res, &err);

    if (ret) {
        g_variant_unref(ret);
    }
    else {
        g_warning("%s", err->message);
        g_error_free(err);
    }
}


void
name_acquired_handler(GDBusConnection *connection, const char *name,
        StatusNotifierItem *trayitem)
{
        trayitem->watcher_id = g_bus_watch_name(G_BUS_TYPE_SESSION,
                "org.kde.StatusNotifierWatcher", G_BUS_NAME_WATCHER_FLAGS_NONE,
                (GBusNameAppearedCallback) watcher_appeared_handler,
                (GBusNameVanishedCallback) watcher_disappeared_handler,
                trayitem, NULL);

}


void
bus_acquired_handler(GDBusConnection *connection, const char *name,
        StatusNotifierItem *trayitem)
{
    const char *item_path = "/StatusNotifierItem";
    GDBusInterfaceInfo *item_iface_info = get_interface_info("trayitem", trayitem);

    trayitem->connection = connection;

    trayitem->registration_id = g_dbus_connection_register_object(
            connection,
            item_path,
            item_iface_info,
            &trayitem_interface_vtable,
            trayitem,
            NULL,
            NULL);

    build_menu(trayitem);
}


void
start_snitray(void *client)
{
    StatusNotifierItem *trayitem;
    trayitem = g_malloc0(sizeof(StatusNotifierItem));

    trayitem->bus_name = g_strdup_printf("%s-%d-%d", "org.kde.StatusNotifierItem", getpid(), 1);
    trayitem->client = client;

    trayitem->owner_id = g_bus_own_name(
            G_BUS_TYPE_SESSION,
            trayitem->bus_name,
            G_BUS_NAME_OWNER_FLAGS_NONE,
            (GBusAcquiredCallback) bus_acquired_handler,
            (GBusNameAcquiredCallback) name_acquired_handler,
            NULL, //(GBusNameLostCallback) name_lost_handler,
            trayitem,   // user_data
            NULL // (GDestroyNotify) freefunc  // user_data_free_func
            );
}
