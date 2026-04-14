// hook_switch.h — Public interface for handset hook switch detection
//
// The hook switch is a simple mechanical switch:
//   Handset DOWN (on cradle) = switch CLOSED (pressed by weight)
//   Handset UP (lifted)      = switch OPEN (spring releases)
//
// Usage:
//   hook_switch_init();
//   hook_switch_on_change(my_callback);

#ifndef HOOK_SWITCH_H
#define HOOK_SWITCH_H

#include <stdbool.h>

typedef enum {
    HOOK_ON = 0,   // handset is on the cradle (down)
    HOOK_OFF = 1,  // handset is lifted (up)
} hook_state_t;

// Called when the hook switch changes state
typedef void (*hook_change_cb_t)(hook_state_t state);

// Set up GPIO and interrupt for hook switch detection
void hook_switch_init(void);

// Register a callback for hook state changes
void hook_switch_on_change(hook_change_cb_t cb);

// Read the current hook state right now
hook_state_t hook_switch_get_state(void);

#endif
