# Maintainer: Samuel Knutsen <aur@knutsen.co>
#
# The released binaries are statically linked, so this package has no dependencies and
# does not rebuild Hyprland's sources on the user's machine.
pkgname=hyprlang2lua-bin
_pkgname=hyprlang2lua
pkgver=0.1.2
pkgrel=1
pkgdesc="Converts legacy Hyprland hyprlang (.conf) configs to the Lua config format (static binary)"
arch=('x86_64' 'aarch64')
url="https://github.com/Demonstrandum/hyprlang2lua"
license=('BSD-3-Clause')
# the binary is already stripped and UPX-packed, so makepkg must not try to strip it
# again or build a debug package from it
options=('!strip' '!debug')
provides=("$_pkgname")
conflicts=("$_pkgname")
source_x86_64=("$_pkgname-$pkgver-x86_64::$url/releases/download/v$pkgver/$_pkgname-$pkgver-x86_64-linux")
source_aarch64=("$_pkgname-$pkgver-aarch64::$url/releases/download/v$pkgver/$_pkgname-$pkgver-aarch64-linux")
source=("LICENSE::$url/raw/v$pkgver/LICENSE")
sha256sums=('SKIP')
sha256sums_x86_64=('SKIP')
sha256sums_aarch64=('SKIP')

package() {
    local binary="$_pkgname-$pkgver-$CARCH"

    install -Dm755 "$srcdir/$binary" "$pkgdir/usr/bin/$_pkgname"
    install -Dm644 "$srcdir/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
