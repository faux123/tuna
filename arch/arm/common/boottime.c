/*
 * Copyright (C) ST-Ericsson SA 2009-2010
 *
 * Author: Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson
 *
 * License terms: GNU General Public License (GPL) version 2
 *
 * Store boot times measured during for example u-boot startup.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/boottime.h>
#include <linux/string.h>
#include <asm/setup.h>

static u32 bootloader_idle;
static u32 bootloader_total;

static int __init boottime_parse_tag(const struct tag *tag)
{
	int i;
	char buff[BOOTTIME_MAX_NAME_LEN];

	bootloader_idle = tag->u.boottime.idle;
	bootloader_total = tag->u.boottime.total;

	for (i = 0; i < tag->u.boottime.num; i++) {
		snprintf(buff, BOOTTIME_MAX_NAME_LEN, "%s+0x0/0x0",
			 tag->u.boottime.entry[i].name);
		buff[BOOTTIME_MAX_NAME_LEN - 1] = '\0';
		boottime_mark_wtime(buff, tag->u.boottime.entry[i].time);
	}

	return 0;
}

__tagtable(ATAG_BOOTTIME,  boottime_parse_tag);

int boottime_bootloader_idle(void)
{
	if (bootloader_total == 0)
		return 0;

	return (int) ((bootloader_idle) / (bootloader_total / 100));
}
