#pragma once

#include <string>

using namespace std;

static bool handle_prompt(string vpn_name, string message, string qr_image_b64);
static bool prompt_gtk_dialog(string vpn_name, string message, string qr_image_b64);

static int do_when_no_hints(string vpn_name, string vpn_uuid, string vpn_service,  bool allow_interaction, bool external_ui_mode);
static int do_when_hints(string vpn_name, string vpn_uuid, string vpn_service,  bool allow_interaction, bool external_ui_mode, char **hints_cstr_array);
