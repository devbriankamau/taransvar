//module_stolen.c

unsigned int hookfnHandleStolenPacket(...) 
{
    struct _PacketInspection *pPacket = pSetup->pStolenPackage[0];
    dev_queue_xmit(pSetup->skb);
}


void saveStolenPackage(struct _PacketInspection *pPacket)
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

    pSetup->pStolenPackage[0] = pPacket;    //Only handle one for now...


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

