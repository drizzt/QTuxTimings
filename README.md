<h1 align="center">QTuxTimings</h1>

<p align="center">Qt6 port of <a href="https://github.com/Death4two/TuxTimings">TuxTimings</a> — AMD Ryzen DRAM timings and CPU telemetry viewer.</p>

![QTuxTimings](screenshot.png "QTuxTimings")

### Supported CPUs

Zen 5 Granite Ridge (Ryzen 9000-series AM5) is the primary tested target for PM-table based telemetry. Other AMD Zen families (Vermeer, Cezanne, Matisse, Renoir, Raven Ridge) have PM table mappings and may work with varying completeness.

If your CPU family is not listed or shows incomplete readings, you can help by running the **debug dump** from the app (PM table + AOD dump) and opening a GitHub issue with the dump attached so I can add proper PM table support.

### Documentation

- **PM table indices reference**: [`Linux/docs/PM_TABLE_INDICES.md`](Linux/docs/PM_TABLE_INDICES.md)

### Prerequisites

- **[ryzen_smu](https://github.com/amkillam/ryzen_smu/)** kernel module — required for all readings. Install it separately; QTuxTimings will load/unload it automatically if present.
- **Qt6 runtime** — needed to run the application  
  - Arch: `qt6-base`  
  - Debian/Ubuntu: `libqt6widgets6`

> If building manually: also install `gcc`, `cmake`, Qt6 development headers, and GMP headers (Pi benchmark).  
> - Arch: `qt6-base` + `cmake` + `gmp`  
> - Debian/Ubuntu: `qt6-base-dev` + `cmake` + `libgmp-dev`

> **Note:** QTuxTimings automatically loads `ryzen_smu` and `aod_voltages` on startup and unloads them on exit. No manual `modprobe` is needed.

### Installing

#### Arch Linux / CachyOS (recommended)

Install `ryzen_smu` first, then build and install QTuxTimings:

```bash
yay -S ryzen_smu-dkms-git
makepkg -si
```

This uses the included PKGBUILD to build and install via pacman. The `aod-voltages` kernel module (for memory voltage readings) is bundled and built automatically via DKMS. Uninstall with `sudo pacman -R qtuxtimings`.

#### Fedora

All three kernel modules (`aod_voltages`, `tuxbench`, and `ryzen_smu`) are
packaged as **akmods**, so they are rebuilt automatically whenever you install a
new kernel — the Fedora equivalent of DKMS. `ryzen_smu` is built from a pinned
upstream commit (it has no Fedora package), so `./install.sh --rpm` needs network
access to fetch it.

##### From COPR (prebuilt)

```bash
sudo dnf copr enable tredaell/QTuxTimings
sudo dnf install qtuxtimings
```

`akmod-ryzen_smu` is pulled in automatically (it is a hard dependency);
`akmod-aod-voltages` and `akmod-tuxbench` come in via `Recommends`. akmods
compiles the modules for your running kernel on install and rebuilds them on
every kernel update, so make sure the prerequisites are present:

```bash
sudo dnf install akmods kernel-devel
```

The modules are x86_64-only (they use AVX2).

##### From source

```bash
# Module build prerequisites (akmods rebuilds the .ko on kernel updates)
sudo dnf install akmods kernel-devel

# Build deps (only needed to create the RPMs)
sudo dnf install gcc-c++ cmake qt6-qtbase-devel gmp-devel rpm-build rpmdevtools

# Build the app RPM + the akmod packages (downloads ryzen_smu source)
./install.sh --rpm

# Install everything; akmods compiles the modules for your running kernel
sudo dnf install ./qtuxtimings-*.rpm ./akmod-*.rpm ./*-kmod-common-*.rpm
```

Uninstall with (the `kmod-*` globs drop the per-kernel modules akmods built):

```bash
sudo dnf remove qtuxtimings \
    'akmod-ryzen_smu'    'kmod-ryzen_smu-*' \
    'akmod-aod-voltages' 'kmod-aod-voltages-*' \
    'akmod-tuxbench'     'kmod-tuxbench-*'
```

#### Ubuntu / Debian

```bash
sudo apt update

# Runtime deps (needed to run/install the .deb)
sudo apt install -y libqt6widgets6 libgmp10 policykit-1 dmidecode kmod

# Build deps (only needed if building from source / creating the .deb)
sudo apt install -y build-essential cmake qt6-base-dev libgmp-dev

./install.sh --deb
sudo apt install ./qtuxtimings_*.deb
```

#### Any Linux (direct install)

```bash
./install.sh
```

Builds from source and installs to `/opt/QTuxTimings/` with a launcher at `/usr/bin/qtuxtimings`, polkit policy, desktop file, and icon. Root privileges are requested automatically via sudo.

#### Uninstall (direct install)

```bash
./install.sh --uninstall
```

### Updating

#### From a git clone (recommended)

Update your local clone to the latest `main`, then rebuild/reinstall:

```bash
./update.sh
./install.sh          # or: ./install.sh --deb
```

Notes:
- `./update.sh` force-updates tracked repo files to match the latest `origin/main` (overwrites local edits).
- By default it also removes untracked files/dirs (build artifacts, local `.deb` output, etc).
- Add `--keep-untracked` to preserve untracked files/dirs.

#### Stable builds (tags / releases)

For a reproducible/stable build, download the source files from the latest stable release.

### Building without installing

```bash
cmake -S Linux -B Linux/build -DCMAKE_BUILD_TYPE=Release
cmake --build Linux/build
sudo ./Linux/build/qtuxtimings
```

### License

This project is licensed under the **GNU General Public License v3.0**. See [LICENSE](LICENSE) for the full text.

### References and projects used

- **[ryzen_smu](https://github.com/amkillam/ryzen_smu/)** — Kernel module for reading AMD SMN and PM table; build and load separately at runtime.
- **[ZenStates-Core](https://github.com/irusanov/ZenStates-Core)** — PM table offsets and timing formulas (reimplemented in our Linux backend; used with permission).
- **[TuxTimings](https://github.com/Death4two/TuxTimings)** — original GTK4 Linux version; QTuxTimings is a Qt6 port of it.
- **[ZenTimings](https://github.com/nickspacewalker/ZenTimings)** — Windows version; QTuxTimings is based on this concept.
- **[Qt6](https://www.qt.io/)** — Cross-platform UI toolkit.
- **[Linux kernel](https://github.com/torvalds/linux)** — SMN/sysfs interface and platform support.
- **[AMD's public documentation](https://www.amd.com/en/support/tech-docs)** — SMN/PM table and DRAM timing references.
- **Tux icon** — Tux the penguin originally by Larry Ewing, created with GIMP (`lewing@isc.tamu.edu`), used and/or modified under the terms of the original image permission.

Huge credit goes to ZenTimings and irusanov without his project this wouldn't be possible.

### Disclaimer

This project's code was made with the assistance of [Claude](https://claude.ai/) (Anthropic). While it has been reviewed and tested, use it at your own risk.
