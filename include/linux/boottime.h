/*
 * Copyright (C) ST-Ericsson SA 2009-2010
 *
 * Author: Jonas Aaberg <jonas.aberg@stericsson.com> for ST-Ericsson
 *
 * License terms: GNU General Public License (GPL) version 2
 *
 * boottime is a tool for collecting start-up timing
 * information and can together with boot loader support
 * display a total system start-up time.
 *
 */

#ifndef LINUX_BOOTTIME_H
#define LINUX_BOOTTIME_H

#ifdef CONFIG_BOOTTIME
#include <linux/kernel.h>

/**
 * struct boottime_timer - Callbacks for generic timer.
 * @init: Function to call at boottime initialization
 * @get_time: Returns the number of us since start-up
 *            Preferable this is based upon a free running timer.
 *            This is the only required entry.
 * @finalize: Called before init is executed and boottime is done.
 */
struct boottime_timer {
	int (*init)(void);
	unsigned long (*get_time)(void);
	void (*finalize)(void);
};

/**
 * boottime_mark_wtime()
 * Add a sample point with a given time. Useful for adding data collected
 * by for example a boot loader.
 * @name: The name of the sample point
 * @time: The time in us when this point was reached
 */
void __init boottime_mark_wtime(char *name, unsigned long time);

/**
 * boottime_mark()
 * Add a sample point with the current time.
 * @name: The name of this sample point
 */
void __init boottime_mark(char *name);

/**
 * boottime_mark_symbolic()
 * Add a sample point where the name is a symbolic function
 * and %pF is needed to get the correct function name.
 * @name: function name.
 */
void __init boottime_mark_symbolic(void *name);

/**
 * boottime_activate()
 * Activates boottime and register callbacks.
 * @bt: struct with callbacks.
 */
void __ref boottime_activate(struct boottime_timer *bt);

/**
 * boottime_deactivate()
 * This function is called when the kernel boot is done.
 * (before "free init memory" is called)
 */
void __init boottime_deactivate(void);

/**
 * boottime_system_up()
 * A function is called when the basics of the kernel
 * is up and running.
 */
void __init boottime_system_up(void);

#else

#define boottime_mark_wtime(name, time)
#define boottime_mark(name)
#define boottime_mark_symbolic(name)
#define boottime_activate(bt)
#define boottime_deactivate()
#define boottime_system_up()
#endif

#endif /* LINUX_BOOTTIME_H */
