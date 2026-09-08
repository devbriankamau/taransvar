#!/bin/sh
# Verify that TaraSec traffic stays on the kernel inspection path.
# Usage:
#   sudo ./misc/check_tagging_path.sh gateway [demo-endpoint-ip]
#   sudo ./misc/check_tagging_path.sh receiver [gateway-netbird-ip]

set -u

mode="${1:-}"
peer="${2:-}"

case "$mode" in
	gateway|receiver) ;;
	*)
		echo "Usage: sudo $0 gateway [demo-endpoint-ip]"
		echo "       sudo $0 receiver [gateway-netbird-ip]"
		exit 2
		;;
esac

echo "=== TaraSec tagging-path check: $mode ==="
hostname

echo "=== Components ==="
lsmod | grep '^tarakernel ' || echo "ERROR: tarakernel is not loaded"
pgrep -a taralink || echo "ERROR: taralink is not running"
if [ -r /sys/module/tarakernel/parameters/debug_level ]; then
	printf 'tarakernel debug_level='
	cat /sys/module/tarakernel/parameters/debug_level
fi

if [ "$mode" = gateway ]; then
	echo "=== Forwarding ==="
	sysctl net.ipv4.ip_forward

	echo "=== Configured demo endpoints ==="
	if [ -r /etc/tarasecfw.conf ]; then
		grep -E '^(DEMO_NODES|DEMO_NODE_NAMES|HOTSPOT_INTERFACE|WAN_INTERFACE)=' /etc/tarasecfw.conf || true
	else
		echo "ERROR: /etc/tarasecfw.conf is missing"
	fi

	echo "=== openNDS flow-offload ordering ==="
	rules="$(nft -a list ruleset 2>/dev/null || true)"
	chain="$(printf '%s\n' "$rules" | sed -n '/chain nds_ft_OUT {/,/^\t}/p')"
	printf '%s\n' "$chain"
	marker_line="$(printf '%s\n' "$chain" | grep -n -m1 'tarasec-inspection-no-offload' | cut -d: -f1)"
	flow_line="$(printf '%s\n' "$chain" | grep -n -m1 'flow add @ndsftOUT' | cut -d: -f1)"
	if [ -z "$marker_line" ]; then
		echo "ERROR: TaraSec no-offload rule is missing"
	elif [ -n "$flow_line" ] && [ "$marker_line" -ge "$flow_line" ]; then
		echo "ERROR: flow offload precedes TaraSec inspection exclusion"
	else
		echo "OK: hotspot traffic returns before flow-offload admission"
	fi

	if [ -n "$peer" ]; then
		echo "=== Route to $peer ==="
		ip route get "$peer" || true
	fi

	echo "=== Recent tagging decisions ==="
	dmesg | grep -E 'tarakernel: (tagged traffic|FW: outbound tag|Protection unavailable)' | tail -n 20 || true
else
	if [ -n "$peer" ]; then
		echo "=== Recent TCP traffic received from $peer ==="
		if mysql -N -B taransvar -e "SHOW COLUMNS FROM traffic LIKE 'tagSeverity';" 2>/dev/null | grep -q '^tagSeverity'; then
			severity_column=",tagSeverity"
		else
			severity_column=""
			echo "NOTE: traffic.tagSeverity is unavailable; showing the raw tag."
		fi
		mysql taransvar -e "SELECT trafficId,INET_NTOA(ipFrom) AS ipFrom,portFrom,INET_NTOA(ipTo) AS ipTo,portTo,tag${severity_column},lastSeen FROM traffic WHERE ipFrom=INET_ATON('$peer') ORDER BY trafficId DESC LIMIT 20;" || true
	else
		echo "Peer IP omitted; skipping receiver database query"
	fi
	echo "=== Receiver demo capability ==="
	if [ -r /etc/tarasecfw.conf ]; then
		grep -E '^DEMO_NODE=' /etc/tarasecfw.conf || echo "DEMO_NODE is not configured"
	fi
fi
