#!/bin/sh
set -eu

if [ "$#" -ne 2 ] || [ "$1" != "tcp" ]; then
    echo "usage: dxa-socket-healthcheck tcp <port>" >&2
    exit 2
fi

port="$2"
case "$port" in
    ''|*[!0-9]*)
        echo "port must be an integer" >&2
        exit 2
        ;;
esac
if [ "$port" -lt 1 ] || [ "$port" -gt 65535 ]; then
    echo "port must be between 1 and 65535" >&2
    exit 2
fi

port_hex="$(printf '%04X' "$port")"
for table in /proc/net/tcp /proc/net/tcp6; do
    [ -r "$table" ] || continue
    while read -r _ local_address _ state _; do
        case "$local_address" in
            *:"$port_hex")
                [ "$state" = "0A" ] && exit 0
                ;;
        esac
    done < "$table"
done

exit 1
