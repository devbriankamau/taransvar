//Kernel 5.4.0 version originally taken from https://gist.github.com/arunk-s/c897bb9d75a6c98733d6

//(Originally taken from) https://stackoverflow.com/questions/15215865/netlink-sockets-in-c-using-the-3-x-linux-kernel?lq=1

//10 is full system, 1 is just running the core system without and hokks or messages sent...
#define SYSTEM_OPERATION_LEVEL 10

#define C_VERSION 16
#define DEBUG_VERSION C_VERSION

/*There is some unsolved problem related to C_SEGMENT_MAX_SIZE. If we use 4000, the system can only store around 10 addresses to block, 
partners or whatever before freezing. We should figure out why and find the optimal value before needing the big numbers.*/
//#define C_SEGMENT_MAX_SIZE 4000
//#define C_SEGMENT_MAX_SIZE 50 Test with this....
#define C_SEGMENT_MAX_SIZE 1000

#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

/*Netfilter includes*/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_ecache.h>

#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/skbuff.h>



#include "module_globals.h"
#include "tarakernel.h"
//#define CONFIG_MODULE_SIG 0

#define KSOCKET_NAME	"tarakernel"
#define KSOCKET_VERSION	"0.0.1"
#define KSOCKET_DESCPT	"Taransvar Security Solution"
#define KSOCKET_AUTHOR	"http://taransvar.no"
#define KSOCKET_DATE	"2025-01-13"

MODULE_AUTHOR(KSOCKET_AUTHOR);
MODULE_DESCRIPTION(KSOCKET_NAME"-"KSOCKET_VERSION"\n"KSOCKET_DESCPT);
MODULE_LICENSE("Dual BSD/GPL");

//Notes... sturcts are mostly defined in taralink.h
static unsigned int bReceivedConfiguration = 0;

void warn(char *lpMsg);
int isprintable(char ch);
//int packetInterpreter(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
void initPacket(struct _PacketInspection *pPacket, struct sk_buff *skb, const struct nf_hook_state *state, bool bSetupOwned);
struct _PacketInspection *getPacketInfo(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
void checkFree(struct _PacketInspection *pPacket, bool bLeavingPostRouting);
int isRequestForThreatElaboration(char *lpPayload);
void initElaboratedThreatInfo(struct _PacketInspection *pPacket);

void getTcpPayload(struct sk_buff *skb, char *lpBuffer, u32 nBufSize);
void sendMessage(int pid, char *msg);
void sendMessage2(int pid, char *lpReply);
void checkRequestForStatus(int pid, char *lpPayload);  //module_status.c
void test_send_message(int pid, char *lpMsg);
void checkTimedOperation(void);
int inspectThis(u32 sourceIp);
char *interpretSetup(char *lpBlockDescriptor, char *lpIpList);
char *interpretInspection(char *lpBlockDescriptor, char *lpIpList);
void listAssistRequests(void);
int requestedAssistance(unsigned int ipAddress, unsigned int nPort);
int isInfected(volatile uint32_t ipAddress);
void removeInfection(volatile uint32_t ipAddress, volatile uint32_t ipNettmask, short port);
bool trafficReportToTaralinkFound(int nProcessId);
void sendCheckRequests(int nProcessId);
void checkPartner(u32 nIp);
unsigned int tagThePacket(struct _PacketInspection *pPacket, const struct nf_hook_state *state);	//module_tagging.c
int isNewConnection(struct sk_buff *skb);
void saveStolenPackage(struct _PacketInspection *pPacket);
uint8_t getDscp(struct _PacketInspection *pPacket);
void setDscp(struct iphdr *iph, uint8_t newDscp);
void recalcChecksum(struct _PacketInspection *pPacket);
static unsigned int sendUdpPackageAndQueueRetransmit(struct sk_buff *skb, const struct nf_hook_state *state, char *lpString);
static int send_udp_json(__be32 daddr, __be16 dport, const char *json);

void checkThatTcp(struct _PacketInspection *pPacket, char *lpFromWhere);	//260320 - asdf... got problem with this....

//static int tcp_read_timestamp_option(struct sk_buff *skb, __be32 *tsval_be, __be32 *tsecr_be);
//static int tcp_set_timestamp_option(struct sk_buff *skb, bool set_tsval, __be32 new_tsval_be, bool set_tsecr, __be32 new_tsecr_be);

#define IPADDRESS(addr) \
	((unsigned char *)&addr)[3], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[0]

static char *cBlockDescriptor[] = {"SERVERS","INFECTIONS","WHITE_LIST","BLACK_LIST","PARTNERS","INSPECT","HONEYPORT","ASSIST","DROP"};

static struct _Setup *pSetup;

#include "module_configuration.h"
#include "module_store_configuration.h"
#include "module_packet_interpreter.h"

#include "module_TCP_functions.c"

#include "module_pre_routing_handler.c"
#include "module_status.c"
#include "module_timed_operations.c"
#include "module_packet_interpreter.c"
#include "module_store_configuration.c"

#include "module_forwarding.c"
#include "module_configuration.c"
#include "module_pointer_list.c"
#include "module_tagging.c"
#include "module_stolen.c"

#define NETLINK_USER 31

void warn(char *lpMsg){
	char* lpBuff = NULL;
	lpBuff = kmalloc(255 * sizeof(char), GFP_KERNEL);
	sprintf(lpBuff, "tarakernel (ver %d): %s", DEBUG_VERSION, lpMsg);
	printk(KERN_DEBUG "%s", lpBuff); 
	kfree(lpBuff);
}

void sendMessage(int pid, char *msg)
{
	struct nlmsghdr *nlh;
	int msg_size;
	int res;
	struct sk_buff *skb_out;
	msg_size=strlen(msg);
	skb_out = nlmsg_new(msg_size,0);
	if(!skb_out)
	{
		printk(KERN_ERR "tarakernel: **** ERROR **** Failed to allocate new skb\n");
		return;
  	} 
	nlh=nlmsg_put(skb_out,0,0,NLMSG_DONE,msg_size,0);  
	NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
  	strncpy(nlmsg_data(nlh),msg,msg_size);

	res=nlmsg_unicast(pSetup->nl_sk,skb_out,pid);

	if(res<0)
		printk(KERN_INFO "tarakernel: **** Error **** while sending to taralink\n");
	else
	{
		if (pSetup->cShowInstructions.bits.showOther)
			printk(KERN_INFO "tarakernel: Sent to taralink: %s\n", msg);
	}
}

void sendTestMessage(int nProcessId);
void sendTestMessage(int nProcessId)
{
}

int udpMsgFromSender(char *lpPayload);
int udpMsgFromSender(char *lpPayload)
{
	if (strstr(lpPayload, UDP_MSG_PREFIX)== lpPayload)
	{
		printk("tarakernel SENDING: ***** RECEIVED UDP message from sender via taralink: %s\n", lpPayload);

        //Search "Tagging UDP msg coding/decoding" for usage in source	(in module_tagging.c)
        //Coded by: sprintf(cUdpTagString, "%d:%d^%d^%d^%d", pPacket->ip_header->saddr, pPacket->tcp_header->source , cUnion.cTag.version_no, cUnion.cTag.presumed_infected, cUnion.cTag.botnet_id);
		unsigned int sIp, sPort, dVersion, dInfected, dBotnet;
		int nFlds, nAvailable;
		nAvailable = -1; 	//To tack if changed by findRemoteInfectionInfoReceived()

		if ((nFlds = sscanf(lpPayload, "%d:%d^%d^%d^%d", &sIp, &sPort, &dVersion, &dInfected, &dBotnet)) == 5) 
		{
			printk("tarakernel SENDING: ******* UDP message content (via taralink) successfully decoded (****and should be saved***): %d:%d^%d^%d^%d\n", sIp, sPort, dVersion, dInfected, dBotnet);
			struct _Remote_infection *pAlreadyHave = findRemoteInfectionInfoReceived(sIp, sPort, &nAvailable);

			if (pAlreadyHave)
				printk("tarakernel SENDING: Already have info for %d:%d - ignoring\n", sIp, sPort);
			else
			{
				//Store the new info if available slot...
				if (nAvailable >= 0)
				{
					//findRemoteInfectionInfoReceived() found an available slot
					pAlreadyHave = pSetup->cRemoteInfectionInfoReceived[nAvailable] = kmalloc(sizeof(struct _Remote_infection), GFP_KERNEL);
				}
				else
				{
					printk("tarakernel: ***** ERROR **** No available threat info slots.. Should deleted the oldest.\n");
				}
			}


		}
		else
			printk("tarakernel SENDING: ****** ERROR ***** UDP message decoding failed. Found %d fields. Should have been 5\n", nFlds);

		return 1;
	}

	//printk("tarakernel SENDING: Not UDP message: %s\n", lpPayload);
	return 0;
}

static void hello_nl_recv_msg(struct sk_buff *skb)
{
        //The idea here is that taralink manages the traffic by directing tarakernel on what to do. (lpPayload will hold the instructions)
	struct nlmsghdr *nlhead;
	char cReply[100], *lpPayload, *lpConfigPrefix;
		
	nlhead = (struct nlmsghdr*)skb->data;    //nlhead message comes from skb's data... (sk_buff: unsigned char *data)
	lpPayload = (char*)nlmsg_data(nlhead);    

	//if (!bReceivedConfiguration || pSetup->cShowInstructions.bits.showOther)
		printk(KERN_INFO "tarakernel: Received: %s\n",lpPayload);

	//Check if it's config info (starts with CONFIG 0..n)
	lpConfigPrefix = strstr(lpPayload, "CONFIG ");
    
	if (lpConfigPrefix && (lpConfigPrefix == lpPayload))
	{
		module_storeConfiguration(lpPayload + strlen("CONFIG ")); //Check if this is the configuration....
		return;
	}

	//Now send reply...

	if (!bReceivedConfiguration)
		strcpy(cReply, "configuration 0?"); //Configuration not yet received.. Request it..
	else
	{
	    sendCheckRequests(nlhead->nlmsg_pid);    //Defined in module_timed_operations.c 
	    debugRoutine(); //Defined in module_timed_operations.c

		if (udpMsgFromSender(lpPayload))
			return;
		else
		    //*************** NOTE! Make changes here... probably never sends status as long as there's traffic....
			if (trafficReportToTaralinkFound(nlhead->nlmsg_pid)) //Defined in module_timed_operations.c
			{
			        //Also logged by trafficReportToTaralinkFound()
				//if (pSetup->cShowInstructions.bits.showOther)
				//	printk(KERN_INFO "tarakernel: Traffic report sent to taralink\n");
					
				return;
			}
			else
				if (!strcmp(lpPayload, "request_tarakernel_status"))
			{
			        //sendTestMessage(nlhead->nlmsg_pid); //Or do other debug stuff...
				checkRequestForStatus(nlhead->nlmsg_pid, lpPayload);   //Defined in module_status.c
				return;
			}
			else
    			{
        			strcpy(cReply, "Hello msg from kernel");
			}
        }
	sendMessage(nlhead->nlmsg_pid, cReply);
}

static int __init hello_init(void) 
{
    //doPointerTest();  //See module_pointer_list.c
    //return 0;

    printk("tarakernel: Started (version %d). Start taralink to send configuration.\n", C_VERSION);
	
	if (!configuraton_init())		//defined in module_configuration.c;
		printk("tarakernel: ****** ERROR ***** configuraton_init() returned false\n");
		//return -1;
        
        //printk("tarakernel: Finished testing... Going idle.\n");
        //return 0;

	if (SYSTEM_OPERATION_LEVEL > 1) //Without this there's nothing
	{
		struct netlink_kernel_cfg cfg = {
    			.input = hello_nl_recv_msg,
		};

        	//Create socket and register call back funciton (hello_nl_recv_msg) for receiving messages from user space program (taralink)
		pSetup->nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);

		if(!pSetup->nl_sk)
		{
			printk(KERN_ALERT "tarakernel: ************* Error creating socket.\n");
			return -10;
		}
	}

	//******* Register PRE ROUTING hook ****************
	if (SYSTEM_OPERATION_LEVEL > 5) //Not really needed
	{
		pSetup->nf_PRE_ROUTING_hook_ops = (struct nf_hook_ops*)kcalloc(1, sizeof(struct nf_hook_ops), GFP_KERNEL);
		if (pSetup->nf_PRE_ROUTING_hook_ops != NULL) 
		{
			pSetup->nf_PRE_ROUTING_hook_ops->hook = (nf_hookfn*)module_ip4_pre_routing_handler;//nf_blockipaddr_handler;
			pSetup->nf_PRE_ROUTING_hook_ops->hooknum = NF_INET_PRE_ROUTING;
			pSetup->nf_PRE_ROUTING_hook_ops->pf = NFPROTO_IPV4;
			pSetup->nf_PRE_ROUTING_hook_ops->priority = NF_IP_PRI_CONNTRACK + 1; //NF_IP_PRI_FIRST;	NOTE! Start it after CONNTRACK is attached to the queue

			nf_register_net_hook(&init_net, pSetup->nf_PRE_ROUTING_hook_ops);
		}
	}

	//******* Register POST ROUTING hook ****************
	if (SYSTEM_OPERATION_LEVEL > 5) //Not really needed
	{
		pSetup->nf_POST_ROUTING_hook_ops = (struct nf_hook_ops*)kcalloc(1, sizeof(struct nf_hook_ops), GFP_KERNEL);
		if (pSetup->nf_POST_ROUTING_hook_ops != NULL) 
		{
			pSetup->nf_POST_ROUTING_hook_ops->hook = (nf_hookfn*)module_ip4_post_routing_handler;//nf_blockipaddr_handler;
			pSetup->nf_POST_ROUTING_hook_ops->hooknum = NF_INET_POST_ROUTING;
			pSetup->nf_POST_ROUTING_hook_ops->pf = NFPROTO_IPV4;
			pSetup->nf_POST_ROUTING_hook_ops->priority = NF_IP_PRI_FIRST;

			nf_register_net_hook(&init_net, pSetup->nf_POST_ROUTING_hook_ops);
		}
	}

	//******* Register FORWARDING hook ****************
	
	if (SYSTEM_OPERATION_LEVEL > 2) //This is a main function
	{
		pSetup->nf_FORWARDING_hook_ops = (struct nf_hook_ops*)kcalloc(1, sizeof(struct nf_hook_ops), GFP_KERNEL);
		if (pSetup->nf_FORWARDING_hook_ops != NULL) 
		{
			pSetup->nf_FORWARDING_hook_ops->hook = (nf_hookfn*)module_forwarding_handler;//nf_blockipaddr_handler;
			pSetup->nf_FORWARDING_hook_ops->hooknum = NF_INET_FORWARD;
			pSetup->nf_FORWARDING_hook_ops->pf = NFPROTO_IPV4;
			pSetup->nf_FORWARDING_hook_ops->priority = NF_IP_PRI_FIRST + 1;

			nf_register_net_hook(&init_net, pSetup->nf_FORWARDING_hook_ops);
		}
	}
	warn("tarakernel: Netfilter hooks registered\n");
  
	return 0;
}

static void __exit hello_exit(void) 
{
    if (pSetup->nf_PRE_ROUTING_hook_ops != NULL) {
		nf_unregister_net_hook(&init_net, pSetup->nf_PRE_ROUTING_hook_ops);
		kfree(pSetup->nf_PRE_ROUTING_hook_ops);
	}
    if (pSetup->nf_POST_ROUTING_hook_ops != NULL) {
		nf_unregister_net_hook(&init_net, pSetup->nf_POST_ROUTING_hook_ops);
		kfree(pSetup->nf_POST_ROUTING_hook_ops);
	}
	if (pSetup->nf_FORWARDING_hook_ops != NULL) {
		nf_unregister_net_hook(&init_net, pSetup->nf_FORWARDING_hook_ops);
		kfree(pSetup->nf_FORWARDING_hook_ops);
	}

    printk(KERN_INFO "tarakernel: Tarakernel stopped.\n");
	netlink_kernel_release(pSetup->nl_sk);
}

module_init(hello_init); 
module_exit(hello_exit);

MODULE_LICENSE("GPL");
