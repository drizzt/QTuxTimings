# Upstream (Death4two/TuxTimings) release this fork is based on; bump only when
# rebasing onto a newer upstream tag. The package is a git snapshot of the fork,
# so a post-release "^<date>git<rev>" field (Fedora Versioning Guidelines) is
# appended at SRPM build time via --define "snap <date>git<rev>" (see
# .copr/Makefile and install.sh). Without it, Version falls back to the bare base.
%global baseversion 1.0.5

Name:           qtuxtimings
Version:        %{baseversion}%{?snap:^%{snap}}
Release:        1%{?dist}
Summary:        AMD Ryzen DRAM timings and CPU telemetry viewer (Qt6)
License:        GPL-3.0-or-later
URL:            https://github.com/drizzt/QTuxTimings
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  qt6-qtbase-devel
BuildRequires:  gmp-devel

Requires:       qt6-qtbase
# qt6-qtwayland ships the Wayland QPA plugin; main.cpp forces
# QT_QPA_PLATFORM=wayland on Wayland sessions, so it must be present.
Requires:       qt6-qtwayland
Requires:       gmp
Requires:       polkit
Requires:       dmidecode
Requires:       kmod
# ryzen_smu is mandatory: without /sys/kernel/ryzen_smu_drv the app prints an
# error and exits before the UI (main.cpp), so hard-require its akmod.
Requires:       akmod-ryzen_smu
# aod_voltages (memory voltages) and tuxbench (benchmark; has a userspace
# fallback) only degrade gracefully when absent — weak deps.
Recommends:     akmod-aod-voltages
Recommends:     akmod-tuxbench

%description
QTuxTimings displays real-time AMD Ryzen DRAM timings, CPU frequencies,
temperatures, voltages and other telemetry. It is a Qt6 port of TuxTimings.

%prep
%setup -q

%build
# Use the Fedora cmake macros so %{optflags} (incl. -g, hardening, LTO) are
# honored -- that is what produces a valid -debuginfo package. The CMakeLists
# lives in the Linux/ subdir, hence -S Linux.
%cmake -S Linux
%cmake_build

%install
# FHS layout for Fedora/COPR: the binary goes straight into %{_bindir}. No
# launcher wrapper is needed -- main.cpp self-elevates via pkexec (forwarding
# the session env itself), and with no /opt path present it pkexec's
# /proc/self/exe, i.e. this very %{_bindir}/qtuxtimings, which matches the
# polkit exec.path below. (The Arch/Debian packages keep their /opt layout.)
# CMakeLists installs the target to bin/, so the cmake install step lands the
# binary in %%{_bindir}; polkit/desktop/icon files are installed by hand below.
%cmake_install

# Polkit policy
install -d -m 0755 %{buildroot}%{_datadir}/polkit-1/actions
cat > %{buildroot}%{_datadir}/polkit-1/actions/it.belloworld.QTuxTimings.policy << 'POLICY'
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
    <annotate key="org.freedesktop.policykit.exec.path">/usr/bin/qtuxtimings</annotate>
    <annotate key="org.freedesktop.policykit.exec.allow_gui">true</annotate>
  </action>
</policyconfig>
POLICY

# Desktop file
install -d -m 0755 %{buildroot}%{_datadir}/applications
cat > %{buildroot}%{_datadir}/applications/qtuxtimings.desktop << 'DESKTOP'
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
install -D -m 0644 Linux/qtuxtimings.png \
    %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/qtuxtimings.png

%files
%license LICENSE
%{_bindir}/qtuxtimings
%{_datadir}/polkit-1/actions/it.belloworld.QTuxTimings.policy
%{_datadir}/applications/qtuxtimings.desktop
%{_datadir}/icons/hicolor/256x256/apps/qtuxtimings.png

%changelog
* Thu Jun 25 2026 Timothy Redaelli <timothy.redaelli@gmail.com> - 1.0.5-1
- Initial RPM packaging
