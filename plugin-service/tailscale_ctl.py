import json
import logging
import os
import shlex
import stat
import subprocess
import sys
from ipaddress import ip_address

from .common import ConnectionResult, VPNConnectionControlBase
from .utils import (
    Subprocess,
    find_valid_if_name,
    ip_interface_addresses_by_family,
    iter_until,
    timeout,
)

_DEFAULT_SOCKPATH = f"/var/run/tailscale/tailscaled.sock"
_DEFAULT_TAILSCALED_UP_TIMEOUT_SEC = 90


class TailscaleControl(VPNConnectionControlBase):
    _proc_tailscaled: Subprocess = None
    _proc_tailscale_cli: Subprocess = None
    _tailscale_socket_appear_timeout_sec = 60

    def start(self, *, connection_uuid: str, connection_name: str, vpn_data: dict[str, str]):
        self._assert_processes_not_running()
        if _sockpath := vpn_data.get("sockpath", "").strip():
            _sockpath = _DEFAULT_SOCKPATH if _sockpath == "default" else _sockpath
        else:
            _sockpath = f"{os.getenv('XDG_RUNTIME_DIR','/var/run/tailscale')}/tailscaled.{connection_uuid}.sock"
        self._sockpath = _sockpath
        if self._tailscale_sock_is_available():
            raise RuntimeError("tailscale socket is already present")
        os.makedirs(os.path.dirname(_sockpath), exist_ok=True)

        self.tailscale_cli_cmd = ["tailscale", "--socket=" + self._sockpath]
        self.tailscaled_cmd = ["tailscaled", "-socket", self._sockpath]

        if self.state_home_dir:
            self.tailscaled_cmd.extend(("-statedir", os.path.join(self.state_home_dir, connection_uuid)))

        if listening_port := vpn_data.get("listening-port", "").strip():
            try:
                self.tailscaled_cmd.extend(("-port", str(int(listening_port))))
            except:
                raise RuntimeError(f"Invalid port value: '{listening_port}'")

        dev = vpn_data.get("tun-device-name", "").strip() or find_valid_if_name(connection_name)
        self.tailscaled_cmd.extend(("-tun", dev))

        if log_verbosity := vpn_data.get("log-verbosity", "").strip():
            self.tailscaled_cmd.extend(("-verbose", log_verbosity))

        if extra_tailscaled_args := vpn_data.get("tailscaled-args", "").strip():
            self.tailscaled_cmd.extend(shlex.split(extra_tailscaled_args))

        logging.info(f"Exec: {self.tailscaled_cmd}")
        stderr = sys.stderr
        if log_file := vpn_data.get("log-file", ""):
            stderr = open(log_file, "wb+")
        self._proc_tailscaled = Subprocess(self.tailscaled_cmd, name="tailscaled", stderr=stderr)

        logging.info("Wating for tailscaled to be up and running")
        with timeout(self._tailscale_socket_appear_timeout_sec) as t:
            while True:
                if self._tailscale_sock_is_available():
                    break
            logging.debug("Wating for tailscaled. will timeout after: %ds", t.remaining)
            t.sleep(0.5)

        self._call_cli_up(vpn_data)
        status = self._get_status()
        logging.info("tailscale status: %s", status)
        if status["BackendState"] != "Running":
            raise RuntimeError("Could not up tailscale")
        ipv4, ipv6 = ip_interface_addresses_by_family(status["Self"]["TailscaleIPs"])
        result = ConnectionResult(
            # dns=[ip_address("100.100.100.100")],  if use this, NerworkManager will update /etc/resolv.conf, whcih overwrites changes updated by tailscaled.
            mtu=1280,
            ipv4=ipv4,
            ipv6=ipv6,
            dev=dev,
            gateway=ip_address("255.255.255.255"),  # dummy
        )

        return result

    def _get_status(self):
        status = Subprocess.check_output_json(*self.tailscale_cli_cmd, "status", "--json", process_timeout=10)
        return status

    def _get_ips(self):
        stdout = Subprocess.check_output_text(*self.tailscale_cli_cmd, "ip", process_timeout=10)
        addresses = []
        for line in stdout.split("\n"):
            addresses.append(ip_address(line))
        return addresses

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
            *tailscale_cli_up_cmd, name="tailscale up", process_timeout=up_timeout + 1, stdout=subprocess.PIPE
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
        self.service.prompt_auth(
            {
                "auth_type": "web_link",
                "message": message,
                "qr_image": qr_png_b64,
            },
            "__dummy__",
        )

    def _tailscale_sock_is_available(self):
        try:
            return stat.S_ISSOCK(os.stat(self._sockpath).st_mode)
        except FileNotFoundError:
            return False

    def _assert_processes_not_running(self):
        if self._proc_tailscaled and self._proc_tailscaled.poll() is not None:
            raise RuntimeError("tailscaled process already running")
        if self._proc_tailscale_cli and self._proc_tailscale_cli.poll() is not None:
            raise RuntimeError("tailscale process already running")

    def stop(self):
        if self._proc_tailscale_cli:
            self._proc_tailscale_cli.graceful_kill()
            self._proc_tailscale_cli = None
        if self._proc_tailscaled:
            self._proc_tailscaled.graceful_kill()
            self._proc_tailscaled = None
