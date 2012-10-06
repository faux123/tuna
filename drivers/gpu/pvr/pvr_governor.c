/*
 * Author: Paul Reioux aka Faux123 <reioux@gmail.com>
 *
 * drivers/gpu/pvr/pvr_governor.c
 *
 * Copyright 2012 Paul Reioux
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include "pvr_governor.h"

static uint sgxBusyCount = 0;
static long unsigned sgxGovernorStats[SGX_SPEED_STEPS] =
	{0, 0, 0};

uint PVRSimpleGovernor(bool enabled)
{
	uint index;

	/* don't count if too busy */
	if (sgxBusyCount < 20)
		sgxBusyCount++;

	// faux123 debug
	//pr_info("PVR Busy Count: %u\n", sgxBusyCount);

	/* if requested to turn off the clock */
	if (!enabled) {
		index = SGX_SPEED_IDLE;
		/* reset busy count */
		sgxBusyCount = 0;
		sgxGovernorStats[SGX_SPEED_IDLE]++;
	} else {
		if (sgxBusyCount <= SGX_SPEED_THRESHOLD) {
			index = SGX_SPEED_TURBO;
			//pr_info("PVR Turbo Activated!\n");
			sgxGovernorStats[SGX_SPEED_TURBO]++;
		}
		else {
			index = SGX_SPEED_NOMINAL;
			sgxGovernorStats[SGX_SPEED_NOMINAL]++;
		}
	}
	return index;
}

/* **************************** SYSFS interface **************************** */
static ssize_t pvr_simple_gov_stats_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *out = buf;

	out += sprintf(out, "pvr idle: %lu\n", sgxGovernorStats[SGX_SPEED_IDLE]);
	out += sprintf(out, "pvr nominal: %lu\n", sgxGovernorStats[SGX_SPEED_NOMINAL]);
	out += sprintf(out, "pvr turbo: %lu\n", sgxGovernorStats[SGX_SPEED_TURBO]);

	return out-buf;
}

static ssize_t pvr_simple_gov_version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "version: %u\n", PVR_GOVERNOR_VERSION);
}

static struct kobj_attribute pvr_simple_gov_stats_attribute =
	__ATTR(simple_gov_stats, 0444 , pvr_simple_gov_stats_show, NULL);

static struct kobj_attribute pvr_simple_gov_version_attribute =
	__ATTR(simple_gov_version, 0444 , pvr_simple_gov_version_show, NULL);

static struct attribute *pvr_simple_gov_attrs[] =
	{
		&pvr_simple_gov_stats_attribute.attr,
		&pvr_simple_gov_version_attribute.attr,
		NULL,
	};

static struct attribute_group pvr_simple_gov_attr_group =
	{
		.attrs = pvr_simple_gov_attrs,
	};

static struct kobject *pvr_simple_gov_kobj;

static int pvr_simple_gov_init(void)
{
	int sysfs_result;

	pvr_simple_gov_kobj = kobject_create_and_add("pvr_simple_gov", kernel_kobj);
	if (!pvr_simple_gov_kobj) {
		pr_err("%s pvr_simple_gov kobject create failed!\n", __FUNCTION__);
		return -ENOMEM;
        }

	sysfs_result = sysfs_create_group(pvr_simple_gov_kobj, &pvr_simple_gov_attr_group);

        if (sysfs_result) {
		pr_info("%s pvr_simple_gov sysfs create failed!\n", __FUNCTION__);
		kobject_put(pvr_simple_gov_kobj);
	}
	return sysfs_result;
}

static void pvr_simple_gov_exit(void)
{
	if (pvr_simple_gov_kobj != NULL)
		kobject_put(pvr_simple_gov_kobj);
}

module_init(pvr_simple_gov_init);
module_exit(pvr_simple_gov_exit);

