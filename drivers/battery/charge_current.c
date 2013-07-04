/*
 * charge_current.c -- charge current control interface for the sgs2/3
 *
 *  Copyright (C) 2011 Gokhan Moral
 * 
 * 	GT-I9300/N7100 support by simone201
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of the GNU General Public License as published by the
 *  Free Software Foundation;
 *
 */

#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#define MAX_CURRENT   2100

int charge_current_ac = 1700;
int charge_current_cdp = 1000;
int charge_current_usb = 475;
int charge_current_dock = 1700;

bool ignore_power = false;
bool ignore_margin = false;

static ssize_t charge_current_show(struct device *dev, struct device_attribute *attr, char *buf) {
	return sprintf(buf, "AC: %d\nCDP: %d\nUSB: %d\nDock: %d\n",
		charge_current_ac, charge_current_cdp, charge_current_usb, charge_current_dock);
}

static ssize_t charge_current_store(struct device *dev, struct device_attribute *attr, const char *buf,
									size_t count) {
	unsigned int ret = -EINVAL;
	int temp[4];
	ret = sscanf(buf, "%d %d %d %d", &temp[0], &temp[1], &temp[2], &temp[3]);
	if (ret != 4) {
		return -EINVAL;
	}
	else {
		charge_current_ac = (temp[0] < MAX_CURRENT) ? temp[0] : MAX_CURRENT;
		charge_current_cdp = (temp[1] < MAX_CURRENT) ? temp[1] : MAX_CURRENT;
		charge_current_usb = (temp[2] < MAX_CURRENT) ? temp[2] : MAX_CURRENT;
		charge_current_dock = (temp[3] < MAX_CURRENT) ? temp[3] : MAX_CURRENT;
	}
	return count;	
}

static ssize_t ignore_pwr_show(struct device *dev, struct device_attribute *attr, char *buf) {
	return sprintf(buf, "%u\n", (ignore_power ? 1 : 0));
}

static ssize_t ignore_pwr_store(struct device *dev, struct device_attribute *attr, const char *buf, 
								size_t size) {
	unsigned int data;

	if(sscanf(buf, "%u\n", &data) == 1) {
		pr_devel("%s: %u \n", __FUNCTION__, data);

		if (data == 1) {
			ignore_power = true;
		} else if (data == 0) {
			ignore_power = false;
		} else {
			pr_info("%s: invalid input range %u\n", __FUNCTION__, data);
		}
	} else 	{
		pr_info("%s: invalid input\n", __FUNCTION__);
	}

	return size;
}

static ssize_t ignore_mar_show(struct device *dev, struct device_attribute *attr, char *buf) {
	return sprintf(buf, "%u\n", (ignore_power ? 1 : 0));
}

static ssize_t ignore_mar_store(struct device *dev, struct device_attribute *attr, const char *buf, 
								size_t size) {
	unsigned int data;

	if(sscanf(buf, "%u\n", &data) == 1) {
		pr_devel("%s: %u \n", __FUNCTION__, data);

		if (data == 1) {
			ignore_margin = true;
		} else if (data == 0) {
			ignore_margin = false;
		} else {
			pr_info("%s: invalid input range %u\n", __FUNCTION__, data);
		}
	} else 	{
		pr_info("%s: invalid input\n", __FUNCTION__);
	}

	return size;
}

static DEVICE_ATTR(charge_current, S_IRUGO | S_IWUGO, charge_current_show, charge_current_store);
static DEVICE_ATTR(ignore_pwr, S_IRUGO | S_IWUGO, ignore_pwr_show, ignore_pwr_store);
static DEVICE_ATTR(ignore_mar, S_IRUGO | S_IWUGO, ignore_mar_show, ignore_mar_store);

static struct attribute *charge_current_attributes[] = {
	&dev_attr_charge_current.attr,
	&dev_attr_ignore_pwr.attr,
	&dev_attr_ignore_mar.attr,
	NULL
};

static struct attribute_group charge_current_group = {
	.attrs = charge_current_attributes,
};

static struct miscdevice charge_current_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "charge_current",
};

void charge_current_start(void)
{
	printk("Initializing charge current control interface\n");
	
	misc_register(&charge_current_device);
	if (sysfs_create_group(&charge_current_device.this_device->kobj,
				&charge_current_group) < 0) {
		printk("%s sysfs_create_group failed\n", __FUNCTION__);
		pr_err("Unable to create group for %s\n", charge_current_device.name);
	}
}

