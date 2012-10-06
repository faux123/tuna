/*
 * Author: Paul Reioux aka Faux123 <reioux@gmail.com>
 *
 * drivers/gpu/pvr/pvr_governor.h
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
#include <linux/kernel.h>

#define PVR_GOVERNOR_VERSION	1

#define SGX_SPEED_TURBO		2
#define SGX_SPEED_NOMINAL	1
#define SGX_SPEED_IDLE		0

/* actual use is more like 4~5 due to delay in the idle timeouts */
#define SGX_SPEED_THRESHOLD	3

#define SGX_SPEED_STEPS		3

uint PVRSimpleGovernor(bool enabled);

