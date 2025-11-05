#!/usr/bin/env bash

DEPS=(
    gtk4-devel
    libadwaita-devel
    libdex-devel
    flatpak-devel
    libxmlb-devel
    appstream-devel
    glycin-devel
    glycin-gtk4-devel
    libyaml-devel
    libsoup3-devel
    json-glib-devel
    md4c-devel
)

dnf install ${DEPS[@]}
