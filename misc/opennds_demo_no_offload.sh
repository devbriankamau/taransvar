#!/bin/bash

# Keep TaraSec demo traffic in the normal Netfilter path. openNDS flow
# offloading would otherwise bypass tarakernel after the first packet.

set -u

CONF="${1:-/etc/tarasecfw.conf}"
TABLE_FAMILY="ip"
TABLE_NAME="nds_mangle"
CHAIN_NAME="nds_ft_OUT"
COMMENT="tarasec-demo-no-offload"

[ "$(id -u)" -eq 0 ] || {
    echo "This script must run as root." >&2
    exit 1
}

[ -r "$CONF" ] || exit 0

# shellcheck disable=SC1090
source "$CONF"
DEMO_NODES="${DEMO_NODES:-${HOTSPOT_ALLOWED_NETBIRD_NODES:-}}"

[ -n "$DEMO_NODES" ] || exit 0
command -v nft >/dev/null 2>&1 || exit 0

for _attempt in $(seq 1 20); do
    if nft list chain "$TABLE_FAMILY" "$TABLE_NAME" "$CHAIN_NAME" >/dev/null 2>&1; then
        break
    fi
    sleep 1
done

if ! nft list chain "$TABLE_FAMILY" "$TABLE_NAME" "$CHAIN_NAME" >/dev/null 2>&1; then
    echo "openNDS flow-offload chain is unavailable; no exclusion installed." >&2
    exit 0
fi

IFS=',' read -ra NODE_LIST <<< "$DEMO_NODES"
for NODE_IP in "${NODE_LIST[@]}"; do
    NODE_IP="${NODE_IP//[[:space:]]/}"
    [ -n "$NODE_IP" ] || continue

    if ! [[ "$NODE_IP" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]]; then
        echo "Ignoring invalid DEMO_NODES address: $NODE_IP" >&2
        continue
    fi

    if nft -a list chain "$TABLE_FAMILY" "$TABLE_NAME" "$CHAIN_NAME" |
       grep -Fq "ip daddr $NODE_IP" &&
       nft -a list chain "$TABLE_FAMILY" "$TABLE_NAME" "$CHAIN_NAME" |
       grep -Fq "comment \"$COMMENT\""; then
        continue
    fi

    nft insert rule "$TABLE_FAMILY" "$TABLE_NAME" "$CHAIN_NAME"         ip daddr "$NODE_IP" counter return comment "$COMMENT"
    echo "Excluded $NODE_IP from openNDS flow offloading."
done
