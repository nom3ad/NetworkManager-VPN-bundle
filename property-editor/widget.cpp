#include <NetworkManager.h>
#include <glib-object.h>
#include <gtk/gtk.h>
// #include <glib/gi18n-lib.h>
#include <json-glib/json-glib.h>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/nm-service-defines.h"
#include "widget.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN ("nm-vpn-plugin-" THIS_VPN_PROVIDER_ID "-widget-gtk" G_STRINGIFY(THIS_VPN_WIDGET_GTK_MAJOR_VER))

#define EDITOR_PLUGIN_ERROR (g_quark_from_static_string("nm-connection-error-quark"))
#define set_invalid_property_error(error, ...) g_set_error(error, EDITOR_PLUGIN_ERROR, NM_CONNECTION_ERROR_INVALID_PROPERTY, __VA_ARGS__)

#if !GTK_CHECK_VERSION(4, 0, 0)
typedef void GtkRoot;
#define gtk_editable_set_text(editable, text) gtk_entry_set_text(GTK_ENTRY(editable), (text))
#define gtk_editable_get_text(editable) gtk_entry_get_text(GTK_ENTRY(editable))
#define gtk_widget_get_root(widget) gtk_widget_get_toplevel(widget)
#define gtk_window_destroy(window) gtk_widget_destroy(GTK_WIDGET(window))
#define gtk_check_button_get_active(button) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))
#define gtk_check_button_set_active(button, active) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), active)
#define gtk_box_append(box, widget) gtk_box_pack_start(box, widget, false, false, 0)
#define GTK_TYPE_PASSWORD_ENTRY GTK_TYPE_ENTRY
#define gtk_button_new_from_icon_name_(name) gtk_button_new_from_icon_name(name, GTK_ICON_SIZE_BUTTON)
#define IS_LIST_VIEW_WIDGET(widget) G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_TREE_VIEW)
#define apply_vector_to_list_model(widget, vec)                                                                                                                \
    {                                                                                                                                                          \
        GtkListStore *string_list = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(widget)));                                                            \
        for (const string v : vec) {                                                                                                                           \
            GtkTreeIter iter;                                                                                                                                  \
            gtk_list_store_append(string_list, &iter);                                                                                                         \
            gtk_list_store_set(string_list, &iter, 0, v.c_str(), -1);                                                                                          \
        }                                                                                                                                                      \
    }
#define remove_selected_from_list_view(listview)                                                                                                               \
    {                                                                                                                                                          \
        GtkTreeView *listview = GTK_TREE_VIEW(data);                                                                                                           \
        GtkTreeModel *model = gtk_tree_view_get_model(listview);                                                                                               \
        GtkTreeIter iter;                                                                                                                                      \
        GtkTreeSelection *selection = gtk_tree_view_get_selection(listview);                                                                                   \
        if (selection && gtk_tree_selection_get_selected(selection, &model, &iter)) {                                                                          \
            gtk_list_store_remove(GTK_LIST_STORE(model), &iter);                                                                                               \
        }                                                                                                                                                      \
    }
#define append_to_ist_view(listview, item)                                                                                                                     \
    {                                                                                                                                                          \
        GtkListStore *string_list = GTK_LIST_STORE(listview);                                                                                                  \
        GtkTreeIter iter;                                                                                                                                      \
        gtk_list_store_append(string_list, &iter);                                                                                                             \
        gtk_list_store_set(string_list, &iter, 0, "<edit>", -1);                                                                                               \
    }
#else

#define gtk_button_new_from_icon_name_(name) gtk_button_new_from_icon_name(name)
#define IS_LIST_VIEW_WIDGET(widget) G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_LIST_VIEW)
#define apply_vector_to_list_model(widget, vec)                                                                                                                \
    {                                                                                                                                                          \
        GtkSelectionModel *ss = gtk_list_view_get_model(GTK_LIST_VIEW(widget));                                                                                \
        GtkStringList *string_list = GTK_STRING_LIST(gtk_single_selection_get_model(GTK_SINGLE_SELECTION(ss)));                                                \
        for (const string v : values)                                                                                                                          \
            gtk_string_list_append(string_list, v.c_str());                                                                                                    \
    }

#define remove_selected_from_list_view(listview)                                                                                                               \
    {                                                                                                                                                          \
        GtkSelectionModel *ss = gtk_list_view_get_model(GTK_LIST_VIEW(listview));                                                                              \
        GtkStringList *string_list = GTK_STRING_LIST(gtk_single_selection_get_model(GTK_SINGLE_SELECTION(ss)));                                                \
        GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(string_list));                                                                   \
        guint selected = gtk_single_selection_get_selected(selection);                                                                                         \
        if (selected != GTK_INVALID_LIST_POSITION) {                                                                                                           \
            gtk_string_list_remove(string_list, selected);                                                                                                     \
        }                                                                                                                                                      \
    }
#define append_to_ist_view(listview, item)                                                                                                                     \
    {                                                                                                                                                          \
        GtkSelectionModel *ss = gtk_list_view_get_model(GTK_LIST_VIEW(listview));                                                                              \
        GtkStringList *string_list = GTK_STRING_LIST(gtk_single_selection_get_model(GTK_SINGLE_SELECTION(ss)));                                                \
        gtk_string_list_append(string_list, item);                                                                                                             \
    }
#endif

using namespace std;

typedef struct {
    GtkWidget *widget;
    JsonObject *input_def_obj;
} InputItem;

typedef struct {
    GtkWidget *widget;
    bool is_new_connection;
    unordered_map<std::string, InputItem> input_widgets;
} ThisVPNEditorWidgetPrivate;

static void this_vpn_editor_widget_interface_init(NMVpnEditorInterface *iface_class);

#define _G_ADD_PRIVATE(TypeName) G_ADD_PRIVATE(TypeName) // hack to ensure that ThisVPNEditorWidget is expanded
G_DEFINE_TYPE_WITH_CODE(ThisVPNEditorWidget,
                        this_vpn_editor_widget,
                        G_TYPE_OBJECT,
                        _G_ADD_PRIVATE(ThisVPNEditorWidget) G_IMPLEMENT_INTERFACE(NM_TYPE_VPN_EDITOR, this_vpn_editor_widget_interface_init))

static inline void set_prefixed_widget_name(GtkWidget *widget, string s1)
{
    string name(THIS_VPN_PROVIDER_ID);
    name += "-" + name;
    gtk_widget_set_name(widget, name.c_str());
}

static bool check_validity(ThisVPNEditorWidget *self, GError **error)
{
    ThisVPNEditorWidgetPrivate *priv = (ThisVPNEditorWidgetPrivate *)this_vpn_editor_widget_get_instance_private(THIS_VPN_EDITOR_WIDGET(self));

    for (const auto &pair : priv->input_widgets) {
        string id = pair.first;
        JsonObject *input_def_obj = pair.second.input_def_obj;
        GtkWidget *widget = pair.second.widget;
        string value;
        bool is_required = json_object_get_boolean_member_with_default(input_def_obj, "required", false);
        if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_SPIN_BUTTON)) {
        } else if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_ENTRY) || G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_PASSWORD_ENTRY)) {
            value = STR(gtk_editable_get_text(GTK_EDITABLE(widget)));
            if (is_required && value.empty()) {
                set_invalid_property_error(error, "Property %s is required", id.c_str());
                return false;
            }
            int min_length = json_object_get_int_member_with_default(input_def_obj, "min_length", 0);
            if (min_length && value.length() < min_length && !value.empty()) {
                set_invalid_property_error(error, "Property %s must be at least %d characters long", id.c_str(), min_length);
                return false;
            }
            int max_length = json_object_get_int_member_with_default(input_def_obj, "max_length", 0);
            if (max_length && value.length() > max_length) {
                set_invalid_property_error(error, "Property %s must be at most %d characters long", id.c_str(), max_length);
                return false;
            }
            string regex_pattern = STR(json_object_get_string_member_with_default(input_def_obj, "regex", ""));
            try {
                if (!regex_pattern.empty() && !value.empty()) {
                    g_debug("Checking regex: %s %s", regex_pattern.c_str(), value.c_str());
                    regex regex_obj(regex_pattern);
                    if (!regex_match(value, regex_obj)) {
                        set_invalid_property_error(error, "Property %s must match regex %s", id.c_str(), regex_pattern.c_str());
                        return false;
                    }
                }
            } catch (const std::regex_error &e) {
                g_critical("Regex Error on %s |  %s", regex_pattern.c_str(), e.what());
            }

        } else if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_CHECK_BUTTON)) {
            //
        }
    }
    return true;
}

static void dispose_editor_widget(GObject *object)
{
    g_debug("dispose_editor_widget()");
    ThisVPNEditorWidgetPrivate *priv = (ThisVPNEditorWidgetPrivate *)this_vpn_editor_widget_get_instance_private(THIS_VPN_EDITOR_WIDGET(object));
    if (priv->widget)
        gtk_widget_unparent(priv->widget);

    G_OBJECT_CLASS(this_vpn_editor_widget_parent_class)->dispose(object);
    g_debug("done dispose_editor_widget()");
}

static void this_vpn_editor_widget_init(G_GNUC_UNUSED ThisVPNEditorWidget *plugin)
{
}

static bool apply_connection_proprties(ThisVPNEditorWidget *self, NMConnection *connection, G_GNUC_UNUSED GError **error)
{
    NMSettingVpn *s_vpn = nm_connection_get_setting_vpn(connection);

    nm_setting_vpn_foreach_data_item(
        s_vpn,
        [](const char *key_cstr, const char *value_cstr, gpointer user_data) {
            GError *e = nullptr;
            string value = STR(value_cstr);
            string key = STR(key_cstr);
            ThisVPNEditorWidget *self = (ThisVPNEditorWidget *)user_data;
            ThisVPNEditorWidgetPrivate *priv = (ThisVPNEditorWidgetPrivate *)this_vpn_editor_widget_get_instance_private(THIS_VPN_EDITOR_WIDGET(self));
            if (priv->input_widgets.find(key) == priv->input_widgets.end()) {
                g_warning("apply_connection_proprties() No input widget for key %s", key.c_str());
                return;
            }
            InputItem inp = priv->input_widgets.at(key);
            GtkWidget *widget = inp.widget;
            g_debug("apply_connection_proprties() widget type %s for key=%s value=%s", G_OBJECT_TYPE_NAME(widget), key.c_str(), value.c_str());
            if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_SPIN_BUTTON)) {
                int v = stoi(value);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), v);
            } else if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_ENTRY) || G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_PASSWORD_ENTRY)) {
                gtk_editable_set_text(GTK_EDITABLE(widget), value.c_str());
            } else if (IS_LIST_VIEW_WIDGET(widget)) {
                JsonParser *parser = json_parser_new();
                if (!json_parser_load_from_data(parser, value.c_str(), -1, &e)) {
                    g_warning("apply_connection_proprties() Unable to parse JSON: %s | %s", value.c_str(), e->message);
                    return;
                }
                JsonArray *arr = json_node_get_array(json_parser_get_root(parser));
                if (!arr) {
                    g_warning("apply_connection_proprties() Expected array object: %s", value.c_str());
                    return;
                }
                vector<string> values;
                for (int k = 0; k < json_array_get_length(arr); k++) {
                    JsonNode *n = json_array_get_element(arr, k);
                    string v = STR(json_node_get_string(n));
                    g_debug("apply_connection_proprties() Insert listview of key=%s with entry=%s", key.c_str(), v.c_str());
                    values.push_back(v);
                }
                apply_vector_to_list_model(widget, values);
            } else if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_CHECK_BUTTON)) {
                bool v = value == "true";
                gtk_check_button_set_active(GTK_CHECK_BUTTON(widget), v);
            }
#if GTK_CHECK_VERSION(4, 0, 0)
            else if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_DROP_DOWN)) {
                GListModel *model = gtk_drop_down_get_model(GTK_DROP_DOWN(widget));
                for (int i = 0; i < g_list_model_get_n_items(model); i++) {
                    auto model_item = g_list_model_get_item(model, i);
                    string enum_value = STR(gtk_string_object_get_string(GTK_STRING_OBJECT(model_item)));
                    g_debug("apply_connection_proprties() Insert dropdown of key=%s with entry=%s", key.c_str(), enum_value.c_str());
                    if (value == enum_value) {
                        gtk_drop_down_set_selected(GTK_DROP_DOWN(widget), i);
                        break;
                    }
                }
            }
#else
            else if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_COMBO_BOX_TEXT)) {
                GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
                GtkTreeIter iter;
                for (gboolean valid = gtk_tree_model_get_iter_first(model, &iter); valid; valid = gtk_tree_model_iter_next(model, &iter)) {
                    char *enume_value_cstr;
                    gtk_tree_model_get(model, &iter, 0, &enume_value_cstr, -1);
                    g_debug("apply_connection_proprties() Set combobox: key=%s value=%s", key.c_str(), enume_value_cstr);
                    if (value == enume_value_cstr) {
                        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(widget), &iter);
                        break;
                    }
                }
            }
#endif

            else {
                g_warning("apply_connection_proprties() Unhandled widget type %s for key %s", G_OBJECT_TYPE_NAME(widget), key.c_str());
                return;
            }
        },
        self);
    // g_return_val_if_fail(widget != nullptr, false);
    g_debug("apply_connection_proprties() Done");
    return true;
}

G_MODULE_EXPORT NMVpnEditor *this_vpn_editor_widget_factory(G_GNUC_UNUSED NMVpnEditorPlugin *plugin, NMConnection *connection, GError **error)
{
    NMVpnEditor *editor_obj;
    ThisVPNEditorWidgetPrivate *priv;
    NMSettingVpn *s_vpn;
    bool is_new = false;
    GError *e = nullptr;

    if (error)
        g_return_val_if_fail(*error == nullptr, nullptr);

    editor_obj = (NMVpnEditor *)g_object_new(THIS_VPN_EDITOR_WIDGET_TYPE, nullptr);
    if (!editor_obj) {
        g_set_error(error, EDITOR_PLUGIN_ERROR, 0, "could not create vpn editor object");
        return nullptr;
    }

    priv = (ThisVPNEditorWidgetPrivate *)this_vpn_editor_widget_get_instance_private(THIS_VPN_EDITOR_WIDGET(editor_obj));

    priv->input_widgets = unordered_map<std::string, InputItem>();

    // gtk_builder_set_translation_domain(priv->builder, GETTEXT_PACKAGE);

    ////////////////////////////////////////////////////////////////////////
    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, THIS_VPN_PROVIDER_INPUT_FORM_JSON, -1, &e)) {
        g_set_error(error, EDITOR_PLUGIN_ERROR, 0, "Unable to parse THIS_VPN_PROVIDER_INPUT_FORM_JSON JSON: %s", e->message);
        return nullptr;
    }
    JsonArray *section_array = json_node_get_array(json_parser_get_root(parser));

    GtkWidget *box_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    set_prefixed_widget_name(box_main, "MainBox");
    // margin set

    for (int i = 0; i < json_array_get_length(section_array); i++) {
        JsonNode *section_node = json_array_get_element(section_array, i);
        JsonObject *section_obj = json_node_get_object(section_node);
        if (!section_obj) {
            g_set_error(error, EDITOR_PLUGIN_ERROR, 0, "THIS_VPN_PROVIDER_INPUT_FORM_JSON: Invalid section obj at index %d", i);
            return nullptr;
        }
        string section_title = STR(json_object_get_string_member_with_default(section_obj, "section", "<unnamed>"));
        g_debug("Processing section_obj: section_title=%s", section_title.c_str());

        JsonArray *input_def_array = json_object_get_array_member(section_obj, "inputs");
        if (!input_def_array) {
            g_set_error(error, EDITOR_PLUGIN_ERROR, 0, "THIS_VPN_PROVIDER_INPUT_FORM_JSON:  Missing inputs array obj at index %d", i);
            return nullptr;
        }

        g_debug("Creating label for section: %s", section_title.c_str());
        GtkWidget *lbl_section = gtk_label_new(section_title.c_str());
        set_prefixed_widget_name(lbl_section, section_title + "Label");
        gtk_label_set_markup(GTK_LABEL(lbl_section), ("<b>" + section_title + "</b>").c_str());
        gtk_widget_set_halign(lbl_section, GTK_ALIGN_START);
        gtk_widget_set_margin_top(lbl_section, 10);
        gtk_box_append(GTK_BOX(box_main), lbl_section);

        g_debug("Creating box for section: %s", section_title.c_str());
        GtkWidget *box_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        set_prefixed_widget_name(box_section, section_title + "Box");
        gtk_box_append(GTK_BOX(box_main), box_section);
        gtk_widget_set_margin_start(box_section, 15);

        g_debug("Adding input widgets to section: %s", section_title.c_str());
        for (int j = 0; j < json_array_get_length(input_def_array); j++) {
            string value_change_signal_name = "changed";
            JsonNode *input_def_node = json_array_get_element(input_def_array, j);
            JsonObject *input_def_obj = json_node_get_object(input_def_node);
            if (!input_def_obj) {
                g_set_error(error,
                            EDITOR_PLUGIN_ERROR,
                            0,
                            "THIS_VPN_PROVIDER_INPUT_FORM_JSON: Invalid input def at index [%s].%d.%d",
                            section_title.c_str(),
                            i,
                            j);
                return nullptr;
            }
            string id = STR(json_object_get_string_member_with_default(input_def_obj, "id", ""));
            string type = STR(json_object_get_string_member_with_default(input_def_obj, "type", "string"));
            string label = STR(json_object_get_string_member_with_default(input_def_obj, "label", id.c_str()));
            string description = STR(json_object_get_string_member_with_default(input_def_obj, "description", ""));
            if (id.empty()) {
                g_set_error(error,
                            EDITOR_PLUGIN_ERROR,
                            0,
                            "THIS_VPN_PROVIDER_INPUT_FORM_JSON: Missing input def id at index [%s].%d.%d",
                            section_title.c_str(),
                            i,
                            j);
                continue;
            }
            GtkWidget *widget_input = nullptr;
            GtkWidget *widget_input_holder = nullptr;
            if (type == "integer") {
                gint64 min_v = json_object_get_int_member_with_default(input_def_obj, "min_value", 0);
                gint64 max_v = json_object_get_int_member_with_default(input_def_obj, "max_value", 999999);
                widget_input = gtk_spin_button_new_with_range(min_v, max_v, 1);
                gint64 default_v = json_object_get_int_member_with_default(input_def_obj, "default", 0);
                g_debug("Found type=%s: id=%s default=%d, min_v=%d max_v=%d", type.c_str(), id.c_str(), default_v, min_v, max_v);
                if (default_v) {
                    gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget_input), default_v);
                }
            } else if (type == "string") {
                string default_v = STR(json_object_get_string_member_with_default(input_def_obj, "default", ""));
                gint64 max_length = json_object_get_int_member_with_default(input_def_obj, "max_length", 128);
                bool is_secret = json_object_get_boolean_member_with_default(input_def_obj, "is_secret", false);
                if (is_secret) {
#if GTK_CHECK_VERSION(4, 0, 0)
                    widget_input = gtk_password_entry_new();
                    gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(widget_input), true);
#else
                    widget_input = gtk_entry_new();
                    gtk_entry_set_visibility(GTK_ENTRY(widget_input), false);
                    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(widget_input), GTK_ENTRY_ICON_SECONDARY, "view-reveal-symbolic.symbolic");
                    gtk_entry_set_icon_activatable(GTK_ENTRY(widget_input), GTK_ENTRY_ICON_SECONDARY, true);
                    g_signal_connect(widget_input,
                                     "icon-press",
                                     G_CALLBACK(+[](GtkWidget *widget, gpointer user_data) -> void {
                                         bool visible = gtk_entry_get_visibility(GTK_ENTRY(widget));

                                         if (visible) {
                                             gtk_entry_set_visibility(GTK_ENTRY(widget), false);
                                             gtk_entry_set_icon_from_icon_name(GTK_ENTRY(widget), GTK_ENTRY_ICON_SECONDARY, "view-reveal-symbolic.symbolic");
                                         } else {
                                             gtk_entry_set_visibility(GTK_ENTRY(widget), true);
                                             gtk_entry_set_icon_from_icon_name(GTK_ENTRY(widget), GTK_ENTRY_ICON_SECONDARY, "view-conceal-symbolic.symbolic");
                                         }
                                     }),
                                     nullptr);
#endif
                } else {
                    widget_input = gtk_entry_new();
                }
                g_debug("Found type=%s: id=%s  max_length=%d", type.c_str(), id.c_str(), max_length);
                if (!default_v.empty()) {
                    gtk_editable_set_text(GTK_EDITABLE(widget_input), default_v.c_str());
                }
                if (max_length) {
                    gtk_entry_set_max_length(GTK_ENTRY(widget_input), max_length);
                }
                if (json_object_has_member(input_def_obj, "placeholder")) {
                    gtk_entry_set_placeholder_text(GTK_ENTRY(widget_input), json_object_get_string_member_with_default(input_def_obj, "placeholder", ""));
                }
                // gint64 min_length = json_object_get_int_member(input_def_obj, "min_length");
            } else if (type == "array") {
                vector<string> default_values;
                JsonArray *default_value_array = json_object_get_array_member(input_def_obj, "default");
                string default_value;
                if (!default_value_array) {
                    for (int k = 0; k < json_array_get_length(default_value_array); k++) {
                        JsonNode *n = json_array_get_element(default_value_array, k);
                        default_values.push_back(STR(json_node_get_string(n)));
                    }
                }
#if GTK_CHECK_VERSION(4, 0, 0)
                GtkStringList *string_list = gtk_string_list_new(NULL);
                for (const string v : default_values) {
                    gtk_string_list_append(string_list, v.c_str());
                }
                GtkSingleSelection *ss = gtk_single_selection_new(G_LIST_MODEL(string_list));
                GtkWidget *listview = gtk_list_view_new(GTK_SELECTION_MODEL(GTK_SELECTION_MODEL(ss)), nullptr);
                gtk_list_view_set_single_click_activate(GTK_LIST_VIEW(listview), true);

#else
                GtkTreeIter iter;
                GtkListStore *string_list = gtk_list_store_new(1, G_TYPE_STRING);
                for (const string v : default_values) {
                    gtk_list_store_append(string_list, &iter);
                    gtk_list_store_set(string_list, &iter, 0, v.c_str(), -1);
                }
                GtkWidget *listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(string_list));
                GtkCellRenderer *cell_renderer = gtk_cell_renderer_text_new();
                g_object_set(cell_renderer, "editable", true, nullptr);
                g_signal_connect(cell_renderer,
                                 "edited",
                                 G_CALLBACK(+[](GtkCellRendererText *cell, const gchar *path_string, const gchar *new_text, gpointer data) {
                                     GtkListStore *store = (GtkListStore *)data;
                                     GtkTreeIter iter;
                                     GtkTreePath *path = gtk_tree_path_new_from_string(path_string);
                                     gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);
                                     gtk_list_store_set(store, &iter, 0, new_text, -1);
                                     gtk_tree_path_free(path);
                                 }),
                                 string_list);
                GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Strings", cell_renderer, "text", 0, nullptr);
                gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
                gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(listview), false);
#endif

                GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
                GtkWidget *hbox_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
                gtk_box_append(GTK_BOX(vbox), listview);
                gtk_box_append(GTK_BOX(vbox), hbox_buttons);

                GtkWidget *add_button = gtk_button_new_from_icon_name_("list-add-symbolic");
                gtk_box_append(GTK_BOX(hbox_buttons), add_button);
                g_signal_connect(add_button,
                                 "clicked",
                                 G_CALLBACK(+[](GtkButton *button, gpointer data) {
                                     append_to_ist_view(data, "<edit>")
                                 }),
                                 string_list);

                GtkWidget *delete_button = gtk_button_new_from_icon_name_("list-remove-symbolic");
                gtk_box_append(GTK_BOX(hbox_buttons), delete_button);
                g_signal_connect(delete_button,
                                 "clicked",
                                 G_CALLBACK(+[](GtkButton *button, gpointer data) {
                                     remove_selected_from_list_view(data)
                                 }),
                                 listview);

                widget_input = listview;
                widget_input_holder = vbox;

            } else if (type == "boolean") {
                widget_input = gtk_check_button_new();
                bool default_v = json_object_get_boolean_member_with_default(input_def_obj, "default", false);
                g_debug("Found type=%s: id=%s default=%d", type.c_str(), id.c_str(), default_v);
                gtk_check_button_set_active(GTK_CHECK_BUTTON(widget_input), default_v);
                value_change_signal_name = "toggled";
            } else if (type == "enum") {
                JsonArray *enum_array = json_object_get_array_member(input_def_obj, "values");
                if (!enum_array) {
                    g_set_error(error,
                                EDITOR_PLUGIN_ERROR,
                                0,
                                "THIS_VPN_PROVIDER_INPUT_FORM_JSON: Invalid enum input def at index [%s].%d.%d",
                                section_title.c_str(),
                                i,
                                j);
                    return nullptr;
                }
                vector<string> enum_values;
                for (int k = 0; k < json_array_get_length(enum_array); k++) {
                    JsonNode *enum_node = json_array_get_element(enum_array, k);
                    enum_values.push_back(STR(json_node_get_string(enum_node)));
                }
                string default_v = STR(json_object_get_string_member_with_default(input_def_obj, "default", ""));
                // g_debug("Found type=%s: id=%s default=%s enume_values: %s", type.c_str(), id.c_str(), default_v.c_str(),
                // JOIN_STRING_VEC(enum_values).c_str());
                int default_val_idx = -1;
                if (!default_v.empty()) {
                    default_val_idx = enum_values.size() - 1;
                    for (; default_val_idx; default_val_idx--) {
                        if (enum_values[default_val_idx] == default_v) {
                            break;
                        }
                    }
                }

#if GTK_CHECK_VERSION(4, 0, 0)
                GtkStringList *string_list = gtk_string_list_new(NULL);
                for (const auto &v : enum_values) {
                    gtk_string_list_append(string_list, v.c_str());
                }
                widget_input = gtk_drop_down_new(G_LIST_MODEL(string_list), NULL);
                if (default_val_idx >= 0) {
                    gtk_drop_down_set_selected(GTK_DROP_DOWN(widget_input), default_val_idx);
                }

#else
                widget_input = gtk_combo_box_text_new();
                for (const auto &v : enum_values) {
                    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget_input), v.c_str());
                }
                if (default_val_idx >= 0) {
                    gtk_combo_box_set_active(GTK_COMBO_BOX(widget_input), default_val_idx);
                }
#endif
            }

            else {
                g_warning("Found unknown type %s", type.c_str());
                continue;
            }
            set_prefixed_widget_name(widget_input, id + "InputWidget");
            gtk_widget_set_tooltip_text(widget_input, description.c_str());
            // XXX: sourcery (https://stackoverflow.com/questions/49638121/gtk-g-signal-connect-and-c-lambda-results-in-invalid-cast-errors)
            g_signal_connect(G_OBJECT(widget_input),
                             value_change_signal_name.c_str(),
                             G_CALLBACK(+[](G_GNUC_UNUSED GtkWidget *widget, gpointer user_data) -> void {
                                 g_debug("stuff_changed_cb() widget: %p", gtk_widget_get_name(widget));
                                 g_signal_emit_by_name(THIS_VPN_EDITOR_WIDGET(user_data), "changed");
                             }),
                             editor_obj);

            GtkWidget *lbl_input = gtk_label_new(label.c_str());
            set_prefixed_widget_name(lbl_input, id + "InputLabel");
            gtk_label_set_use_markup(GTK_LABEL(lbl_input), true);
            gtk_widget_set_halign(lbl_input, GTK_ALIGN_START);
            gtk_widget_set_tooltip_text(lbl_input, description.c_str());

            if (widget_input_holder == nullptr) {
                widget_input_holder = widget_input;
            }
            GtkWidget *grid_input = gtk_grid_new();
            set_prefixed_widget_name(grid_input, id + "InputGrid");
            gtk_grid_set_column_homogeneous(GTK_GRID(grid_input), true);
            gtk_grid_attach(GTK_GRID(grid_input), lbl_input, 0, 0, 1, 1);
            gtk_grid_attach(GTK_GRID(grid_input), widget_input_holder, 1, 0, 1, 1);

            gtk_box_append(GTK_BOX(box_section), grid_input);

            InputItem input_item = {};
            input_item.widget = widget_input;
            input_item.input_def_obj = json_node_get_object(json_node_copy(input_def_node));

            priv->input_widgets[id] = input_item;
        }
    }
    priv->widget = box_main;
    g_object_unref(parser);

    ////////////////////////////////////////////////////////////////////////

    s_vpn = nm_connection_get_setting_vpn(connection);

    if (s_vpn)
        nm_setting_vpn_foreach_data_item(
            s_vpn,
            [](const char *key_cstr, const char *value_cstr, gpointer user_data) {
                bool *is_new = (bool *)user_data;
                *is_new = false;
            },
            &is_new);
    priv->is_new_connection = is_new;

    if (!apply_connection_proprties(THIS_VPN_EDITOR_WIDGET(editor_obj), connection, error)) {
        g_object_unref(editor_obj);
        return nullptr;
    }

    g_debug("this_vpn_editor_widget_factory() Done: %p", editor_obj);
    return editor_obj;
}

static gboolean update_connection(NMVpnEditor *iface, NMConnection *connection, GError **error)
{
    ThisVPNEditorWidget *self = THIS_VPN_EDITOR_WIDGET(iface);
    ThisVPNEditorWidgetPrivate *priv = (ThisVPNEditorWidgetPrivate *)this_vpn_editor_widget_get_instance_private(THIS_VPN_EDITOR_WIDGET(self));
    NMSettingVpn *s_vpn;

    g_debug("update_connection()");

    if (!check_validity(self, error))
        return false;

    s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());
    g_object_set(s_vpn, NM_SETTING_VPN_SERVICE_TYPE, THIS_VPN_PROVIDER_DBUS_SERVICE, nullptr);

    for (const auto &pair : priv->input_widgets) {
        string id = pair.first;
        GtkWidget *widget = pair.second.widget;
        g_debug("update_connection_properties() Processing widget: id=%s w=%p", id.c_str(), widget);
        string value;
        if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_SPIN_BUTTON)) {
            value = to_string((int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)));
        } else if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_ENTRY)) {
            value = STR(gtk_editable_get_text(GTK_EDITABLE(widget)));
        } else if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_CHECK_BUTTON)) {
            value = gtk_check_button_get_active(GTK_CHECK_BUTTON(widget)) ? "true" : "false";
        } else if (IS_LIST_VIEW_WIDGET(widget)) {
            JsonArray *arr = json_array_new();
#if GTK_CHECK_VERSION(4, 0, 0)
            GtkSelectionModel *ss = gtk_list_view_get_model(GTK_LIST_VIEW(widget));
            GtkStringList *string_list = GTK_STRING_LIST(gtk_single_selection_get_model(GTK_SINGLE_SELECTION(ss)));
            for (int i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(string_list)); i++) {
                auto model_item = g_list_model_get_item(G_LIST_MODEL(string_list), i);
                const char *v = gtk_string_object_get_string(GTK_STRING_OBJECT(model_item));
#else
            GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
            GtkTreeIter iter;
            for (gboolean valid = gtk_tree_model_get_iter_first(model, &iter); valid; valid = gtk_tree_model_iter_next(model, &iter)) {
                char *v;
                gtk_tree_model_get(model, &iter, 0, &v, -1);
#endif
                g_debug("update_connection() get listview of key=%s with entry=%s", id.c_str(), v);
                json_array_add_string_element(arr, v);
            }
            JsonNode *n = json_node_alloc();
            json_node_init_array(n, arr);
            value = STR(json_to_string(n, false));
        }
#if GTK_CHECK_VERSION(4, 0, 0)
        else if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_DROP_DOWN)) {
            auto selected = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(widget));
            if (!selected) {
                continue;
            }
            value = STR(gtk_string_object_get_string(GTK_STRING_OBJECT(selected)));
        }
#else
        else if (G_TYPE_CHECK_INSTANCE_TYPE((widget), GTK_TYPE_COMBO_BOX_TEXT)) {
            value = STR(gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget)));
        }
#endif
        else {
            g_warning("update_connection() Unknown widget type for key %s : %s", id.c_str(), G_OBJECT_TYPE_NAME(widget));
        }
        if (!value.empty()) {
            g_debug("Set key=%s value=%s", id.c_str(), value.c_str());
            nm_setting_vpn_add_data_item(s_vpn, id.c_str(), value.c_str());
        }
    }
    nm_connection_add_setting(connection, NM_SETTING(s_vpn));

    return true;
}

static void this_vpn_editor_widget_class_init(ThisVPNEditorWidgetClass *req_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS(req_class);

    object_class->dispose = dispose_editor_widget;
}

static void this_vpn_editor_widget_interface_init(NMVpnEditorInterface *iface_class)
{
    /* interface implementation */
    iface_class->get_widget = [](NMVpnEditor *iface) -> GObject * {
        ThisVPNEditorWidget *self = THIS_VPN_EDITOR_WIDGET(iface);
        ThisVPNEditorWidgetPrivate *priv = (ThisVPNEditorWidgetPrivate *)this_vpn_editor_widget_get_instance_private(THIS_VPN_EDITOR_WIDGET(self));
        g_debug("get_widget() iface=%s widget=%s", G_OBJECT_TYPE_NAME(iface), priv->widget ? G_OBJECT_TYPE_NAME(priv->widget) : "null");
        return G_OBJECT(priv->widget);
    };
    iface_class->update_connection = update_connection;
}