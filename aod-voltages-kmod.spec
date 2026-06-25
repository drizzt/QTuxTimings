# NOTE: the spec file name must stay "<Name>.spec" (aod-voltages-kmod.spec).
# kmodtool's akmod_install step re-runs rpmbuild on the spec under _specdir to
# drop the SRPM that akmods rebuilds on every new kernel; a mismatched file
# name breaks that step.

%global debug_package %{nil}

# On Fedora build only the akmod package; akmods compiles the real .ko on the
# target machine (and rebuilds it automatically on kernel updates).
%if 0%{?fedora}
%global buildforkernels akmod
%endif

%global kmod_name aod-voltages

# Upstream module version (matches src/aod-voltages/dkms.conf); bump only on an
# upstream rebase. Built from this fork's git, so a post-release snapshot field
# "^<date>git<rev>" (Fedora Versioning Guidelines) is appended at SRPM build time
# via --define "snap ..." (see .copr/Makefile and install.sh).
%global baseversion 0.5

Name:           %{kmod_name}-kmod
Version:        %{baseversion}%{?snap:^%{snap}}
Release:        1%{?dist}
Summary:        AMD AOD memory voltage reader kernel module for QTuxTimings
License:        GPL-2.0-only
URL:            https://github.com/drizzt/QTuxTimings
Source0:        %{kmod_name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  kmodtool
# Only needed for a per-kernel (non-akmod) build; the akmod package itself
# builds without kernel headers.
%{!?kernels:BuildRequires: kernel-devel}

# kmodtool expands the akmod and kmod subpackages here.
%{expand:%(kmodtool --target %{_target_cpu} --kmodname %{kmod_name} %{?buildforkernels:--%{buildforkernels}} %{?kernels:--for-kernels "%{?kernels}"} 2>/dev/null) }

%description
Kernel module that exposes AMD AOD (AMD Overclocking) memory voltage values
(VDDIO, VDDQ, VPP, ...) via sysfs for the QTuxTimings telemetry viewer.

# The akmod/kmod packages Require a "-common" package for the shared files.
%package -n %{kmod_name}-kmod-common
Summary:        Common files for the %{kmod_name} kernel module

%description -n %{kmod_name}-kmod-common
Files (license) shared by the %{kmod_name} kmod and akmod packages.

%prep
# Bail out early if kmodtool emitted an error instead of a spec fragment.
%{?kmodtool_check}
%setup -q -c
# One source copy per target kernel (empty list in akmod-only builds).
for kernel_version in %{?kernel_versions}; do
    cp -a %{kmod_name} _kmod_build_${kernel_version%%___*}
done

%build
for kernel_version in %{?kernel_versions}; do
    ksrc=${kernel_version##*___}
    # kbuild's `modules` target bypasses the module Makefile's clang autodetect,
    # so detect it here (matches ryzen_smu-kmod.spec) to build on Clang-built
    # kernels (CachyOS, etc).
    llvm=
    if grep -q "^CONFIG_CC_IS_CLANG=y" "${ksrc}/.config" 2>/dev/null; then
        llvm="LLVM=1"
    fi
    make %{?_smp_mflags} -C "${ksrc}" \
        M=${PWD}/_kmod_build_${kernel_version%%___*} ${llvm} modules
done

%install
for kernel_version in %{?kernel_versions}; do
    mkdir -p %{buildroot}%{kmodinstdir_prefix}/${kernel_version%%___*}/%{kmodinstdir_postfix}/
    install -D -m 755 _kmod_build_${kernel_version%%___*}/*.ko \
        %{buildroot}%{kmodinstdir_prefix}/${kernel_version%%___*}/%{kmodinstdir_postfix}/
done
%{?akmod_install}

%files -n %{kmod_name}-kmod-common
%license %{kmod_name}/LICENSE

%changelog
* Thu Jun 25 2026 Timothy Redaelli <timothy.redaelli@gmail.com> - 0.5-1
- Initial akmods packaging
