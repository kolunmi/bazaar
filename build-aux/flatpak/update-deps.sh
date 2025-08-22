#!/usr/bin/env bash

set -xeuo pipefail

GLYCIN_TARGET_VERSION="${1:-1.2.3}"
BAZAAR_TARGET_VERSION="${2:-0.3.0}"

BAZAAR_LOCK_FILE_DIR=$(mktemp -d)
trap 'rm -r "${BAZAAR_LOCK_FILE_DIR}"' EXIT
curl -s -L "https://github.com/kolunmi/bazaar/archive/${BAZAAR_TARGET_VERSION}.tar.gz" | tar xzf - -C "${BAZAAR_LOCK_FILE_DIR}"

podman run --rm -it \
  -v .:/tmp/build:Z \
  -v "${BAZAAR_LOCK_FILE_DIR}:${BAZAAR_LOCK_FILE_DIR}:Z" \
  --pull newer \
  docker.io/library/python:latest \
  sh -c "pip install aiohttp toml && /tmp/build/flatpak-builder-tools/cargo/flatpak-cargo-generator.py ${BAZAAR_LOCK_FILE_DIR}/bazaar-${BAZAAR_TARGET_VERSION}/bazaar-ui/Cargo.lock -o /tmp/build/bazaar-cargo-sources.json"

GLYCIN_LOCK_FILE_DIR=$(mktemp -d)
trap 'rm -r "${GLYCIN_LOCK_FILE_DIR}"' EXIT
curl -s -L "https://gitlab.gnome.org/GNOME/glycin/-/archive/${GLYCIN_TARGET_VERSION}/glycin-${GLYCIN_TARGET_VERSION}.tar.gz" | tar xzf - -C "${GLYCIN_LOCK_FILE_DIR}"

podman run --rm -it \
  -v .:/tmp/build:Z \
  -v "${GLYCIN_LOCK_FILE_DIR}:${GLYCIN_LOCK_FILE_DIR}:Z" \
  --pull newer \
  docker.io/library/python:latest \
  sh -c "pip install aiohttp toml && /tmp/build/flatpak-builder-tools/cargo/flatpak-cargo-generator.py ${GLYCIN_LOCK_FILE_DIR}/glycin-${GLYCIN_TARGET_VERSION}/Cargo.lock -o /tmp/build/glycin-cargo-sources.json"
