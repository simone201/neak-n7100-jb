/*
 * charge_control.c -- charge current control interface for the sgs2/3
 *
 *  Copyright (C) 2011 Gokhan Moral
 *
 * 	GT-I9300 support by simone201
 * 
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of the GNU General Public License as published by the
 *  Free Software Foundation;
 *
 */

extern int charge_current_ac;
extern int charge_current_cdp;
extern int charge_current_usb;
extern int charge_current_dock;

extern void charge_current_start(void);
