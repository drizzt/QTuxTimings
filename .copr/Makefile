# COPR "make srpm" entry point. For each configured package COPR clones the repo
# and runs, from the repo root:
#
#   make -f .copr/Makefile srpm outdir=<dir> spec=<spec-from-package-config>
#
# The four packages (qtuxtimings, ryzen_smu-kmod, aod-voltages-kmod,
# tuxbench-kmod) share this one Makefile and differ only by the "Spec File"
# field; $(spec) selects which Source0 to generate. All version/commit truth
# stays in the spec files (and, for the kmods, their dkms.conf) -- this Makefile
# duplicates no versions, mirroring the source prep in install.sh --rpm.

outdir ?= $(CURDIR)
spec   ?= qtuxtimings.spec

# rpmbuild tree, kept out of the checkout so a local run leaves no mess.
TOPDIR  := $(CURDIR)/.copr-rpmbuild
SOURCES := $(TOPDIR)/SOURCES

.PHONY: srpm
srpm:
	# COPR's srpm builder is minimal; rpmdevtools provides spectool. Skip the
	# install when the tools are already present so a non-root local run works.
	@command -v rpmbuild >/dev/null && command -v spectool >/dev/null \
		|| dnf -y install rpm-build rpmdevtools git make
	@set -eu; \
	rm -rf "$(TOPDIR)"; \
	mkdir -p "$(SOURCES)" "$(TOPDIR)/SPECS" "$(outdir)"; \
	name=$$(basename "$(spec)"); \
	snap="$$(git log -1 --date=format:%Y%m%d --format=%cd HEAD)git$$(git rev-parse --short=7 HEAD)"; \
	base=$$(grep -m1 '^%global baseversion' "$(spec)" | awk '{print $$3}'); \
	ver="$${base}^$${snap}"; \
	case "$$name" in \
	  qtuxtimings.spec) \
	    git archive --format=tar.gz --prefix="qtuxtimings-$$ver/" \
	      -o "$(SOURCES)/qtuxtimings-$$ver.tar.gz" HEAD; \
	    { echo "%global snap $$snap"; cat "$(spec)"; } > "$(TOPDIR)/SPECS/$$name" ;; \
	  aod-voltages-kmod.spec|tuxbench-kmod.spec) \
	    mod=$${name%-kmod.spec}; \
	    work="$(TOPDIR)/.tar-$$mod"; \
	    rm -rf "$$work"; mkdir -p "$$work/$$mod"; \
	    cp Linux/src/$$mod/* "$$work/$$mod/"; \
	    cp LICENSE "$$work/$$mod/LICENSE"; \
	    tar czf "$(SOURCES)/$$mod-$$ver.tar.gz" -C "$$work" "$$mod"; \
	    rm -rf "$$work"; \
	    { echo "%global snap $$snap"; cat "$(spec)"; } > "$(TOPDIR)/SPECS/$$name" ;; \
	  ryzen_smu-kmod.spec) \
	    spectool -g -C "$(SOURCES)" "$(spec)"; \
	    cp "$(spec)" "$(TOPDIR)/SPECS/$$name" ;; \
	  *) echo "unknown spec: $$name" >&2; exit 1 ;; \
	esac; \
	rpmbuild \
		--define "_topdir $(TOPDIR)" \
		--define "_sourcedir $(SOURCES)" \
		--define "_srcrpmdir $(outdir)" \
		-bs "$(TOPDIR)/SPECS/$$name"
