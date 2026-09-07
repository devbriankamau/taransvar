// module_packet_interpreter.c
//
// Experimental packet-inspection hook used by the TaraSec research prototype.
// Production traffic handling must not inspect or log application payloads here.

#include "module_packet_interpreter.h"

/*
 * Return whether this source is selected for experimental metadata inspection.
 * The current allow-list is intentionally narrow. This function never inspects
 * packet payload and has no effect on whether the packet is accepted or dropped.
 */
int inspectThis(u32 sourceIp)
{
	char sourceIpAddr[16];
	int n;

	const char *drop[] = {
		"151.101",       // Fastly
		"172.217",       // Google
		"20.114.189.70", // MS Ads
		"172.232",       // Proxy data
	};
	const char *onlyShowIf[] = {"81.88.18.98", "98.18.88.81"};

	snprintf(sourceIpAddr, sizeof(sourceIpAddr), "%u.%u.%u.%u", IPADDRESS(sourceIp));

	for (n = 0; n < ARRAY_SIZE(drop); n++)
		if (strstr(sourceIpAddr, drop[n]) == sourceIpAddr)
			return 0;

	for (n = 0; n < ARRAY_SIZE(onlyShowIf); n++)
		if (strcmp(sourceIpAddr, onlyShowIf[n]) == 0)
			return 1;

	return ARRAY_SIZE(onlyShowIf) == 0;
}

/*
 * Historical versions of this function sampled bytes after the TCP header and
 * printed them to the kernel log. Besides being unnecessary for TaraSec's
 * cooperative-security protocol, that code did not validate skb bounds and
 * could expose application data. It has deliberately been removed.
 *
 * Keep this hook metadata-only. Any future packet parser must use kernel-safe
 * skb accessors, validate all header lengths/bounds, avoid payload logging, and
 * receive separate security review before being enabled.
 */
int packetInterpreter(struct _PacketInspection *pPacket)
{
	if (!pPacket || !pPacket->skb || !pPacket->ip_header)
		return NF_ACCEPT;

	if (pPacket->ip_header != (struct iphdr *)skb_network_header(pPacket->skb))
		return NF_ACCEPT;

	if (++nPackageSequenceNumber < N_INSPECT_PACKAGE_START_NUMBER)
		return NF_ACCEPT;

	if (!inspectThis(ntohl(pPacket->ip_header->saddr)))
		return NF_ACCEPT;

	/* Metadata accounting only; do not read or log TCP payload. */
	if (pPacket->ip_header->protocol == IPPROTO_TCP &&
	    nPacketsInspected < N_INSPECTION_PACKETS_TO_SHOW)
		nPacketsInspected++;

	return NF_ACCEPT;
}
