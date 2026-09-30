#!/bin/sh
set -e

# Debian's Apache configuration obtains its runtime paths and worker user here.
. /etc/apache2/envvars
set -u
mkdir -p "$APACHE_RUN_DIR" "$APACHE_LOCK_DIR"
# A stopped container may leave its PID file behind when restarted.
rm -f "$APACHE_PID_FILE"

exec /usr/sbin/apache2 -D FOREGROUND
