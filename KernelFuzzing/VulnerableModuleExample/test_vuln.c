#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

// Assume that a user has control of this at some point
// For the sake of the exersize
static char assume_controlable[256] = {'\0'};

struct vuln_struct {
    char some_string[16];
    int *a_pointer;
};

int my_custom_setter(const char*, struct kernel_param*);

// module parameters - input_number
static int input_number = 0;
module_param_call(input_number, my_custom_setter, param_get_int, &input_number, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(input_number, "A number that a user inputs");

noinline int the_vuln_function(int input_number, char *input_string) {
    struct vuln_struct a = { '\0' };

    a.a_pointer = &input_number;

    // Turn string 150-154 into a uint
    uint32_t bad_cast = *(uint32_t*)(&(input_string[150]));

    if ((bad_cast + input_number) > 20) {
        // printk("num too big\n");
        return 0;
    }

    // This could overwrite first byte of pointer
    memcpy(a.some_string, input_string, bad_cast + input_number);

    return *(a.a_pointer);
}

int my_custom_setter(const char *val, struct kernel_param *kp)
{
    int res;
    res = param_set_int(val, kp);
    if ( res < 0 )
    {
        printk("param_set_int() FAILED\n");
        return -1;
    }

    printk("Your Number = %d\n", input_number);

    res = the_vuln_function(input_number, assume_controlable);

    printk("And the number is %d\n", res);

    return 0;
}

int test_vuln_module_init(void) {
    printk("test vuln module loaded\n");
    printk("Address of the_vuln_function is %p\n", the_vuln_function);

    return 0;
}

void test_vuln_module_exit(void) {
    printk("test vuln module exit\n");

    return;
}

module_init(test_vuln_module_init);
module_exit(test_vuln_module_exit);
