#!/usr/bin/env bash

set -xeuo pipefail

GLYCIN_TARGET_VERSION="${1:-1.2.2}"

GLYCIN_LOCK_FILE_DIR=$(mktemp -d)
trap 'rm -r "${GLYCIN_LOCK_FILE_DIR}"' EXIT
curl -s -L "https://gitlab.gnome.org/GNOME/glycin/-/archive/${GLYCIN_TARGET_VERSION}/glycin-${GLYCIN_TARGET_VERSION}.tar.gz" | tar xzf - -C "${GLYCIN_LOCK_FILE_DIR}"

podman run --rm -it \
  -v .:/tmp/build:Z \
  -v "${GLYCIN_LOCK_FILE_DIR}:${GLYCIN_LOCK_FILE_DIR}:Z" \
  --pull newer \
  docker.io/library/python:latest \
  sh -c "pip install aiohttp toml && /tmp/build/flatpak-builder-tools/cargo/flatpak-cargo-generator.py ${GLYCIN_LOCK_FILE_DIR}/glycin-${GLYCIN_TARGET_VERSION}/Cargo.lock -o /tmp/build/glycin-cargo-sources.json"
