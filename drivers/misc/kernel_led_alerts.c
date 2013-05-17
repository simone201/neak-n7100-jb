/* drivers/misc/kernel_led_alerts.c
*
* Copyright 2013 Simone Renzo
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/kernel_led_alerts.h>
#include <linux/miscdevice.h>

#define LED_ALERTS_VERSION 1

static bool led_alerts_state = false;

void enable_led_alert(struct led_trigger *trigger, enum led_brightness brightness) {
	if(led_alerts_state) {
		led_trigger_event(trigger, brightness);
	}
	return;
}
EXPORT_SYMBOL(enable_led_alert);

void disable_led_alert(struct led_trigger *trigger) {
	if(led_alerts_state) {
		led_trigger_event(trigger, LED_OFF);
	}
	return;
}
EXPORT_SYMBOL(disable_led_alert);

int register_led_alert(struct led_trigger *trigger) {
	return led_trigger_register(trigger);
}
EXPORT_SYMBOL(register_led_alert);

void unregister_led_alert(struct led_trigger *trigger) {
	led_trigger_unregister(trigger);
}
EXPORT_SYMBOL(unregister_led_alert);

static ssize_t alerts_status_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%u\n", (led_alerts_state ? 1 : 0));
}

static ssize_t alerts_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
    unsigned int data;

    if(sscanf(buf, "%u\n", &data) == 1) {
		pr_devel("%s: %u \n", __FUNCTION__, data);
		
		if (data == 1) {
			pr_info("%s: LED Alerts enabled\n", __FUNCTION__);
			led_alerts_state = true;
		} else if (data == 0) {
			pr_info("%s: LED Alerts disabled\n", __FUNCTION__);
			led_alerts_state = false;
		} else {
			pr_info("%s: invalid input range %u\n", __FUNCTION__, data);
		}
	} else 	{
		pr_info("%s: invalid input\n", __FUNCTION__);
	}

    return size;
}

static ssize_t alerts_version_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%u\n", LED_ALERTS_VERSION);
}

static DEVICE_ATTR(led_alerts_state, S_IRUGO | S_IWUGO, alerts_status_show, alerts_status_store);
static DEVICE_ATTR(version, S_IRUGO , alerts_version_show, NULL);

static struct attribute *led_alerts_attributes[] = {
	&dev_attr_led_alerts_state.attr,
	&dev_attr_version.attr,
	NULL
};

static struct attribute_group led_alerts_group = {
	.attrs = led_alerts_attributes,
};

static struct miscdevice led_alerts_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "led_alerts",
};

static int __init led_alerts_init(void) {
    int ret;

    pr_info("%s misc_register(%s)\n", __FUNCTION__, led_alerts_device.name);

    ret = misc_register(&led_alerts_device);

    if (ret) {
		pr_err("%s misc_register(%s) fail\n", __FUNCTION__, led_alerts_device.name);
		return 1;
    }

    if (sysfs_create_group(&led_alerts_device.this_device->kobj, &led_alerts_group) < 0) {
		pr_err("%s sysfs_create_group fail\n", __FUNCTION__);
		pr_err("Failed to create sysfs group for device (%s)!\n", led_alerts_device.name);
    }

    return 0;
}

device_initcall(led_alerts_init);
