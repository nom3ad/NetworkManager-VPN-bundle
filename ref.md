# Networkmanager plugin development

## Links

- <https://fedoramagazine.org/using-python-and-networkmanager-to-control-the-network/>

- Complete example of D-BUS client and server using Qt5 and CMake. : <https://gist.github.com/magthe/2cf7220655bd8bf431259cc7dee99f64>
- pydbus vs. dbus-python  <https://gitlab.freedesktop.org/dbus/dbus-python/-/issues/30>

- <https://mail.gnome.org/archives/networkmanager-list/2017-April/msg00060.html>

- <https://sgros.blogspot.com/2015/12/networkmanager-and-openvpn-how-it-works.html>

- <https://people.freedesktop.org/~lkundrak/nm-dbus-api/nm-vpn-dbus-types.html#NMVpnServiceState>

- <https://developer-old.gnome.org/libnm/stable/ref-overview.html#intro>

- NetworkManager VPN plugins: <https://wiki.gnome.org/Projects/NetworkManager/VPN>

- Implementing a VPN plugin (Rust): <https://mail.gnome.org/archives/networkmanager-list/2017-December/msg00034.html> <https://mail.gnome.org/archives/networkmanager-list/2017-December/msg00038.html>

## Sources

- <https://github.dev/NetworkManager/NetworkManager>
- <https://github.com/seveas/python-networkmanager/blob/main/examples/openvpn_over_ssh>
- <https://github.com/KDE/plasma-nm>
- <https://github.com/LEW21/pydbus>
- <https://github.com/danfruehauf/NetworkManager-ssh> (service + gtk integration with auth dialog)
- <https://github.com/naoki9911/networkmanager-softethervpn>
- <https://github.dev/grahamwhiteuk/NetworkManager-anyconnect/tree/master/properties> small vpn plugin project
- KDE ECM: <https://github.com/KDE/extra-cmake-modules>
- <https://github.com/manuels/wg-p2p-nm-plugin> (In Rust)

## Commands

----

```bash
ls -al /usr/lib/qt/plugins/plasma/network/vpn/plasmanetworkmanagement_*
ldd /usr/lib/qt/plugins/plasma/network/vpn/plasmanetworkmanagement_foobarui.so 

plasmashell --replace  # for reflecting in nm applet (may be not required at all)
kded5 --replace     # for actually relaoding everything
journalctl -u NetworkManager -f 

sudo systemctl restart NetworkManager

cat /etc/NetworkManager/system-connections/ts-dev.nmconnection 

# t0 start GTK based connection editor UI
nm-connection-editor  #In Arch its priovided by man nm  libnm-gtk (old) or  nm-connection-editor

# dump connection info
nmcli connection show <connection-name> | grep vpn 


# debian deps
apt install ninja-build cmake make gcc pkg-config libnm-dev libgtk-3-dev python3-pydbus


```

## Files

### sample service file

```plain
       │ File: /usr/lib/NetworkManager/VPN/nm-<provider>-service.name
───────┼────────────────────────────────────────────────────────────
   1   │ [VPN Connection]
   2   │ name=<provider>
   3   │ service=org.freedesktop.NetworkManager.<provider>
   4   │ program=/usr/lib/nm-<provider>-service
   5   │ supports-multiple-connections=true
   6   │ 
   7   │ [libnm]
   8   │ plugin=libnm-vpn-plugin-<provider>.so
   9   │ 
  10   │ [GNOME]
  11   │ auth-dialog=/usr/lib/nm-<provider>-auth-dialog
  12   │ properties=libnm-<provider>-properties
  13   │ supports-external-ui-mode=true
  14   │ supports-hints=true
```

- `/etc/dbus-1/system.d/nm-<provider>-service.conf, /usr/share/dbus-1/system.d/nm-<provider>-service.conf`   Allow system dbus access for the service

- `/etc/NetworkManager/system-connections/<connection-name>.nmconnection`  - Stores connection

- `/etc/NetworkManager/VPN/nm-<provider>-service.name, /usr/lib/NetworkManager/VPN/nm-<provider>-service.name`    VPN plugin discovery is based on this config file (Both Frontends and NetworkManager might be using this)

- `/usr/lib/nm-<provider>-service`     Executable for vpn service process (started by Networkmanager. passes dbus name for connection)

- `/usr/lib/nm-<provider>-auth-dialog`  Executable to show auth dialog (for GNOME integration)

- `/usr/lib/NetworkManager/libnm-vpn-plugin-<provider>.so`   Lib for GNOME based integration. Wraps dbus calls as objects

- `/usr/lib/NetworkManager/libnm-<provider>-properties`   Plugin properties editor (GNOME integration)

- `/usr/lib/qt/plugins/plasma/network/vpn/plasmanetworkmanagement_<provider>ui.so`   Lib for KDE plasma UI integration.

In KDE single lib `plasmanetworkmanagement_<provider>ui.so` is required instead of `libnm-vpn-plugin-<provider>.so`, `nm-<provider>-auth-dialog` and `libnm-<provider>-properties`.

`nm-<provider>-service` service executable is independent of DE environment (as it is directly used by NetworkManager service)

## Knowledge

*adapted from  <https://github.com/naoki9911/networkmanager-softethervpn/edit/master/README.md>*

### Service (the Plugin itself)

The service is responsible for setting up a VPN connection with the supplied parameters. For this, it has to implement a [D-Bus interface](https://developer.gnome.org/NetworkManager/stable/gdbus-org.freedesktop.NetworkManager.VPN.Plugin.html) and listen to incoming requests, which will be sent by NetworkManager in due time (i.e. when the user tells NM to set up the appropriate VPN connection).  
If the binary service is not running at the time when NM wants to set up the connection, it will try to start the binary ad hoc.

In principle, this piece of software can be written in any language, but in order to make the implementation sane, there should at least exist convenient D-Bus bindings for the language. Further, there are parts of the code already implemented in C, which might make it more convenient to just stick to that.

### Auth-Dialog

The auth-dialog is responsible for figuring out missing bits of required sensitive information (such as passwords).

It reads the required secrets (and bits of data) for the VPN connection from STDIN in a key/value pair format (see below), until the string "DONE" occurs.  
If there are still secrets (i.e. passwords) that are required but not supplied (which passwords are required can be determined by looking at the supplied `hints` flags), the auth-dialog will check if the keyring contains those secrets.  
If there are still secrets missing (and user interaction is allowed per flag), a GTK dialog will be built up in order to prompt the user for passwords.

After all is said and done, the binary writes the found secrets to STDOUT (in a line-based format, as seen below) and waits for "QUIT" to be read from STDIN before exiting.

The behaviour of the binary can be modified by passing various options:

- `-u UUID`: The UUID of the VPN connection, used for looking up secrets from the keyring
- `-n NAME`: The name of the VPN connection, shown on the popup dialog
- `-s SERVICE`: Specifies the name of the VPN service, e.g. `org.freedesktop.NetworkManager.openvpn` (used to check for compatibility)
- `-i`: Allow interaction with the user (i.e. allow a GUI dialog to be created)
- `--external-ui-mode`: Give a textual description of the dialog instead of creating a GTK dialog
- `-r`: Force the creation of a dialog, even if all passwords were already found
- `-t HINT`: Give hints about what passwords are required

Example input:

```text
DATA_KEY=key
DATA_VAL=value
DATA_KEY=another-key
DATA_VAL=another-value
SECRET_KEY=password
SECRET_VAL=verysecurepassword
DONE
```

Example output:

```text
password
verysecurepassword
```

### Connection Editor Plugin

The Connection Editor Plugin is responsible for providing a GUI inside NetworkManager where all relevant properties for a VPN connection can be specified. If you don't know what I'm talking about, just think about the GUI where you entered the information needed to connect to your local Wifi. That's probably pretty similar.

The Editor Plugin is also responsible for providing means of importing and exporting VPN connections from and to external files in a custom format.

NetworkManager integrates the VPN editors by looking up *shared objects* in the above mentioned configuration file and accessing them at run-time.  
This means however that the editor plugin GUI has to be provided by a shared object, which means that the editor cannot be written in just any language.

### Storage of the Connections

Saved connections are stored in `/etc/NetworkManager/system-connections`, with owner `root:root` and access permissions `0700`.  
This guarantees that nobody can have a look at the saved system-wide connections (and their stored secrets) that isn't supposed to.

An example of such a system-connection file would be (one can see that the user-input data is stored as key-value pairs with internally used keys in the vpn section):

```ini
[connection]
id=wiretest
uuid=8298d5ea-73d5-499b-9376-57409a7a2331
type=vpn
autoconnect=false
permissions=

[vpn]
local-ip4=192.168.1.2/24
local-listen-port=51820
local-private-key=CBomGS37YC4ak+J2+NPuHtmgIk6gC7yQZKHnboJd3F8=
peer-allowed-ips=192.168.1.254
peer-endpoint=8.16.32.11:51820
peer-public-key=GRk7K3A3JCaoVN1ZhFEtEvyU6+g+FdGaCtSObIYvXX0=
service-type=org.freedesktop.NetworkManager.wireguard

[vpn-secrets]
password
verysecurepassword

[ipv4]
dns-search=
method=auto

[ipv6]
addr-gen-mode=stable-privacy
dns-search=
ip6-privacy=0
method=auto
```

## Resources

NM VPN Plugin:  
<https://developer.gnome.org/libnm-glib/stable/libnm-glib-NMVPNPlugin.html>  
<https://developer.gnome.org/NetworkManager/stable/gdbus-org.freedesktop.NetworkManager.VPN.Plugin.html>  

Settings VPN (sent via DBus on Connect(a{sa{sv}}) method):  
<https://developer.gnome.org/libnm/stable/NMSettingVpn.html#nm-setting-vpn-get-data-item>
