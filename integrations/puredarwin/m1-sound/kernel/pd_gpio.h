/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Pins of an Apple GPIO block ("gpio,t8101": /arm-io/gpio, /arm-io/nub-gpio, ...), by
 * device tree node and pin number. Register layout from Linux
 * drivers/pinctrl/pinctrl-apple-gpio.c. No pin interrupts: callers poll.
 *
 * Each call is one read-modify-write of the pin's own register; callers that share a
 * pin serialise among themselves.
 */
#ifndef _PD_GPIO_H
#define _PD_GPIO_H

#include "pd_soc.h"

struct PDGpio {
	PDMmio   regs;
	uint32_t npins;

	PDGpio() : npins(0) {}

	bool attach(const char *path);          /* e.g. "/arm-io/nub-gpio" */
	uint32_t raw(unsigned pin) const;       /* the pin's configuration register */
	int  get(unsigned pin) const;           /* 0 or 1; -1 for a bad pin */
	void output(unsigned pin, int value);   /* GPIO function, driven */
	void input(unsigned pin);               /* GPIO function, input, interrupt off */
	void set(unsigned pin, int value);      /* value of a pin that is already an output */
	void function(unsigned pin, unsigned func); /* peripheral function 1..3 (0 is GPIO) */
};

#endif
