#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/module.h>

int init_function(void);
void exit_function (void);
module_init (init_function);
module_exit (exit_function);

MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Grapefruit");
MODULE_DESCRIPTION("Hello World");

int init_function(void) {

    pr_info("%s Built on %s %s\n", __func__, __DATE__, __TIME__ );
    pr_info("Hello World!\n");

    return 0;
} // init_function()

void exit_function (void) {

    pr_info("%s\n", __func__);

} // exit_function()