#include "pcd_platform_driver_dt_sysfs.h"

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
    pr_info("Number of bytes successfully written: %zu\n",count);
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