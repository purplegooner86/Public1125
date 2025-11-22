/* 
 * LAB: Interception / kretprobe hooking / Neuter file permission checks
*/

#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/module.h>
#include <linux/kprobes.h>

int init_function(void);
void exit_function (void);
module_init (init_function);
module_exit (exit_function);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Ninjas LLC");
MODULE_DESCRIPTION("Neuter file permission checks");

static int hook_return_handler(struct kretprobe_instance *ri, struct pt_regs *regs);

// Step #1: 
// Initialize the kretprobe structure fields to probe the kernel function "generic_permission"
// Register hook_return_handler() as the return handler 
// No need for per return-instance private data, so set the data_size appropriately
// Number of active kretprobe_instances can be set to 20 
static struct kretprobe hook_probe = {
    .kp.symbol_name     = "generic_permission",
    .handler            = hook_return_handler,
    .data_size          = 0, // kretprobe_instance->data would be NULL
    .maxactive          = 20,
};

// generic_permission() checks for access rights on a filesystem 
// int generic_permission( struct user_namespace *mnt_userns, struct inode *inode, int mask)
// returns 0 on success
// hook_return_handler() is called when a call to generic_permission() returns
static int hook_return_handler(struct kretprobe_instance *ri, struct pt_regs *regs) {
    // Step #2:
    // Display the probed return address, RIP, RSP and Flags registers
    // but only if the call to generic_permission() returned failure
    // and then force the function to return success (0)
    if ( regs->ax != 0 ) {
        pr_info( "ret_addr=%px(%pS) rip=0x%016lx rsp=0x%016lx flags=0x%016lx rax=0x%016lx\n", 
            ri->ret_addr, ri->ret_addr, 
            regs->ip, regs->sp, regs->flags, regs->ax);
        regs->ax = 0;
    }

    return 0;
} // hook_return_handler()

int register_probe(void) {
    int ret;

    // Step #3:
    // Register the kretprobe in the structure hook_probe
    ret = register_kretprobe(&hook_probe);
    if (ret < 0) {
        pr_info("register_kretprobe() ERROR=%d\n", ret);
        return ret;
    }

    return 0;
} // register_probe()

void unregister_probe(void) {

    // Step #4:
    // Unregister the kretprobe in the structure hook_probe
    unregister_kretprobe(&hook_probe);

} // unregister_probe()

int init_function(void) {

    pr_info("%s Built on %s %s\n", __func__, __DATE__, __TIME__ );

    register_probe();

    return 0;
} // init_function()

void exit_function (void) {

    unregister_probe();

    pr_info("%s\n", __func__);
} // exit_function()

