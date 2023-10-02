import ipaddress
import logging
import sys

from .common import ConnectionResult, VPNConnectionControlBase
from .utils import (
    Subprocess,
    find_valid_if_name,
    get_iface_addresses,
    is_interface_ready,
    timeout,
)


class WeronControl(VPNConnectionControlBase):
    _ready_check_interval_sec = 2
    _ready_timeout_sec = 30
    _proc_weron: Subprocess = None

    def start(self, *, connection_uuid: str, connection_name: str, vpn_data: dict[str, str]):
        dev = self._run_weron(connection_name, vpn_data)
        ipv4, ipv6 = None, None
        for a in get_iface_addresses(dev):
            if a.version == 4:
                ipv4 = a
            else:
                ipv6 = a
        return ConnectionResult(
            ipv4=ipv4,
            ipv6=ipv6,
            # mtu=mtu,
            # dns=dns,
            dev=dev,
            gateway=ipaddress.IPv4Address("255.255.255.255"),  # dummy
        )

    def stop(self):
        if self._proc_weron:
            self._proc_weron.graceful_kill()

    def _run_weron(self, connection_name, vpn_data: dict[str, str]):
        weron_bin = vpn_data.get("weron-bin", "weron").strip()  # ip or ethernet
        mode = "ip"
        if "ETHERNET" in vpn_data.get("mode", "").upper():
            mode = "ethernet"
        weron_cmd = [weron_bin, "vpn", mode]
        weron_cmd.extend(("--community", vpn_data["community"]))
        weron_cmd.extend(("--password", vpn_data["password"]))
        weron_cmd.extend(("--key", vpn_data["key"]))
        dev = vpn_data.get("dev", "").strip() or find_valid_if_name(connection_name)
        weron_cmd.extend(("--dev", dev))
        if mode == "ip":
            weron_cmd.extend(("--ips", vpn_data["ips"]))
            if vpn_data.get("static", "").strip() == "true":
                weron_cmd.extend(("--static",))
        if raddr := vpn_data.get("raddr", "").strip():
            weron_cmd.extend(("--raddr", raddr))
        if vpn_data.get("force-relay", "").strip() == "true":
            weron_cmd.extend(("--force-relay",))
        if ice := vpn_data.get("ice", "").strip():
            weron_cmd.extend(("--ice", ice))
        if verbose := vpn_data.get("verbose", "").strip():
            weron_cmd.extend(("--verbose", verbose))

        stderr = sys.stderr
        if log_file := vpn_data.get("log-file", ""):
            stderr = open(log_file, "wb+")
        logging.info("Run weron: %r", weron_cmd)
        self._proc_weron = Subprocess(weron_cmd, name="weron", stderr=stderr)

        with timeout(self._ready_timeout_sec, description=f"Wait for interface: {dev}") as t:
            while not is_interface_ready(dev):
                if not self._proc_weron.is_running:
                    raise RuntimeError("weron cmd exited prematurely")
                t.sleep(self._ready_check_interval_sec)

        return dev
