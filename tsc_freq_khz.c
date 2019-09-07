#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <asm/tsc.h> // extern unsigned int tsc_khz;
#include <linux/printk.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>


#define DRIVER_NAME "tsc_freq_khz"
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Trail of Bits");
MODULE_DESCRIPTION("Exports tsc_khz via syfs");
MODULE_VERSION("0.1");

static ssize_t tsc_freq_khz_show(struct kobject *kobj, struct kobj_attribute *attr,
                      char *buf)
{
	ssize_t r;
	r = sprintf(buf, "%u\n", tsc_khz);
	return r;
}

static struct kobj_attribute tsc_freq_khz_attribute =
	__ATTR(tsc_freq_khz,
				 S_IRUGO, // world readable, unchangeable
				 tsc_freq_khz_show,
				 NULL); // store not implemented

static int __init tsc_khz_init(void){
	struct device *dev;
	int error = -ENOENT;
	printk(KERN_INFO DRIVER_NAME ": starting driver\n");

	dev = get_cpu_device(0); // assumes always a cpu0
	if (!dev) {
		printk(KERN_INFO DRIVER_NAME ": could not get device for CPU %d\n", 0);
		error = -ENOENT;
	} else {
		printk(KERN_INFO DRIVER_NAME ": registering with sysfs\n");
		error = sysfs_create_file(&dev->kobj, &tsc_freq_khz_attribute.attr);
		if(error)
		{
			printk(KERN_INFO DRIVER_NAME ": could not register with sysfs (%d)\n", error);
		} else {
			printk(KERN_INFO DRIVER_NAME ": successfully registered\n");
		}
	}

	return error;
}
 
static void __exit tsc_khz_exit(void) {
	struct device *dev;
	printk(KERN_INFO DRIVER_NAME ": unloading driver\n");
	dev = get_cpu_device(0); // assumes always a cpu0
	if (!dev) {
		printk(KERN_INFO DRIVER_NAME ": could not get device for CPU %d\n", 0);
		return;
	}
	sysfs_remove_file(&dev->kobj, &tsc_freq_khz_attribute.attr);
}
 
module_init(tsc_khz_init);
module_exit(tsc_khz_exit);
