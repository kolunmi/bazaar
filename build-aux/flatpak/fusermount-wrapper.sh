#!/bin/sh
set -eo pipefail
echo "Running fusermount wrapper, redirecting to host..."
export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/flatpak/bus

binary="fusermount"
if flatpak-spawn --host fusermount3 --version &> /dev/null ; then
  binary="fusermount3"
fi

[ ! -z "$_FUSE_COMMFD" ] && export FD_ARGS="--env=_FUSE_COMMFD=${_FUSE_COMMFD} --forward-fd=${_FUSE_COMMFD}"
exec flatpak-spawn --host ${FD_ARGS} "$binary" "$@"
