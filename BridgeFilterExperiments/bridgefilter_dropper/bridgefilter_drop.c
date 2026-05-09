#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_bridge.h>

#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/udp.h>

MODULE_LICENSE("GPL");

#define ALLOWED_OUTBOUND_UDP_PORT 8989

static unsigned int br_drop_hook_func_outb_udp_allow(
    unsigned int hooknum,
    struct sk_buff *skb,
    const struct net_device *in,
    const struct net_device *out,
    int (*okfn)(struct sk_buff *))
{
    struct ethhdr *eth;
    struct iphdr *iph;
    struct udphdr *udph;

    unsigned int need;
    unsigned int ip_hlen;

    /* Basic sanity */
    if (!skb)
        return NF_DROP;

    /*
     * skb_mac_header() handling was a little rough in older kernels,
     * so just use skb->mac.raw for 2.6.32-era compatibility.
     */
    eth = eth_hdr(skb);
    if (!eth)
        return NF_DROP;

    if (ntohs(eth->h_proto) == ETH_P_ARP)
        return NF_ACCEPT;

    /* Only care about IPv4 */
    if (ntohs(eth->h_proto) != ETH_P_IP) {
        printk("Not IP, dropping\n");
        goto drop;
    }

    /* Ensure IP header exists */
    if (!pskb_may_pull(skb, sizeof(struct ethhdr) + sizeof(struct iphdr))) {
        printk("!pskb_may_pull. Dropping\n");
        goto drop;
    }

    iph = ip_hdr(skb);
    if (!iph) {
        printk("!iph. Dropping\n");
        goto drop;
    }

    /* Only care about UDP */
    if (iph->protocol != IPPROTO_UDP) {
        printk("Not UDP, dropping\n");
        goto drop;
    }

    ip_hlen = iph->ihl * 4;

    if (ip_hlen < sizeof(struct iphdr)) {
        printk("ip_hlen < sizeof(struct iphdr). Dropping\n");
        goto drop;
    }

    need = ((unsigned char *)iph - skb->data) + ip_hlen + sizeof(struct udphdr);

    if (!pskb_may_pull(skb, need)) {
        printk("UDP header not pullable: need=%u len=%u data_len=%u headlen=%u\n",
            need, skb->len, skb->data_len, skb_headlen(skb));
        goto drop;
    }

    /* pskb_may_pull can reallocate/linearize, so reload pointers */
    iph = ip_hdr(skb);
    udph = (struct udphdr *)((unsigned char *)iph + (iph->ihl * 4));

    /* Allow matching UDP destination port */
    if (ntohs(udph->dest) == ALLOWED_OUTBOUND_UDP_PORT) {
        printk(KERN_INFO
               "br_drop_hook: allowing UDP dport %u\n",
               ALLOWED_OUTBOUND_UDP_PORT);

        return NF_ACCEPT;
    }
    printk("UDP dport did not match: %u\n", ntohs(udph->dest));

drop:
    printk(KERN_INFO
           "br_drop_hook_func_outb_udp_allow: dropping packet on hook %u\n",
           hooknum);

    return NF_DROP;
}

static unsigned int br_drop_hook_func(
    unsigned int hooknum,
    struct sk_buff *skb,
    const struct net_device *in,
    const struct net_device *out,
    int (*okfn)(struct sk_buff *))
{
    struct ethhdr *eth;

    /* Basic sanity */
    if (!skb)
        return NF_DROP;

    /*
     * skb_mac_header() handling was a little rough in older kernels,
     * so just use skb->mac.raw for 2.6.32-era compatibility.
     */
    eth = eth_hdr(skb);
    if (!eth)
        return NF_DROP;

    if (ntohs(eth->h_proto) == ETH_P_ARP)
        return NF_ACCEPT;

    printk(KERN_INFO "br_drop_hook: dropping packet on hook %u\n", hooknum);
    return NF_DROP;
}

static struct nf_hook_ops br_nf_drop_ops[] = {
    // {
    //     .hook     = br_drop_hook_func,
    //     .pf       = PF_BRIDGE,
    //     .hooknum  = NF_BR_PRE_ROUTING,
    //     .priority = NF_BR_PRI_FIRST,
    // },
    {
        .hook     = br_drop_hook_func,
        .pf       = PF_BRIDGE,
        .hooknum  = NF_BR_FORWARD,
        .priority = NF_BR_PRI_FIRST,
    },
    {
        .hook     = br_drop_hook_func,
        .pf       = PF_BRIDGE,
        .hooknum  = NF_BR_LOCAL_IN,
        .priority = NF_BR_PRI_FIRST,
    },
    {
        .hook     = br_drop_hook_func_outb_udp_allow,
        .pf       = PF_BRIDGE,
        .hooknum  = NF_BR_LOCAL_OUT,
        .priority = NF_BR_PRI_FIRST,
    },
    {
        .hook     = br_drop_hook_func_outb_udp_allow,
        .pf       = PF_BRIDGE,
        .hooknum  = NF_BR_POST_ROUTING,
        .priority = NF_BR_PRI_FIRST,
    },
};

static int __init br_drop_init(void)
{
    int i, ret;

    for (i = 0; i < ARRAY_SIZE(br_nf_drop_ops); i++) {
        ret = nf_register_hook(&br_nf_drop_ops[i]);
        if (ret) {
            while (--i >= 0)
                nf_unregister_hook(&br_nf_drop_ops[i]);
            return ret;
        }
    }

    printk(KERN_INFO "br_drop_hook: registered\n");
    return 0;
}

static void __exit br_drop_exit(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(br_nf_drop_ops); i++)
        nf_unregister_hook(&br_nf_drop_ops[i]);

    printk(KERN_INFO "br_drop_hook: unregistered\n");
}

module_init(br_drop_init);
module_exit(br_drop_exit);