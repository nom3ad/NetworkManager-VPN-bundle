import enum
import ipaddress
from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Optional, TypedDict


class NMVpnConnectionState(enum.IntEnum):
    """
    VPN connection states

    https://people.freedesktop.org/~lkundrak/nm-dbus-api/nm-vpn-dbus-types.html#NMVpnConnectionState
    """

    # The VPN connection is preparing to connect.
    NM_VPN_CONNECTION_STATE_PREPARE = 1

    # The VPN connection needs authorization credentials.
    NM_VPN_CONNECTION_STATE_NEED_AUTH = 2

    # The VPN connection is being established.
    NM_VPN_CONNECTION_STATE_CONNECT = 3

    # The VPN connection is getting an IP address.
    NM_VPN_CONNECTION_STATE_IP_CONFIG_GET = 4

    # The VPN connection is active.
    NM_VPN_CONNECTION_STATE_ACTIVATED = 5

    # The VPN connection failed.
    NM_VPN_CONNECTION_STATE_FAILED = 6

    # The VPN connection is disconnected.
    NM_VPN_CONNECTION_STATE_DISCONNECTED = 7


class NMVpnConnectionStateReason(enum.IntEnum):
    """VPN connection state reasons"""

    # The reason for the VPN connection state change is unknown.
    NM_VPN_CONNECTION_STATE_REASON_UNKNOWN = 0

    # No reason was given for the VPN connection state change.
    NM_VPN_CONNECTION_STATE_REASON_NONE = 1

    # The VPN connection changed state because the user disconnected it.
    NM_VPN_CONNECTION_STATE_REASON_USER_DISCONNECTED = 2

    # The VPN connection changed state because the device it was using was disconnected.
    NM_VPN_CONNECTION_STATE_REASON_DEVICE_DISCONNECTED = 3

    # The service providing the VPN connection was stopped.
    NM_VPN_CONNECTION_STATE_REASON_SERVICE_STOPPED = 4

    # The IP config of the VPN connection was invalid.
    NM_VPN_CONNECTION_STATE_REASON_IP_CONFIG_INVALID = 5

    # The connection attempt to the VPN service timed out.
    NM_VPN_CONNECTION_STATE_REASON_CONNECT_TIMEOUT = 6

    # A timeout occurred while starting the service providing the VPN connection.
    NM_VPN_CONNECTION_STATE_REASON_SERVICE_START_TIMEOUT = 7

    # Starting the service starting the service providing the VPN connection failed.
    NM_VPN_CONNECTION_STATE_REASON_SERVICE_START_FAILED = 8

    # Necessary secrets for the VPN connection were not provided.
    NM_VPN_CONNECTION_STATE_REASON_NO_SECRETS = 9

    # Authentication to the VPN server failed.
    NM_VPN_CONNECTION_STATE_REASON_LOGIN_FAILED = 10

    # The connection was deleted from settings.
    NM_VPN_CONNECTION_STATE_REASON_CONNECTION_REMOVED = 11


_IPConfig = TypedDict(
    "_IPConfig",
    {
        "address-data": list[str],
        "addresses": list[str],
        "dns": list[str],
        "dns-search": list[str],
        "may-fail": bool,
        "method": str,
        "never-default": bool,
        "route-data": list[str],
        "routes": list[str],
    },
)

VPNConnectionConfiguration = TypedDict(
    "VPNConnectionConfiguration",
    {
        "connection": TypedDict(
            "Connection",
            {
                "id": str,
                "permissions": list[str],
                "type": str,
                "uuid": str,
            },
        ),
        "vpn": TypedDict(
            "VPN",
            {
                "data": TypedDict(
                    "VPNData",
                    {
                        "auth-type": str,
                        "local-ip": str,
                        "netmask": str,
                        "password-flags": str,
                        "port": str,
                        "remote": str,
                        "remote-ip": str,
                    },
                ),
                "secrets": dict,
                "service-type": str,
            },
        ),
        "ipv4": _IPConfig,
        "ipv6": _IPConfig,
        "proxy": dict,
    },
)


@dataclass
class ConnectionResult(ABC):
    gateway: ipaddress.IPv4Address
    ipv4: ipaddress.IPv4Interface
    ipv6: Optional[ipaddress.IPv6Interface] = None
    dns: tuple[ipaddress.IPv4Address | ipaddress.IPv6Address] = ()
    dev: Optional[str] = None
    mtu: int = None
    banner: str = "Beep Beep!"
    never_default_route: bool = True


class VPNConnectionControlBase(ABC):
    def __init__(self, service: "ServiceBase", state_home_dir: str) -> None:
        self.service = service
        self.state_home_dir = state_home_dir

    @abstractmethod
    def start(self, *, connection_uuid: str, connection_name: str, vpn_data: dict[str, str]) -> ConnectionResult:
        pass

    @abstractmethod
    def stop(self):
        pass


class ServiceBase(ABC):
    def __init__(self, ctl: VPNConnectionControlBase, *args) -> None:
        pass

    @abstractmethod
    def prompt_auth(self, prompt: dict, *items) -> None:
        pass
