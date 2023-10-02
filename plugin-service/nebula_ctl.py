import ipaddress
import json
import logging
import os
import sys
from collections import defaultdict
from contextlib import suppress

from .common import ConnectionResult, VPNConnectionControlBase
from .utils import (
    Subprocess,
    find_valid_if_name,
    get_iface_addresses,
    is_interface_ready,
    timeout,
)


class NebulaControl(VPNConnectionControlBase):
    _ready_check_interval_sec = 2
    _ready_timeout_sec = 30
    _proc_nebula: Subprocess = None

    def start(self, *, connection_uuid: str, connection_name: str, vpn_data: dict[str, str]):
        self._config_file = f"{os.getenv('XDG_RUNTIME_DIR','/var/run')}/nebula-nm/config.{connection_uuid}.json"
        os.makedirs(os.path.dirname(self._config_file), exist_ok=True)

        dev = self._run_nebula(connection_name, vpn_data)
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
        if self._proc_nebula:
            self._proc_nebula.graceful_kill()
        with suppress(FileNotFoundError):
            os.unlink(self._config_file)

    def _run_nebula(self, connection_name, vpn_data: dict[str, str]):
        nebula_bin = vpn_data.get("nebula-bin", "nebula").strip()
        nebula_cmd = [nebula_bin, "-config", self._config_file]
        config = defaultdict(dict)
        config["pki"] = {
            "ca": vpn_data["pki-ca"],
            "cert": vpn_data["pki-cert"],
            "key": vpn_data["pki-key"],
        }
        config["static_host_map"] = {vpn_data["lighthouse-overlay-ip"]: [vpn_data["lighthouse-host-port"]]}
        config["lighthouse"] = {
            "am_lighthouse": False,
            "interval": 60,
            "hosts": [vpn_data["lighthouse-overlay-ip"]],
        }
        config["tun"]["dev"] = vpn_data.get("tun-dev", "").strip() or find_valid_if_name(connection_name)
        if logging_level := vpn_data.get("logging-level", "").strip():
            config["logging"]["level"] = logging_level
        if "realy-use_relays" in vpn_data:
            config["realy"]["use_relays"] = vpn_data["realy-use_relays"]
        if listen_host := vpn_data.get("listen-host", "").strip():
            config["listen"]["host"] = listen_host
        if listen_port := int(vpn_data.get("listen-port", "0").strip()):
            config["listen"]["port"] = listen_port

        config["firewall"]["outbound"] = [{"port": "any", "proto": "any", "host": "any"}]
        config["firewall"]["inbound"] = [{"port": "any", "proto": "any", "host": "any"}]

        with open(self._config_file, "w") as f:
            json.dump(config, f, indent=4)

        stderr = sys.stderr
        if log_file := vpn_data.get("log-file", ""):
            stderr = open(log_file, "wb+")
        logging.info("Run nebula %r", nebula_cmd)
        self._proc_nebula = Subprocess(nebula_cmd, name="nebula", stderr=stderr)

        dev = config["tun"]["dev"]
        with timeout(self._ready_timeout_sec, description=f"Wait for interface: {dev}") as t:
            while not is_interface_ready(dev):
                if not self._proc_nebula.is_running:
                    raise RuntimeError("nebula cmd exited prematurely")
                t.sleep(self._ready_check_interval_sec)

        return dev
