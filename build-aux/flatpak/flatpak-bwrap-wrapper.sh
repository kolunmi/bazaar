#!/bin/sh
echo "Running flatpak-bwrap wrapper, redirecting to host..."
export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/flatpak/bus

# Inspect which fds are currently opened, and forward them to the host side.
echo "Open file descriptors:"
fds=""
for fd in $(ls /proc/$$/fd); do
  case "$fd" in
    0|1|2|3|255)
      ;;
    *)
      fds="${fds} --forward-fd=$fd"
      echo "Forwarding fd $fd"
      ;;
  esac
done

if flatpak-spawn --host bwrap --version ; then
  echo "Using bwrap."
  binary="bwrap"
else
  echo "Unable to execute bwrap, falling back to flatpak-bwrap."
  binary="flatpak-bwrap"
fi

# The actual call on the host side
echo "Executing actual command on host system using flatpak-spawn..."
exec flatpak-spawn --host $fds $binary "$@"
