# These are just convenience scripts, NOT a build system!

appid := env("BAZAAR_APPID", "io.github.kolunmi.Bazaar")
manifest := "./build-aux/flatpak/" + appid + ".json"
branch := env("BAZAAR_BRANCH", "master")
just := just_executable()

alias run := run-base

run-base: build-base
    ./build/src/bazaar

build-base:
    meson setup build --wipe
    ninja -C build

build-flatpak $manifest=manifest $branch=branch:
    #!/usr/bin/env bash
    set -xeuo pipefail
    flatpak --user remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

    FLATPAK_BUILDER_DIR=$(realpath ".flatpak-builder")
    BUILDER_ARGS+=(--default-branch="${branch}")
    BUILDER_ARGS+=(--state-dir="${FLATPAK_BUILDER_DIR}/flatpak-builder")
    BUILDER_ARGS+=("--user")
    BUILDER_ARGS+=("--ccache")
    BUILDER_ARGS+=("--force-clean")
    BUILDER_ARGS+=("--install")
    BUILDER_ARGS+=("--disable-rofiles-fuse")
    BUILDER_ARGS+=("${FLATPAK_BUILDER_DIR}/build-dir")
    BUILDER_ARGS+=("${manifest}")

    if which flatpak-builder &>/dev/null ; then
        flatpak-builder "${BUILDER_ARGS[@]}"
    else
        flatpak run org.flatpak.Builder "${BUILDER_ARGS[@]}"
    fi

build-flatpak-bundle $appid=appid:
    flatpak build-export repo "${FLATPAK_BUILDER_DIR}/build-dir"
    flatpak build-bundle repo "${appid}".flatpak "${appid}"

build-rpm:
    #!/usr/bin/env bash
    mkdir -p rpmbuild
    podman run --rm -i -v .:/build:Z -v ./rpmbuild:/root/rpmbuild:Z registry.fedoraproject.org/fedora:latest <<'EOF'
    set -xeuo pipefail
    dnf install -y rpmdevtools
    mkdir -p $HOME/rpmbuild/SOURCES
    RPMDIR="/build/build-aux/rpm"
    cp "${RPMDIR}"/* $HOME/rpmbuild/SOURCES
    spectool -agR "${RPMDIR}"/bazaar.spec
    dnf builddep -y "${RPMDIR}"/bazaar.spec
    rpmbuild -bb "${RPMDIR}"/bazaar.spec
    EOF

[private]
default:
    @{{ just }} --list

# Check just Syntax
[group('just')]
check:
    #!/usr/bin/bash
    find . -type f -name "*.just" | while read -r file; do
    	echo "Checking syntax: $file"
    	{{ just }} --unstable --fmt --check -f $file
    done
    echo "Checking syntax: Justfile"
    {{ just }} --unstable --fmt --check -f Justfile

# Fix {{ just }} Syntax
[group('{{ just }}')]
fix:
    #!/usr/bin/bash
    find . -type f -name "*.just" | while read -r file; do
    	echo "Checking syntax: $file"
    	{{ just }} --unstable --fmt -f $file
    done
    echo "Checking syntax: Justfile"
    {{ just }} --unstable --fmt -f Justfile || { exit 1; }

update-flatpak:
