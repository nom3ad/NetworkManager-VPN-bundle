import ipaddress
import json
import logging
import os
import re
import shutil
import sys
from collections import defaultdict
from contextlib import suppress

from .common import ConnectionResult, VPNConnectionControlBase
from .utils import (
    Subprocess,
    find_valid_if_name,
    get_iface_addresses,
    getter,
    is_interface_ready,
    timeout,
)


class TincControl(VPNConnectionControlBase):
    _ready_check_interval_sec = 2
    _ready_timeout_sec = 30
    _proc_tincd: Subprocess = None

    def start(self, *, connection_uuid: str, connection_name: str, vpn_data: dict[str, str]):
        self._config_dir = f"{os.getenv('XDG_RUNTIME_DIR','/var/run')}/tinc-nm/config.{connection_uuid}"
        os.makedirs(self._config_dir, exist_ok=True)
        dev, routes = self._run_tincd(connection_name, vpn_data)
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
            routes=tuple(routes),
            dev=dev,
            gateway=ipaddress.IPv4Address("255.255.255.255"),  # dummy
        )

    def stop(self):
        if self._proc_tincd:
            self._proc_tincd.graceful_kill()
        # with suppress(FileNotFoundError):
        #     shutil.rmtree(self._config_dir)

    def _run_tincd(self, connection_name, vpn_data: dict[str, str]):
        vpn_data_get = getter(vpn_data)

        tinc_bin = vpn_data_get("tincd-bin", "tincd")  # ip or ethernet
        debug_level = int(vpn_data_get("debug-level", 1))
        node_name = vpn_data_get("node-name", "$HOST")
        tincd_cmd = [tinc_bin, "--config", self._config_dir, "--no-detach", "--debug", str(debug_level)]

        device_type = "tun"
        if "ETHERNET" in vpn_data_get("net-mode", "").upper():
            device_type = "tap"
        dev = vpn_data_get("dev") or find_valid_if_name(connection_name)
        listen_port = vpn_data_get("listen-port", 655)
        server_conf = {
            "Name": node_name,
            "DeviceType": device_type,
            "Interface": dev,
            "AddressFamily": "any",
            "BindToAddress": f"* {listen_port}",
        }
        rsa_private_key_file = vpn_data_get("rsa-private-key", ...)
        server_conf["PrivateKeyFile"] = rsa_private_key_file
        if additional_server_cfg := vpn_data_get("additional-server-conf"):
            server_conf.update(self._parse_config_entries(additional_server_cfg))

        cidrs = [ipaddress.ip_interface(s) for s in vpn_data_get("cidrs", ..., split=", ")]

        external_address = vpn_data_get("external-address", ...)
        if ":" not in external_address:
            external_address = f"{external_address}:{listen_port}"
        host_conf = {
            "Address": external_address.replace(":", " "),
            "Subnet": [str(cidr.network) for cidr in cidrs],
        }
        if additional_host_cfg := vpn_data_get("additional-host-conf"):
            host_conf.update(self._parse_config_entries(additional_host_cfg))

        tinc_up_file = os.path.join(self._config_dir, "tinc-up")
        with open(tinc_up_file, "w") as f:
            f.write("#!/bin/sh\nset -ex\n")
            for cidr in cidrs:
                if cidr.network.network_address != cidr.ip or (
                    cidr.version == 4 and cidr.network.prefixlen == 32
                ):  # CIDR is not a network address
                    f.write(f"ip addr add {cidr.with_prefixlen} dev $INTERFACE\n")
        os.chmod(tinc_up_file, 0o755)

        hosts_dir = os.path.join(self._config_dir, "hosts")
        os.makedirs(hosts_dir, exist_ok=True)

        routes: set[ipaddress.IPv4Network | ipaddress.IPv6Address] = set()
        for peer_name, peer_cfg in self._parse_peer_config(vpn_data_get("peers", [])).items():
            peer_cfg_content = self._stringify_conf(peer_cfg["entries"]) + "\n" + peer_cfg["public_key"]
            logging.debug("tinc peer host config: %s: %r", peer_name, peer_cfg_content)
            peer_host_file = os.path.join(hosts_dir, peer_name)
            for cidr in peer_cfg["entries"]["Subnet"]:
                routes.add(ipaddress.ip_network(cidr))
            with open(peer_host_file, "w") as f:
                f.write(peer_cfg_content)

        server_conf_file = os.path.join(self._config_dir, "tinc.conf")
        server_conf_content = self._stringify_conf(server_conf)
        logging.debug("tinc server config: %r", server_conf_content)
        with open(server_conf_file, "w") as f:
            f.write(server_conf_content)

        host_conf_file = os.path.join(hosts_dir, node_name)
        host_conf_content = self._stringify_conf(host_conf)
        logging.debug("tinc host config: %r", host_conf_content)
        with open(host_conf_file, "w") as f:
            f.write(host_conf_content)

        stderr = sys.stderr
        if log_file := vpn_data_get("log-file"):
            stderr = open(log_file, "wb+")
        logging.info("Run tincd: %r", tincd_cmd)
        self._proc_tincd = Subprocess(tincd_cmd, name="tincd", stderr=stderr)

        with timeout(self._ready_timeout_sec, description=f"Wait for interface: {dev}") as t:
            while not is_interface_ready(dev):
                if not self._proc_tincd.is_running:
                    raise RuntimeError("tincd cmd exited prematurely")
                t.sleep(self._ready_check_interval_sec)

        return dev, routes

    def _parse_peer_config(self, peer_configs_serilaized):
        peers = {}
        peerr_configs = json.loads(peer_configs_serilaized)
        for config_str in peerr_configs:
            try:
                entries = defaultdict(list)
                peer_name = ""
                public_key = ""
                expected = "name"
                for w in config_str.split(" "):
                    w = w.strip()
                    if not w or w == "<edit>":
                        continue
                    if expected == "name":
                        if not re.match(r"^[a-zA-Z0-9_-]+$", w):
                            raise ValueError(f"Invalid peer name: {w}")
                        peer_name = w
                        expected = "address"
                        continue
                    if expected == "address":
                        entries["Address"] = w.replace(":", " ")
                        expected = "subnet_csv"
                        continue
                    elif expected == "subnet_csv":
                        entries["Subnet"] = w.split(",")
                        expected = "kv_csv"
                        continue
                    elif expected == "kv_csv":
                        try:
                            for kv in w.split(","):
                                k, v = kv.split("=")
                                k = k.strip()
                                v = v.strip()
                                if v not in entries[k]:
                                    entries[k].append(v)
                            continue
                        except ValueError:
                            pass
                        finally:
                            expected = "publickey"
                    assert expected == "publickey"
                    public_key += w
                public_key = public_key.strip().replace("\\n", "\n").replace(" ", "\n")
                if "--BEGIN" not in public_key:
                    public_key = f"-----BEGIN RSA PUBLIC KEY-----\n{public_key}\n-----END RSA PUBLIC KEY-----"
                if peer_name:
                    peers[peer_name] = {"public_key": public_key, "entries": entries}
            except Exception as e:
                raise ValueError(f"Could not parse peer config: {config_str}") from e
        return peers

    def _parse_config_entries(self, confg_entries_serilaized):
        entries = defaultdict(list)
        try:
            for kv in json.loads(confg_entries_serilaized):
                if not kv.strip() or kv == "<edit>":
                    continue
                k, v = kv.split("=")
                k = k.strip()
                v = v.strip()
                if k and v and v not in entries[k]:
                    entries[k].append(v)
        except Exception as e:
            raise ValueError(f"Invalid config entries: {confg_entries_serilaized}") from e
        return entries

    def _stringify_conf(self, entries):
        content = ""
        for k, v in entries.items():
            if isinstance(v, list):
                for i in v:
                    content += f"{k} = {i}\n"
            else:
                content += f"{k} = {v}\n"

        return content
