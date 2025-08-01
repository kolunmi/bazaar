#!/bin/sh

INSTR="$1"

FALLBACK_VERSION='0.2.3 no-git-info'

case "$INSTR" in
    get-vcs)
        git -C "$MESON_SOURCE_ROOT" describe --always --dirty || echo "$FALLBACK_VERSION"
        ;;
    *)
        echo invalid arguments 1>&2
        exit 1
        ;;
esac
