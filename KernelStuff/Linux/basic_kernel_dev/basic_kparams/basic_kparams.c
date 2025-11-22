#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/module.h>

int init_function(void);
void exit_function (void);
module_init (init_function);
module_exit (exit_function);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grapefruit");
MODULE_DESCRIPTION("Basic KParams");

// Forward declaration
int my_custom_setter(const char*, const struct kernel_param*);

struct kernel_param_ops callback_structure = {
    .set = my_custom_setter, // custom setter
    .get = param_get_int, // standard getter
};

// module parameters - input_number
static int input_number = 0;
module_param_cb(input_number, &callback_structure, &input_number, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(input_number, "A number that a user inputs");


int my_custom_setter(const char *val, const struct kernel_param *kp)
{
    int res;
    res = param_set_int(val, kp);
    if ( res < 0 )
    {
        pr_info("param_set_int() FAILED\n");
        return -1;
    }

    pr_info("Your Number + 15 = %d\n", input_number + 15);

    return 0;
}

int init_function(void) {

    pr_info("%s Built on %s %s\n", __func__, __DATE__, __TIME__ );



    return 0;
} // init_function()

void exit_function (void) {

    pr_info("%s\n", __func__);

} // exit_function()