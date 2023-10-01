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


class N2NControl(VPNConnectionControlBase):
    _ready_check_interval_sec = 2
    _ready_timeout_sec = 30
    _proc_n2n_edge: Subprocess = None

    def start(self, *, connection_uuid: str, connection_name: str, vpn_data: dict[str, str]):
        dev = self._run_n2n_edge(connection_name, vpn_data)
        ipv4, ipv6 = None, None
        for a in get_iface_addresses(dev):
            if a.version == 4 and not ipv4:
                ipv4 = a
            elif not ipv6:
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
        if self._proc_n2n_edge:
            self._proc_n2n_edge.graceful_kill()

    def _run_n2n_edge(self, connection_name, vpn_data: dict[str, str]):
        edge_bin = vpn_data.get("edge-bin", "edge").strip()  # ip or ethernet
        edge_cmd = [edge_bin, "-f"]  # -f: foreground
        edge_cmd.extend(("-c", vpn_data["community"]))
        edge_cmd.extend(("-k", vpn_data["encryption-key"]))
        if password := vpn_data.get("password", "").strip():
            edge_cmd.extend(("-J", password))
        dev = vpn_data.get("dev", "").strip() or find_valid_if_name(connection_name)
        edge_cmd.extend(("-d", dev))
        if snodes := vpn_data.get("supernodes", "").strip().split(","):
            for l in snodes:
                edge_cmd.extend(("-l", l))
        if static_ip := vpn_data.get("static-ip", "").strip():
            edge_cmd.extend(("-a", static_ip))
        if vpn_data.get("force-relay-via-supernode", "").strip() == "true":
            edge_cmd.append("-S1")
        for _ in range(0, int(vpn_data.get("verbose", "0").strip())):
            edge_cmd.append("-v")

        stderr = sys.stderr
        if log_file := vpn_data.get("log-file", ""):
            stderr = open(log_file, "wb+")
        logging.info("Run n2n edge %r", edge_cmd)
        self._proc_n2n_edge = Subprocess(edge_cmd, name="n2n-edge", stderr=stderr)

        with timeout(self._ready_timeout_sec, description=f"Wait for interface: {dev}") as t:
            while not is_interface_ready(dev):
                t.sleep(self._ready_check_interval_sec)

        return dev