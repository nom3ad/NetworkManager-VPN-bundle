import logging
import sys
import time
import os
import stat
import select
import re
import ipaddress
import json
import shlex
import subprocess
from .utils import timeout, as_valid_if_name, Subprocess, iter_until
from .common import VPNConnectionControlBase, ServiceBase, ConnectionResult, VPNConnectionConfiguration

_DEFAULT_SOCKPATH = f"/var/run/tailscale/tailscaled.sock"
_DEFAULT_TAILSCALED_UP_TIMEOUT_SEC = 90


class TailscaleControl(VPNConnectionControlBase):
    def __init__(self, state_base_dir: str) -> None:
        self._state_base_dir = state_base_dir
        self._proc_tailscaled: Subprocess = None
        self._proc_tailscale_cli: Subprocess = None
        self.timeout = 60

    def init(self, plugin: ServiceBase):
        logging.info("TailscaleControl init")
        self.plugin = plugin

    def start(self, connection: VPNConnectionConfiguration):
        self._assert_processes_not_running()
        if self._tailscale_sock_is_available():
            raise RuntimeError("tailscale socket is already present")
        connection_uuid = connection["connection"]["uuid"]
        connection_name = connection["connection"]["id"]
        vpn_data = connection["vpn"]["data"]

        self.sock_path = vpn_data.get("sockpath", "").strip() or _DEFAULT_SOCKPATH
        self.tailscale_cli_cmd = ["tailscale", "--socket=" + self.sock_path]
        self.tailscaled_cmd = ["tailscaled", "-socket", self.sock_path]

        if self._state_base_dir:
            self.tailscaled_cmd.extend(("-statedir", os.path.join(self._state_base_dir, connection_uuid)))

        if listening_port := vpn_data.get("listening-port", "").strip():
            try:
                self.tailscaled_cmd.extend(("-port", str(int(listening_port))))
            except:
                raise RuntimeError(f"Invalid port value: '{listening_port}'")

        tundev = vpn_data.get("tun-device-name", "").strip() or as_valid_if_name(connection_name)
        self.tailscaled_cmd.extend(("-tun", tundev))

        if log_verbosity := vpn_data.get("log-verbosity", "").strip():
            self.tailscaled_cmd.extend(("-verbose", log_verbosity))

        if extra_tailscaled_args := vpn_data.get("tailscaled-args", "").strip():
            self.tailscaled_cmd.extend(shlex.split(extra_tailscaled_args))

        logging.info(f"Exec: {self.tailscaled_cmd}")
        stderr = sys.stderr
        if log_file := vpn_data.get("log-file", ""):
            stderr = open(log_file, "wb+")
        self._proc_tailscaled = Subprocess(self.tailscaled_cmd, stderr=stderr)

        logging.info("Wating for tailscaled to be up and running")
        with timeout(self.timeout) as t:
            while True:
                if self._tailscale_sock_is_available():
                    break
            logging.debug("Wating for tailscaled. will timeout after: %ds", t.remaining)
            time.sleep(0.5)

        self._call_cli_up(vpn_data)
        status = self._get_status()
        logging.info("tailscale status: %s", status)
        if status["BackendState"] != "Running":
            raise RuntimeError("Could not up tailscale")
        for ip in (ipaddress.ip_address(ip) for ip in status["Self"]["TailscaleIPs"]):
            if ip.version == 4:
                ipv4 = ip
            if ip.version == 6:
                ipv6 = ip
        result = ConnectionResult(
            # dns=["100.100.100.100"],  if use this, NerworkManager will update /etc/resolv.conf, whcih overwrites changes updated by tailscaled.
            mtu=1280,
            ipv4prefix=32,
            ipv4=ipv4,
            ipv6=ipv6,
            tundev=tundev,
            gateway=ipaddress.IPv4Address("255.255.255.255"),  # dummy
        )

        return result

    def _get_status(self):
        stdout = subprocess.check_output([*self.tailscale_cli_cmd, "status", "--json"], timeout=10)
        status = json.loads(stdout)
        return status

    def _get_ips(self):
        stdout = subprocess.check_output([*self.tailscale_cli_cmd, "ip"], timeout=10)
        ipv4, ipv6 = None, None
        for line in stdout.split(b"\n"):
            try:
                ip = ipaddress.ip_address(line)
                if ip.version == 4:
                    ipv4 = str(ip)
                if ip.version == 6:
                    ipv6 = str(ip)
            except ValueError:
                pass
        return ipv4, ipv6

    def _call_cli_up(self, vpn_data: dict[str, str]):
        tailscale_cli_up_cmd = [*self.tailscale_cli_cmd, "up", "--reset", "--json"]
        up_timeout = vpn_data.get("tailscale-up-timeout", "").strip() or _DEFAULT_TAILSCALED_UP_TIMEOUT_SEC
        tailscale_cli_up_cmd.append(f"--timeout={up_timeout}s")
        tailscale_cli_up_cmd.append("--accept-dns=" + vpn_data.get("is-accept-dns", "false"))
        tailscale_cli_up_cmd.append("--accept-routes=" + vpn_data.get("is-accept-routes", "false"))
        tailscale_cli_up_cmd.append("--ssh=" + vpn_data.get("is-ssh", "false"))
        tailscale_cli_up_cmd.append("--advertise-exit-node=" + vpn_data.get("is-advertise-exit-node", "false"))
        tailscale_cli_up_cmd.append(
            "--exit-node-allow-lan-access=" + vpn_data.get("is-exit-node-allow-lan-access=false", "false")
        )
        tailscale_cli_up_cmd.append("--snat-subnet-routes=" + vpn_data.get("is-snat-subnet-routes", "false"))
        if tags := vpn_data.get("advertise-tags", "").strip():
            tailscale_cli_up_cmd.append("--advertise-tags=" + tags)
        if routes := vpn_data.get("advertise-routes", "").strip():
            tailscale_cli_up_cmd.append("--advertise-routes=" + routes)
        if hostname := vpn_data.get("hostname", "").strip():
            tailscale_cli_up_cmd.append("--hostname=" + hostname)
        if exit_node := vpn_data.get("exit-node"):
            tailscale_cli_up_cmd.append("--exit-node=" + exit_node)
        if extra_tailscale_up_args := vpn_data.get("tailscale-up-args", "").strip():
            tailscale_cli_up_cmd.extend(shlex.split(extra_tailscale_up_args))
        logging.info("Calling tailscale up: %r", tailscale_cli_up_cmd)
        with Subprocess.bg_process(
            tailscale_cli_up_cmd, name="tailscale up", process_timeout=up_timeout + 1, stdout=subprocess.PIPE
        ) as p:
            output = ""
            try:
                auth_url = False
                for line in iter_until(p.stdout.readline, b"", ValueError):
                    output += line.decode("utf-8", errors="replace")
                    try:
                        parsed = json.loads(output)
                        output = ""
                        logging.info("(tailscale up) parsed> %r", parsed)
                        if auth_url := parsed.get("AuthURL"):
                            qr_png_b64 = parsed.get("QR", "").removeprefix("data:image/png;base64,")
                            logging.debug("Found auth url: %s | qr_png=%s", auth_url, qr_png_b64)
                            self._prompt_auth(auth_url, qr_png_b64)
                        if parsed.get("BackendState") == "Running":
                            logging.info("tailscale is already up and running")
                    except json.JSONDecodeError:
                        pass
                    except Exception as e:
                        logging.exception("Unexpected: %r", e)
                        raise
            finally:  # EOF
                ec = p.wait()
                logging.log(logging.INFO if ec == 0 else logging.ERROR, "tailscale up exited with code: %d", ec)
                if output.strip():
                    logging.warn("(tailscale up) stdout> %s", output)
                if ec is not None and ec != 0 and not p.gracefully_killed:  # not due to graceful exit
                    raise RuntimeError(f"While reading stdout, {p} exited")
                return

    def _prompt_auth(self, auth_url: str, qr_png_b64: str) -> None:
        message = f'<b>To authenticate, visit: <i><a href="{auth_url}">{auth_url}</a></i></b>'
        self.plugin.prompt_auth(
            {
                "auth_type": "web_link",
                "message": message,
                "qr_image": qr_png_b64,
            },
            "__dummy__",
        )

    def _tailscale_sock_is_available(self):
        try:
            return stat.S_ISSOCK(os.stat(_DEFAULT_SOCKPATH).st_mode)
        except FileNotFoundError:
            return False

    def _assert_processes_not_running(self):
        if self._proc_tailscaled and self._proc_tailscaled.poll() is not None:
            raise RuntimeError("tailscaled process already running")
        if self._proc_tailscale_cli and self._proc_tailscale_cli.poll() is not None:
            raise RuntimeError("tailscale process already running")

    def terminate(self):
        if self._proc_tailscale_cli:
            self._proc_tailscale_cli.graceful_kill()
            self._proc_tailscale_cli = None
        if self._proc_tailscaled:
            self._proc_tailscaled.graceful_kill()
            self._proc_tailscaled = None
