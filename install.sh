#!/bin/sh
# Install the already-built binary and a systemd system unit.
# Not for NixOS. See README, Packaging.
set -eu
cd "$(dirname "$0")"

if [ -e /etc/NIXOS ]; then
	echo "daeboard: NixOS does not use this script. Start with systemd-run, or add a module by hand." >&2
	exit 1
fi
if [ "$(id -u)" -ne 0 ]; then
	echo "daeboard: run as root: sudo ./install.sh" >&2
	exit 1
fi
if [ ! -x ./daeboard ]; then
	echo "daeboard: build first (make), then re-run." >&2
	exit 1
fi
if ! command -v systemctl >/dev/null 2>&1; then
	echo "daeboard: systemd is required." >&2
	exit 1
fi

install -d /usr/local/bin /etc/systemd/system
install -m 755 ./daeboard /usr/local/bin/daeboard
install -m 644 contrib/daeboard.service /etc/systemd/system/daeboard.service
systemctl daemon-reload
systemctl enable --now daeboard.service
echo "daeboard: installed and started (/usr/local/bin/daeboard)"
