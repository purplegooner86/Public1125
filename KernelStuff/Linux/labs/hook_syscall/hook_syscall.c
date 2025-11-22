/* 
 * LAB: Interception / SYSCALL hooking / Display SYSCALL parameters
*/

#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/module.h>
#include <asm/uaccess.h> // strncpy_from_user()
#include <asm/syscall.h> // sys_call_ptr_t()
#include <linux/slab.h> // kmalloc(), kfree()
#include <linux/kprobes.h> // register_kprobe(), unregister_kprobe()

int init_function(void);
void exit_function (void);
module_init (init_function);
module_exit (exit_function);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Ninjas LLC");
MODULE_DESCRIPTION("Display SYSCALL parameters");

// data structures
struct hook_syscall_context {
    sys_call_ptr_t* sys_call_table;
    sys_call_ptr_t original_function;
    sys_call_ptr_t hooked_function;
};

// forward declarations
void memcpy_bwp (void* dst, void* src, size_t size);
void* get_symbol_address(const char *name);
bool is_file_excluded (char* filename);
bool hook_syscall_init  (struct hook_syscall_context *context);
bool hook_syscall_exit (struct hook_syscall_context *context);
long hook_syscall_sys_openat(const struct pt_regs *regs); // sys_call_ptr_t()

// System call number for openat() SYSCALL
#define SYS_CALL_NR __NR_openat

// globals 

// Filenames that will not be displayed.
// These files are accessed too frequently and flood the debug log.
char* excluded_filenames[] = {
    "/run/log/journal/",
    "/proc/meminfo",
    "/sys/fs/cgroup/"
};

struct hook_syscall_context g_hook_syscall_context = {0};

bool hook_syscall_init  (struct hook_syscall_context *context) {

    // Grab the pointer to the original openat() function from
    // the appropriate slot in the sys_call_table.
    context->original_function = context->sys_call_table[SYS_CALL_NR];

    // Store the SYSCALL hook function in the context structure.
    context->hooked_function = hook_syscall_sys_openat;

    pr_info("%s: original_sysopen=%px(%pS)\n", __func__, 
        context->sys_call_table[SYS_CALL_NR], 
        context->sys_call_table[SYS_CALL_NR]
    );

    // Step #1:
    // Write the pointer to the hook function (context->hooked_function)
    // to the appropriate slot in the sys_call_table
    memcpy_bwp ( 
        &context->sys_call_table[SYS_CALL_NR], 
        &(context->hooked_function), 
        sizeof(void*)
    );

    pr_info("%s: hooked_sysopenat=%px(%pS)\n", __func__, 
        context->sys_call_table[SYS_CALL_NR],
        context->sys_call_table[SYS_CALL_NR]
    );

    return true;
} // hook_syscall_init()

bool hook_syscall_exit (struct hook_syscall_context *context) {

    pr_info("%s: hooked_sysopenat=%px(%pS)\n", __func__, 
        context->sys_call_table[SYS_CALL_NR], 
        context->sys_call_table[SYS_CALL_NR]
    );

    // Step #2:
    // Restore back the pointer to the original function (context->original_function)
    // to the appropriate slot in the sys_call_table
    memcpy_bwp ( 
        &context->sys_call_table[SYS_CALL_NR], 
        &(context->original_function), 
        sizeof(void*)
    );

    pr_info("%s: original_sysopen=%px(%pS)\n", __func__, 
        context->sys_call_table[SYS_CALL_NR], 
        context->sys_call_table[SYS_CALL_NR]
    );

    return true;
} // hook_syscall_exit()

// SYSCALL parameters to openat()
// long sys_openat(
//     int dfd, // edi
//     const char __user *filename, //rsi
//     int flags, // rdx
//     umode_t mode); // r10d
// The dfd parameter can contain a special value (AT_FDCWD -100)
// which indicates openat() should use the current working directory.
long hook_syscall_sys_openat(const struct pt_regs *regs) {
    struct hook_syscall_context *context = &g_hook_syscall_context;
    char *kernel_filename = NULL;
    long size_filename = 0;
    long syscall_retval;
    // parameters to SYSCALL openat()
    int dfd;
    char __user *user_filename; 
    int flags;
    umode_t mode;

    // Step #3:
    // Read the SYSCALL parameters from the relevant registers (regs) 
    // and initialize the variables - dfd, user_filename, flags, mode
    dfd = (int)regs->di; 
    user_filename = (char *)regs->si; 
    flags = (int)regs->dx;
    mode = (umode_t)regs->r10;

    // Step #4:
    // Get the length of the user_filename string
    // Use PATH_MAX as the maximum strings size
    size_filename = strnlen_user(user_filename, PATH_MAX);
    if ( size_filename == 0) {
        pr_info("%s: strnlen_user(%px) FAIL\n", __func__,  user_filename);
        goto exit;
    }

    // Step #5:
    // Allocate kernel memory of size PATH_MAX to read the filename
    // from user mode and store the filename in kernel_name
    kernel_filename = (char*)kmalloc(size_filename, GFP_KERNEL);
    if (!kernel_filename) {
        pr_info("%s: kmalloc(%lu) FAIL\n", __func__,  size_filename);
        goto exit;
    }

    // Step #6:
    // Copy the filename from the user mode buffer (user_filename) 
    // to the kernel mode buffer in kernel_filename
    // size_filename is the length of the file name
    if ( strncpy_from_user(kernel_filename, user_filename, size_filename) <= 0 ) {
        pr_info("%s: strncpy_from_user(%px, %px) FAIL\n", __func__, kernel_filename, user_filename);
        goto exit;
    }

    // Exclude a list of files
    // Without this check the debugger log will get flooded.
    if ( is_file_excluded(kernel_filename) ) {
        goto exit;
    }

    // Display the parameters to the SYSCALL openat()
    pr_info("%s: dfd=%u flags=%08x mode=%08x filename=%s\n", __func__, dfd, flags, mode, kernel_filename );

exit :
    // Step #7:
    // Free the kernel buffer in kernel_filename
    // if the allocation was successful
    if (kernel_filename) {
        kfree(kernel_filename);
    }

    // Step #8:
    // Invoke the original openat() function and 
    // store the return value in syscall_retval
    syscall_retval = context->original_function(regs);

    return syscall_retval;
} // hook_syscall_sys_openat()

//
// SYMBOL Lookup
//

static int kprobe_pre_handler(struct kprobe *p, struct pt_regs *regs) {
    return 0;
} // kprobe_pre_handler()

static void kprobe_post_handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags) {
    return;
} // kprobe_post_handler()

// get_symbol_address()
// Does not work for functions that are not probable through kprobe
void* get_symbol_address(const char *name) {
    void* function = NULL;
    struct kprobe probe = {
        .pre_handler    = kprobe_pre_handler,
        .post_handler   = kprobe_post_handler,
    };

    // Cannot be set as a constant in struct kprobe initializers
    probe.symbol_name = name;

    // Translate the symbol name to the function address
    // by registering a probe and then removing it immediately
    // but capturing the probe address which is the address of the fucntion
    register_kprobe(&probe);
    function = (void*)probe.addr;
    unregister_kprobe(&probe);

    return function;
} // get_symbol_address()

// Check the given filename against the list of excluded filenames
bool is_file_excluded (char* filename) {
    int i;
    for ( i = 0 ; i < ARRAY_SIZE(excluded_filenames) ; i++ ) {
        if ( strstr(filename, excluded_filenames[i]) ) {
            return true;
        }
    }
    return false;
} // is_file_excluded()

//
// Utility function
//

// memcpy() with write protection bypass through CR0.WP
void memcpy_bwp (void* dst, void* src, size_t size) {
    u64 cr0_org = 0;
    u64 cr0_new = 0;

    // read the current value from cr0 and store it in cr0_org
    asm volatile("mov %%cr0, %0" : "=r" (cr0_org));

    // turn off but 16 (CR0.WP)
    cr0_new = cr0_org & ~( 1 << 16 );

    // write cr0_new to CR0 to disable write protection enforcement
    asm volatile("mov %0, %%cr0" : : "r" (cr0_new));

    // perform the copy to the read-only region
    memcpy(dst, src, size);

    // restore back the original value of CR0
    asm volatile("mov %0, %%cr0" : : "r" (cr0_org));
}

int init_function(void) {
    struct hook_syscall_context *context = &g_hook_syscall_context;

    pr_info("%s Built on %s %s\n", __func__, __DATE__, __TIME__ );

    pr_info("%s: openat() SYS_CALL_NR=%u\n", __func__, SYS_CALL_NR);

    context->sys_call_table = (sys_call_ptr_t*)get_symbol_address("sys_call_table");
    if (!context->sys_call_table) {
        pr_info("%s: get_symbol_address() FAIL\n", __func__);
        goto exit;
    }
    pr_info("%s: sys_call_table=%px\n", __func__, context->sys_call_table );

    hook_syscall_init(context);

exit:
    return 0;
} // init_function()

void exit_function (void) {
    struct hook_syscall_context *context = &g_hook_syscall_context;

    hook_syscall_exit(context);

    pr_info("%s\n", __func__);
} // exit_function()
