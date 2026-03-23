//module_tagging.c

#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <linux/netfilter.h>
//#include <linux/skbuff.h>
//#include <linux/byteorder/generic.h>
//#include <net/checksum.h>

#define TCPOPT_TIMESTAMP 8
#define TCPOLEN_TIMESTAMP 10

//static void queue_udp_send_from_skb(struct sk_buff *skb);       //Defined in module_stolen.c


int isNewTcpConnection(struct sk_buff *skb);
int isNewTcpConnection(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct tcphdr _tcph, *tcph;

    if (!skb)
        return 0;

    if (!pskb_may_pull(skb, sizeof(struct iphdr)))
        return 0;

    iph = ip_hdr(skb);
    if (!iph || iph->version != 4 || iph->protocol != IPPROTO_TCP)
        return 0;

    tcph = skb_header_pointer(skb, ip_hdrlen(skb), sizeof(_tcph), &_tcph);
    if (!tcph)
        return 0;

    if (tcph->syn && !tcph->ack) {
        //printk("tarakernel: TCP SYN seen\n");
        return 1;
    }

    return 0;
}

#define CTMARK_THREAT_SENT 0x100

int shouldSendThreatUdp(struct sk_buff *skb);
int shouldSendThreatUdp(struct sk_buff *skb)
{
    struct nf_conn *ct;
    enum ip_conntrack_info ctinfo;
    struct iphdr *iph;
    struct tcphdr _tcph, *tcph;

    if (!skb)
        return 0;

    iph = ip_hdr(skb);
    if (!iph || iph->version != 4 || iph->protocol != IPPROTO_TCP)
        return 0;

    tcph = skb_header_pointer(skb, ip_hdrlen(skb), sizeof(_tcph), &_tcph);
    if (!tcph)
        return 0;

    ct = nf_ct_get(skb, &ctinfo);
    if (!ct)
        return 0;

    /* only on SYN */
    if (!(tcph->syn && !tcph->ack))
        return 0;

    printk("tarakernel: ct=%px ctinfo=%d mark(before)=0x%x\n", ct, ctinfo, ct->mark);

    if (ct->mark & CTMARK_THREAT_SENT) {
        printk("tarakernel: threat UDP already sent for this conntrack\n");
        return 0;
    }

    ct->mark |= CTMARK_THREAT_SENT;

    printk("tarakernel: ct=%px mark(after)=0x%x\n", ct, ct->mark);
    printk("tarakernel: first SYN for this conntrack, sending threat UDP\n");
    return 1;
}

int isNew(struct sk_buff *skb);
int isNew(struct sk_buff *skb)
{
    //This way of figuring out if it's a new connection cause trouble because we steal packets and retransmit them causing it to always be new...

	//Check if new connection
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;

	ct = nf_ct_get(skb, &ctinfo);
	if (ct) 
	{
		//printk(KERN_INFO "tarakernel: PREROUTING ct=%px mark=%u ctinfo=%d\n", ct, ct->mark, ctinfo);		

		if (ctinfo == IP_CT_NEW || ctinfo == IP_CT_RELATED) 
		{
			printk("tarakernel: ************* NEW connection ********** Add spceial handling..\n");
			//NOTE! Can't only send tag info for new connections... in case tagged in same... all packets will be tagged, though...
            return 1;
		}
		//else
		//	printk("tarakernel: ************* related connection **********\n");
	}

    printk("tarakernel: ************* Established connection.\n");
    return 0;

}

#define SYN_SEEN_TIMEOUT (5 * HZ)

int seen_recently(struct sk_buff *skb);
int seen_recently(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct tcphdr _tcph, *tcph;
    int nAvailable = -1;
    int n;
    unsigned long now = jiffies;

    if (!skb)
        return 0;

    iph = ip_hdr(skb);
    if (!iph || iph->version != 4 || iph->protocol != IPPROTO_TCP)
        return 0;

    tcph = skb_header_pointer(skb, ip_hdrlen(skb), sizeof(_tcph), &_tcph);
    if (!tcph)
        return 0;

    for (n = 0; n < N_MAX_SYN_SEEN; n++)
    {
        struct syn_seen_key *pCheck = pSetup->pSynSeen[n];

        if (!pCheck) {
            if (nAvailable == -1)
                nAvailable = n;
            continue;
        }

        if (0)//time_after(now, pCheck->expires)) 
        {
            kfree(pCheck);
            pSetup->pSynSeen[n] = NULL;
            if (nAvailable == -1)
                nAvailable = n;
            continue;
        }

        //NOTE! No need to save from- to combination... we're only interested in infections on source.. no matter who they're communicating with
//        if (iph->saddr == pCheck->saddr && iph->daddr == pCheck->daddr && tcph->source == pCheck->sport && tcph->dest == pCheck->dport)
        if (iph->saddr == pCheck->saddr && tcph->source == pCheck->sport)
        {
            printk("tarakernel: Record found at slot %d\n",n);
            return 1;
        }
    }

    if (nAvailable > -1)
    {
        struct syn_seen_key *pNew;

        pNew = kmalloc(sizeof(*pNew), GFP_ATOMIC);
        if (!pNew) {
            printk("tarakernel: WARNING failed to allocate syn_seen_key\n");
            return 1;
        }

        pNew->saddr = iph->saddr;
        //pNew->daddr = iph->daddr;
        pNew->sport = tcph->source;
        //pNew->dport = tcph->dest;
        pNew->expires = now + SYN_SEEN_TIMEOUT;

        pSetup->pSynSeen[nAvailable] = pNew;

        printk("tarakernel: Address saved at slot %d (%pI4:%d)\n", nAvailable, &iph->saddr, ntohs(tcph->source));   
        return 0;
    }

    printk("tarakernel: WARNING setup->pSynSeen table is full (%d slots)\n", N_MAX_SYN_SEEN);
    return 1;
}

int isNewConnectionBasedOnStoredIpPortCombo(struct sk_buff *skb);
int isNewConnectionBasedOnStoredIpPortCombo(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct tcphdr _tcph, *tcph;

    if (!skb)
        return 0;

    iph = ip_hdr(skb);
    if (!iph || iph->version != 4 || iph->protocol != IPPROTO_TCP)
        return 0;

    tcph = skb_header_pointer(skb, ip_hdrlen(skb), sizeof(_tcph), &_tcph);

    if (!tcph)
        return 0;

    if (!(tcph->syn && !tcph->ack))
        return 0;

    if (seen_recently(skb)) {
        printk("tarakernel: SYN already seen recently (based on stored info)\n");
        return 0;
    }

    printk("tarakernel: Not a stored ip/port combo. Send threat UDP\n");
    return 1;
}


int isNewConnection(struct sk_buff *skb)
{
    //return false;   //ØT 260317 - not working...

    //bool bNew = isNew(skb);       Not working... they're all new
    //bool bNew = isNewTcpConnection(skb);  NOT WORKING... said all packets are new connection
    //bool bNew = shouldSendThreatUdp(skb);

    
    bool bNew = isNewConnectionBasedOnStoredIpPortCombo(skb);
    if (bNew)
		printk("tarakernel: ************* NEW connection ********** Add spceial handling..\n");
    else
		printk("tarakernel: ************* related connection **********\n");

    return bNew;
   
}


/****************** Implement later.... another way of tagging...
static int tcp_read_timestamp_option(struct sk_buff *skb,
                                     __be32 *tsval_be,
                                     __be32 *tsecr_be)
{
    struct iphdr *iph;
    struct tcphdr *tcph;
    unsigned char *optptr;
    int tcph_len;
    int optlen;
    int i;

    if (!skb || !tsval_be || !tsecr_be)
        return 0;

    iph = ip_hdr(skb);
    if (!iph || iph->protocol != IPPROTO_TCP)
        return 0;

    tcph = tcp_hdr(skb);
    if (!tcph)
        return 0;

    tcph_len = tcph->doff * 4;

    if (tcph_len < sizeof(struct tcphdr))
        return 0;

    optlen = tcph_len - sizeof(struct tcphdr);
    if (optlen <= 0)
        return 0;

    optptr = (unsigned char *)(tcph + 1);

    for (i = 0; i < optlen; ) {

        u8 kind = optptr[i];

        if (kind == 0)
            break;              // End of options *

        if (kind == 1) {        // NOP *
            i++;
            continue;
        }

        if (i + 1 >= optlen)
            break;

        u8 len = optptr[i + 1];

        if (len < 2 || i + len > optlen)
            break;

        if (kind == TCPOPT_TIMESTAMP && len == TCPOLEN_TIMESTAMP) {

            memcpy(tsval_be, &optptr[i + 2], sizeof(__be32));
            memcpy(tsecr_be, &optptr[i + 6], sizeof(__be32));

            return 1;
        }

        i += len;
    }

    return 0;
}
    */

/* ************* Implement later....
static int tcp_set_timestamp_option(struct sk_buff *skb, bool set_tsval, __be32 new_tsval_be,
                                    bool set_tsecr, __be32 new_tsecr_be)
{
    struct iphdr *iph;
    struct tcphdr *tcph;
    unsigned char *optptr;
    int tcph_len;
    int optlen;
    int i;

    __be32 old_tsval_be, old_tsecr_be;

    if (!skb)
        return 0;

    if (skb_linearize(skb))
        return 0;

    iph = ip_hdr(skb);
    if (!iph || iph->protocol != IPPROTO_TCP)
        return 0;

    tcph = tcp_hdr(skb);
    if (!tcph)
        return 0;

    tcph_len = tcph->doff * 4;
    if (tcph_len < sizeof(struct tcphdr))
        return 0;

    if (skb_ensure_writable(skb, skb_transport_offset(skb) + tcph_len))
        return 0;

    // Re-fetch pointers after skb_ensure_writable *
    iph = ip_hdr(skb);
    tcph = tcp_hdr(skb);

    optlen = tcph_len - sizeof(struct tcphdr);
    if (optlen <= 0)
        return 0;

    optptr = (unsigned char *)(tcph + 1);

    for (i = 0; i < optlen; ) {
        u8 kind = optptr[i];

        if (kind == 0) {
            break; // End of options 
        } else if (kind == 1) {
            i++;
            continue; // NOP *
        } else {
            u8 len;

            if (i + 1 >= optlen)
                break;

            len = optptr[i + 1];
            if (len < 2 || i + len > optlen)
                break;

            if (kind == TCPOPT_TIMESTAMP && len == TCPOLEN_TIMESTAMP) {
                memcpy(&old_tsval_be, &optptr[i + 2], sizeof(__be32));
                memcpy(&old_tsecr_be, &optptr[i + 6], sizeof(__be32));

                if (set_tsval) {
                    memcpy(&optptr[i + 2], &new_tsval_be, sizeof(__be32));
                    inet_proto_csum_replace4(&tcph->check, skb, old_tsval_be, new_tsval_be, false);
                }

                if (set_tsecr) {
                    memcpy(&optptr[i + 6], &new_tsecr_be, sizeof(__be32));
                    inet_proto_csum_replace4(&tcph->check, skb, old_tsecr_be, new_tsecr_be, false);
                }

                return 1;
            }

            i += len;
        }
    }

    return 0;
}
    */

uint8_t getDscp(struct _PacketInspection *pPacket)
{
    //struct iphdr *ip_header = pPacket->ip_header;
    return pPacket->ip_header->tos >> 2;
}

void setDscp(struct iphdr *iph, uint8_t newDscp)
{
        uint8_t ecn = iph->tos & 0x03;          // preserve ECN
        iph->tos = (newDscp << 2) | ecn;           // set DSCP
        ip_send_check(iph);     //unlike urg_ptr, tos is in IP header so have to recalc check sum
}

void recalc_tcp_checksum(struct sk_buff *skb);
void recalc_tcp_checksum(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct tcphdr *tcph;
    unsigned int ihl;
    unsigned int tcplen;

    if (!skb)
    {
        printk("tarakernel: ** ERROR ** No skb\n");
        return;
    }

    if (!pskb_may_pull(skb, sizeof(struct iphdr)))
    {
        printk("tarakernel: ** ERROR ** unable to pskb_may_pull()\n");
        return;
    }

    iph = ip_hdr(skb);
    if (!iph || iph->version != 4)
    {
        printk("tarakernel: ** ERROR ** Not ipv4 when recalcing TPC checksum\n");
        return;
    }

    if (iph->protocol != IPPROTO_TCP)
    {
        printk("tarakernel: ** ERROR ** Not TCP when recalcing TPC checksum\n");
        return;
    }


    ihl = iph->ihl * 4;

    if (!pskb_may_pull(skb, ihl + sizeof(struct tcphdr)))
    {
        printk("tarakernel: ** ERROR ** may not pull (2)\n");
        return;
    }

    tcph = tcp_hdr(skb);

    /* TCP length from IP total length, not skb->len */
    tcplen = ntohs(iph->tot_len) - ihl;

    tcph->check = 0;
    skb->ip_summed = CHECKSUM_NONE;

    tcph->check = tcp_v4_check(tcplen,
                               iph->saddr,
                               iph->daddr,
                               csum_partial((char *)tcph, tcplen, 0));
}


void recalcChecksum(struct _PacketInspection *pPacket)
{
    recalc_tcp_checksum(pPacket->skb);
    /* old version not safe...
    //Recalculate check sums. Wrong check sum is most likely reason why packets are being dropped after setting urg_ptr

    struct iphdr *iph = ip_hdr(pPacket->skb);
    struct tcphdr *tcph = tcp_hdr(pPacket->skb);
    unsigned int tcplen = pPacket->skb->len - ip_hdrlen(pPacket->skb);

    tcph->check = 0;
    tcph->check = tcp_v4_check(tcplen,
                           iph->saddr,
                           iph->daddr,
                           csum_partial((char *)tcph, tcplen, 0));
            */
}


void sendUdpPacketToReceiver(struct _PacketInspection *pPacket);
void sendUdpPacketToReceiver(struct _PacketInspection *pPacket)
{
    printk("tarakernel: new session and infected sender.. This is when to send UDP packet.. (but not finished)\n");
    //NOTE! Caller return NF_STOLEN
}//sendUdpPacketToReceiver()


struct _Remote_infection *findRemoteInfectionInfoReceived(unsigned int sIp, unsigned int sPort, unsigned int *pAvailable)
{
	int n;
	for (n=0; n<N_MAX_REMOTE_INFECTION_INFOS; n++)
	{
		struct _Remote_infection *pInfection = pSetup->cRemoteInfectionInfoReceived[n];
		if (pInfection && pInfection->saddr == sIp && pInfection->sport == sPort)
			return pInfection;

		if (!pInfection && *pAvailable == -1)
			*pAvailable = n;
	}			

	return NULL;	//Not found
}


void initElaboratedThreatInfo(struct _PacketInspection *pPacket)
{
	int nAvailable;
	nAvailable = -1; 	//To tack if changed by findRemoteInfectionInfoReceived()

	struct _Remote_infection *pAlreadyHave = findRemoteInfectionInfoReceived(pPacket->ip_header->saddr, pPacket->tcp_header->source, &nAvailable);

	if (pAlreadyHave)
		printk("tarakernel SENDING: Already have info for %pI4:%d (implement usage??)\n", &pPacket->ip_header->saddr, ntohs(pPacket->tcp_header->source));    
	else
	{
		printk("tarakernel SENDING: Didn't receive elaborated threat info (due to recent reboot or delayed receival??) for %pI4:%d (implement request for it)\n", &pPacket->ip_header->saddr, ntohs(pPacket->tcp_header->source));
		//Store the new info if available slot...

        struct iphdr *iph = ip_hdr(pPacket->skb);
        if (!iph || iph->version != 4 || iph->protocol != IPPROTO_TCP)
        {
            printk("tarakernel SENDING: **** ERROR **** Sending UDP request. Not IPv4 TCP\n");
            return;// NF_ACCEPT;   /* fail open */
        }        

        char cBuf[200];
//        sprintf(cBuf, "%s-%pI4:%d->%pI4:%d", UDP_THREAT_INFO_REQUEST_PREFIX, &iph->saddr, pPacket->tcp_header->source,  &iph->daddr, pPacket->tcp_header->dest);
//        sprintf(cBuf, "%s-%08X:%d->%08X:%d", UDP_THREAT_INFO_REQUEST_PREFIX, ntohl(iph->saddr), ntohs(pPacket->tcp_header->source),  ntohl(iph->daddr), ntohs(pPacket->tcp_header->dest));
        snprintf(cBuf, sizeof(cBuf), "%s-%08X:%u->%08X:%u", UDP_THREAT_INFO_REQUEST_PREFIX, ntohl(iph->saddr), ntohs(pPacket->tcp_header->source),ntohl(iph->daddr),ntohs(pPacket->tcp_header->dest));
        send_udp_json(iph->saddr, htons(TARAKERNEL_LISTENING_TO_PORT), cBuf);
        printk("tarakernel SENDING: *** Requesting threat info from sender: %s\n", cBuf);
    }
}


#include <linux/netfilter.h>
#include <linux/netfilter/nf_conntrack_tuple_common.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_tuple.h>


static struct nf_conn *tara_ct_lookup_v4(__be32 saddr, __be16 sport,
                                         __be32 daddr, __be16 dport,
                                         u8 protonum)
{
    struct nf_conntrack_tuple tuple = {};
    const struct nf_conntrack_tuple_hash *h;
    struct nf_conn *ct;

    tuple.src.l3num = AF_INET;
    tuple.src.u3.ip = saddr;

    tuple.dst.protonum = protonum;
    tuple.dst.dir = IP_CT_DIR_ORIGINAL;
    tuple.dst.u3.ip = daddr;

    switch (protonum) {
    case IPPROTO_TCP:
        tuple.src.u.tcp.port = sport;
        tuple.dst.u.tcp.port = dport;
        break;
    case IPPROTO_UDP:
        tuple.src.u.udp.port = sport;
        tuple.dst.u.udp.port = dport;
        break;
    default:
        return NULL;
    }

    h = nf_conntrack_find_get(&init_net, &nf_ct_zone_dflt, &tuple);
    if (!h)
        return NULL;

    ct = nf_ct_tuplehash_to_ctrack(h);
    return ct; /* caller must nf_ct_put(ct) */
}


int parse_threat_tuple_hang_candidate(const char *lpStartAt, __be32 *src_ip, __be16 *src_port, __be32 *dst_ip, __be16 *dst_port);
int parse_threat_tuple_hang_candidate(const char *lpStartAt, __be32 *src_ip, __be16 *src_port, __be32 *dst_ip, __be16 *dst_port)
{
    char buf[128];
    char *p, *from_ip, *from_port, *to_ip, *to_port, *arrow;

    strscpy(buf, lpStartAt, sizeof(buf));

    p = buf;

    from_ip = p;
    p = strchr(p, ':');
    if (!p) return 0;
    *p++ = '\0';

    from_port = p;
    arrow = strstr(p, "->");
    if (!arrow) return 0;
    *arrow = '\0';
    p = arrow + 2;

    to_ip = p;
    p = strchr(p, ':');
    if (!p) return 0;
    *p++ = '\0';

    to_port = p;

    if (strlen(from_ip) != 8 || strlen(to_ip) != 8)
        return 0;

    *src_ip   = hexstr_to_ip(from_ip);
    *dst_ip   = hexstr_to_ip(to_ip);
    *src_port = htons((u16)simple_strtoul(from_port, NULL, 10));
    *dst_port = htons((u16)simple_strtoul(to_port, NULL, 10));

    return 1;
}


int parse_threat_tuple(const char *lpStartAt, __be32 *src_ip, __be16 *src_port, __be32 *dst_ip, __be16 *dst_port);
int parse_threat_tuple(const char *lpStartAt, __be32 *src_ip, __be16 *src_port, __be32 *dst_ip, __be16 *dst_port)
{
    char buf[128];
    char *p, *from_ip, *from_port, *to_ip, *to_port, *arrow;
    unsigned long sp, dp;

    printk("tara: parse 1 start='%s'\n", lpStartAt);

    strscpy(buf, lpStartAt, sizeof(buf));
    printk("tara: parse 2 buf='%s'\n", buf);

    p = buf;

    from_ip = p;
    p = strchr(p, ':');
    if (!p) { printk("tara: parse fail A\n"); return 0; }
    *p++ = '\0';
    printk("tara: parse 3 from_ip='%s'\n", from_ip);

    from_port = p;
    arrow = strstr(p, "->");
    if (!arrow) { printk("tara: parse fail B\n"); return 0; }
    *arrow = '\0';
    p = arrow + 2;
    printk("tara: parse 4 from_port='%s'\n", from_port);

    to_ip = p;
    p = strchr(p, ':');
    if (!p) { printk("tara: parse fail C\n"); return 0; }
    *p++ = '\0';
    to_port = p;

    printk("tara: parse 5 to_ip='%s' to_port='%s'\n", to_ip, to_port);

    if (strlen(from_ip) != 8 || strlen(to_ip) != 8) {
        printk("tara: parse fail D bad ip lengths %zu %zu\n",
               strlen(from_ip), strlen(to_ip));
        return 0;
    }

    printk("tara: parse 6 before hexstr_to_ip\n");
    *src_ip = hexstr_to_ip(from_ip);
    printk("tara: parse 7 after src hexstr_to_ip\n");
    *dst_ip = hexstr_to_ip(to_ip);
    printk("tara: parse 8 after dst hexstr_to_ip\n");

    sp = simple_strtoul(from_port, NULL, 10);
    dp = simple_strtoul(to_port, NULL, 10);
    printk("tara: parse 9 ports sp=%lu dp=%lu\n", sp, dp);

    *src_port = htons((u16)sp);
    *dst_port = htons((u16)dp);

    printk("tara: parse 10 done %pI4:%u -> %pI4:%u\n",
           src_ip, ntohs(*src_port), dst_ip, ntohs(*dst_port));
    return 1;
}


void initThreatInfo(__be32 ip, __be32 port, struct _Remote_infection *pThreat);
void initThreatInfo(__be32 ip, __be32 port, struct _Remote_infection *pThreat)
{
    pThreat->saddr = ip;
    pThreat->cTag.version_no = TAG_VERSION_NO;
	pThreat->cTag.presumed_infected = 5; //Presumably bot. TO DO: Diversify this....
	//pThreat->cTag.cTag.owners_id = 99; //To be assigned by Taransvar.. To be implemented later...
}

int isRequestForThreatElaboration(char *lpPayload,  struct iphdr *iph, struct udphdr *udph)
{
    /* Now you can read payload[0..payload_len-1] */
    if (strstr(lpPayload, UDP_THREAT_INFO_REQUEST_PREFIX) == lpPayload) {
        //char szFromIp[9];   /* 8 hex chars + NUL */
        //char szToIp[9];
        //unsigned int from_port, to_port;
        __be32 src_ip, dst_ip;
        __be16 src_port, dst_port;
        u8 proto = IPPROTO_TCP;
        struct nf_conn *ct;

        printk("tarakernel: THREAT_INFO_REQUEST payload: %s\n", lpPayload);

        /*
        * Valid payload:
        * THREAT_INFO_REQUEST-0A0A0A0A:55806->0A0A0A03:80
        */
    
       // char *lpStartAt = lpPayload + strlen(UDP_THREAT_INFO_REQUEST_PREFIX)+1;
        //int nFlds;

// Returns 3 when should have returned 4 so parse_threat_tuple() suggested by chatGPT. In case of need for change, maybe better simplify sscanf??         
//        nFlds = sscanf(lpStartAt, "%8[0-9A-Fa-f]:%u->%8[0-9A-Fa-f]:%u", szFromIp, &from_port, szToIp, &to_port);

        if (!parse_threat_tuple(lpPayload + strlen(UDP_THREAT_INFO_REQUEST_PREFIX) + 1,
                            &src_ip, &src_port, &dst_ip, &dst_port)) {
            printk("tarakernel: parse_threat_tuple() failed\n");
            return NF_DROP;
        }

        //printk("tarakernel: parsed %pI4:%u -> %pI4:%u\n", &src_ip, ntohs(src_port), &dst_ip, ntohs(dst_port));
        //printk("tara: lookup tuple %pI4:%u -> %pI4:%u proto=%u\n", &src_ip, ntohs(src_port), &dst_ip, ntohs(dst_port), proto);

        ct = tara_ct_lookup_v4(src_ip, src_port, dst_ip, dst_port, proto);

        if (!ct)
            ct = tara_ct_lookup_v4(dst_ip, dst_port, src_ip, src_port, proto);

        if (ct) {
            const struct nf_conntrack_tuple *t;
            __be16 ct_sport = 0, ct_dport = 0;

            t = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;

            if (t->dst.protonum == IPPROTO_TCP) {
                ct_sport = t->src.u.tcp.port;
                ct_dport = t->dst.u.tcp.port;
            } else if (t->dst.protonum == IPPROTO_UDP) {
                ct_sport = t->src.u.udp.port;
                ct_dport = t->dst.u.udp.port;
            }

            printk("tarakernel: conntrack returned: %pI4:%u -> %pI4:%u proto=%u (** WARNING ** - Not yet sending this to receiver..)\n",
               &t->src.u3.ip, ntohs(ct_sport),
               &t->dst.u3.ip, ntohs(ct_dport),
               t->dst.protonum);

            nf_ct_put(ct);

            //ØT 260323 - Sending reply to request for thread info (at least did here before)... 
           // char cReply[200];
//Orginal(sent from elsewhere)            snprintf(cReply, sizeof(cReply), "Maybe nonsense: %s-%08X:%u->%08X:%u", UDP_THREAT_INFO_REQUEST_PREFIX, ntohl(iph->saddr), ntohs(udph->source),ntohl(iph->daddr),ntohs(iph->dest));
//            snprintf(cReply, sizeof(cReply), "Maybe nonsense: %s-%08X:%u->%08X:%u", UDP_THREAT_INFO_REQUEST_PREFIX, ntohl(iph->daddr), ntohs(udph->source),ntohl(iph->daddr),ntohs(iph->dest));
//            printk("tarakernel SENDING: Used to send this: %s\n", cReply);

            //send_udp_json(iph->saddr, htons(TARAKERNEL_LISTENING_TO_PORT), cReply);
            //struct _InfectionSpecification cThreat;

            __be32 nClientIp = t->src.u3.ip;
            //__be16 nClientPort = ct_sport;

            struct _InfectionSpecification *pInfected = isInfected(nClientIp);

            if (!pInfected)
            {
                printk("tarakernel SENDING: ***** ERROR ***** This connected unit (%pI4) is not registered as infected. Aborting\n", &nClientIp);
                return 1;
            }

            //struct _Remote_infection cThreat;
            //initThreatInfo(t->src.u3.ip, ct_sport, &cThreat);
            printk("tarakernel SENDING: ********** ERROR ****** Fix before sending back  ****DROPPING SENDING FOR NOW*****\n");
            
            sendUdpThreatPackage(iph->saddr, t->src.u3.ip, ct_sport, &pInfected->cTag);    //ØT 260323 - send this??

            return 1;
        }

        printk("tarakernel: conntrack lookup failed for %pI4:%u -> %pI4:%u proto=%u\n",
        &src_ip, ntohs(src_port), &dst_ip, ntohs(dst_port), proto);

    }
    return 0;
}


void checkThatTcp(struct _PacketInspection *pPacket, char *lpFromWhere)
{
    struct iphdr *iph = ip_hdr(pPacket->skb);

    if (iph->protocol != IPPROTO_TCP)
    {
        printk("tarakernel: ** ERROR ** Not TCP when (%s\n", lpFromWhere);
    }
}

unsigned int tagThePacket(struct _PacketInspection *pPacket, const struct nf_hook_state *state)
{
    checkThatTcp(pPacket,"start of tagThePacket");	//260320 - asdf... got problem with this....

	//Outbound traffic to partner and tagging is turned on.. Tag it.
    //struct _InfectionSpecification cThreatInfo;
    struct _Remote_infection cThreatInfo;

    initThreatInfo(pPacket->ip_header->saddr, pPacket->tcp_header->source, &cThreatInfo);
	
    #define DO_URG_PTR_TAGGING
    #ifdef DO_URG_PTR_TAGGING
    //***************Using urg_ptr **************************
	pPacket->tcp_header->urg_ptr= cThreatInfo.nTag;//(__be16)cTag;//htons(0xFF00);  //Tag the package.
	pSetup->cGlobalStatistics.nOutboundTagged++;
    #endif

    //***********************Using DSCP - 6-bit (max value: 63) part of TOS (Type of Service)
    //Normal used values for DSCP:
    
    //Value     Name            Use
    //0         Best effort     normal traffic
    //46        EF              VoIP
    //34        AF41            video
    //10        AF11            low priority

    //Suggested values used by us:
    //11    suspicious activity
    //12    confirmed suspicious
    //13    whitehat or penetration tester
    //14    known hacker
    //15    botnet
    //16    botnet command & control

    //#define DO_DSCP_TAGGING
    
    #ifdef DO_DSCP_TAGGING
    struct iphdr *iph = ip_hdr(pPacket->skb);   //NOTE! Not necessary... iph is already in pPacket scruct????

    uint8_t currentDscp = iph->tos >> 2;    //Or just getDscp(pPacket);

    if (currentDscp == 0) {             //It's suggested we only set the DSCP if it's not already sent to avoid messing with QoS services

        if (skb_try_make_writable(pPacket->skb, ip_hdrlen(pPacket->skb)))
        {
            printk("tarakernel: Can't make it writable for DSCP tagging so dropping packet.\n");
            return NF_DROP; 
        }

        uint8_t newDscp = 11;
        setDscp(iph, newDscp);
    }    
    else
        printk("tarakernel: DSCP field was already set. Dropping altering.\n");
    #endif

    checkThatTcp(pPacket,"in tagThePacket");	//260320 - asdf... got problem with this....

    recalcChecksum(pPacket);

    if (isNewConnection(pPacket->skb))
    {

        //cUdpTagString is the string to send as UDP packet to receiver if it's a new connection... 
        //Search "Tagging UDP msg coding/decoding" for usage in source (in tarakernel.c??)

        sendUdpThreatPackage(pPacket->ip_header->daddr, pPacket->ip_header->saddr, pPacket->tcp_header->source, &cThreatInfo.cTag);

        /*  Don't do this for now... Sending it directly for callback
        int nAvailable = -1;
        //Mark this as stolen so not getting handled again...
        for (int n = 0; n< N_MAX_STOLEN_PACKETS; n++)
            if (pSetup->pStolenPacket[n] == pPacket->skb)
            {
                printk("Already sent UDP to this.. Dropping it\n");
                return NF_ACCEPT;
            }
            else
                if (nAvailable == -1 && pSetup->pStolenPacket[n] == 0)
                    nAvailable = n;
                    
        if (nAvailable == -1)
        {
            //No available spots found
            printk("Too many packets stolen.. Dropping it..\n");
            return NF_ACCEPT;
        }
        //Note! Meaning only 10 messages will be sent... and probably the same one again and again....
        pSetup->pStolenPacket[nAvailable] = pPacket;
            */

        //sendUdpPacketToReceiver(pPacket);
        return queueRetransmit(pPacket->skb, state);
        //queue_resend_from_skb(pPacket->skb);       //Defined in module_stolen.c
    }
    /*else
        if (pPacket != pSetup->pStolenPacket[0])
        {
            printk("tarakernel SENDING: Not a new session, but seems not sent before... so sending UDP with threat info to receiver.");
            pSetup->pStolenPacket[0] = pPacket;
            //queue_udp_send_from_skb(pPacket->skb);       //Defined in module_stolen.c
            //queue_resend_from_skb(pPacket->skb);       //Defined in module_stolen.c
            return sendUdpPackageAndQueueRetransmit(pPacket->skb, state);
        }
    */

    //printk("tarakernel SENDING: Threat data for this session sent before... dropping sending.\n");
    return NF_ACCEPT;
}//tagThePacket()
