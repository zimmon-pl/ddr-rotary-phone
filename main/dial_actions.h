// dial_actions.h — what to do once the rotary dial has produced a number
//
// Wire as the rotary_dial number callback:
//   rotary_dial_on_number(dial_actions_handle_number);

#ifndef DIAL_ACTIONS_H
#define DIAL_ACTIONS_H

#include <stdint.h>

void dial_actions_handle_number(const char *number, uint8_t length);

#endif
