
#ifndef PCD_PLATFORM_DRIVER_DT_SYSFS_H
#define PCD_PLATFORM_DRIVER_DT_SYSFS_H

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h> //for copy_to_user, copy_from_user
#include <linux/kdev_t.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>

#define pr_fmt(fmt) "%s : "fmt,__func__ //WARNING EXPECTED. redefined pr_fmt. and it works because kernel builds these cases for pr_* cases

//*************************Pre-processor macros*****************************//


/* LOCAL ERROR/STATUS DEFINES */
#define GPIO_DRV_SUCCESS 0

//************************* ENUMS *****************************//

//************************* STRUCTS *****************************//

/* Device private data struct - review. per device? */ 
struct gpiodev_private_data
{
    char label[20]; //for label name
    struct gpio_desc *desc; //for descriptor
};

/* Driver private data struct - review */
struct gpiodrv_private_data
{
    int total_devices;
    struct class *class_gpio;
    struct device **dev;
};

//************************* FUNCTION DECLARATIONS *****************************//
#if 0 
/* File ops (system call functions) */
loff_t pcd_lseek(struct file *filep, loff_t offset, int whence);
ssize_t pcd_read(struct file *filep, char __user *buff, size_t count, loff_t *f_pos);
ssize_t pcd_write(struct file *filep, const char __user *buff, size_t count, loff_t *f_pos);
int pcd_open(struct inode *inode, struct file *filep);
int pcd_release(struct inode *inode, struct file *filep);
int check_permission(int dev_perm, int access_mode);
#endif

/* Sysfs attributes */
ssize_t direction_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t direction_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
ssize_t value_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
ssize_t label_show(struct device *dev, struct device_attribute *attr, char *buf);

#endif //PCD_PLATFORM_DRIVER_DT_SYSFS_H