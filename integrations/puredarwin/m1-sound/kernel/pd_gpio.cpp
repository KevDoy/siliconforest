// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apple GPIO pins. Ported from Linux drivers/pinctrl/pinctrl-apple-gpio.c
 * (apple_gpio_get, apple_gpio_set, apple_gpio_direction_input/output,
 * apple_gpio_pinmux_set):
 *   Copyright (C) The Asahi Linux Contributors
 *   Copyright (C) 2020 Corellium LLC
 *   Based on: pinctrl-pistachio.c, Copyright (C) 2014 Imagination Technologies Ltd.,
 *   Copyright (C) 2014 Google, Inc.
 */
#include "pd_gpio.h"

#include <IOKit/IOLib.h>

#define REG_GPIO(x)            (4 * (x))
#define REG_GPIOx_DATA         (1u << 0)
#define REG_GPIOx_MODE         (7u << 1)
#define REG_GPIOx_OUT          1
#define REG_GPIOx_IN_IRQ_OFF   7
#define REG_GPIOx_PERIPH       (3u << 5)
#define REG_GPIOx_INPUT_ENABLE (1u << 9)

bool
PDGpio::attach(const char *path)
{
	if (!pd_adt_u32(path, "#gpio-pins", 0, &npins) || npins == 0 || npins > 1024) {
		IOLog("PDGpio: %s has no usable #gpio-pins\n", path);
		return false;
	}
	return regs.mapNode(path, 0);
}

uint32_t
PDGpio::raw(unsigned pin) const
{
	return pin < npins && regs.va ? regs.r32(REG_GPIO(pin)) : 0;
}

int
PDGpio::get(unsigned pin) const
{
	if (pin >= npins || regs.va == NULL)
		return -1;
	return (regs.r32(REG_GPIO(pin)) & REG_GPIOx_DATA) ? 1 : 0;
}

void
PDGpio::output(unsigned pin, int value)
{
	if (pin >= npins || regs.va == NULL)
		return;
	regs.mask32(REG_GPIO(pin), REG_GPIOx_PERIPH | REG_GPIOx_MODE | REG_GPIOx_DATA,
	    (REG_GPIOx_OUT << 1) | (value ? REG_GPIOx_DATA : 0));
}

void
PDGpio::input(unsigned pin)
{
	if (pin >= npins || regs.va == NULL)
		return;
	regs.mask32(REG_GPIO(pin), REG_GPIOx_PERIPH | REG_GPIOx_MODE | REG_GPIOx_DATA | REG_GPIOx_INPUT_ENABLE,
	    (REG_GPIOx_IN_IRQ_OFF << 1) | REG_GPIOx_INPUT_ENABLE);
}

void
PDGpio::set(unsigned pin, int value)
{
	if (pin >= npins || regs.va == NULL)
		return;
	regs.mask32(REG_GPIO(pin), REG_GPIOx_DATA, value ? REG_GPIOx_DATA : 0);
}

void
PDGpio::function(unsigned pin, unsigned func)
{
	if (pin >= npins || regs.va == NULL || func > 3)
		return;
	regs.mask32(REG_GPIO(pin), REG_GPIOx_PERIPH | REG_GPIOx_INPUT_ENABLE, (func << 5) | REG_GPIOx_INPUT_ENABLE);
}
