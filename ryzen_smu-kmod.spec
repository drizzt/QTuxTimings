# NOTE: the spec file name must stay "<Name>.spec" (ryzen_smu-kmod.spec).
# kmodtool's akmod_install step re-runs rpmbuild on the spec under _specdir to
# drop the SRPM that akmods rebuilds on every new kernel; a mismatched file
# name breaks that step.

%global debug_package %{nil}

# On Fedora build only the akmod package; akmods compiles the real .ko on the
# target machine (and rebuilds it automatically on kernel updates).
%if 0%{?fedora}
%global buildforkernels akmod
%endif

%global kmod_name   ryzen_smu

# ryzen_smu has no release tags; pin a known-good upstream commit here. This is
# the single source of truth: install.sh --rpm fetches Source0 from this spec.
%global commit      0bb95d961664c7a0ac180f849fa16fe7da71922d
%global shortcommit %(c=%{commit}; echo ${c:0:7})
# Date the pinned commit last modified the source (its commit date). Upstream
# has never tagged a release, so per the Fedora Versioning Guidelines this is a
# snapshot: Version is "0" (no upstream version) plus the "^<date>git<rev>"
# snapshot field. Bump this together with the commit above.
%global snapdate    20260425

Name:           %{kmod_name}-kmod
Version:        0^%{snapdate}git%{shortcommit}
Release:        1%{?dist}
Summary:        AMD Ryzen SMU/SMN driver kernel module (akmod) for QTuxTimings
License:        GPL-2.0-only
URL:            https://github.com/amkillam/ryzen_smu
Source0:        %{url}/archive/%{commit}/%{kmod_name}-%{commit}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  kmodtool
# Only needed for a per-kernel (non-akmod) build; the akmod package itself
# builds without kernel headers.
%{!?kernels:BuildRequires: kernel-devel}

# kmodtool expands the akmod and kmod subpackages here.
%{expand:%(kmodtool --target %{_target_cpu} --kmodname %{kmod_name} %{?buildforkernels:--%{buildforkernels}} %{?kernels:--for-kernels "%{?kernels}"} 2>/dev/null) }

%description
ryzen_smu exposes the AMD Ryzen System Management Unit (SMU/SMN) and the PM
table via sysfs. QTuxTimings requires it for all telemetry readings. This
packages the external upstream module (pinned to commit %{shortcommit}) as a
Fedora akmod.

# The akmod/kmod packages Require a "-common" package for the shared files.
%package -n %{kmod_name}-kmod-common
Summary:        Common files for the %{kmod_name} kernel module

%description -n %{kmod_name}-kmod-common
Files (license) shared by the %{kmod_name} kmod and akmod packages.

%prep
# Bail out early if kmodtool emitted an error instead of a spec fragment.
%{?kmodtool_check}
# Upstream tarball unpacks to ryzen_smu-<commit>/.
%setup -q -c -n %{name}-%{version}
# One source copy per target kernel (empty list in akmod-only builds).
for kernel_version in %{?kernel_versions}; do
    cp -a %{kmod_name}-%{commit} _kmod_build_${kernel_version%%___*}
done

%build
for kernel_version in %{?kernel_versions}; do
    ksrc=${kernel_version##*___}
    # ryzen_smu's Makefile has no clang/gcc autodetect; mirror what the bundled
    # modules do so it builds on Clang-built kernels (CachyOS, etc).
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
%license %{kmod_name}-%{commit}/LICENSE

%changelog
* Thu Jun 25 2026 Timothy Redaelli <timothy.redaelli@gmail.com> - 0^20260425git0bb95d9-1
- Initial akmods packaging (pinned upstream commit 0bb95d9)
