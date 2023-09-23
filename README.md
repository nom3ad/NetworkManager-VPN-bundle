
## On Gnome based DE

```bash
make configure build-gtk  dev-install
```

- Run `nm-connection-editor [--uuid=uuid]` to test with GTK

- `cat /etc/NetworkManager/system-connections/test.nmconnection`

- Run `plasmoidviewer -a org.kde.plasma.networkmanagement` to test config editor in Plasma
- Run `systemd-run --user  kded5 --replace` to realod plasma vpn lib
