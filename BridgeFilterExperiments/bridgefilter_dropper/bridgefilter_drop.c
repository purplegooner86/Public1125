#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_bridge.h>

MODULE_LICENSE("GPL");

static unsigned int br_drop_hook_func(
    unsigned int hooknum,
    struct sk_buff *skb,
    const struct net_device *in,
    const struct net_device *out,
    int (*okfn)(struct sk_buff *))
{
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
        .hook     = br_drop_hook_func,
        .pf       = PF_BRIDGE,
        .hooknum  = NF_BR_LOCAL_OUT,
        .priority = NF_BR_PRI_FIRST,
    },
    {
        .hook     = br_drop_hook_func,
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