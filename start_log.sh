#!/usr/bin/env sh

set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 <ip-or-hostname> [port]"
    exit 1
fi

TARGET_HOST="$1"
TARGET_PORT="${2:-2323}"

echo "Waiting for ${TARGET_HOST} to respond to ping..."
while ! ping -c 1 -W 1 "$TARGET_HOST" >/dev/null 2>&1; do
    sleep 1
done

echo "${TARGET_HOST} is online. Connecting with nc to port ${TARGET_PORT}..."
exec nc "$TARGET_HOST" "$TARGET_PORT"
