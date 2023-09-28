# shellcheck disable=SC2148,SC2034,SC2154

pkgname=networkmanager-vpn-bundle
pkgver=0.0.1
pkgrel=1
pkgdesc="Collection of many 3rdparty VPN support for NetworkManager"
arch=('any')
url="https://github.com/nom3ad/NetworkManager-vpn-bundle"
license=('MIT')
depends=('networkmanager'
    'libnm'
    # 'python-dbus'  # provided by dbus-python
    'python-pydbus'
    'python-netifaces'
)
optdepends=()
options=()
makedepends=('git' 'make' 'gcc' 'cmake' 'ninja' 'extra-cmake-modules')
provides=('networkmanager-vpn-bundle')

source=("$pkgname::git://github.com/nom3ad/NNetworkManager-vpn-bundle.git#tag=${pkgver}")
md5sums=('SKIP')

build() {
    # declare -p
    cd "$pkgname" || cd "$srcdir/.." && echo "dev mode!"
    make configure
    make all
}

package() {
    cd "$pkgname" || cd "$srcdir/.." && echo "dev mode!"
    DESTDIR="${pkgdir}" make install
}
