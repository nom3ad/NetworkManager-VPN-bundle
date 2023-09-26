#pragma once

#define THIS_VPN_EDITOR_WIDGET_TYPE (this_vpn_editor_widget_get_type())
#define THIS_VPN_EDITOR_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), THIS_VPN_EDITOR_WIDGET_TYPE, ThisVPNEditorWidget))

typedef struct _ThisVPNEditorWidget ThisVPNEditorWidget;
typedef struct _ThisVPNEditorWidgetClass ThisVPNEditorWidgetClass;

struct _ThisVPNEditorWidget {
    GObject parent;
};

struct _ThisVPNEditorWidgetClass {
    GObjectClass parent;
};

GType this_vpn_editor_widget_get_type(void);

extern "C"
NMVpnEditor *this_vpn_editor_widget_factory(NMVpnEditorPlugin *plugin, NMConnection *connection, GError **error);