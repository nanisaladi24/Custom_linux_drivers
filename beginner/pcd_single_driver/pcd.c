#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h> //for copy_to_user, copy_from_user
#include <linux/kdev_t.h>
#include <linux/version.h>
#include <linux/err.h>

//*************************Pre-processor macros*****************************//
#define DEV_MEM_SIZE 512
#define MINOR_COUNT 1

#define pr_fmt(fmt) "%s : "fmt,__func__ //WARNING EXPECTED. redefined pr_fmt. and it works because kernel builds these cases for pr_* cases
//************************* FUNCTION DECLARATIONS *****************************//

loff_t pcd_lseek(struct file *filep, loff_t offset, int whence);
ssize_t pcd_read(struct file *filep, char __user *buff, size_t count, loff_t *f_pos);
ssize_t pcd_write(struct file *filep, const char __user *buff, size_t count, loff_t *f_pos);
int pcd_open(struct inode *inode, struct file *filep);
int pcd_release(struct inode *inode, struct file *filep);

//************************* GLOBALS *****************************//
/* Pseudo device' memory */
char device_buffer[DEV_MEM_SIZE];

/* Global device number */
dev_t device_number;

/* CDev variable */
struct cdev pcd_cdev;

/* File Ops of driver */
struct file_operations pcd_fops = 
{
    .llseek = pcd_lseek,
    .open = pcd_open,
    .read = pcd_read,
    .write = pcd_write,
    .release = pcd_release,
    .owner = THIS_MODULE
};

/* Device class/device structs */
struct class *class_pcd;
struct device *device_pcd;

//************************* FUNCTIONS *****************************//

loff_t pcd_lseek(struct file *filep, loff_t offset, int whence)
{   
    loff_t temp=0;
    pr_info("lseek requested\n");
    pr_info("Current file position: %lld\n",filep->f_pos);
    switch(whence)
    {
        case SEEK_SET:
            if((offset>DEV_MEM_SIZE) || (offset<0))
            {
                return -EINVAL;
            }
            filep->f_pos = offset;
            break;
        case SEEK_CUR:
            temp = filep->f_pos + offset;
            if ((temp>DEV_MEM_SIZE) || (temp<0))
            {
                return -EINVAL;
            }
            filep->f_pos = temp;
            break;
        case SEEK_END:
            temp = DEV_MEM_SIZE + offset;
            if ((temp>DEV_MEM_SIZE) || (temp<0))
            {
                return -EINVAL;
            }
            filep->f_pos = temp;
            break;
        default:
            return -EINVAL; //invalid arg received for whence
    }

    pr_info("Updated file position: %lld\n",filep->f_pos);

    /* return update file position */
    return filep->f_pos;
}

ssize_t pcd_read(struct file *filep, char __user *buff, size_t count, loff_t *f_pos)
{
    pr_info("read requested for %zu bytes\n",count);
    pr_info("Current file position: %lld\n",*f_pos);

    /*Adjust the count*/
    if ((*f_pos+count) > DEV_MEM_SIZE)
    {
        count = DEV_MEM_SIZE - *f_pos;
    }

    /*copy to user*/
    if (copy_to_user(buff,&device_buffer[*f_pos],count))
    {
        return -EFAULT;
    }

    /*update current file position*/
    *f_pos += count;
    pr_info("Number of bytes successfully read: %zu\n",count);
    pr_info("Updated file position: %lld\n",*f_pos);

    /* return number of bytes successfully read */
    return count;
}

ssize_t pcd_write(struct file *filep, const char __user *buff, size_t count, loff_t *f_pos)
{
    pr_info("write requested for %zu bytes\n",count);
    pr_info("Current file position: %lld\n",*f_pos);

    /*Adjust the count*/
    if ((*f_pos+count) > DEV_MEM_SIZE)
    {
        count = DEV_MEM_SIZE - *f_pos;
    }

    if (!count)
    {
        pr_err("No space left on the device\n");
        return -ENOMEM;
    }

    /*copy to user*/
    if (copy_from_user(&device_buffer[*f_pos],buff,count))
    {
        return -EFAULT;
    }

    /*update current file position*/
    *f_pos += count;
    pr_info("Number of bytes successfully written: %zu\n",count);
    pr_info("Updated file position: %lld\n",*f_pos);

    /* return number of bytes successfully written */
    return count;
}

int pcd_open(struct inode *inode, struct file *filep)
{
    pr_info("open is successful\n");
    return 0;
}

int pcd_release(struct inode *inode, struct file *filep)
{
    pr_info("release is successful\n");
    return 0;
}

static int __init pcd_driver_init(void)
{   
    int ret=0;
    /* 1. Dynamically allocate device number */
    ret=alloc_chrdev_region(&device_number,0,MINOR_COUNT,"pcd_devices"); //it can fail. Handle error
    if (ret<0)
    {
        pr_err("alloc char dev failed\n");
        goto out;
    }
    pr_info("Device number <major>:<minor> = %d:%d\n",MAJOR(device_number),MINOR(device_number));

    /* 2. Initialize cdev struct with fops */
    cdev_init(&pcd_cdev,&pcd_fops);

    /* 3. Register cdev struct with VFS*/
    pcd_cdev.owner=THIS_MODULE;
    ret = cdev_add(&pcd_cdev,device_number,MINOR_COUNT);
    if (ret<0)
    {
        pr_err("cdev add failed\n");
        goto ureg_char_dev;
    }

    /* 4. Create device class under /sys/class/ */
    #if ( LINUX_VERSION_CODE >= KERNEL_VERSION( 6, 4, 0 ) ) // Hack to support newer kernel versions (host linux is newer currently)
    class_pcd=class_create("pcd_class"); //error handle later
    #else
    class_pcd=class_create(THIS_MODULE,"pcd_class"); //error handle later
    #endif
    if(IS_ERR(class_pcd))
    {
        pr_err("class creation failed\n");
        ret = PTR_ERR(class_pcd);
        goto cdev_del;
    }

    /* 4. Populate sysfs with device information (creating device) */
    device_pcd=device_create(class_pcd,NULL,device_number,NULL,"pcd"); //error handle later
    if(IS_ERR(device_pcd))
    {
        pr_err("device creation failed\n");
        ret = PTR_ERR(device_pcd);
        goto class_destroy;
    }

    pr_info("PCD Module init successful\n");

    return 0;

class_destroy:
    class_destroy(class_pcd);
cdev_del:
    cdev_del(&pcd_cdev);
ureg_char_dev:
    unregister_chrdev_region(device_number,1);
out:
    pr_info("PCD Module init failed\n");
    return ret;
}

static void __exit pcd_driver_cleanup(void)
{
    device_destroy(class_pcd,device_number);
    class_destroy(class_pcd);
    cdev_del(&pcd_cdev);
    unregister_chrdev_region(device_number,1);
    pr_info("PCD Module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

//************************* MODULE INFO UPDATE *****************************//

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NANI_LINUX_DRIVERS");
MODULE_DESCRIPTION("Pseudo char driver module");
MODULE_INFO(board,"Beaglebone Black REV A5");