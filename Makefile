export LC_ALL=C

AUTH_CONFIG_HINT_PREFIX=x-vpn-message:
TEST_VPN_MESSAGE=Please authenticate using: <b><a href=\'http://example.com\'>http://example.com</a></b>
TEST_VPN_QR_IMAGE_B64=iVBORw0KGgoAAAANSUhEUgAAAIAAAACAAQMAAAD58POIAAAABlBMVEX///8AAABVwtN+AAABQklEQVR42uTUMY6kMBAF0G85cNbcgL5GB5Z8JYdkdkbIlSwRcA24gTtz0Kq/aqZHQ7QYT7Zb4QsQ1Kc+/rPRpLjMFODItRESehdVglioSljYP7r7HMWG5y/ATJx5Ddy06jH/AhJ6k7c53g4fdwKkPPw2j7fDCi/CewwFRg7p/R10CixBpeGZhk9QZwD4bckbx6zL5zHXwU1cMl8Gdsh1oBO25BVL1uP3Cq8CEEgqRjLWwgNqmcTx+09uAL3EO+PGIjo+6wBmUgXva9ClGRDujHwBevzEfQY6+dVl9TJMg7RC8WIiX4NY8xVDBWAFOMesx6+wm6B/+N6GjpQ62LPqSN4QUAf7zYXeQmw77E0ndhAM3VoH72LzvYv7q7SDy+TYpZ+CPQcEsYZzYSsk9PCwwKEcT2DfmEo/LXUd/uH5EwAA//9iCrA7VlI+bwAAAABJRU5ErkJggg==
TEST_VPN_NAME=test-vpn
TEST_VPN_SERVICE=org.freedesktop.NetworkManager.test
TEST_VPN_UUID=$(shell nmcli connection show $(TEST_VPN_NAME) | (grep connection.uuid || echo '_ missing!') | awk '{ print $$2 }')
AUTH_CONFIG_HINT=$(AUTH_CONFIG_HINT_PREFIX)$(shell jq -cn --arg message "$(TEST_VPN_MESSAGE)" --arg qr_image "$(TEST_VPN_QR_IMAGE_B64)" '{message:$$message,qr_image:$$qr_image}' | sed 's|"|\\"|g')
VPN_BUNDLE_INCLUDED_PROVIDERS ?= all
VPN_BUNDLE_GTK_VERSION ?= detected
VPN_BUNDLE_DISABLE_BUILD_GTK_PLUGIN ?= OFF
VPN_BUNDLE_DISABLE_BUILD_PLASMA_PLUGIN ?= OFF

all: configure
	ninja -C build all

cmake-gui:
	mkdir -p build && cmake-gui -S . -B build

full: all cpack makepkg

venv:
	virtualenv  .venv --system-site-packages

venv-pip:
	pip install -r requirements.txt

clean:
	rm -rf build/

configure:
	mkdir -p build && cd build && cmake .. -G Ninja -DCMAKE_INSTALL_PREFIX=/usr \
		-DVPN_BUNDLE_INCLUDED_PROVIDERS=$(VPN_BUNDLE_INCLUDED_PROVIDERS) \
		-DVPN_BUNDLE_GTK_VERSION=$(VPN_BUNDLE_GTK_VERSION) \
		-DVPN_BUNDLE_DISABLE_BUILD_GTK_PLUGIN=$(VPN_BUNDLE_DISABLE_BUILD_GTK_PLUGIN) \
		-DVPN_BUNDLE_DISABLE_BUILD_PLASMA_PLUGIN=$(VPN_BUNDLE_DISABLE_BUILD_PLASMA_PLUGIN)

build-plasma:
	ninja -C build $$(ninja -C build -t targets | grep -oE "^plasmanetworkmanagement_\w+ui" | sort | uniq)

build-gtk: build-auth-dialog
	ninja -C build $$(ninja -C build -t targets | grep -oE "^libnm-vpn-plugin-\w+" | sort | uniq)

build-auth-dialog:
	ninja -C build nm-vpn-bundle-auth-dialog

run-plugin-service:
	select p in $$(find plugin-service  -name '*_ctl.py' | cut -d '/' -f2 | cut -d '_' -f1 ); do [[ -n $$p ]] && break; done && \
	./plugin-service/provider-exec --provider=$$p

run-nm-connection-editor:
	nm-connection-editor --edit "$(TEST_VPN_UUID)"

gdb-nm-connection-editor:
	G_MESSAGES_DEBUG=all gdb -ex="set confirm off" -ex="run --edit \"$(TEST_VPN_UUID)\"" nm-connection-editor

gdb-auth-dialog:
	make build-auth-dialog && G_MESSAGES_DEBUG=all gdb -ex="set confirm off" -ex="run -s \"$(TEST_VPN_SERVICE)\" -n \"$(TEST_VPN_NAME)\" -u \"$(TEST_VPN_UUID)\" -t \"$(AUTH_CONFIG_HINT)\" -t foo -t bar" ./build/bin/nm-vpn-bundle-auth-dialog

run-auth-dialog:
	make build-auth-dialog && \
	G_MESSAGES_DEBUG=all ./build/auth-dialog/build/bin/nm-vpn-bundle-auth-dialog -s "$(TEST_VPN_SERVICE)" -n "$(TEST_VPN_NAME)" -u "$(TEST_VPN_UUID)"  -t "$(AUTH_CONFIG_HINT)" -t foo -t bar

run-test-gtk4-editor:
	@set -x; \
	ninja -C build test-gtk4-editor; \
	VPN_PROVIDER=$$provider G_MESSAGES_DEBUG=all ./build/bin/test-gtk4-editor

dev-install-watch:
	while inotifywait --recursive --event close_write,moved_to,create common/ plugin-service/ providers/ property-editor/ plasma-nm-applet-ui/ auth-dialog/ CMakeLists.txt *.in; \
	do \
		make configure && make all && sudo make install ||  echo -e "\e[31mBuild failed!\e[0m"; \
	done

install: 
	 ninja -C build install

package-all: 
	ninja -C build package;
	make makepkg

makepkg: 
	if command -v makepkg >/dev/null; then PKGDEST=build/packages/ makepkg --noextract -f && rmdir src; else echo "Arch package not built!"; fi

makepkg-install:
	make makepkg && sudo pacman -S build/*.zst

deb-dump:
	dpkg --contents build/*.deb && dpkg --info build/*.deb

deb-install:
	sudo dpkg --install build/*.deb

launch-nested-gnome:
	env MUTTER_DEBUG_DUMMY_MODE_SPECS=1024x768 dbus-run-session -- gnome-shell --nested --wayland --no-x11

launch-plasmoid:
	plasmoidviewer --applet org.kde.plasma.networkmanagement

launch-gnome-control-center-network:
	G_MESSAGES_DEBUG=all gnome-control-center network

setup-kde-build-deps-pacman:
	 sudo pacman -S networkmanager-qt networkmanager extra-cmake-modules  nm-connection-editor python-pydbus