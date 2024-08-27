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
//************************* FUNCTION DECLARATIONS *****************************//

loff_t pcd_lseek(struct file *filep, loff_t offset, int whence);
ssize_t pcd_read(struct file *filep, char __user *buff, size_t count, loff_t *f_pos);
ssize_t pcd_write(struct file *filep, const char __user *buff, size_t count, loff_t *f_pos);
int pcd_open(struct inode *inode, struct file *filep);
int pcd_release(struct inode *inode, struct file *filep);
int check_permission(int dev_perm, int access_mode);

/* Probe / Remove functions */
int pcd_platform_driver_probe(struct platform_device *pdev);
int pcd_platform_driver_remove(struct platform_device *pdev);

//************************* ENUMS *****************************//
typedef enum
{
    PCDEVA1X,
    PCDEVB1X,
    PCDEVC1X,
    PCDEVD1X
}DeviceIds;

//************************* GLOBALS *****************************//

struct device_config 
{
    int config_item1;
    int config_item2;
};

struct device_config pcdev_cfg[] =
{
    [PCDEVA1X] = {.config_item1 = 60, .config_item2 = 21},
    [PCDEVB1X] = {.config_item1 = 50, .config_item2 = 22},
    [PCDEVC1X] = {.config_item1 = 40, .config_item2 = 23},
    [PCDEVD1X] = {.config_item1 = 30, .config_item2 = 24}
};

//this array is null terminated
struct platform_device_id pcdev_ids[] = 
{
    [0] = {.name = "pcdev-A1x",.driver_data = PCDEVA1X}, //storing number (index only). not the address for driver_data
    [1] = {.name = "pcdev-B1x",.driver_data = PCDEVB1X},
    [2] = {.name = "pcdev-C1x",.driver_data = PCDEVC1X},
    [3] = {.name = "pcdev-D1x",.driver_data = PCDEVD1X}
};

struct platform_driver pcd_platform_driver = 
{
    .probe = pcd_platform_driver_probe,
    .remove = pcd_platform_driver_remove,
    .id_table = pcdev_ids,
    .driver = {
        .name = "pseudo-char-device" //name is don't case if id_table way used.
    }
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

struct pcdrv_private_data pcdrv_data;


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

//************************* FUNCTIONS *****************************//

loff_t pcd_lseek(struct file *filep, loff_t offset, int whence)
{   
    struct pcdev_private_data *pcdev_data = (struct pcdev_private_data*)(filep->private_data);
    int max_size = pcdev_data->pdata.size;  
    loff_t temp=0;
    pr_info("lseek requested\n");
    pr_info("Current file position: %lld\n",filep->f_pos);
    switch(whence)
    {
        case SEEK_SET:
            if((offset>max_size) || (offset<0))
            {
                return -EINVAL;
            }
            filep->f_pos = offset;
            break;
        case SEEK_CUR:
            temp = filep->f_pos + offset;
            if ((temp>max_size) || (temp<0))
            {
                return -EINVAL;
            }
            filep->f_pos = temp;
            break;
        case SEEK_END:
            temp = max_size + offset;
            if ((temp>max_size) || (temp<0))
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
    struct pcdev_private_data *pcdev_data = (struct pcdev_private_data*)(filep->private_data);
    int max_size = pcdev_data->pdata.size;
    pr_info("read requested for %zu bytes\n",count);
    pr_info("Current file position: %lld\n",*f_pos);

    /*Adjust the count*/
    if ((*f_pos+count) > max_size)
    {
        count = max_size - *f_pos;
    }

    /*copy to user*/
    if (copy_to_user(buff,pcdev_data->buffer+(*f_pos),count))
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
    struct pcdev_private_data *pcdev_data = (struct pcdev_private_data*)(filep->private_data);
    int max_size = pcdev_data->pdata.size;  
    pr_info("write requested for %zu bytes\n",count);
    pr_info("Current file position: %lld\n",*f_pos);

    /*Adjust the count*/
    if ((*f_pos+count) > max_size)
    {
        count = max_size - *f_pos;
    }

    if (!count)
    {
        pr_err("No space left on the device\n");
        return -ENOMEM;
    }

    /*copy to user*/
    if (copy_from_user(pcdev_data->buffer+(*f_pos),buff,count)) 
    /*
    * Found a bug above during development. 
    * Used &pcdev_data->buffer instead of direct dereference for pointer. 
    * This caused mem overwrite. Kernel memory is so insecure. This caused segmentation fault big crash (memory leak).
    */
    {
        return -EFAULT;
    }

    /*update current file position*/
    *f_pos += count;
    pr_info("Number of bytes successfully writter: %zu\n",count);
    pr_info("Updated file position: %lld\n",*f_pos);

    /* return number of bytes successfully written */
    return count;
}

int check_permission(int dev_perm, int access_mode)
{
    if (DEV_DRV_PERM_RDWR==dev_perm)
        return PCD_DRV_SUCCESS;
    /*Ensure Read only access*/
    if ((DEV_DRV_PERM_RDONLY==dev_perm)&&((access_mode&FMODE_READ) && !(access_mode&FMODE_WRITE)))
        return PCD_DRV_SUCCESS;
    /*Ensure Write only access*/
    if ((DEV_DRV_PERM_WRONLY==dev_perm)&&(!(access_mode&FMODE_READ) && (access_mode&FMODE_WRITE)))
        return PCD_DRV_SUCCESS;
    pr_info("FAILED. DEBUG msg: access_mode: 0x%x, fmode_values: FMODE_READ: 0x%x and FMODE_WRITE: 0x%x\n",access_mode,FMODE_READ,FMODE_WRITE);
    pr_info("FAILED. DEBUG msg: local dev perm: 0x%x\n",dev_perm);
    return -EPERM;
}

int pcd_open(struct inode *inode, struct file *filep)
{
    int ret=0;
    struct pcdev_private_data *pcdev_data;
    /* find out on which device file open was attempted by userspace */
    int minor_n=MINOR(inode->i_rdev);
    pr_info("minor access: %d\n",minor_n);

    /* Get device private data struct */
    pcdev_data = container_of(inode->i_cdev,struct pcdev_private_data,cdev);

    /* Supply device private data to other methods (seek,read,write). 
    Other methods will not have access to inode. 
    That's why you can store in filep and reuse. */
    filep->private_data = (void*)pcdev_data;

    /* check permission */
    ret = check_permission(pcdev_data->pdata.perm,filep->f_mode);
    (!ret)?pr_info("open is successful\n"):pr_info("open is unsuccessful\n");
    return ret;
}

int pcd_release(struct inode *inode, struct file *filep)
{
    pr_info("release is successful\n");
    return 0;
}

int pcd_platform_driver_probe(struct platform_device *pdev)
{
    struct pcdev_private_data *dev_data;
    struct pcdev_platform_data *pdata;
    int ret=0;

    pr_info("A device is detected\n");

    /* 1. Get the platform data */
    //pdata = pdev->dev.platform_data;
    //OR
    pdata = (struct pcdev_platform_data *)dev_get_platdata(&pdev->dev); //returns void ptr. so casting is necessary
    if (!pdata)
    {
        pr_info("No platform data available\n");
        ret = -EINVAL;
        goto out;
    }

    /* 2. Dynamically allocate memory for the device private data */
    //dev_data = kzalloc(sizeof(*dev_data),GFP_KERNEL); //can use devm_kmalloc to avoid free. Refer: https://github.com/niekiran/linux-device-driver-1/blob/master/custom_drivers/004_pcd_platform_driver/pcd_platform_driver.c (line 302)
    dev_data = devm_kzalloc(&pdev->dev,sizeof(*dev_data),GFP_KERNEL); //devm usage
    if(!dev_data)
    {
        pr_info("Cannot allocate memory\n");
        ret = -ENOMEM;
        goto out;
    }
    dev_set_drvdata(&pdev->dev,dev_data);
    dev_data->pdata.serial_number=pdata->serial_number;
    dev_data->pdata.size=pdata->size;
    dev_data->pdata.perm=pdata->perm;
    pr_info("Device serial number: %s\n",dev_data->pdata.serial_number);
    pr_info("Device size: %d bytes\n",dev_data->pdata.size);
    pr_info("Device permission: 0x%x\n",dev_data->pdata.perm);

    pr_info("Config Item 1: %d",pcdev_cfg[pdev->id_entry->driver_data].config_item1);
    pr_info("Config Item 2: %d",pcdev_cfg[pdev->id_entry->driver_data].config_item2);

    /* 3. Dynamically allocate memory for device buffer using size information from the platform data */
    //dev_data->buffer = kzalloc(dev_data->pdata.size,GFP_KERNEL);
    dev_data->buffer = devm_kzalloc(&pdev->dev,dev_data->pdata.size,GFP_KERNEL); //devm usage
    if(!dev_data->buffer)
    {
        pr_info("Cannot allocate memory\n");
        ret = -ENOMEM;
        goto dev_data_free;
    }

    /* 4. Get the device number */
    dev_data->dev_num=pcdrv_data.device_num_base + pdev->id;

    /* 5. Do cdev init and cdev add */
    cdev_init(&dev_data->cdev,&pcd_fops);
    dev_data->cdev.owner=THIS_MODULE;
    ret = cdev_add(&dev_data->cdev,dev_data->dev_num,1); //add 1 device per loop
    if (ret<0)
    {
        pr_err("cdev add failed\n");
        goto buffer_free;
    }

    /* 6. Create device file for the detected platform device */
    pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd,NULL,dev_data->dev_num,NULL,"pcdev-%d",pdev->id); //error handle later
    if(IS_ERR(pcdrv_data.device_pcd))
    {
        pr_err("device creation failed\n");
        ret = PTR_ERR(pcdrv_data.device_pcd);
        goto cdev_del; //Class already created in platform_driver init (this is in probe function)
    }

    pcdrv_data.total_devices++;

    pr_info("The probe was successful\n");
    return 0;

    /* 7. Error handling */
cdev_del:
    cdev_del(&dev_data->cdev);
buffer_free:
    //kfree(dev_data->buffer);
    devm_kfree(&pdev->dev,dev_data->buffer); //devm function use. Actually it not required. if probe fails, dev resources will be cleared!
dev_data_free:
    //kfree(dev_data);
    devm_kfree(&pdev->dev,dev_data); //devm function use. Actually it not required. if probe fails, dev resources will be cleared!
out:
    pr_info("Device probe failed\n");
    return ret;
}
int pcd_platform_driver_remove(struct platform_device *pdev)
{
    struct pcdev_private_data *dev_data;
    dev_data = dev_get_drvdata(&pdev->dev);
    /* 1. Remove device that's created with device_create */
    device_destroy(pcdrv_data.class_pcd,dev_data->dev_num);
    /* 2. Remove a cdev entry from the system */
    cdev_del(&dev_data->cdev);
    /* 3. Free the memory held by the device */
    //kfree(dev_data->buffer); //N/R because devm function used in probe function
    //kfree(dev_data); //N/R because devm function used in probe function
    pcdrv_data.total_devices--;
    pr_info("A device is removed\n");
    return 0;
}

static int __init pcd_platform_driver_init(void)
{   
    int ret=0;
    pcdrv_data.total_devices=0;//Initializing devices count. Increment/Decrement will happen when new device detected/removed in probe/remove functions respectively.
    /* 1. Dynamically allocate device number for MAX_DEVICES */
    ret=alloc_chrdev_region(&pcdrv_data.device_num_base,0,MAX_DEVICES,"pcd_devices"); //it can fail. Handle error
    if (ret<0)
    {
        pr_err("alloc char dev failed\n");
        return ret;
    }

    /* 2. Create device class under /sys/class */
    #if ( LINUX_VERSION_CODE >= KERNEL_VERSION( 6, 4, 0 ) ) // Hack to support newer kernel versions (host linux is newer currently)
    pcdrv_data.class_pcd=class_create("pcd_class"); //error handle later
    #else
    pcdrv_data.class_pcd=class_create(THIS_MODULE,"pcd_class"); //error handle later
    #endif
    if(IS_ERR(pcdrv_data.class_pcd))
    {
        pr_err("class creation failed\n");
        ret = PTR_ERR(pcdrv_data.class_pcd);
        unregister_chrdev_region(pcdrv_data.device_num_base,MAX_DEVICES);
        return ret;
    }

    /* 3. Register a platform driver */
    platform_driver_register(&pcd_platform_driver); //Error handle later

    pr_info("PCD-Platform driver Module loaded\n");
    return 0;
}

static void __exit pcd_platform_driver_cleanup(void)
{
    /* 1. Unregister platform driver */
    platform_driver_unregister(&pcd_platform_driver);

    /* 2. Class destroy */
    class_destroy(pcdrv_data.class_pcd);

    /* 3. Unregister char dev region (all device numbers for MAX_DEVICES) */
    unregister_chrdev_region(pcdrv_data.device_num_base,MAX_DEVICES);

    pr_info("PCD-Platform driver Module unloaded\n");
}

module_init(pcd_platform_driver_init);
module_exit(pcd_platform_driver_cleanup);

//************************* MODULE INFO UPDATE *****************************//

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NANI_LINUX_DRIVERS");
MODULE_DESCRIPTION("Pseudo char driver multiple device support module");
MODULE_INFO(board,"Beaglebone Black REV A5");
