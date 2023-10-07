#include <dirent.h>
#include <dlfcn.h>
#include <gtk/gtk.h>

#include "nm-vpn-plugin-utils.h"
#include "plugin.h"

int main(int argc, char **argv);

static const char *get_libs_directory()
{
    char *current_bin_dir = g_get_current_dir();
    Dl_info dlinfo;
    if (dladdr((void *)main, &dlinfo)) {
        current_bin_dir = g_path_get_dirname(dlinfo.dli_fname);
    }
    return current_bin_dir;
}

static void launch_editor(const char *provider, GtkApplication *app)
{
    g_info(">>>>>>>>> Launching editor for %s", provider);
    GError *error = nullptr;
    const char *check_service = nullptr;

    char *plugin_lib_path = g_build_filename(get_libs_directory(), g_strdup_printf("libnm-vpn-plugin-%s.so", provider), nullptr);
    char *widget_lib_path = g_build_filename(get_libs_directory(), g_strdup_printf("libnm-gtk4-vpn-plugin-%s-editor.so", provider), nullptr);

    g_info("Loading plugin from %s", plugin_lib_path);
    NMVpnEditorPlugin *plugin = nm_vpn_editor_plugin_load(plugin_lib_path, nullptr, &error);
    if (!plugin) {
        g_error("Failed to load plugin: %s", error->message);
        return;
    } else {
        g_info("Loaded plugin from %s", plugin_lib_path);
    }

    NMConnection *connection = nullptr;

    void *dl_module = dlopen(widget_lib_path, RTLD_LAZY | RTLD_LOCAL);
    if (!dl_module) {
        g_error("Failed to load widget: %s", dlerror());
        return;
    } else {
        g_info("Loaded widget from %s", widget_lib_path);
    }
    gpointer factory = dlsym(dl_module, "this_vpn_editor_widget_factory");
    if (!factory) {
        g_error("Failed to find widget factory in %s", dlerror());
        return;
    } else {
        g_info("Found widget factory in %s", widget_lib_path);
    }
    NMVpnEditor *editor = ((ThisVPNEditorWidgetFactory)factory)(plugin, connection, &error);

    if (!editor) {
        g_error("Failed to create editor: %s", error->message);
        return;
    } else {
        g_info("Created editor");
    }

    GtkWidget *editor_widget = GTK_WIDGET(nm_vpn_editor_get_widget(editor));
    gtk_widget_set_margin_top(editor_widget, 12);
    gtk_widget_set_margin_bottom(editor_widget, 12);
    gtk_widget_set_margin_start(editor_widget, 12);
    gtk_widget_set_margin_end(editor_widget, 12);
    GtkWidget *editor_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(editor_window), g_strdup_printf("ViewOnly GTK4 editor for %s", provider));
    gtk_window_set_default_size(GTK_WINDOW(editor_window), 600, 800);

    gtk_window_set_child(GTK_WINDOW(editor_window), editor_widget);
    gtk_widget_set_visible(editor_window, true);
}

static void on_dropdown_changed(GtkDropDown *dropdown, gpointer _, gpointer user_data)
{
    GtkApplication *app = GTK_APPLICATION(user_data);
    gpointer selected_item = gtk_drop_down_get_selected_item(GTK_DROP_DOWN(dropdown));
    if (!selected_item) {
        g_info("No item selected");
        return;
    }
    const char *selected_lib = gtk_string_object_get_string(GTK_STRING_OBJECT(selected_item));
    g_info("selected item: '%s'", selected_lib);
    if (strlen(selected_lib) == 0) {
        return;
    }

    char *provier_name;
    char **parts = g_strsplit(selected_lib, "-", -1);
    int parts_len = g_strv_length(parts);
    if (parts_len < 2) {
        g_error("Invalid library name: %s", selected_lib);
        return;
    }
    provier_name = parts[parts_len - 2];

    launch_editor(provier_name, app);
}

// Add the callback function to the dropdown list

static void activate(GtkApplication *app)
{
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Test GTK4 editor");
    gtk_window_set_default_size(GTK_WINDOW(window), 350, 0);
    GtkWidget *dropdown = gtk_drop_down_new(nullptr, nullptr);

    GListModel *editor_lib_list = G_LIST_MODEL(gtk_string_list_new(nullptr));
    gtk_string_list_append(GTK_STRING_LIST(editor_lib_list), "");

    const char *current_bin_dir = get_libs_directory();
    g_info("current_bin_dir: %s", current_bin_dir);
    struct dirent *dir;
    DIR *d = opendir(get_libs_directory());
    if (!d) {
        g_error("Failed to open directory %s", current_bin_dir);
        g_application_quit(G_APPLICATION(app));
    }
    while ((dir = readdir(d)) != nullptr) {
        const char *lib_fname = dir->d_name;
        // check if name starts with libnm-gtk4
        if (strncmp(lib_fname, "libnm-gtk4", 10) != 0) {
            continue;
        }
        gtk_string_list_append(GTK_STRING_LIST(editor_lib_list), lib_fname);
    }
    closedir(d);
    gtk_drop_down_set_model(GTK_DROP_DOWN(dropdown), editor_lib_list);

    g_signal_connect(dropdown, "notify::selected-item", G_CALLBACK(on_dropdown_changed), app);

    gtk_window_set_child(GTK_WINDOW(window), dropdown);
    gtk_widget_set_visible(window, true);

    // ifthere a an argument, launch the editor for that provider
    const char *provider = getenv("VPN_PROVIDER");
    if (provider) {
        launch_editor(provider, app);
    }
}

int main(int argc, char **argv)
{
    GtkApplication *app = gtk_application_new("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), app);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
