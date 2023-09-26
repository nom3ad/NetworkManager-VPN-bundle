#include <NetworkManager.h>
#include <glib-object.h>
// #include <glib/gi18n-lib.h>

#include "common/nm-service-defines.h"
#include "plugin.h"

#ifdef DISABLE_DYNAMIC_WIDGET_LIB_LOADING
#include "widget.h"
#else
#include "nm-vpn-plugin-utils.h" // TODO: reuse one from NetworkManager
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN ("nm-vpn-plugin-" NM_VPN_PROVIDER_ID)

#define EDITOR_PLUGIN_ERROR (g_quark_from_static_string("nm-connection-error-quark"))

using namespace std;

enum { PROP_0, PROP_NAME, PROP_DESC, PROP_SERVICE };

static void editor_plugin_interface_init(NMVpnEditorPluginInterface *iface_class);
G_DEFINE_TYPE_EXTENDED(ThisVPNEditorPlugin,
                       this_vpn_editor_plugin,
                       G_TYPE_OBJECT,
                       0,
                       G_IMPLEMENT_INTERFACE(NM_TYPE_VPN_EDITOR_PLUGIN, editor_plugin_interface_init))

static void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    g_debug("get_property() prop_id=%d", prop_id);
    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string(value, NM_VPN_PROVIDER_LABEL);
        break;
    case PROP_DESC:
        g_value_set_string(value, NM_VPN_PROVIDER_DESCRIPTION);
        break;
    case PROP_SERVICE:
        g_value_set_string(value, NM_VPN_PROVIDER_DBUS_SERVICE);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void this_vpn_editor_plugin_class_init(ThisVPNEditorPluginClass *req_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS(req_class);

    object_class->get_property = get_property;

    g_object_class_override_property(object_class, PROP_NAME, NM_VPN_EDITOR_PLUGIN_NAME);

    g_object_class_override_property(object_class, PROP_DESC, NM_VPN_EDITOR_PLUGIN_DESCRIPTION);

    g_object_class_override_property(object_class, PROP_SERVICE, NM_VPN_EDITOR_PLUGIN_SERVICE);
}

static void this_vpn_editor_plugin_init(G_GNUC_UNUSED ThisVPNEditorPlugin *plugin)
{
}

static void editor_plugin_interface_init(NMVpnEditorPluginInterface *iface_class)
{
    /* interface implementation */
    iface_class->get_editor = [](G_GNUC_UNUSED NMVpnEditorPlugin *plugin, NMConnection *connection, GError **error) -> NMVpnEditor * {
        g_debug("get_editor()");
        if (!NM_IS_CONNECTION(connection)) {
            g_set_error(error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_FAILED, "Expected NMConnection");
            return nullptr;
        }

#ifdef DISABLE_DYNAMIC_WIDGET_LIB_LOADING
        return this_vpn_editor_widget_factory(plugin, connection, error);
#else
        gpointer gtk3_only_symbol;
        GModule *self_module;
        const char *widget_lib;
        self_module = g_module_open(nullptr, (GModuleFlags)0);
        g_module_symbol(self_module, "gtk_container_add", &gtk3_only_symbol);
        g_module_close(self_module);
        if (gtk3_only_symbol) {
            widget_lib = ("libnm-gtk3-vpn-plugin-" NM_VPN_PROVIDER_ID "-editor.so");
        } else {
            widget_lib = ("libnm-gtk4-vpn-plugin-" NM_VPN_PROVIDER_ID "-editor.so");
        }
        g_message("GTK%d detected, loading %s", gtk3_only_symbol ? 3 : 4, widget_lib);
        return nm_vpn_plugin_utils_load_editor(
            widget_lib,
            "this_vpn_editor_widget_factory",
            [](gpointer factory, NMVpnEditorPlugin *editor_plugin, NMConnection *connection, gpointer user_data, GError **error) -> NMVpnEditor * {
                return ((ThisVPNEditorWidgetFactory)factory)(editor_plugin, connection, error);
            },
            plugin,
            connection,
            nullptr,
            error);
#endif
    };
    iface_class->get_capabilities = [](G_GNUC_UNUSED NMVpnEditorPlugin *plugin) -> NMVpnEditorPluginCapability {
        g_debug("get_capabilities()");
        // /(NM_VPN_EDITOR_PLUGIN_CAPABILITY_IMPORT | NM_VPN_EDITOR_PLUGIN_CAPABILITY_EXPORT | NM_VPN_EDITOR_PLUGIN_CAPABILITY_IPV6);
        return NM_VPN_EDITOR_PLUGIN_CAPABILITY_NONE;
    };
    iface_class->import_from_file =
        [](G_GNUC_UNUSED NMVpnEditorPlugin *plugin, G_GNUC_UNUSED const char *path, G_GNUC_UNUSED GError **error) -> NMConnection * {
        g_warning("NotImplemented: import_from_file(): path=%s", path);
        g_set_error(error, EDITOR_PLUGIN_ERROR, NM_CONNECTION_ERROR_FAILED, "Import not implemented");
        return nullptr;
    };
    iface_class->export_to_file = [](G_GNUC_UNUSED NMVpnEditorPlugin *plugin,
                                     G_GNUC_UNUSED const char *path,
                                     G_GNUC_UNUSED NMConnection *connection,
                                     G_GNUC_UNUSED GError **error) -> gboolean {
        g_warning("NotImplemented: export_to_file() path=%s", path);
        g_set_error(error, 0, 0, "Export is not implemented");
        return false;
    };
    iface_class->get_suggested_filename = [](G_GNUC_UNUSED NMVpnEditorPlugin *plugin, G_GNUC_UNUSED NMConnection *connection) -> char * {
        g_warning("NotImplemented: get_suggested_filename()");
        return nullptr;
    };
}

G_MODULE_EXPORT NMVpnEditorPlugin *nm_vpn_editor_plugin_factory(GError **error)
{
    if (error)
        g_return_val_if_fail(*error == nullptr, nullptr);

    return (NMVpnEditorPlugin *)g_object_new(THIS_VPN_EDITOR_PLUGIN_TYPE, nullptr);
}
