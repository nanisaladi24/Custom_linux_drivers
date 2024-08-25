#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h> //for copy_to_user, copy_from_user
#include <linux/kdev_t.h>
#include <linux/version.h>
#include <linux/err.h>

//*************************Pre-processor macros*****************************//
#define MEM_SIZE_MAX_PCDEV1 1024
#define MEM_SIZE_MAX_PCDEV2 512
#define MEM_SIZE_MAX_PCDEV3 1024
#define MEM_SIZE_MAX_PCDEV4 512

#define NO_OF_DEVICES 4

//#define MINOR_COUNT 1

#define pr_fmt(fmt) "%s : "fmt,__func__ //WARNING EXPECTED. redefined pr_fmt. and it works because kernel builds these cases for pr_* cases
//************************* FUNCTION DECLARATIONS *****************************//

loff_t pcd_lseek(struct file *filep, loff_t offset, int whence);
ssize_t pcd_read(struct file *filep, char __user *buff, size_t count, loff_t *f_pos);
ssize_t pcd_write(struct file *filep, const char __user *buff, size_t count, loff_t *f_pos);
int pcd_open(struct inode *inode, struct file *filep);
int pcd_release(struct inode *inode, struct file *filep);
int check_permission(void);

//************************* GLOBALS *****************************//
/* Pseudo device' memory buffers */
char device_buffer_pcdev1[MEM_SIZE_MAX_PCDEV1];
char device_buffer_pcdev2[MEM_SIZE_MAX_PCDEV2];
char device_buffer_pcdev3[MEM_SIZE_MAX_PCDEV3];
char device_buffer_pcdev4[MEM_SIZE_MAX_PCDEV4];

/* Device private data struct */
struct pcdev_private_data
{
    char *buffer;
    unsigned size;
    const char *serial_number;
    int perm; //permission. can use u8,u16 etc
    struct cdev cdev;
};

/* Driver private data struct */
struct pcdrv_private_data
{
    int total_devices;
    /* holds device number of base (first device) of all allocated devices */
    dev_t device_number;
    /* Device class/device structs */
    struct class *class_pcd;
    struct device *device_pcd[NO_OF_DEVICES];
    struct pcdev_private_data pcdev_data[NO_OF_DEVICES];
};

struct pcdrv_private_data pcdrv_data =
{
    .total_devices = NO_OF_DEVICES,
    .pcdev_data = {
        [0] = {
            .buffer = device_buffer_pcdev1,
            .size = MEM_SIZE_MAX_PCDEV1,
            .serial_number = "PCDEV1XYZ123",
            .perm = 0x1, /*RD only*/
        },
        [1] = {
            .buffer = device_buffer_pcdev2,
            .size = MEM_SIZE_MAX_PCDEV2,
            .serial_number = "PCDEV2XYZ123",
            .perm = 0x10, /*WR only*/
        },
        [2] = {
            .buffer = device_buffer_pcdev3,
            .size = MEM_SIZE_MAX_PCDEV3,
            .serial_number = "PCDEV3XYZ123",
            .perm = 0x11, /*RDWR*/
        },
        [3] = {
            .buffer = device_buffer_pcdev4,
            .size = MEM_SIZE_MAX_PCDEV4,
            .serial_number = "PCDEV4XYZ123",
            .perm = 0x11, /*RDWR*/
        }
    }
};

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
    #if 0
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
    #endif
    return 0;
}

ssize_t pcd_read(struct file *filep, char __user *buff, size_t count, loff_t *f_pos)
{
    struct pcdev_private_data *pcdev_data = (struct pcdev_private_data*)(filep->private_data);
    int max_size = pcdev_data->size;
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
    #if 0
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
    pr_info("Number of bytes successfully writter: %zu\n",count);
    pr_info("Updated file position: %lld\n",*f_pos);

    /* return number of bytes successfully written */
    return count;
    #endif
    return -ENOMEM;
}

int check_permission(void)
{
    return 0;
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
    ret = check_permission();

    (!ret)?pr_info("open is successful\n"):pr_info("open is unsuccessful\n");
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
    int i=0;
    /* 1. Dynamically allocate device number */
    ret=alloc_chrdev_region(&pcdrv_data.device_number,0,NO_OF_DEVICES,"pcd_devices"); //it can fail. Handle error
    if (ret<0)
    {
        pr_err("alloc char dev failed\n");
        goto out;
    }
    /* Create device class under /sys/class/ */
    #if ( LINUX_VERSION_CODE >= KERNEL_VERSION( 6, 4, 0 ) ) // Hack to support newer kernel versions (host linux is newer currently)
    pcdrv_data.class_pcd=class_create("pcd_class"); //error handle later
    #else
    pcdrv_data.class_pcd=class_create(THIS_MODULE,"pcd_class"); //error handle later
    #endif
    if(IS_ERR(pcdrv_data.class_pcd))
    {
        pr_err("class creation failed\n");
        ret = PTR_ERR(pcdrv_data.class_pcd);
        goto ureg_char_dev;
    }
    for (i=0;i<NO_OF_DEVICES;i++)
    {
        pr_info("Device number <major>:<minor> = %d:%d\n",MAJOR(pcdrv_data.device_number+i),MINOR(pcdrv_data.device_number+i));

        /* 2. Initialize cdev struct with fops */
        cdev_init(&pcdrv_data.pcdev_data[i].cdev,&pcd_fops);

        /* 3. Register cdev struct with VFS*/
        pcdrv_data.pcdev_data[i].cdev.owner=THIS_MODULE;
        ret = cdev_add(&pcdrv_data.pcdev_data[i].cdev,pcdrv_data.device_number+i,1); //add 1 device per loop
        if (ret<0)
        {
            pr_err("cdev add failed\n");
            goto cdev_del;
        }


        /* 4. Populate sysfs with device information */
        pcdrv_data.device_pcd[i]=device_create(pcdrv_data.class_pcd,NULL,pcdrv_data.device_number+i,NULL,"pcdev-%d",i); //error handle later
        if(IS_ERR(pcdrv_data.device_pcd[i]))
        {
            pr_err("device creation failed\n");
            ret = PTR_ERR(pcdrv_data.device_pcd[i]);
            goto class_del;
        }
    }

    pr_info("PCD Module init successful\n");

    return 0;

cdev_del:
class_del:
    for(;i>=0;i--) //reverse loop from current i
    {
        device_destroy(pcdrv_data.class_pcd,pcdrv_data.device_number+i); // can be called for invalid case also. it is safe
        cdev_del(&pcdrv_data.pcdev_data[i].cdev);
    }
    class_destroy(pcdrv_data.class_pcd);
ureg_char_dev:
    unregister_chrdev_region(pcdrv_data.device_number,NO_OF_DEVICES);
out:
    pr_info("PCD Module init failed\n");
    return ret;
}

static void __exit pcd_driver_cleanup(void)
{
    int i=0;
    for(i=0;i<NO_OF_DEVICES;i++)
    {
        device_destroy(pcdrv_data.class_pcd,pcdrv_data.device_number+i);
        cdev_del(&pcdrv_data.pcdev_data[i].cdev);
    }
    class_destroy(pcdrv_data.class_pcd);
    unregister_chrdev_region(pcdrv_data.device_number,NO_OF_DEVICES);
    pr_info("PCD Module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

//************************* MODULE INFO UPDATE *****************************//

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NANI_LINUX_DRIVERS");
MODULE_DESCRIPTION("Pseudo char driver multiple device support module");
MODULE_INFO(board,"Beaglebone Black REV A5");