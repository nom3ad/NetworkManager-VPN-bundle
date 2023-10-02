import ipaddress
import json
import logging

from .common import ConnectionResult, VPNConnectionControlBase
from .utils import (
    Subprocess,
    getter,
    http_rquest,
    ip_interface_addresses_by_family,
    timeout,
)


class ZeroTierControl(VPNConnectionControlBase):
    _status_check_interval_sec = 2
    _join_timeout_sec = 30
    _cli_invoke_timeout_sec = 30

    def start(self, *, connection_uuid: str, connection_name: str, vpn_data: dict[str, str]):
        vpn_data_get = getter(vpn_data)
        self.network_id = vpn_data_get("network-id")
        if not self.network_id:
            raise RuntimeError("network-id is not specified")
        self.api_token = vpn_data_get("api-token")
        self.primray_port = int(vpn_data_get("primary-port") or 9993)
        self.service_work_dir = vpn_data_get("service-working-directory") or "/var/lib/zerotier-one"
        self._zerotier_service_cmd = ["zerotier-one"]
        self._zerotier_cli_cmd = ["zerotier-one", "-q", f"-D{self.service_work_dir}", f"-p{self.primray_port}"]

        logging.info("Zerotier version: %s", self._cli_version())
        state = self._cli_join(self.network_id)
        logging.info("Join state: %s", state)
        member_info = self._cli_info()
        logging.debug("Member info: %s", member_info)
        self.member_id = member_info["address"]
        has_found_access_denied = False
        with timeout(self._join_timeout_sec, description="wait for connection OK") as t:
            # [Literal["REQUESTING_CONFIGURATION", "ACCESS_DENIED", "OK"]]
            while state["status"] != "OK":
                state = self._network_state(self.network_id)
                logging.debug("Wating for status OK.  current state: %s", state)
                if state["status"] == "ACCESS_DENIED" and not has_found_access_denied:
                    has_found_access_denied = True
                    if self.api_token:
                        logging.info("Status is ACCESS_DENIED. Self authorize using given API token")
                        resp = self._api_authorize_memeber(self.network_id, self.member_id, self.api_token)
                        logging.debug("Self authorize API response: %s", resp)
                    else:
                        self._prompt_auth()
                t.sleep(self._status_check_interval_sec)

        ipv4, ipv6 = ip_interface_addresses_by_family(state["assignedAddresses"])
        dns = [ipaddress.ip_address(n) for n in state["dns"]["servers"]]
        mtu = state["mtu"]

        # iface_in_devicemap_file = None
        # try:
        #     for line in pathlib.Path(self.service_work_dir, "devicemap").read_text().splitlines():
        #         if line.startswith(f"{self.network_id}="):
        #             iface_in_devicemap_file = line.split("=")[1].strip()
        #             break
        # except Exception:
        #     pass

        # for iface in get_network_interfaces_by_ip(ipv4 or ipv6):
        #     if iface_in_devicemap_file is None or iface == iface_in_devicemap_file:
        #         dev = iface
        #         break
        # else:
        #     raise RuntimeError(f"failed to find dev having ip {ipv4 or ipv6} | {iface_in_devicemap_file=}")

        dev = state["portDeviceName"]

        return ConnectionResult(
            ipv4=ipv4,
            ipv6=ipv6,
            mtu=mtu,
            # dns=dns,
            dev=dev,
            gateway=ipaddress.IPv4Address("255.255.255.255"),  # dummy
        )

    def stop(self):
        if hasattr(self, "network_id"):
            resp = self._cli_leave(self.network_id)
            logging.debug("Leave action esponse: %s", resp)

    def _prompt_auth(self):
        page_url = f"https://my.zerotier.com/network/{self.network_id}"
        message = f'<b>Please authorize member <i>{self.member_id}</i><br>visit: <i><a href="{page_url}">{page_url}</a></i></br>'
        self.service.prompt_auth(
            {
                "auth_type": "web_link",
                "message": message,
            },
            "__dummy__",
        )

    def _network_state(self, network_id):
        for n in self._cli_listnetworks():
            if n["id"] == network_id:
                return n
        return None

    def _cli_version(self):
        ver = Subprocess.check_output_text(
            *self._zerotier_service_cmd, "-v", process_timeout=self._cli_invoke_timeout_sec
        ).strip()
        return ver

    def _cli_join(self, network_id: str):
        """
        eg:
        {"allowDNS":false,"allowDefault":false,"allowGlobal":false,"allowManaged":true,"assignedAddresses":[],"bridge":false,"broadcastEnabled":false,"dhcp":false,"dns":{"domain":"","servers":[]},"id":"1e2938dfd34a83ee","mac":"ee:e6:cb:c8:76:41","mtu":2800,"multicastSubscriptions":[],"name":"","netconfRevision":0,"nwid":"9e1948db634a83ee","portDeviceName":"ztiv5aewen","portError":0,"routes":[],"status":"REQUESTING_CONFIGURATION","type":"PRIVATE"}
        """
        return Subprocess.check_output_json(
            *self._zerotier_cli_cmd, "-j", "join", network_id, process_timeout=self._cli_invoke_timeout_sec
        )

    def _cli_leave(self, network_id: str):
        return Subprocess.check_output_json(
            *self._zerotier_cli_cmd, "-j", "leave", network_id, process_timeout=self._cli_invoke_timeout_sec
        )

    def _cli_info(self):
        return Subprocess.check_output_json(
            *self._zerotier_cli_cmd, "-j", "info", process_timeout=self._cli_invoke_timeout_sec
        )

    def _cli_listnetworks(self):
        return Subprocess.check_output_json(
            *self._zerotier_cli_cmd, "-j", "listnetworks", process_timeout=self._cli_invoke_timeout_sec
        )

    def _api_authorize_memeber(self, network_id: str, member_id: str, api_token: str):
        # call POST: https://my.zerotier.com/api/v1/network/{netowrk_id}/member/{member_id}
        resp_text = http_rquest(
            "POST",
            f"https://my.zerotier.com/api/v1/network/{network_id}/member/{member_id}",
            data={"hidden": False, "config": {"authorized": True}},
            headers={"Authorization": f"Bearer {api_token}"},
        )
        try:
            return json.loads(resp_text)
        except Exception:
            logging.error("failed to api response: %s", resp_text)
            raise
