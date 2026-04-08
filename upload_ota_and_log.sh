#!/usr/bin/env sh

set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <ip-address>"
    exit 1
fi

TARGET_IP="$1"

pio run -e ota -t upload --upload-port "$TARGET_IP" && ./start_log.sh "$TARGET_IP"
