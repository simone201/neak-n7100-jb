/* drivers/misc/touch_boost_control.c
*
* Copyright 2012 Francisco Franco
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#define TOUCH_BOOST_CONTROL_VERSION 1

static unsigned int input_boost_freq = 700000;

extern void update_boost_freq(unsigned int input_boost_freq);

static ssize_t touch_boost_status_read(struct device * dev, struct device_attribute * attr, char * buf) {
    return sprintf(buf, "%u\n", input_boost_freq);
}

static ssize_t touch_boost_status_write(struct device * dev, struct device_attribute * attr, const char * buf, size_t size) {
    unsigned int value;

    if(sscanf(buf, "%u\n", &value) == 1) {
	if (value >= 200000 || value <= 1400000) {
		input_boost_freq = value;
		update_boost_freq(input_boost_freq);
		pr_info("TOUCH_BOOST STATUS: %u", value);
	}
    } else {
	pr_info("%s: invalid input\n", __FUNCTION__);
    }

    return size;
}

static ssize_t touch_boost_version(struct device * dev, struct device_attribute * attr, char * buf) {
    return sprintf(buf, "%u\n", TOUCH_BOOST_CONTROL_VERSION);
}

static DEVICE_ATTR(input_boost_freq, S_IRUGO | S_IWUGO, touch_boost_status_read, touch_boost_status_write);
static DEVICE_ATTR(version, S_IRUGO , touch_boost_version, NULL);

static struct attribute *touch_boost_attributes[] = {
	&dev_attr_input_boost_freq.attr,
	&dev_attr_version.attr,
	NULL
};

static struct attribute_group touch_boost_group = {
	.attrs = touch_boost_attributes,
};

static struct miscdevice touch_boost_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "touchboost",
};

static int __init touch_boost_init(void) {
    int ret;

    pr_info("%s misc_register(%s)\n", __FUNCTION__, touch_boost_device.name);

    ret = misc_register(&touch_boost_device);

    if (ret) {
	pr_err("%s misc_register(%s) fail\n", __FUNCTION__, touch_boost_device.name);
	return 1;
    }

    if (sysfs_create_group(&touch_boost_device.this_device->kobj, &touch_boost_group) < 0) {
	pr_err("%s sysfs_create_group fail\n", __FUNCTION__);
	pr_err("Failed to create sysfs group for device (%s)!\n", touch_boost_device.name);
    }

    return 0;
}

device_initcall(touch_boost_init);
