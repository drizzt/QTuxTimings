#!/bin/bash
# Build and install QTuxTimings (C++/Qt6) natively.
# Requires: gcc, cmake, qt6 development headers
#
# Usage:
#   ./install.sh              Build and install to system
#   ./install.sh --uninstall  Remove all installed files
#   ./install.sh --deb        Build a .deb package (Ubuntu/Debian)
#
# On Arch-based distros, prefer:  makepkg -si  (from the repo root)
set -e

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
LINUX_DIR="$ROOT_DIR/Linux"
INSTALL_DIR="/opt/QTuxTimings"
MAKE=/usr/bin/make

REAL_USER="${SUDO_USER:-$(whoami)}"
REAL_HOME=$(getent passwd "$REAL_USER" | cut -d: -f6)

if [ ! -f "$LINUX_DIR/CMakeLists.txt" ]; then
    echo "ERROR: Linux/CMakeLists.txt not found. Run this script from the repo root."
    exit 1
fi

# Compatible with both dkms 2.x ("name, ver,") and dkms 3.x ("name/ver:")
_dkms_ver() {
    dkms status "$1" 2>/dev/null \
        | grep -oP "(?<=$1[/, ])[0-9][0-9.]*" | head -1
}

# ── Uninstall ─────────────────────────────────────────────────────────
if [ "${1:-}" = "--uninstall" ]; then
    if [ "$(id -u)" -ne 0 ]; then
        exec sudo "$0" --uninstall
    fi
    echo "==> Uninstalling QTuxTimings..."

    rm -rf "$INSTALL_DIR"
    rm -f /usr/bin/qtuxtimings
    rm -f /usr/share/polkit-1/actions/it.belloworld.QTuxTimings.policy
    rm -f /usr/share/applications/qtuxtimings.desktop
    rm -f /usr/share/icons/hicolor/256x256/apps/qtuxtimings.png
    xdg-icon-resource forceupdate --theme hicolor 2>/dev/null || true

    # Desktop shortcut
    DESKTOP_DIR="${REAL_HOME}/Desktop"
    [ ! -d "$DESKTOP_DIR" ] && \
        DESKTOP_DIR=$(su "$REAL_USER" -c 'xdg-user-dir DESKTOP 2>/dev/null' || true)
    if [ -d "$DESKTOP_DIR" ]; then
        rm -f "$DESKTOP_DIR/qtuxtimings.desktop"
    fi


    # Optionally remove aod-voltages DKMS module
    if command -v dkms &>/dev/null; then
        AOD_VER=$(_dkms_ver aod-voltages)
        if [ -n "$AOD_VER" ]; then
            read -rp "    Remove aod-voltages DKMS module ($AOD_VER)? [y/N] " answer
            case "$answer" in
                [yY]*)
                    rmmod aod_voltages 2>/dev/null || true
                    dkms remove aod-voltages/"$AOD_VER" --all 2>/dev/null || true
                    rm -rf "/usr/src/aod-voltages-$AOD_VER"
                    echo "    aod-voltages removed."
                    ;;
            esac
        fi
    fi

    # Optionally remove tuxbench DKMS module
    if command -v dkms &>/dev/null; then
        TB_VER=$(_dkms_ver tuxbench)
        if [ -n "$TB_VER" ]; then
            read -rp "    Remove tuxbench DKMS module ($TB_VER)? [y/N] " answer
            case "$answer" in
                [yY]*)
                    rmmod tuxbench 2>/dev/null || true
                    dkms remove tuxbench/"$TB_VER" --all 2>/dev/null || true
                    rm -rf "/usr/src/tuxbench-$TB_VER"
                    echo "    tuxbench removed."
                    ;;
            esac
        fi
    fi

    echo "==> QTuxTimings has been uninstalled."
    exit 0
fi

# ── Build .deb package ────────────────────────────────────────────────
if [ "${1:-}" = "--deb" ]; then
    echo "==> Building .deb package..."

    # Build binary first. Drop any stale build/ so a cache from a different
    # source path (e.g. the tree was moved) can't abort the configure step.
    rm -rf "$LINUX_DIR/build"
    if [ "$(id -u)" -eq 0 ]; then
        su "$REAL_USER" -c "cmake -S '$LINUX_DIR' -B '$LINUX_DIR/build' -DCMAKE_BUILD_TYPE=Release && cmake --build '$LINUX_DIR/build'"
    else
        cmake -S "$LINUX_DIR" -B "$LINUX_DIR/build" -DCMAKE_BUILD_TYPE=Release && cmake --build "$LINUX_DIR/build"
    fi

    # Prefer a version derived from git tags when available, otherwise fall back
    # to a stable placeholder.
    PKG_VERSION="0.0.0"
    if command -v git >/dev/null 2>&1 && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        # Examples:
        #   v1.2.3          -> 1.2.3
        #   v1.2.3-5-g<sha> -> 1.2.3+5.g<sha>
        ver="$(git -C "$ROOT_DIR" describe --tags --always --dirty 2>/dev/null || true)"
        ver="${ver#v}"
        # Debian version: allow [0-9A-Za-z.+~:-]
        # Translate git-describe output into something dpkg accepts.
        ver="${ver//-/.}"      # separators
        ver="${ver//_/.}"      # safety
        PKG_VERSION="${ver//.g/+g}"  # "…+g<sha>" instead of "…g<sha>"
    fi
    DEB_ROOT="$LINUX_DIR/deb-build/qtuxtimings_${PKG_VERSION}_amd64"
    rm -rf "$LINUX_DIR/deb-build"
    mkdir -p "$DEB_ROOT/DEBIAN"
    mkdir -p "$DEB_ROOT/opt/QTuxTimings/bin"
    mkdir -p "$DEB_ROOT/usr/bin"
    mkdir -p "$DEB_ROOT/usr/share/polkit-1/actions"
    mkdir -p "$DEB_ROOT/usr/share/applications"
    mkdir -p "$DEB_ROOT/usr/share/icons/hicolor/256x256/apps"

    # Control file
    cat > "$DEB_ROOT/DEBIAN/control" << EOF
Package: qtuxtimings
Version: $PKG_VERSION
Section: utils
Priority: optional
Architecture: amd64
Depends: libqt6widgets6, libqt6gui6, libqt6core6, qt6-wayland, libgmp10, policykit-1, dmidecode, kmod
Recommends: dkms, linux-headers-generic
Maintainer: drizzt <https://github.com/drizzt>
Description: AMD Ryzen DRAM timings and CPU telemetry viewer (Qt6)
 Displays real-time DRAM timings, CPU frequencies, temperatures,
 and other telemetry data for AMD Ryzen processors.
EOF

    # Binary
    install -m755 "$LINUX_DIR/build/qtuxtimings" "$DEB_ROOT/opt/QTuxTimings/bin/qtuxtimings"

    # Launcher script
    cat > "$DEB_ROOT/usr/bin/qtuxtimings" << 'LAUNCHER'
#!/bin/bash
if [ "$(id -u)" -eq 0 ]; then
    exec /opt/QTuxTimings/bin/qtuxtimings "$@"
fi
ENV_ARGS=()
for VAR in DISPLAY WAYLAND_DISPLAY XDG_RUNTIME_DIR XAUTHORITY \
           DBUS_SESSION_BUS_ADDRESS XDG_CONFIG_HOME HOME; do
    eval VAL=\$$VAR
    [ -n "$VAL" ] && ENV_ARGS+=("--env-$VAR=$VAL")
done
[ -n "$WAYLAND_DISPLAY" ] && ENV_ARGS+=("--env-QT_QPA_PLATFORM=wayland")
exec pkexec /opt/QTuxTimings/bin/qtuxtimings "${ENV_ARGS[@]}" "$@"
LAUNCHER
    chmod 755 "$DEB_ROOT/usr/bin/qtuxtimings"

    # Polkit policy
    cat > "$DEB_ROOT/usr/share/polkit-1/actions/it.belloworld.QTuxTimings.policy" << 'POLICY'
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
POLICY

    # Desktop file
    cat > "$DEB_ROOT/usr/share/applications/qtuxtimings.desktop" << 'DESKTOP'
[Desktop Entry]
Name=QTuxTimings
Comment=AMD Ryzen DRAM timings viewer
Exec=qtuxtimings
Icon=qtuxtimings
Terminal=false
Type=Application
Categories=Utility;
DESKTOP

    # Icon
    if [ -f "$LINUX_DIR/qtuxtimings.png" ]; then
        install -m644 "$LINUX_DIR/qtuxtimings.png" "$DEB_ROOT/usr/share/icons/hicolor/256x256/apps/qtuxtimings.png"
    fi

    dpkg-deb --build "$DEB_ROOT"
    mv "$DEB_ROOT.deb" "$ROOT_DIR/"
    rm -rf "$LINUX_DIR/deb-build"
    echo "==> .deb package created: $ROOT_DIR/qtuxtimings_${PKG_VERSION}_amd64.deb"
    echo "    Install with: sudo dpkg -i qtuxtimings_${PKG_VERSION}_amd64.deb"
    exit 0
fi

# ── Build ─────────────────────────────────────────────────────────────

# When re-invoked with sudo for installation, skip build
INSTALL_ONLY=0
if [ "${1:-}" = "--install-only" ]; then
    INSTALL_ONLY=1
    shift
fi

if [ "$INSTALL_ONLY" -eq 0 ]; then

echo "==> Building qtuxtimings (C++/Qt6)..."
# Drop any stale build/ so a cache from a different source path can't abort configure.
rm -rf "$LINUX_DIR/build"
if [ "$(id -u)" -eq 0 ]; then
    su "$REAL_USER" -c "cmake -S '$LINUX_DIR' -B '$LINUX_DIR/build' -DCMAKE_BUILD_TYPE=Release && cmake --build '$LINUX_DIR/build'"
else
    cmake -S "$LINUX_DIR" -B "$LINUX_DIR/build" -DCMAKE_BUILD_TYPE=Release && cmake --build "$LINUX_DIR/build"
fi

fi # end INSTALL_ONLY check

# ── Install to system (needs root) ──────────────────────────────────────

if [ "$(id -u)" -ne 0 ]; then
    echo "==> Elevating to root for system installation..."
    exec sudo "$0" --install-only "$@"
fi

echo "==> Installing QTuxTimings to system..."

# Binary
mkdir -p "$INSTALL_DIR/bin"
cp "$LINUX_DIR/build/qtuxtimings" "$INSTALL_DIR/bin/"
chmod +x "$INSTALL_DIR/bin/qtuxtimings"

# Launcher script in PATH
cat > /usr/bin/qtuxtimings << 'LAUNCHER'
#!/bin/bash
if [ "$(id -u)" -eq 0 ]; then
    exec /opt/QTuxTimings/bin/qtuxtimings "$@"
fi
ENV_ARGS=()
for VAR in DISPLAY WAYLAND_DISPLAY XDG_RUNTIME_DIR XAUTHORITY \
           DBUS_SESSION_BUS_ADDRESS XDG_CONFIG_HOME HOME; do
    eval VAL=\$$VAR
    [ -n "$VAL" ] && ENV_ARGS+=("--env-$VAR=$VAL")
done
[ -n "$WAYLAND_DISPLAY" ] && ENV_ARGS+=("--env-QT_QPA_PLATFORM=wayland")
exec pkexec /opt/QTuxTimings/bin/qtuxtimings "${ENV_ARGS[@]}" "$@"
LAUNCHER
chmod +x /usr/bin/qtuxtimings

# Polkit policy
mkdir -p /usr/share/polkit-1/actions
cat > /usr/share/polkit-1/actions/it.belloworld.QTuxTimings.policy << 'POLICY'
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
POLICY

# Desktop file
cat > /usr/share/applications/qtuxtimings.desktop << 'DESKTOP'
[Desktop Entry]
Name=QTuxTimings
Comment=AMD Ryzen DRAM timings viewer
Exec=qtuxtimings
Icon=qtuxtimings
Terminal=false
Type=Application
Categories=Utility;
DESKTOP

# Desktop shortcut
DESKTOP_DIR="${REAL_HOME}/Desktop"
[ ! -d "$DESKTOP_DIR" ] && \
    DESKTOP_DIR=$(su "$REAL_USER" -c 'xdg-user-dir DESKTOP 2>/dev/null' || true)
if [ -d "$DESKTOP_DIR" ]; then
    cp /usr/share/applications/qtuxtimings.desktop "$DESKTOP_DIR/"
    chown "$REAL_USER":"$REAL_USER" "$DESKTOP_DIR/qtuxtimings.desktop"
    chmod +x "$DESKTOP_DIR/qtuxtimings.desktop"
fi

# Icon
if [ -f "$LINUX_DIR/qtuxtimings.png" ]; then
    mkdir -p /usr/share/icons/hicolor/256x256/apps
    cp "$LINUX_DIR/qtuxtimings.png" /usr/share/icons/hicolor/256x256/apps/
    xdg-icon-resource forceupdate --theme hicolor 2>/dev/null || true
fi

# ── ryzen_smu kernel module ──────────────────────────────────────────
# QTuxTimings requires ryzen_smu for PM-table/SMN readings, but ryzen_smu is a
# separate project and is not bundled/installed by this script.
if modprobe ryzen_smu 2>/dev/null; then
    echo "==> ryzen_smu module available."
else
    echo "==> WARNING: ryzen_smu kernel module not found."
    echo "    QTuxTimings will have missing readings until you install it."
    echo ""
    echo "    Install it manually:"
    echo "      Arch:   yay -S ryzen_smu-dkms-git"
    echo "      Source: https://github.com/amkillam/ryzen_smu"
fi

# ── aod-voltages kernel module ────────────────────────────────────────
install_aod_voltages() {
    local AOD_SRC="$LINUX_DIR/src/aod-voltages"

    if [ ! -d "$AOD_SRC" ]; then
        echo "    WARNING: aod-voltages source not found at $AOD_SRC, skipping."
        return 0
    fi

    if ! command -v dkms &>/dev/null; then
        echo "    WARNING: dkms not found — cannot install aod-voltages module."
        return 0
    fi

    local AOD_VER
    AOD_VER=$(grep '^PACKAGE_VERSION=' "$AOD_SRC/dkms.conf" | cut -d= -f2 | tr -d '"')
    echo "==> Installing aod-voltages $AOD_VER via DKMS..."

    # Always refresh sources so upgrades (e.g. 0.4 → 0.5) are not skipped when the
    # old module still loads successfully.
    rmmod aod_voltages 2>/dev/null || true
    for old_ver in $(dkms status 2>/dev/null | sed -n 's/^aod-voltages\/\([0-9.]*\).*/\1/p' | sort -u); do
        [ "$old_ver" = "$AOD_VER" ] && continue
        dkms remove aod-voltages/"$old_ver" --all 2>/dev/null || true
        rm -rf "/usr/src/aod-voltages-$old_ver"
    done
    dkms remove aod-voltages/"$AOD_VER" --all 2>/dev/null || true

    local DKMS_SRC="/usr/src/aod-voltages-$AOD_VER"
    mkdir -p "$DKMS_SRC"
    cp "$AOD_SRC/dkms.conf" "$AOD_SRC/Makefile" "$AOD_SRC"/*.c "$DKMS_SRC/"

    if dkms add aod-voltages/"$AOD_VER" 2>/dev/null || true; then
        if dkms build aod-voltages/"$AOD_VER" && dkms install aod-voltages/"$AOD_VER"; then
            modprobe aod_voltages && echo "==> aod-voltages loaded successfully." || true
        else
            echo "    WARNING: aod-voltages DKMS build failed. Check kernel headers."
            return 1
        fi
    fi
}

install_aod_voltages || true

# ── tuxbench kernel module ────────────────────────────────────────────
install_tuxbench() {
    local TB_SRC="$LINUX_DIR/src/tuxbench"

    if [ ! -d "$TB_SRC" ]; then
        echo "    WARNING: tuxbench source not found at $TB_SRC, skipping."
        return 0
    fi

    if ! command -v dkms &>/dev/null; then
        echo "    WARNING: dkms not found — cannot install tuxbench module."
        return 0
    fi

    local TB_VER
    TB_VER=$(grep '^PACKAGE_VERSION=' "$TB_SRC/dkms.conf" | cut -d= -f2 | tr -d '"')
    echo "==> Installing tuxbench $TB_VER via DKMS..."

    local DKMS_SRC="/usr/src/tuxbench-$TB_VER"
    mkdir -p "$DKMS_SRC"
    cp "$TB_SRC/dkms.conf" "$TB_SRC/Makefile" "$TB_SRC"/*.c "$TB_SRC"/*.h "$DKMS_SRC/"

    # Remove all versions except the current one so fresh source is always built
    rmmod tuxbench 2>/dev/null || true
    dkms status tuxbench 2>/dev/null | grep -oP "(?<=tuxbench[/, ])[0-9][0-9.]*" | while read -r old_ver; do
        [ "$old_ver" = "$TB_VER" ] && continue
        dkms remove tuxbench/"$old_ver" --all 2>/dev/null || true
        rm -rf "/usr/src/tuxbench-$old_ver"
    done
    dkms remove tuxbench/"$TB_VER" --all 2>/dev/null || true

    if dkms add tuxbench/"$TB_VER" 2>/dev/null || true; then
        if dkms build tuxbench/"$TB_VER" && dkms install tuxbench/"$TB_VER"; then
            echo "==> tuxbench installed successfully (loaded on demand by QTuxTimings)."
        else
            echo "    WARNING: tuxbench DKMS build failed. Check kernel headers."
            return 1
        fi
    fi
}

install_tuxbench || true

# ── nct6775 kernel module (Nuvoton Super I/O fan/temp) ────────────────
install_nct6775() {
    # nct6775 is in mainline Linux since ~5.15 — try loading the built-in first.
    if modprobe nct6775 2>/dev/null; then
        echo "==> nct6775 module loaded (covers NCT6775F–NCT6799D fan/temp chips)."
        return 0
    fi

    # Module not present in this kernel — offer DKMS option.
    echo "    NOTE: nct6775 kernel module not found (fan/temp readings will be unavailable)."
    if command -v dkms &>/dev/null; then
        echo "    To enable fan readings, install the nct6775 DKMS module:"
        echo "      Arch:   yay -S nct6775-dkms-git"
        echo "      Other:  https://github.com/Fred78290/nct6775"
    fi
    return 0  # non-fatal
}

install_nct6775 || true

# ── msr kernel module ─────────────────────────────────────────────────
if modprobe msr 2>/dev/null; then
    echo "==> msr module loaded (required for BCLK reading)."
else
    echo "    NOTE: msr module unavailable — BCLK will show as 0.0 MHz."
fi

echo "==> Installation complete!"
echo "    Binary:    $INSTALL_DIR/bin/qtuxtimings"
echo "    Launcher:  /usr/bin/qtuxtimings"
echo "    Policy:    /usr/share/polkit-1/actions/it.belloworld.QTuxTimings.policy"
echo "    Desktop:   /usr/share/applications/qtuxtimings.desktop"
echo ""
echo "    Run with:  qtuxtimings"

