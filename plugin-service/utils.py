import time
import subprocess
import contextlib
import threading
import socket
import struct
import ipaddress
import logging
import re
import json
from typing import Any
from weakref import WeakSet
import atexit
from urllib import request


def ipv4_to_u32(addr: str | ipaddress.IPv4Address):
    return struct.unpack("I", socket.inet_aton(str(addr)))[0]  # native byte-order
    # XX int(ipaddress.IPv4Address(addr)) # big-endian/netowrk-byte-order


def ipv6_to_u8_slice(addr: str | ipaddress.IPv6Address):
    return struct.unpack("16B", socket.inet_pton(socket.AF_INET6, str(addr)))


class timeout:
    def __init__(self, timeout_sec=0, description=None) -> None:
        self.timeout_sec = timeout_sec
        self.starttime = None
        self.description = description or f"After {timeout_sec}s"
        self._sleep_orginal = time.sleep

    @property
    def remaining(self):
        if not self.timeout_sec:
            return float("inf")
        return self.timeout_sec - self.elapsed

    @property
    def elapsed(self):
        if not self.starttime:
            raise RuntimeError("Timer not started yet")
        return time.time() - self.starttime

    @property
    def timedout(self):
        return self.elapsed > self.timeout_sec

    def clear(self):
        if not self.starttime:
            raise RuntimeError("Timer not started yet")
        setattr(self, "sleep", self._sleep_orginal)

    def sleep(self, sec):
        if self.timedout:
            raise TimeoutError(self.description)
        self._sleep_orginal(sec)

    def __enter__(self):
        if self.starttime:
            raise RuntimeError("Timer aleady started")

        self.starttime = time.time()
        return self

    def __exit__(self, exc_type, value, traceback):
        self.starttime = time.time()
        self.clear()


class Subprocess(subprocess.Popen):
    _refs = WeakSet()

    DEFAULT_GRACEFUL_EXIT_TIMEOUT = 3

    @property
    def is_running(self):
        return self.poll() is None

    def __init__(self, *args, name=None, **kwargs):
        logging.debug("Exec() %s", args[0])
        super().__init__(*args, **kwargs)
        Subprocess._refs.add(self)
        self.name = self.args[0] if name is None else name
        self.started_time = time.time()
        self.gracefully_killed = None

    def __repr__(self) -> str:
        r = f"Proc({self.pid}) {self.name}"
        if self.returncode is not None:
            r += f" ec={self.returncode}"
        return f"<{r}>"

    def graceful_kill(self, graceful_exit_timeout=None):
        if graceful_exit_timeout is None:
            graceful_exit_timeout = Subprocess.DEFAULT_GRACEFUL_EXIT_TIMEOUT
        if ec := self.poll():
            logging.warn("Process already exited %r", self)
            return ec
        logging.warn("Terminating %r", self)
        try:
            self.terminate()
            logging.debug("Waiting for %r to die gracefully, timeout=%ds", self, graceful_exit_timeout)
            return self.wait(graceful_exit_timeout)
        except Exception as e:
            if isinstance(e, subprocess.TimeoutExpired):
                logging.warn("%r sigterm timeout. send kill signal", self)
            else:
                logging.exception("%r unexpected error. send kill signal", self)
            self.kill()
            return self.wait()
        finally:
            self.gracefully_killed = True
            logging.warn("Exited %r", self)

    @staticmethod
    @contextlib.contextmanager
    def bg_process(*popenargs, name=None, process_timeout=None, graceful_exit_timeout=None, **kwargs):
        if graceful_exit_timeout is None:
            graceful_exit_timeout = Subprocess.DEFAULT_GRACEFUL_EXIT_TIMEOUT
        _proc_called = threading.Event()
        _proc: Subprocess = None

        def _run():
            nonlocal _proc_called
            nonlocal _proc
            try:
                with Subprocess(popenargs, name=name, **kwargs) as p:
                    _proc = p
                    _proc_called.set()
                    try:
                        p.wait(timeout=process_timeout)
                    except:  # Including KeyboardInterrupt, wait handled that.
                        p.graceful_kill(graceful_exit_timeout)
                        raise
            except Exception as e:
                _proc.__error = e
            finally:
                _proc_called = True

        try:
            t = threading.Thread(target=_run, args=())
            t.start()
            _proc_called.wait(1)
            err = getattr(_proc, "__error", None)
            if err is not None:
                raise err
            yield _proc
        finally:
            if _proc and _proc.is_running:
                _proc.graceful_kill(graceful_exit_timeout)
            t.join(1)

    @staticmethod
    def check_output(*popenargs, name=None, input="", process_timeout=None, graceful_exit_timeout=None, **kwargs):
        with Subprocess(popenargs, name=name, **kwargs, stdin=subprocess.PIPE, stdout=subprocess.PIPE) as p:
            try:
                stdout, stderr = p.communicate(input, timeout=process_timeout)
            except:  # Including KeyboardInterrupt, communicate handled that.
                p.graceful_kill(graceful_exit_timeout)
                # We don't call process.wait() as .__exit__ does that for us.
                raise
            retcode = p.poll()
            if retcode:
                logging.error("Process %r failed with exit code %d| stdout=%s stderr=%s", p, retcode, stdout, stderr)
                raise subprocess.CalledProcessError(retcode, p.args, output=stdout, stderr=stderr)
        return stdout

    @staticmethod
    def check_output_json(*popenargs, name=None, process_timeout=None, graceful_exit_timeout=None, **kwargs) -> Any:
        stdout = Subprocess.check_output(
            *popenargs,
            name=name,
            process_timeout=process_timeout,
            graceful_exit_timeout=graceful_exit_timeout,
            **kwargs,
        )
        try:
            return json.loads(stdout)
        except json.JSONDecodeError:
            logging.error("Failed to decode json: %s", stdout)
            raise

    @staticmethod
    def check_output_text(*popenargs, name=None, process_timeout=None, graceful_exit_timeout=None, **kwargs) -> str:
        stdout = Subprocess.check_output(
            *popenargs,
            name=name,
            process_timeout=process_timeout,
            graceful_exit_timeout=graceful_exit_timeout,
            **kwargs,
        )
        return stdout.decode("utf-8", errors="replace")

    @atexit.register
    @staticmethod
    def _cleanup():
        for p in Subprocess._refs:
            if p.is_running:
                logging.warn("atexit(): Process %r still running. try gracefull kill", p)
                p.graceful_kill(Subprocess.DEFAULT_GRACEFUL_EXIT_TIMEOUT)


def set_proc_name(name: str):
    import ctypes

    libc = ctypes.CDLL("libc.so.6")
    name_ = name.encode("utf-8")[:15]
    NULL = ctypes.c_ulong(0)
    PR_SET_NAME = ctypes.c_ulong(15)
    libc.prctl(PR_SET_NAME, name_, NULL, NULL, NULL)


def as_valid_if_name(s: str):
    return re.sub(r"[^a-zA-Z0-9]", "", s)[:15]


def iter_until(fn, sentinal_value, sentinal_error):
    while True:
        try:
            v = fn()
            if v == sentinal_value:
                return v
            yield v
        except sentinal_error as e:
            return e


def http_rquest(method, url, data, headers=None, timeout=10):
    headers = headers or {}
    if isinstance(data, (dict, list, tuple)):
        data = json.dumps(data).encode("utf-8")
        headers["Content-Type"] = "application/json"
    elif isinstance(data, str):
        data = data.encode("utf-8")
    req = request.Request(url, method=method, data=data, headers=headers)
    try:
        with request.urlopen(req, timeout=timeout) as r:
            resp = r
            return r.read().decode("utf-8")
    except request.HTTPError as e:
        resp = e
        logging.error(
            "HTTP Error - Response:%r | Headers:%r", resp.read().decode("utf-8", errors="replace"), dict(resp.headers)
        )
        raise
    finally:
        logging.debug(
            "http_rquest(%s %s d=%r h=%r) -> (code=%s h=%r",
            method,
            url,
            data,
            headers,
            resp.status if resp else None,
            dict(resp.headers) if resp else None,
        )


def ip_interface_addresses_by_family(addrs):
    ipv4, ipv6 = None, None
    for iface in (ipaddress.ip_interface(a) for a in addrs):
        if iface.version == 4:
            ipv4 = iface
        if iface.version == 6:
            ipv6 = iface
    return ipv4, ipv6


def get_network_interfaces_by_ip(
    ip: ipaddress.IPv4Address | ipaddress.IPv6Address | ipaddress.IPv4Interface | ipaddress.IPv6Interface | str,
):
    import netifaces
    import socket

    try:
        addr = ipaddress.ip_address(ip)
        mask = None
    except ValueError:
        ip_if = ipaddress.ip_interface(ip)
        addr = ip_if.ip
        mask = ip_if.netmask

    interfaces_with_ip = []
    family = socket.AF_INET if addr.version == 4 else socket.AF_INET6
    for iface in netifaces.interfaces():
        for iface_addr in netifaces.ifaddresses(iface).get(family, []):
            print(iface_addr, addr, mask)
            if ipaddress.ip_address(iface_addr["addr"]) == addr and (
                mask is None or ipaddress.ip_address(iface_addr["netmask"]) == mask
            ):
                interfaces_with_ip.append(iface)
    return interfaces_with_ip
