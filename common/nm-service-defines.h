#pragma once

// #undef __USE_GNU
// #define __USE_GNU  //TODO

// XXX: this is coming from Desktop environment. KDE sets this. what about Gnome?
#define AUTH_CONFIG_HINT_PREFIX "x-vpn-message:"

#define AUTH_EXEC_EXTERNAL_UI_KEYFILE_GROUP "VPN Plugin UI"

#define STR(char_ptr) (char_ptr ? std::string((const char *)char_ptr) : "")
#define BOOL_STR(b) ((b) ? "true" : "false")

#define JOIN_STRING_VEC(array, separator)                                                                                                                      \
    (std::accumulate(array.begin(), array.end(), std::string(), [](std::string &ss, const std::string &s) {                                                    \
        return ss.empty() ? s : ss + separator + s;                                                                                                            \
    }))

#define G_STRING_HASAHTABLE_DUMP_TO(g_hash_table_ptr, ss)                                                                                                      \
    g_hash_table_foreach(                                                                                                                                      \
        g_hash_table_ptr,                                                                                                                                      \
        [](gpointer key, gpointer value, gpointer user_data) {                                                                                                 \
            std::stringstream &ss = *(std::stringstream *)user_data;                                                                                           \
            ss << ((char *)key) << "=" << ((char *)value) << " ";                                                                                              \
        },                                                                                                                                                     \
        &ss);

#define EDITOR_PLUGIN_ERROR (g_quark_from_static_string("nm-connection-error-quark"))

// #undef g_debug
// #undef g_info
// #define g_debug g_warning
// #define g_info g_warning
// #define DEBUG_SET_STDERR_TO_FILE