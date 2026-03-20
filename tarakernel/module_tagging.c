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
        sprintf(cBuf, "%s-%pI4:%d", UDP_THREAT_INFO_REQUEST_PREFIX, &iph->saddr, ntohs(TARAKERNEL_LISTENING_TO_PORT));
        send_udp_json(iph->saddr, htons(TARAKERNEL_LISTENING_TO_PORT), cBuf);
        printk("tarakernel SENDING: *** Requesting threat info from sender: %s\n", cBuf);
    }
}

int isRequestForThreatElaboration(char *lpPayload)
{
    /* Now you can read payload[0..payload_len-1] */
    printk("tarakernel: ******* NOT HANDLED! ***** Probably request for elaborated threat info????: %s\n", lpPayload);

    return 1;
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

	union _TagUnion cUnion;
	cUnion.cTag.version_no = TAG_VERSION_NO;
	cUnion.cTag.presumed_infected = 5; //Presumably bot. TO DO: Diversify this....
	cUnion.cTag.botnet_id = 99; //To be assigned by Taransvar.. To be implemented later...

    #define DO_URG_PTR_TAGGING
    #ifdef DO_URG_PTR_TAGGING
    //***************Using urg_ptr **************************
	pPacket->tcp_header->urg_ptr= cUnion.nBe16;//(__be16)cTag;//htons(0xFF00);  //Tag the package.
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
        char cUdpTagString[100];

        //cUdpTagString is the string to send as UDP packet to receiver if it's a new connection... 
        //Search "Tagging UDP msg coding/decoding" for usage in source (in tarakernel.c??)
        sprintf(cUdpTagString, "%d:%d^%d^%d^%d", pPacket->ip_header->saddr, pPacket->tcp_header->source , cUnion.cTag.version_no, cUnion.cTag.presumed_infected, cUnion.cTag.botnet_id);
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
        printk("tarakernel SENDING: New session. Sending UDP with threat info to receiver (%pI4:%d): %s.\n", &pPacket->ip_header->daddr, ntohs(TARALINK_LISTENING_TO_PORT), cUdpTagString);
        return sendUdpPackageAndQueueRetransmit(pPacket->skb, state, cUdpTagString);
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
