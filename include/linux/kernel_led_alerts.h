/* include/linux/kernel_led_alerts.h
 *
 * Copyright 2013  Simone Renzo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#include <linux/leds.h>

#ifndef _LINUX_LED_ALERTS_H
#define _LINUX_LED_ALERTS_H

void enable_led_alert(struct led_trigger *trigger, enum led_brightness brightness);
void disable_led_alert(struct led_trigger *trigger);
int register_led_alert(struct led_trigger *trigger);
void unregister_led_alert(struct led_trigger *trigger);

#endif
