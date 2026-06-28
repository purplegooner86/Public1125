# Parameters to Kernel Modules

Getting input from user mode to kernel mode is a whole lot easier on Linux than it is on Windows  

We just have to do a few things within the kernel module to make a `/sys/module/...` file accessible from user mode  

First we need a `struct kernel_param_ops` to store callbacks for a parameter's getter and setters:  
```C
struct kernel_param_ops callback_structure = {
    .set = my_custom_setter, // custom setter
    .get = param_get_int, // standard getter
};
```

In this case, we wan't a custom callback function `my_custom_setter` to be invoked when the parameter is set, but we are leaving the default getter for ints  

Next, we have to register our callback and our variable:  
```C
// module parameters - input_number
static int input_number = 0;
module_param_cb(input_number, &callback_structure, &input_number, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(input_number, "A number that a user inputs");
```

Finally, we will write our custom callback function `my_custom_setter`:  
```C
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
```

I put this all together into a module in: [basic_kparams](./basic_kparams/basic_kparams.c)  

Once the module is running, run:  
- `echo 13 | sudo tee /sys/module/basic_kparams/parameters/input_number`  
