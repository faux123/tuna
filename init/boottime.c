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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/boottime.h>
#include <linux/kernel_stat.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>

/*
 * BOOTTIME_MAX_NAME_LEN is defined in arch/arm/include/asm/setup.h to 64.
 * No crisis if they don't match.
 */
#ifndef BOOTTIME_MAX_NAME_LEN
#define BOOTTIME_MAX_NAME_LEN 64
#endif

/*
 * We have a few static entries, since it is good to have measure points
 * before the system is up and running properly
 */
#define NUM_STATIC_BOOTTIME_ENTRIES 16

struct boottime_list {
	struct list_head list;
	char name[BOOTTIME_MAX_NAME_LEN];
	/* Time in us since power on, possible including boot loader. */
	unsigned long time;
	bool cpu_load;
	struct cpu_usage_stat cpu_usage[NR_CPUS];
};

enum boottime_filter_type {
	BOOTTIME_FILTER_OUT_ZERO,
	BOOTTIME_FILTER_OUT_LESS_100,
	BOOTTIME_FILTER_NOTHING,
};

enum boottime_symbolic_print {
	BOOTTIME_SYMBOLIC_PRINT,
	BOOTTIME_NORMAL_PRINT,
};

enum boottime_cpu_load {
	BOOTTIME_CPU_LOAD,
	BOOTTIME_NO_CPU_LOAD,
};

static LIST_HEAD(boottime_list);
static __initdata DEFINE_SPINLOCK(boottime_list_lock);
static __initdata struct boottime_timer boottime_timer;
static __initdata int num_const_boottime_list;
static struct boottime_list const_boottime_list[NUM_STATIC_BOOTTIME_ENTRIES];
static unsigned long time_kernel_done;
static unsigned long time_bootloader_done;
static __initdata bool system_up;
static bool boottime_done;

int __attribute__((weak)) boottime_arch_startup(void)
{
	return 0;
}

int __attribute__((weak)) boottime_bootloader_idle(void)
{
	return 0;
}

static void __init boottime_mark_core(char *name,
				      unsigned long time,
				      enum boottime_symbolic_print symbolic,
				      enum boottime_cpu_load cpu_load)
{
	struct boottime_list *b;
	unsigned long flags = 0;
	int i;

	if (system_up) {
		b = kmalloc(sizeof(struct boottime_list), GFP_KERNEL);
		if (!b) {
			printk(KERN_ERR
			       "boottime: failed to allocate memory!\n");
			return;
		}

	} else {
		if (num_const_boottime_list < NUM_STATIC_BOOTTIME_ENTRIES) {
			b = &const_boottime_list[num_const_boottime_list];
			num_const_boottime_list++;
		} else {
			printk(KERN_ERR
			       "boottime: too many early measure points!\n");
			return;
		}
	}

	INIT_LIST_HEAD(&b->list);

	if (symbolic == BOOTTIME_SYMBOLIC_PRINT)
		snprintf(b->name, BOOTTIME_MAX_NAME_LEN, "%pF", name);
	else
		strncpy(b->name, name, BOOTTIME_MAX_NAME_LEN);

	b->name[BOOTTIME_MAX_NAME_LEN - 1] = '\0';
	b->time = time;
	b->cpu_load = cpu_load;

	if (cpu_load == BOOTTIME_CPU_LOAD && system_up)
		for_each_possible_cpu(i) {
			b->cpu_usage[i].system = kstat_cpu(i).cpustat.system;
			b->cpu_usage[i].idle = kstat_cpu(i).cpustat.idle;
			b->cpu_usage[i].iowait = kstat_cpu(i).cpustat.iowait;
			b->cpu_usage[i].irq = kstat_cpu(i).cpustat.irq;
			/*
			 * TODO: Make sure that user, nice, softirq, steal
			 * and guest are not used during boot
			 */
		}
	else
		b->cpu_load = BOOTTIME_NO_CPU_LOAD;

	if (system_up) {
		spin_lock_irqsave(&boottime_list_lock, flags);
		list_add(&b->list, &boottime_list);
		spin_unlock_irqrestore(&boottime_list_lock, flags);
	} else {
		list_add(&b->list, &boottime_list);
	}
}

void __init boottime_mark_wtime(char *name, unsigned long time)
{
	boottime_mark_core(name, time,
			   BOOTTIME_NORMAL_PRINT,
			   BOOTTIME_NO_CPU_LOAD);
}

void __ref boottime_mark_symbolic(void *name)
{

	if (boottime_done)
		return;

	if (boottime_timer.get_time)
		boottime_mark_core((char *) name,
				   boottime_timer.get_time(),
				   BOOTTIME_SYMBOLIC_PRINT,
				   BOOTTIME_CPU_LOAD);
}

void __init boottime_mark(char *name)
{
	if (boottime_timer.get_time)
		boottime_mark_core(name,
				   boottime_timer.get_time(),
				   BOOTTIME_NORMAL_PRINT,
				   BOOTTIME_CPU_LOAD);
}

void __init boottime_activate(struct boottime_timer *bt)
{
	struct boottime_list *b;
	int res = 0;
	unsigned long flags;

	if (bt == NULL) {
		printk(KERN_ERR
		       "boottime: error: bad configured\n");
		return;
	}

	if (bt->get_time == NULL) {
		printk(KERN_ERR
		       "boottime: error: you must provide a get_time() function\n");
		return;
	}
	memcpy(&boottime_timer, bt, sizeof(struct boottime_timer));

	if (boottime_timer.init)
		res = boottime_timer.init();

	if (res) {
		printk(KERN_ERR "boottime: initialization failed\n");
		return;
	}

	if (boottime_arch_startup())
		printk(KERN_ERR
		       "boottime: arch specfic initialization failed\n");

	spin_lock_irqsave(&boottime_list_lock, flags);

	if (!list_empty(&boottime_list)) {

		b = list_first_entry(&boottime_list, struct boottime_list,
				     list);
		if (b)
			time_bootloader_done = b->time;
	}

	spin_unlock_irqrestore(&boottime_list_lock, flags);
}

void __init boottime_system_up(void)
{
	system_up = true;
}

void __init boottime_deactivate(void)
{
	struct boottime_list *b;
	unsigned long flags;

	boottime_mark("execute_init+0x0/0x0");

	boottime_done = true;

	spin_lock_irqsave(&boottime_list_lock, flags);
	b = list_first_entry(&boottime_list, struct boottime_list, list);
	spin_unlock_irqrestore(&boottime_list_lock, flags);

	time_kernel_done = b->time;

	if (boottime_timer.finalize)
		boottime_timer.finalize();
}

#ifdef CONFIG_DEBUG_FS
static void boottime_debugfs_load(struct seq_file *s,
				  struct boottime_list *b,
				  struct boottime_list *p)
{
	int i;
	unsigned long total_p, total_b;
	unsigned long system_total, idle_total, irq_total, iowait_total;
	unsigned long system_load, idle_load, irq_load, iowait_load;

	for_each_possible_cpu(i) {
		total_b = (b->cpu_usage[i].system +
			   b->cpu_usage[i].idle +
			   b->cpu_usage[i].iowait +
			   b->cpu_usage[i].irq);

		total_p = (p->cpu_usage[i].system +
			   p->cpu_usage[i].idle +
			   p->cpu_usage[i].iowait +
			   p->cpu_usage[i].irq);

		if (total_b == total_p)
			continue;

		system_total = b->cpu_usage[i].system - p->cpu_usage[i].system;
		idle_total = b->cpu_usage[i].idle - p->cpu_usage[i].idle;
		irq_total = b->cpu_usage[i].irq - p->cpu_usage[i].irq;
		iowait_total = b->cpu_usage[i].iowait - p->cpu_usage[i].iowait;

		system_load = (100 * system_total / (total_b - total_p));
		idle_load = (100 * idle_total / (total_b - total_p));
		irq_load = (100 * irq_total / (total_b - total_p));
		iowait_load = (100 * iowait_total / (total_b - total_p));

		seq_printf(s,
			   " cpu%d system: %lu%% idle: %lu%% iowait: %lu%% irq: %lu%%",
			   i,
			   system_load,
			   idle_load,
			   iowait_load,
			   irq_load);
	}
	seq_printf(s, "\n");
}

static void boottime_debugfs_print(struct seq_file *s,
				   struct boottime_list *b,
				   struct boottime_list *p)
{
	seq_printf(s, "[%5lu.%06lu] calling  %s\n",
		   p->time / 1000000,
		   (p->time  % 1000000),
		   p->name);
	seq_printf(s, "[%5lu.%06lu] initcall %s returned 0 after %ld msecs.",
		   b->time / 1000000,
		   (b->time  % 1000000),
		   p->name, (b->time - p->time) / 1000);

	if (p->cpu_load == BOOTTIME_NO_CPU_LOAD ||
	    b->cpu_load == BOOTTIME_NO_CPU_LOAD) {
		seq_printf(s, "\n");
		return;
	}

	boottime_debugfs_load(s, b, p);
}

static int boottime_debugfs_bootgraph_show(struct seq_file *s, void *iter)
{
	struct boottime_list *b, *p = NULL, *old_p = NULL;
	enum boottime_filter_type filter = (int)s->private;

	list_for_each_entry_reverse(b, &boottime_list, list) {
		if (p) {
			if (!(filter == BOOTTIME_FILTER_OUT_ZERO &&
			     (b->time - p->time) / 1000 == 0)
			   && !(filter == BOOTTIME_FILTER_OUT_LESS_100 &&
				(b->time - p->time) < 100 * 1000))
				boottime_debugfs_print(s, b, p);
			old_p = p;
		}
		p = b;
	}

	if (filter == BOOTTIME_FILTER_NOTHING && p)
		boottime_debugfs_print(s, p, p);

	if (p)
		seq_printf(s, "[%5lu.%06lu] Freeing init memory: 0K\n",
			   p->time / 1000000, p->time  % 1000000);
	return 0;
}

static int boottime_debugfs_summary_show(struct seq_file *s, void *data)
{
	struct boottime_list *b, b_zero;

	if (time_bootloader_done)
		seq_printf(s, "bootloader: %ld msecs\n",
			   time_bootloader_done / 1000);

	seq_printf(s, "kernel: %ld msecs\ntotal: %ld msecs\n",
		   (time_kernel_done - time_bootloader_done) / 1000,
		   time_kernel_done / 1000);
	seq_printf(s, "kernel:");
	b = list_first_entry(&boottime_list,
			     struct boottime_list, list);
	memset(&b_zero, 0, sizeof(struct boottime_list));
	boottime_debugfs_load(s, b, &b_zero);

	if (time_bootloader_done)
		seq_printf(s,
			   "bootloader: cpu0 system: %d%% idle: %d%% iowait: 0%% irq: 0%%\n",
			   100 - boottime_bootloader_idle(),
			   boottime_bootloader_idle());
	return 0;
}

static int boottime_debugfs_bootgraph_open(struct inode *inode,
					   struct file *file)
{
	return single_open(file,
			   boottime_debugfs_bootgraph_show,
			   inode->i_private);
}

static int boottime_debugfs_summary_open(struct inode *inode,
					 struct file *file)
{
	return single_open(file,
			   boottime_debugfs_summary_show,
			   inode->i_private);
}

static const struct file_operations boottime_debugfs_bootgraph_operations = {
	.open		= boottime_debugfs_bootgraph_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations boottime_debugfs_summary_operations = {
	.open		= boottime_debugfs_summary_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void boottime_debugfs_init(void)
{
	struct dentry *dir;

	dir = debugfs_create_dir("boottime", NULL);

	(void) debugfs_create_file("bootgraph", S_IFREG | S_IRUGO,
				   dir, (void *)BOOTTIME_FILTER_NOTHING,
				   &boottime_debugfs_bootgraph_operations);
	(void) debugfs_create_file("bootgraph_all_except0", S_IFREG | S_IRUGO,
				   dir, (void *)BOOTTIME_FILTER_OUT_ZERO,
				   &boottime_debugfs_bootgraph_operations);
	(void) debugfs_create_file("bootgraph_larger100",
				   S_IFREG | S_IRUGO,
				   dir, (void *)BOOTTIME_FILTER_OUT_LESS_100,
				   &boottime_debugfs_bootgraph_operations);
	(void) debugfs_create_file("summary", S_IFREG | S_IRUGO,
				   dir, NULL,
				   &boottime_debugfs_summary_operations);
}
#else
#define boottime_debugfs_init(x)
#endif

static ssize_t show_bootloader(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	return sprintf(buf, "%ld\n", time_bootloader_done / 1000);
}

static ssize_t show_kernel(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return sprintf(buf, "%ld\n",
		       (time_kernel_done - time_bootloader_done) / 1000);
}

DEVICE_ATTR(kernel, 0444, show_kernel, NULL);
DEVICE_ATTR(bootloader, 0444, show_bootloader, NULL);

static struct attribute *boottime_sysfs_entries[] = {
	&dev_attr_kernel.attr,
	&dev_attr_bootloader.attr,
	NULL
};

static struct attribute_group boottime_attr_grp = {
	.name = NULL,
	.attrs = boottime_sysfs_entries,
};

static int __init boottime_init(void)
{
	struct kobject *boottime_kobj;

	boottime_kobj = kobject_create_and_add("boottime", NULL);
	if (!boottime_kobj) {
		printk(KERN_ERR "boottime: out of memory!\n");
		return -ENOMEM;
	}

	if (sysfs_create_group(boottime_kobj, &boottime_attr_grp) < 0) {
		kobject_put(boottime_kobj);
		printk(KERN_ERR "boottime: Failed creating sysfs group\n");
		return -ENOMEM;
	}

	boottime_debugfs_init();

	return 0;
}

late_initcall(boottime_init);
