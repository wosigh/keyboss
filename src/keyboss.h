#ifndef KEYBOSS_H
#define KEYBOSS_H

#include <stdbool.h>
#include <linux/input.h>

#define DBUS_ADDRESS "org.webosinternals.keyboss"
#define KEYPAD_DEVICE "/dev/input/event2"
#define UINPUT_DEVICE "/dev/input/uinput"

/* Default values for maxim7359 keypad can be seen in 
 * linux-2.6.24/arch/arm/mach-omap3pe/board-sirloin-3430.c
 */
#define DEFAULT_DELAY  (500)
#define DEFAULT_PERIOD (100)

int current_delay;
int current_period;
int hold_enabled;
int double_enabled;
static int u_fd;
static int k_fd;

/* function declarations */
bool is_valid_code(int code);
int send_key(__u16 code, __s32 value);
int set_repeat(__s32 delay, __s32 period);

#endif /* KEYBOSS_H */
