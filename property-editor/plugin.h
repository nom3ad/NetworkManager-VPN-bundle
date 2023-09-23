#pragma once

#define THIS_VPN_EDITOR_PLUGIN_TYPE (this_vpn_editor_plugin_get_type())

typedef struct _ThisVPNEditorPlugin ThisVPNEditorPlugin;
typedef struct _ThisVPNEditorPluginClass ThisVPNEditorPluginClass;

struct _ThisVPNEditorPlugin {
    GObject parent;
};

struct _ThisVPNEditorPluginClass {
    GObjectClass parent;
};

GType this_vpn_editor_plugin_get_type(void);

typedef NMVpnEditor *(*ThisVPNEditorWidgetFactory)(NMVpnEditorPlugin *plugin, NMConnection *connection, GError **error);
