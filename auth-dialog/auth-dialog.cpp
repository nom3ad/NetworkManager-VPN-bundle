#include "auth-dialog.h"
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>
// #include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>

#include <NetworkManager.h>

#include "common/nm-service-defines.h"

using namespace std;

static void wait_for_quit_instruction(void)
{
    GString *str;
    char c;
    ssize_t n;
    time_t start;

    str = g_string_sized_new(10);
    start = time(nullptr);
    do {
        errno = 0;
        n = read(0, &c, 1);
        if (n == 0 || (n < 0 && errno == EAGAIN))
            g_usleep(G_USEC_PER_SEC / 10);
        else if (n == 1) {
            g_string_append_c(str, c);
            if (strstr(str->str, "QUIT") || (str->len > 10))
                break;
        } else
            break;
    } while (time(nullptr) < start + 20);
    g_string_free(str, true);
}

static void puts_secrets(map<string, string> secrets_map)
{
    g_debug("puts_secrets()  count: %d", secrets_map.size());
    // dump secrets:   key1\nvalue1\nkey2\nvalue2\n\n format
    for (auto entry : secrets_map) {
        cout << entry.first << endl << entry.second << endl << endl;
    }
}

int main(int argc, char **argv)
{
    g_log_writer_default_set_use_stderr(true);

    #ifdef DEBUG_SET_STDERR_TO_FILE
    dup2(fileno(fopen("/tmp/vpn-bundle-auth-dialog.stderr", "a")), STDERR_FILENO);
    #endif
    char *pp_cmd;
    g_file_get_contents(g_strdup_printf("/proc/%d/cmdline", getppid()), &pp_cmd, nullptr, nullptr);
    g_debug("ARGS: %s \n\tPID: %d UID: %d EUID: %d CWD: %s\n\tParent: %d (%s)\n\tENV: %s\n",
            g_strjoinv(" ", argv),
            getpid(),
            getuid(),
            geteuid(),
            g_get_current_dir(),
            getppid(),
            pp_cmd,
            g_strjoinv(" ", environ));

    bool reprompt = false, allow_interaction = false, external_ui_mode = false;
    char *vpn_name_cstr = nullptr;
    char *vpn_uuid_cstr = nullptr;
    char *vpn_service_cstr = nullptr;
    char **hints_cstr_array = nullptr;

    GOptionContext *context;
    GOptionEntry entries[] = {{"reprompt", 'r', 0, G_OPTION_ARG_NONE, &reprompt, "Reprompt for passwords", nullptr},
                              {"uuid", 'u', 0, G_OPTION_ARG_STRING, &vpn_uuid_cstr, "UUID of VPN connection", nullptr},
                              {"name", 'n', 0, G_OPTION_ARG_STRING, &vpn_name_cstr, "Name of VPN connection", nullptr},
                              {"service", 's', 0, G_OPTION_ARG_STRING, &vpn_service_cstr, "VPN service type", nullptr},
                              {"allow-interaction", 'i', 0, G_OPTION_ARG_NONE, &allow_interaction, "Allow user interaction", nullptr},
                              //
                              {"external-ui-mode", 0, 0, G_OPTION_ARG_NONE, &external_ui_mode, "External UI mode", nullptr},
                              {"hint", 't', 0, G_OPTION_ARG_STRING_ARRAY, &hints_cstr_array, "Hints from the VPN plugin", nullptr},
                              {nullptr}};

    context = g_option_context_new("- auth prompt");
    g_option_context_add_main_entries(context, entries, nullptr); // translation_domain
    g_option_context_add_group(context, gtk_get_option_group(false));
    g_option_context_parse(context, &argc, &argv, nullptr);
    g_option_context_free(context);

    string vpn_name = STR(vpn_name_cstr);
    string vpn_uuid = STR(vpn_uuid_cstr);
    string vpn_service = STR(vpn_service_cstr);

    if (reprompt) {
        g_debug("reprompt argument (-r) is set, but this dialog does not support it");
        return 0;
    }
    if (vpn_name.empty()) {
        g_critical("Missing name argument (-n)");
        return 1;
    }
    if (vpn_service.empty()) {
        g_critical("Missing name argument (-n)");
        return 1;
    }
    if (vpn_uuid.empty()) {
        g_critical("Missing service argument (-s)");
        return 1;
    }

    int hints_count = hints_cstr_array ? g_strv_length(hints_cstr_array) : 0;
    g_info("Options: name=%s uuid=%s service=%s allow_interaction=%s external_ui_mode=%s hints=(%s)",
           vpn_name.c_str(),
           vpn_uuid.c_str(),
           vpn_service.c_str(),
           BOOL_STR(allow_interaction),
           BOOL_STR(external_ui_mode),
           hints_cstr_array ? g_strjoinv(", ", hints_cstr_array) : "");

    int ret = 0;

    if (!hints_count) {
        // called by agent of NetworkManager (eg: gnome-shell, nm-applet) when VPN configuration is being edited
        ret = do_when_no_hints(vpn_name, vpn_uuid, vpn_service, allow_interaction, external_ui_mode);
    } else {
        // Called by NetworkManager agent when VPN service emits NewSecrets signal while connecting
        ret = do_when_hints(vpn_name, vpn_uuid, vpn_service, allow_interaction, external_ui_mode, hints_cstr_array);
    }
    g_debug("Exiting with retrun code: %d", ret);
    return ret;
}

static int do_when_no_hints(string vpn_name, string vpn_uuid, string vpn_service, bool allow_interaction, bool external_ui_mode)
{
    GHashTable *vpn_options, *vpn_secrets = nullptr;
    if (!nm_vpn_service_plugin_read_vpn_details(0, &vpn_options, &vpn_secrets)) {
        g_critical("vpn: '%s' (%s) Failed to read  data and secrets from stdin.", vpn_name.c_str(), vpn_uuid.c_str());
        return 1;
    }

    std::stringstream ss;
    ss << "vpn: '" << vpn_name << "' (" << vpn_uuid << ") vpn_options: {";
    G_STRING_HASAHTABLE_DUMP_TO(vpn_options, ss);
    ss << "} vpn_secrets: {";
    G_STRING_HASAHTABLE_DUMP_TO(vpn_secrets, ss);
    ss << "}";
    g_info(ss.str().c_str());

    if (allow_interaction) {
        g_critical("allow_interaction is unexped when no hints provided");
        return 1;
    }
    if (external_ui_mode) {
        g_critical("external_ui_mode is unexped when no hints provided");
        return 1;
    }
    g_hash_table_foreach(
        vpn_secrets,
        [](gpointer key, gpointer value, gpointer user_data) {
            // dump secrets:   key1\nvalue1\nkey2\nvalue2\n\n format
            cout << (char *)key << endl << (char *)value << endl << endl;
        },
        &ss);

    return 0;
}

static int do_when_hints(string vpn_name, string vpn_uuid, string vpn_service, bool allow_interaction, bool external_ui_mode, char **hints_cstr_array)
{
    string auth_cfg_json;
    const string auth_cfg_hint_prefix = AUTH_CONFIG_HINT_PREFIX;

    map<string, string> secrets_map;

    for (char **iter = hints_cstr_array; iter && *iter; iter++) {
        const string hint = *iter;
        if (hint.compare(0, auth_cfg_hint_prefix.length(), auth_cfg_hint_prefix) == 0)
            auth_cfg_json = hint.substr(auth_cfg_hint_prefix.length());
        else {
            secrets_map[hint] = "_";
        }
    }

    if (auth_cfg_json.empty()) {
        g_critical("No auth config hint provided");
        return 1;
    }
    GError *e = nullptr;
    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, auth_cfg_json.c_str(), -1, &e)) {
        g_critical("Failed to parse auth config hint: %s", e->message);
        return 1;
    }
    JsonObject *auth_cfg_obj = json_node_get_object(json_parser_get_root(parser));
    if (!auth_cfg_obj) {
        g_critical("Invalid  auth config hint json: %s", auth_cfg_json.c_str());
        return 1;
    }
    string message = STR(json_object_get_string_member_with_default(auth_cfg_obj, "message", ""));
    string qr_image_b64 = STR(json_object_get_string_member_with_default(auth_cfg_obj, "qr_image", ""));
    g_object_unref(parser);
    if (!external_ui_mode) {
        if (!prompt_gtk_dialog(vpn_name, message, qr_image_b64)) {
            return 3;
        }
        puts_secrets(secrets_map);
        // https://gitlab.gnome.org/GNOME/gnome-shell/-/issues/6690
        wait_for_quit_instruction();
    } else {
        g_debug("Running as external UI mode");
        GKeyFile *keyfile = g_key_file_new();
        g_key_file_set_integer(keyfile, AUTH_EXEC_EXTERNAL_UI_KEYFILE_GROUP, "Version", 2);
        g_key_file_set_string(keyfile, AUTH_EXEC_EXTERNAL_UI_KEYFILE_GROUP, "Description", message.c_str());
        g_key_file_set_string(keyfile, AUTH_EXEC_EXTERNAL_UI_KEYFILE_GROUP, "Title", "Authenticate VPN");
        for (auto entry : secrets_map) {
            string auth_key = entry.first;
            string auth_value = entry.second;
            g_key_file_set_string(keyfile, auth_key.c_str(), "Value", auth_value.c_str());
            g_key_file_set_string(keyfile, auth_key.c_str(), "Label", auth_key.c_str());
            g_key_file_set_boolean(keyfile, auth_key.c_str(), "IsSecret", true);
            g_key_file_set_boolean(keyfile, auth_key.c_str(), "ShouldAsk", allow_interaction);
        }
        string keyfile_content = g_key_file_to_data(keyfile, nullptr, nullptr);
        g_key_file_unref(keyfile);
        cout << keyfile_content << flush;
    }
    return 0;
}

static bool prompt_gtk_dialog(string vpn_name, string message, string qr_image_b64)
{
    gtk_init(0, nullptr);
    GError *error = nullptr;
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), box);
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_resizable(GTK_WINDOW(window), false);
    gtk_window_set_title(GTK_WINDOW(window), g_strdup_printf("Authentication required (%s)", vpn_name.c_str()));
    g_signal_connect(G_OBJECT(window), "destroy", gtk_main_quit, nullptr);
    gtk_widget_set_margin_start(GTK_WIDGET(box), 12);
    gtk_widget_set_margin_end(GTK_WIDGET(box), 12);

    // set message
    GtkWidget *lbl_promt = gtk_label_new(message.c_str());
    gtk_label_set_markup(GTK_LABEL(lbl_promt), message.c_str());
    // GdkRGBA color;
    // gdk_rgba_parse(&color, "blue");
    // gtk_widget_override_background_color(lbl_promt, GTK_STATE_FLAG_NORMAL, &color);
    g_signal_connect(G_OBJECT(lbl_promt),
                     "activate-link",
                     G_CALLBACK(+[](G_GNUC_UNUSED GtkLabel *self, G_GNUC_UNUSED gchar *uri, gpointer user_data) -> bool {
                         g_debug("link cliked: %s", uri);
                         return true;
                     }),
                     window);
    gtk_box_pack_start(GTK_BOX(box), lbl_promt, true, true, 10);

    if (!qr_image_b64.empty()) {
        gsize qr_image_png_bytes_size = 0;
        guchar *qr_image_png_bytes = g_base64_decode(qr_image_b64.c_str(), &qr_image_png_bytes_size);
        GInputStream *stream = g_memory_input_stream_new_from_data(qr_image_png_bytes, qr_image_png_bytes_size, nullptr);
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_stream(stream, nullptr, &error);
        g_free(qr_image_png_bytes);
        if (!pixbuf) {
            g_printerr("Failed to load image: %s\n", error->message);
            g_error_free(error);
            return false;
        }
        GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
        gtk_box_pack_start(GTK_BOX(box), image, true, true, 10);

    } else
        g_debug("No qr_image provided");

    // show time
    gtk_widget_show_all(window);
    gtk_main();
    return true;
}
