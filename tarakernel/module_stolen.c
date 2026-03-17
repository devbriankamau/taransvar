
//module_stolen.c

/*#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/string.h>
#include <net/sock.h>
*/

static int send_udp_json_to_skb_dest(struct sk_buff *skb, const char *json)
{
  struct iphdr *iph;
  struct udphdr *udph;
  struct socket *sock = NULL;
    struct sockaddr_in addr = {0};
    struct msghdr msg = {0};
    struct kvec iov;
    int ret;

    if (!skb || !json)
        return -EINVAL;

    if (!pskb_may_pull(skb, sizeof(struct iphdr)))
        return -EINVAL;

    iph = ip_hdr(skb);
    if (!iph || iph->version != 4 || iph->protocol != IPPROTO_UDP)
        return -EINVAL;

    if (!pskb_may_pull(skb, ip_hdrlen(skb) + sizeof(struct udphdr)))
        return -EINVAL;

    udph = udp_hdr(skb);
    if (!udph)
        return -EINVAL;

    ret = sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
    if (ret < 0) {
        //pr_err("sock_create_kern failed: %d\n", ret);
        printk("tarakernel: sock_create_kern failed: %d\n", ret);
        return ret;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = iph->daddr;   /* skb destination IP */
    addr.sin_port = TARALINK_LISTENING_TO_PORT;//udph->dest;          /* skb destination UDP port */

    msg.msg_name = &addr;
    msg.msg_namelen = sizeof(addr);

    iov.iov_base = (char *)json;
    iov.iov_len  = strlen(json);

    ret = kernel_sendmsg(sock, &msg, &iov, 1, iov.iov_len);
    if (ret < 0)
        //pr_err("kernel_sendmsg failed: %d\n", ret);
        printk("kernel_sendmsg failed: %d\n", ret);
    else
        //pr_info("Sent %d bytes JSON to %pI4:%u\n", ret, &iph->daddr, ntohs(addr.sin_port));//udph->dest));
        printk("Sent %d bytes JSON to %pI4:%u\n", ret, &iph->daddr, ntohs(addr.sin_port));//udph->dest))

    sock_release(sock);
    return ret;
}

/*unsigned int hookfnHandleStolenPacket(...) 
{

  char json[256];

  snprintf(json, sizeof(json),
         "{\"src_ip\":\"%pI4\",\"src_port\":%u,"
         "\"dst_ip\":\"%pI4\",\"dst_port\":%u,"
         "\"event\":\"new-session\"}",
         &ip_hdr(skb)->saddr, ntohs(udp_hdr(skb)->source),
         &ip_hdr(skb)->daddr, ntohs(udp_hdr(skb)->dest));

  send_udp_json_to_skb_dest(skb, json);

    struct _PacketInspection *pPacket = pSetup->pStolenPacket[0];
    dev_queue_xmit(pSetup->skb);
}*/


void saveStolenPacket(struct _PacketInspection *pPacket)
{

    /*
PRE_ROUTING
  -> clone/copy if needed
  -> return NF_STOLEN
your code later:
  -> dev_queue_xmit() / ip_local_out() / nf_reinject() equivalent path if you built one
  OR
  -> kfree_skb()
*/

    //pSetup->pStolenPacket[0] = pPacket;    //Only handle one for now...


    //struct sk_buff *clone = skb_clone(skb, GFP_ATOMIC);
    //if (!clone) {
    //    kfree_skb(skb);
    //    return NF_STOLEN;
    //}

    //queue_my_clone(clone);

    //kfree_skb(skb);
    //return NF_STOLEN;




	//sendUdfPackage(pPacket);

}




#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <net/sock.h>

struct udp_send_job {
    struct work_struct work;
    __be32 daddr;
    __be16 dport;
    char json[256];
};

static int send_udp_json(__be32 daddr, __be16 dport, const char *json)
{
    struct socket *sock = NULL;
    struct sockaddr_in addr = {0};
    struct msghdr msg = {0};
    struct kvec iov;
    int ret;

    ret = sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
    if (ret < 0) {
        //pr_err("sock_create_kern failed: %d\n", ret);
        printk("tarakernel SENDING: sock_create_kern failed: %d\n", ret);
        return ret;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = daddr;
    addr.sin_port = dport;

    msg.msg_name = &addr;
    msg.msg_namelen = sizeof(addr);

    iov.iov_base = (char *)json;
    iov.iov_len = strlen(json);

    ret = kernel_sendmsg(sock, &msg, &iov, 1, iov.iov_len);
    if (ret < 0)
        //pr_err("kernel_sendmsg failed: %d\n", ret);
        printk("tarakernel SENDING: kernel_sendmsg failed: %d\n", ret);
    else
        //pr_info("Sent %d bytes to %pI4:%u\n", ret, &daddr, ntohs(dport));
        printk("tarakernel SENDING: Sent %d bytes to %pI4:%u\n", ret, &daddr, ntohs(dport));

    sock_release(sock);
    return ret;
}

static void udp_send_work_cb(struct work_struct *work)
{
  //This is the callback function...
  printk("tarakernel SENDING: Called back.. now seding the UDP.\n");
    struct udp_send_job *job = container_of(work, struct udp_send_job, work);

    send_udp_json(job->daddr, job->dport, job->json);

    kfree(job);
}


static void queue_udp_send_from_skb(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct udphdr *udph;
    struct udp_send_job *job;

    if (!skb)
        return;

    if (!pskb_may_pull(skb, sizeof(struct iphdr)))
        return;

    iph = ip_hdr(skb);
    if (!iph || iph->protocol != IPPROTO_UDP)
        return;

    if (!pskb_may_pull(skb, ip_hdrlen(skb) + sizeof(struct udphdr)))
        return;

    udph = udp_hdr(skb);
    if (!udph)
        return;

    job = kzalloc(sizeof(*job), GFP_ATOMIC);
    if (!job)
        return;

    INIT_WORK(&job->work, udp_send_work_cb);

    job->daddr = iph->daddr;
    job->dport = udph->dest;

    snprintf(job->json, sizeof(job->json),
             "{\"event\":\"new-session\",\"dst_port\":%u}",
             ntohs(udph->dest));

    schedule_work(&job->work);
    printk("tarakernel SENDING: Sending UDP is scheduled!\n");
}
