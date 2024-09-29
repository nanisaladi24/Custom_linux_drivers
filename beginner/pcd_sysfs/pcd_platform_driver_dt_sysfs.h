
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

#include "platform.h"

//*************************Pre-processor macros*****************************//
#define MEM_SIZE_MAX_PCDEV1 1024
#define MEM_SIZE_MAX_PCDEV2 512
#define MEM_SIZE_MAX_PCDEV3 1024
#define MEM_SIZE_MAX_PCDEV4 512

#define NO_OF_DEVICES 4 //UNUSED
#define MAX_DEVICES 10

/* LOCAL ERROR/STATUS DEFINES */
#define PCD_DRV_SUCCESS 0

#define pr_fmt(fmt) "%s : "fmt,__func__ //WARNING EXPECTED. redefined pr_fmt. and it works because kernel builds these cases for pr_* cases

//************************* ENUMS *****************************//
typedef enum
{
    PCDEVA1X,
    PCDEVB1X,
    PCDEVC1X,
    PCDEVD1X
}DeviceIds;

//************************* STRUCTS *****************************//

struct device_config 
{
    int config_item1;
    int config_item2;
};

/* Device private data struct */
struct pcdev_private_data
{
    struct pcdev_platform_data pdata;
    char *buffer;
    dev_t dev_num;
    struct cdev cdev;
};

/* Driver private data struct */
struct pcdrv_private_data
{
    int total_devices;
    /* holds device number of base (first device) of all allocated devices */
    dev_t device_num_base;
    /* Device class/device structs */
    struct class *class_pcd;
    struct device *device_pcd;
};

//************************* FUNCTION DECLARATIONS *****************************//

/* File ops (system call functions) */
loff_t pcd_lseek(struct file *filep, loff_t offset, int whence);
ssize_t pcd_read(struct file *filep, char __user *buff, size_t count, loff_t *f_pos);
ssize_t pcd_write(struct file *filep, const char __user *buff, size_t count, loff_t *f_pos);
int pcd_open(struct inode *inode, struct file *filep);
int pcd_release(struct inode *inode, struct file *filep);
int check_permission(int dev_perm, int access_mode);

/* Sysfs attributes */
ssize_t show_max_size(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t show_serial_num(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t store_max_size(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

#endif //PCD_PLATFORM_DRIVER_DT_SYSFS_H