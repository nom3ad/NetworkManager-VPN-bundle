#pragma once

#if !GTK_CHECK_VERSION(4,0,0)
#define gtk_editable_set_text(editable,text)		gtk_entry_set_text(GTK_ENTRY(editable), (text))
#define gtk_editable_get_text(editable)			gtk_entry_get_text(GTK_ENTRY(editable))
#define gtk_widget_get_root(widget)			gtk_widget_get_toplevel(widget)
#define gtk_window_destroy(window)			gtk_widget_destroy(GTK_WIDGET (window))
#define gtk_check_button_get_active(button)		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))
#define gtk_check_button_set_active(button, active)	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), active)
#define gtk_box_append(box, widget)	gtk_box_pack_start(box, widget, false, false, 0)

typedef void GtkRoot;
#endif 



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