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

int isNewConnection(struct sk_buff *skb)
{
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
    return 0;
}



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
            break;              /* End of options */

        if (kind == 1) {        /* NOP */
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

    /* Re-fetch pointers after skb_ensure_writable */
    iph = ip_hdr(skb);
    tcph = tcp_hdr(skb);

    optlen = tcph_len - sizeof(struct tcphdr);
    if (optlen <= 0)
        return 0;

    optptr = (unsigned char *)(tcph + 1);

    for (i = 0; i < optlen; ) {
        u8 kind = optptr[i];

        if (kind == 0) {
            break; /* End of options */
        } else if (kind == 1) {
            i++;
            continue; /* NOP */
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

void tagThePacket(struct _PacketInspection *pPacket)
{
	//Outbound traffic to partner and tagging is turned on.. Tag it.

    #define DO_URG_PTR_TAGGING

    #ifdef DO_URG_PTR_TAGGING
    //***************Using urg_ptr **************************
	union _TagUnion cUnion;
	cUnion.cTag.version_no = TAG_VERSION_NO;
	cUnion.cTag.presumed_infected = 5; //Presumably bot. TO DO: Diversify this....
	cUnion.cTag.botnet_id = 99; //To be assigned by Taransvar.. To be implemented later...
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

    #define DO_DSCP_TAGGING
    
    #ifdef DO_DSCP_TAGGING
    struct iphdr *iph = ip_hdr(pPacket->skb);   //NOTE! Not necessary... iph is already in pPacket scruct????

    uint8_t currentDscp = iph->tos >> 2;    //Or just getDscp(pPacket);

    if (currentDscp == 0) {             //It's suggested we only set the DSCP if it's not already sent to avoid messing with QoS services

        if (skb_try_make_writable(pPacket->skb, ip_hdrlen(pPacket->skb)))
        {
            printk("tarakernel: Can't make it writable for DSCP tagging so dropping packet.\n");
            return;// NF_DROP; it's void function..
        }

        uint8_t newDscp = 11;
        setDscp(iph, newDscp);
    }    
    else
        printk("tarakernel: DSCP field was already set. Dropping altering.\n");
    #endif

    #define CALC_CHECKSUM 
    #ifdef CALC_CHECKSUM
    recalcChecksum(pPacket)
    #endif
}
