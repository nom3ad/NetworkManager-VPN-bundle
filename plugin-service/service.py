from contextlib import suppress
import importlib
import sys
import os
import threading
from typing import Any
import json

# import dbus
from pathlib import Path
import logging
from pydbus import Variant
from pydbus.generic import signal as Signal
import enum
import signal
import argparse
from dbus.mainloop.glib import DBusGMainLoop

dir = Path(__file__).parent

from .utils import ipv4_to_u32, ipv6_to_u8_slice, set_proc_name
from .common import VPNConnectionControlBase, ServiceBase, VPNConnectionConfiguration

from gi.repository import GLib
from pydbus import SessionBus, SystemBus

loop = GLib.MainLoop()

GLib.threads_init()

# https://developer-old.gnome.org/NetworkManager/stable/gdbus-org.freedesktop.NetworkManager.VPN.Plugin.html
# https://people.gnome.org/~ryanl/glib-docs/gvariant-format-strings.html


class NMVpnPluginFailure(enum.IntEnum):
    # Login failed.
    NM_VPN_PLUGIN_FAILURE_LOGIN_FAILED = 1

    # Connect failed.
    NM_VPN_PLUGIN_FAILURE_CONNECT_FAILED = 2

    # Invalid IP configuration returned from the VPN plugin.
    NM_VPN_PLUGIN_FAILURE_BAD_IP_CONFIG = 3


class NMVpnServiceState(enum.IntEnum):
    """
    VPN daemon states
    https://people.freedesktop.org/~lkundrak/nm-dbus-api/nm-vpn-dbus-types.html#NMVpnServiceState
    """

    # The state of the VPN plugin is unknown.
    NM_VPN_SERVICE_STATE_UNKNOWN = 0

    # The VPN plugin is initialized.
    NM_VPN_SERVICE_STATE_INIT = 1

    # Not used.
    NM_VPN_SERVICE_STATE_SHUTDOWN = 2

    # The plugin is attempting to connect to a VPN server.
    NM_VPN_SERVICE_STATE_STARTING = 3

    # The plugin has connected to a VPN server.
    NM_VPN_SERVICE_STATE_STARTED = 4

    # The plugin is disconnecting from the VPN server.
    NM_VPN_SERVICE_STATE_STOPPING = 5

    # The plugin has disconnected from the VPN server.
    NM_VPN_SERVICE_STATE_STOPPED = 6


class VpnDBUSService(ServiceBase):
    def __init__(self, ctl_impl: type[VPNConnectionControlBase], state_base_dir: str) -> None:
        self.ctl = ctl_impl(self, state_base_dir)
        self._state = NMVpnServiceState.NM_VPN_SERVICE_STATE_UNKNOWN
        self._connect_lock = None

    def Connect(self, connection: VPNConnectionConfiguration):
        """Tells the plugin to connect. Interactive secrets requests (eg, emitting
         the SecretsRequired signal) are not allowed.
        Args:
            connection: Describes the connection to be established.
        """
        logging.warning("Not implemented: Connect() %r", connection)

    def ConnectInteractive(self, connection: VPNConnectionConfiguration, details: dict[str, Any]) -> None:
        """Tells the plugin to connect, allowing interactive secrets requests (eg the
        plugin is allowed to emit the SecretsRequired signal if the VPN service
        indicates that it needs additional secrets during the connect process).
        Args:
            connection: Describes the connection to be established.
            details: Additional details about the Connect process.
        """
        logging.info("ConnectInteractive() %r %r  | connect_lock=%s", connection, details, self._connect_lock)
        if self._connect_lock:
            raise RuntimeError("Aleady connecting!")

        connection_uuid = connection["connection"]["uuid"]
        connection_name = connection["connection"]["id"]
        vpn_data = connection["vpn"]["data"]
        
        def _run():
            self._connect_lock = True
            try:
                result = self.ctl.start(connection_uuid=connection_uuid, connection_name=connection_name, vpn_data=vpn_data)
                logging.info("Connection Control started: %s", result)
                # https://cgit.freedesktop.org/NetworkManager/NetworkManager/tree/libnm-core/nm-dbus-interface.h?id=ba6c2211e8f6aebc5e5b07b77ffee938593980a6
                # https://gitlab.freedesktop.org/NetworkManager/NetworkManager/-/blob/main/src/libnm-core-public/nm-vpn-dbus-interface.h
                general_config = {
                    # VPN interface name (tun0, tap0, etc)
                    "tundev": Variant("s", result.tundev),
                    # "pac": Variant("s", ""),  # Proxy PAC, string
                    #  Login message
                    "banner": Variant("s", result.banner),
                    #  uint32  array of uint8: IP address of the public external VPN gateway (network byte order)
                    "gateway": Variant("u", ipv4_to_u32(result.gateway)),
                    # uint32: Maximum Transfer Unit that the VPN interface should use
                    # "mtu": Variant("u", 1433),
                    #  Has IP4 configuratio
                    "has-ip4": Variant("b", result.ipv4 is not None),
                    # Has IP6 configuratio  boolean
                    "has-ip6": Variant("b", result.ipv6 is not None),
                    # # If %TRUE the VPN plugin can persist/reconnect the connection over link changes and VPN server dropouts.
                    # "can-persist":  Variant("b", False)
                }
                self.emit("Config", general_config)
                if result.ipv4:
                    ip4_config = {
                        # IP address of the internal gateway of the subnet the VPN interface is Fon, if the VPN uses subnet configuration (network byte order)
                        # "internal-gateway":  Variant("u", 0)
                        # Internal IP address of the local VPN interface (network byte order) # XXX Workaround: WTF? why byteorder is not respected.
                        "address": Variant("u", ipv4_to_u32(result.ipv4)),
                        "prefix": Variant(
                            "u", result.ipv4prefix
                        ),  # uint32: IP prefix of the VPN interface; 1 - 32 inclusive
                        # IP addresses of DNS servers for the VPN (network byte order) Array<uint32>:
                        "dns": Variant("au", [ipv4_to_u32(i) for i in result.dns if i.version == 4]),
                        # IP addresses of NBNS/WINS servers for the VPN (network byte order) Array<uint32>:
                        # "nbns": Variant("au", []),
                        # # uint32: Message Segment Size that the VPN interface should use
                        # "mss": Variant("u", 0),
                        # # string: DNS domain name
                        # "domain": Variant("s", ""),
                        # # array of strings: DNS domain names
                        # "domains": Variant("as", []),
                        # # custom routes the client should apply, in the format used
                        # "routes":
                        # # ether the previous IP4 routing configuration should be preserved.
                        # "preserve-routes":  Variant("b", False)
                        # boolean: prevent this VPN connection from ever getting the default route
                        "never-default": Variant("b", result.never_default_route),
                    }
                    self.emit("Ip4Config", ip4_config)
                if result.ipv6:
                    ip6_config = {
                        # /* array of uint8: internal IP address of the local VPN interface (network byte order) */
                        "address": Variant("ay", ipv6_to_u8_slice(result.ipv6)),
                        # /* uint32: prefix length of the VPN interface; 0 - 128 inclusive */
                        "prefix": Variant("u", result.ipv6prefix),
                        # /* array of array of uint8: IP addresses of DNS servers for the VPN (network byte order) */
                        "dns": Variant("aay", [ipv6_to_u8_slice(i) for i in result.dns if i.version == 6]),
                        "never-default": Variant("b", result.never_default_route),
                    }
                    self.emit("Ip6Config", ip6_config)
                self._change_state(NMVpnServiceState.NM_VPN_SERVICE_STATE_STARTED)
            except Exception as e:
                logging.error(
                    "ConnectInteractive is failed: %r. Stopping service", e, exc_info=not isinstance(e, (RuntimeError,))
                )
                self._stop()
            finally:
                self._connect_lock = False

        self._change_state(NMVpnServiceState.NM_VPN_SERVICE_STATE_STARTING)
        # TODO: lock
        threading.Thread(target=_run, daemon=True).start()

    def NeedSecrets(self, connection: VPNConnectionConfiguration) -> str:
        """Asks the plugin whether the provided connection will require secrets to
        connect successfully.
        Args:
            connection: Describes the connection that may need secrets.
        Returns:
            setting_name: The setting name within the provided connection that requires secrets, if any.
        """
        logging.info("NeedSecrets() %r", connection)
        return ""

    def NewSecrets(self, connection: VPNConnectionConfiguration) -> None:
        """
        Called in response to a SecretsRequired signal to deliver updated secrets
        or other information to the plugin.
        in_type="a{sa{sv}}
        Args:
            connection: Describes the connection that may need secrets.
        """

        logging.info("NewSecrets() %r", connection)

    def Disconnect(self) -> None:
        """Disconnect the plugin."""
        logging.info("Disconnect() | connect_lock: %s", self._connect_lock)
        self._stop()
        quit_loop()

    def _stop(self):
        if self._state == NMVpnServiceState.NM_VPN_SERVICE_STATE_STOPPED:
            return
        self._change_state(NMVpnServiceState.NM_VPN_SERVICE_STATE_STOPPING)
        self.ctl.terminate()
        self._change_state(NMVpnServiceState.NM_VPN_SERVICE_STATE_STOPPED)

    def SetConfig(self, config: dict[str, Any]) -> None:
        """Set generic connection details on the connection.
        Args:
            config: in_type="a{sv} Generic configuration details for the connection.
        """
        logging.info("SetConfig() %r", config)

    def SetIp4Config(self, config: dict[str, Any]) -> None:
        """Set IPv4 details on the connection
        Args:
            config: Ip4Config details for the connection. You must call SetConfig() before calling this.
        """
        logging.info("SetIp4Config() %r", config)

    def SetIp6Config(self, config: dict[str, Any]) -> None:
        """Set IPv6 details on the connection.
        Args:
            config: Ip6Config details for the connection. You must call SetConfig() before calling this.
        """
        logging.info("SetIp6Config() %r", config)

    def SetFailure(self, reason: str) -> None:
        """Indicate a failure to the plugin.
        Args:
            reason: The reason for the failure.
        """
        logging.info("SetFailure() %r", reason)

    # property
    @property
    def State(self) -> int:
        """The state of the plugin.
        returns: NMVpnServiceState(int)
        """
        return self._state

    # signals

    # Emitted when the plugin state changes.
    StateChanged = Signal()  # type="u"

    # @message: Informational message, if any, about the request. For example, if a second PIN is required, could indicate to the user to wait for the token code to change until entering the next PIN.
    # @secrets: Array of strings of VPN secret names which the plugin thinks secrets may be required for, or other VPN-specific data to be processed by the VPN's front-end.
    # Emitted during an ongoing ConnectInteractive() request when the plugin has
    # determined that new secrets are required. NetworkManager will then call
    # the NewSecrets() method with a connection hash including the new secrets.
    SecretsRequired = Signal()  # type="s",type="as"

    # @config: The configuration information.
    # The plugin obtained generic configuration information.
    Config = Signal()  # type="a{sv}"

    # @ip4config: The IPv4 configuration.
    # The plugin obtained an IPv4 configuration.
    Ip4Config = Signal()  # type="a{sv}"

    # @ip6config: The IPv6 configuration.
    # The plugin obtained an IPv6 configuration.
    Ip6Config = Signal()  # type="a{sv}"

    #  @banner: The login banner string.
    # Emitted when the plugin receives a login banner from the VPN service.
    LoginBanner = Signal()  # type="s"

    # @reason: (<link linkend="NMVpnPluginFailure">NMVpnPluginFailure</link>) Reason code for the failure.
    # Emitted when a failure in the VPN plugin occurs.
    Failure = Signal()  # type="u"

    def emit(self, signal_name: str, value, *args):
        logging.info("Emitting signal %s: %r : %r", signal_name, value, args)
        if isinstance(value, dict):
            value = {k: v for k, v in value.items() if v is not None}
        if isinstance(value, (list, tuple)):
            value = [v for v in value if v is not None]
        getattr(self, signal_name).emit(value, *args)

    def prompt_auth(self, prompt: dict, *items: str):
        message = json.dumps(prompt, separators=(',', ':'))
        logging.info("prompt_auth() prompt=%r items=%r", prompt, items)
        self.SecretsRequired.emit(message, items)

    def _change_state(self, state: NMVpnServiceState):
        logging.info("change_state() %r -> %r", self._state, state)
        self._state = state
        self.StateChanged.emit(state)  # state == state.value

    def _emit_failure(self, *, login_failed=False, connect_failed=False):
        reason = (
            NMVpnPluginFailure.NM_VPN_PLUGIN_FAILURE_LOGIN_FAILED
            if login_failed
            else NMVpnPluginFailure.NM_VPN_PLUGIN_FAILURE_CONNECT_FAILED
        )
        logging.info("emit_failure() %r", reason)
        self.Failure.emit(reason)


VpnDBUSService.__doc__ = (dir / "nm-vpn-plugin.xml").read_text()


def quit_loop(*a, **k):
    logging.info("Quit Glib main loop")
    loop.quit()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--provider", required=True)
    parser.add_argument("--bus-name", required=False)
    parser.add_argument("--state-base-dir", required=False)
    dbus_object_path = "/org/freedesktop/NetworkManager/VPN/Plugin"
    args, unknown_args = parser.parse_known_args()

    provider = args.provider

    dbus_bus_name = args.bus_name
    if not dbus_bus_name:
        dbus_bus_name = f"org.freedesktop.NetworkManager.{provider}"

    state_base_dir = args.state_base_dir
    if not state_base_dir:
        state_base_dir = f"/etc/NetworkManager/{dbus_bus_name}"

    log_level = logging.INFO
    if os.environ.get("NM_VPN_DEV_MODE", "").lower() in ("true", "1"):
        log_level = logging.DEBUG
    else:
        if int(os.environ.get("NM_VPN_LOG_LEVEL", "0")) > 1:
            log_level = logging.DEBUG

    with suppress(Exception):
        set_proc_name(f"NM:{provider}")
    log_format = f"{provider}(%(filename)s) %(levelname)s: %(message)s"
    if os.environ.get("NM_VPN_LOG_SYSLOG") == "1":
        from logging.handlers import SysLogHandler

        log_handler = SysLogHandler(address="/dev/log")
    else:
        log_format = "%(asctime)s " + log_format
        log_handler = logging.StreamHandler(sys.stderr)
    logging.basicConfig(level=log_level, format=log_format, handlers=[log_handler])

    logging.info("args=%r , unknown_args=%r, environ=%s, ", args, unknown_args, os.environ)

    ctl_class = next(
        c
        for c in vars(importlib.import_module(f"{__package__}.{provider}_ctl")).values()
        if isinstance(c, type) and c is not VPNConnectionControlBase and issubclass(c, VPNConnectionControlBase)
    )
    logging.info("Using provider: %s class=%r", provider, ctl_class)

    signal.signal(signal.SIGTERM, quit_loop)
    with (SystemBus if os.getuid() == 0 else SessionBus)() as bus:
        bus.publish(dbus_bus_name, (dbus_object_path, VpnDBUSService(ctl_class, state_base_dir)))
        loop.run()

    logging.info("Exiting...")
