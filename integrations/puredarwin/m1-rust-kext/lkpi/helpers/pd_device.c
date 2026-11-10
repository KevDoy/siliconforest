// SPDX-License-Identifier: GPL-2.0
/*
 * _dev_printk for kernel::device (new file). Like _printk (print.c): the Rust side passes a
 * level string and the fixed format "%pA"; lib/vsprintf.c is not here to interpret it. The
 * line is "<device name>: <text>", the device name being the kobject's (for a platform device
 * the device-tree path, as pd_linux_platform.c names it).
 */
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <pd_linux_compat.h>

void kprintf(const char *, ...);
void dprintf(const char *, ...);

__rust_helper void rust_helper__dev_printk(const char *level, const struct device *dev,
					   const char *fmt, ...)
{
	char line[512];
	char *p = line, *end = line + sizeof(line) - 1;
	const char *name = dev != NULL ? kobject_name(&dev->kobj) : NULL;
	va_list ap;

	p += scnprintf(p, end - p, "%s: ", name != NULL ? name : "(no device)");
	va_start(ap, fmt);
	if (strncmp(fmt, "%pA", 3) == 0) {
		p = rust_fmt_argument(p, end, va_arg(ap, const void *));
		if (p > end)
			p = end;
	}
	va_end(ap);
	*p = '\0';
	if (level != NULL && level[0] == '\001' && level[1] == '7')
		dprintf("%s", line);
	else
		kprintf("%s", line);
}
