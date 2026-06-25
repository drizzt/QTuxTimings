# Maintainer: drizzt <https://github.com/drizzt>
pkgbase=qtuxtimings
pkgname=('qtuxtimings' 'qtuxtimings-dkms')
pkgver=1.0.5
pkgrel=8
pkgdesc="AMD Ryzen DRAM timings and CPU telemetry viewer (Qt6)"
arch=('x86_64')
url="https://github.com/drizzt/QTuxTimings"
license=('GPL3')
makedepends=('gcc' 'cmake' 'qt6-base')
source=("$pkgbase-$pkgver.tar.gz::https://github.com/drizzt/QTuxTimings/archive/refs/heads/main.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$srcdir/QTuxTimings-main/Linux"
    rm -rf build
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
}

package_qtuxtimings() {
    pkgdesc="AMD Ryzen DRAM timings and CPU telemetry viewer (Qt6)"
    # qt6-wayland ships the Wayland QPA platform plugin; the launcher forces
    # QT_QPA_PLATFORM=wayland on Wayland sessions, so it must be present.
    depends=('qt6-base' 'qt6-wayland')
    optdepends=(
        'qtuxtimings-dkms: kernel modules for accurate benchmarking and memory voltages'
        'ryzen_smu-dkms-git: kernel module for reading AMD SMN/PM tables'
        'nct6775-dkms-git: fan readings on boards with Nuvoton Super I/O (NCT6775F through NCT6799D)'
    )

    cd "$srcdir/QTuxTimings-main/Linux"

    # Binary
    install -Dm755 build/qtuxtimings "$pkgdir/opt/QTuxTimings/bin/qtuxtimings"

    # Icon
    install -Dm644 qtuxtimings.png "$pkgdir/usr/share/icons/hicolor/256x256/apps/qtuxtimings.png"

    # Polkit policy
    install -Dm644 /dev/stdin "$pkgdir/usr/share/polkit-1/actions/it.belloworld.QTuxTimings.policy" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE policyconfig PUBLIC
 "-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/PolicyKit/1/policyconfig.dtd">
<policyconfig>
  <action id="it.belloworld.QTuxTimings.run">
    <description>Run QTuxTimings</description>
    <message>Authentication is required to run QTuxTimings</message>
    <defaults>
      <allow_any>auth_admin</allow_any>
      <allow_inactive>auth_admin</allow_inactive>
      <allow_active>auth_admin_keep</allow_active>
    </defaults>
    <annotate key="org.freedesktop.policykit.exec.path">/opt/QTuxTimings/bin/qtuxtimings</annotate>
    <annotate key="org.freedesktop.policykit.exec.allow_gui">true</annotate>
  </action>
</policyconfig>
EOF

    # Desktop file
    install -Dm644 /dev/stdin "$pkgdir/usr/share/applications/qtuxtimings.desktop" << 'DESKTOP'
[Desktop Entry]
Name=QTuxTimings
Comment=AMD Ryzen DRAM timings viewer
Exec=qtuxtimings
Icon=qtuxtimings
Terminal=false
Type=Application
Categories=Utility;
DESKTOP

    # Launcher script (handles env forwarding through pkexec)
    install -Dm755 /dev/stdin "$pkgdir/usr/bin/qtuxtimings" << 'LAUNCHER'
#!/bin/bash
if [ "$(id -u)" -eq 0 ]; then
    exec /opt/QTuxTimings/bin/qtuxtimings "$@"
fi

ENV_ARGS=""
for VAR in DISPLAY WAYLAND_DISPLAY XDG_RUNTIME_DIR XAUTHORITY \
           DBUS_SESSION_BUS_ADDRESS XDG_CONFIG_HOME HOME; do
    eval VAL=\$$VAR
    [ -n "$VAL" ] && ENV_ARGS="$ENV_ARGS --env-$VAR=$VAL"
done
[ -n "$WAYLAND_DISPLAY" ] && ENV_ARGS="$ENV_ARGS --env-QT_QPA_PLATFORM=wayland"

exec pkexec /opt/QTuxTimings/bin/qtuxtimings $ENV_ARGS "$@"
LAUNCHER
}

package_qtuxtimings-dkms() {
    pkgdesc="DKMS kernel modules for QTuxTimings (aod-voltages, tuxbench)"
    depends=('dkms' 'linux-headers')
    optdepends=('clang: required if your kernel was built with Clang (CachyOS, etc)')
    install=qtuxtimings-dkms.install

    cd "$srcdir/QTuxTimings-main/Linux"

    # aod-voltages DKMS module source
    local aod_ver
    aod_ver=$(grep '^PACKAGE_VERSION=' src/aod-voltages/dkms.conf | cut -d= -f2 | tr -d '"')
    install -dm755 "$pkgdir/usr/src/aod-voltages-$aod_ver"
    install -Dm644 src/aod-voltages/aod_voltages.c  "$pkgdir/usr/src/aod-voltages-$aod_ver/aod_voltages.c"
    install -Dm644 src/aod-voltages/Makefile         "$pkgdir/usr/src/aod-voltages-$aod_ver/Makefile"
    install -Dm644 src/aod-voltages/dkms.conf        "$pkgdir/usr/src/aod-voltages-$aod_ver/dkms.conf"

    # tuxbench DKMS module source
    local tb_ver
    tb_ver=$(grep '^PACKAGE_VERSION=' src/tuxbench/dkms.conf | cut -d= -f2 | tr -d '"')
    install -dm755 "$pkgdir/usr/src/tuxbench-$tb_ver"
    install -Dm644 src/tuxbench/tuxbench.c   "$pkgdir/usr/src/tuxbench-$tb_ver/tuxbench.c"
    install -Dm644 src/tuxbench/tuxbench.h   "$pkgdir/usr/src/tuxbench-$tb_ver/tuxbench.h"
    install -Dm644 src/tuxbench/Makefile     "$pkgdir/usr/src/tuxbench-$tb_ver/Makefile"
    install -Dm644 src/tuxbench/dkms.conf    "$pkgdir/usr/src/tuxbench-$tb_ver/dkms.conf"
}
