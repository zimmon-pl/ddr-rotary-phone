// led.h — on-board status LED (GPIO 22 / D4)
//
// Single-LED status indicator:
//   no BT             → slow blink   (1 Hz)
//   BT connected idle → solid on
//   ringing           → fast blink   (4 Hz)
//   in call           → solid on
// State is derived by polling bluetooth_is_*() — no setter required.

#ifndef LED_H
#define LED_H

void led_init(void);

#endif
