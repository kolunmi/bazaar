#!/bin/sh
echo "Running fusermount wrapper, redirecting to host..."
export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/flatpak/bus

if flatpak-spawn --host fusermount --version ; then
  echo "Using fusermount."
  binary="fusermount"
else
  # Some distros don't ship `fusermount` anymore, but `fusermount3` like Alpine
  echo "Unable to execute fusermount, trying to use fusermount3."
  binary="fusermount3"
fi

[ ! -z "$_FUSE_COMMFD" ] && export FD_ARGS="--env=_FUSE_COMMFD=${_FUSE_COMMFD} --forward-fd=${_FUSE_COMMFD}"
exec flatpak-spawn --host $FD_ARGS $binary "$@"
