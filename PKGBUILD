# Maintainer: AhooraZen <https://github.com/AhooraZen>
pkgname=qt6chess
pkgver=1.0.4
pkgrel=1

pkgdesc="Modern high-performance native Qt6 chess application with Stockfish 17.1 and ANSI TUI"
arch=('x86_64')
url="https://github.com/AhooraZen/Qt6Chess"
license=('MIT')
depends=(
    'qt6-base'
    'qt6-declarative'
    'qt6-svg'
    'qt6-multimedia'
)
optdepends=(
    'stockfish: AI opponent and live UCI position evaluation'
)
makedepends=(
    'cmake'
    'ninja'
    'qt6-base'
    'qt6-declarative'
    'qt6-svg'
    'qt6-multimedia'
)
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$srcdir/Qt6Chess-$pkgver"
    cmake -B build \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    ninja -C build
}

check() {
    cd "$srcdir/Qt6Chess-$pkgver"
    ctest --test-dir build --output-on-failure
}

package() {
    DESTDIR="$pkgdir" cmake --install "$srcdir/Qt6Chess-$pkgver/build"
    ln -sf Qt6Chess "$pkgdir/usr/bin/qt6chess"
    install -Dm644 "$srcdir/Qt6Chess-$pkgver/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}

