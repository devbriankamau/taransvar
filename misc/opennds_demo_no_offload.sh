#!/bin/bash

# Keep all openNDS client traffic in the normal Netfilter path. TaraSec must
# inspect every packet because a unit's threat state can change after a flow is
# established, and both production and demo partner traffic may require tags.

set -u

TABLE_FAMILY="ip"
TABLE_NAME="nds_mangle"
CHAIN_NAME="nds_ft_OUT"
COMMENT="tarasec-inspection-no-offload"

[ "$(id -u)" -eq 0 ] || {
    echo "This script must run as root." >&2
    exit 1
}

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

if nft -a list chain "$TABLE_FAMILY" "$TABLE_NAME" "$CHAIN_NAME" |
   grep -Fq "comment \"$COMMENT\""; then
    exit 0
fi

# This unconditional return is inserted at the beginning of nds_ft_OUT, before
# openNDS's "flow add @ndsftOUT" rule. It disables only fast-path admission;
# openNDS authentication, firewall decisions and ordinary forwarding continue.
nft insert rule "$TABLE_FAMILY" "$TABLE_NAME" "$CHAIN_NAME" \
    counter return comment "$COMMENT"

echo "Disabled openNDS flow offloading so TaraSec can inspect all client traffic."
