#include "pcd_platform_driver_dt_sysfs.h"

//************************* FUNCTION DECLARATIONS *****************************//

/* Probe / Remove functions */
int pcd_platform_driver_probe(struct platform_device *pdev);
int pcd_platform_driver_remove(struct platform_device *pdev);

//************************* GLOBALS *****************************//

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
    {.name = "pcdev-A1x",.driver_data = PCDEVA1X}, //storing number (index only). not the address for driver_data
    {.name = "pcdev-B1x",.driver_data = PCDEVB1X},
    {.name = "pcdev-C1x",.driver_data = PCDEVC1X},
    {.name = "pcdev-D1x",.driver_data = PCDEVD1X},
    {}
};

struct of_device_id org_pcdev_dt_match[] = 
{
    {.compatible = "pcdev-A1x",.data = (void*)PCDEVA1X},  
    {.compatible = "pcdev-B1x",.data = (void*)PCDEVB1X},
    {.compatible = "pcdev-C1x",.data = (void*)PCDEVC1X},
    {.compatible = "pcdev-D1x",.data = (void*)PCDEVD1X},
    {}
};

struct platform_driver pcd_platform_driver = 
{
    .probe = pcd_platform_driver_probe,
    .remove = pcd_platform_driver_remove,
    .id_table = pcdev_ids,
    .driver = {
        .name = "pseudo-char-device", //name is don't case if id_table way used.
        .of_match_table = of_match_ptr(org_pcdev_dt_match) //device tree support. This macro call ensures assigning null/actual ptr if config_of is disabled/enabled. 
    }
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

/* Create two  variables of struct device_attribute */
static DEVICE_ATTR(max_size,S_IRUGO|S_IWUSR,show_max_size,store_max_size);
static DEVICE_ATTR(serial_num,S_IRUGO,show_serial_num,NULL);

//************************* FUNCTIONS *****************************//

ssize_t show_max_size(struct device *dev, struct device_attribute *attr, char *buf)
{
    /* get access to the device private data */
    struct pcdev_private_data *dev_data = dev_get_drvdata(dev->parent);
    return sprintf(buf,"%d\n",dev_data->pdata.size);
}

ssize_t show_serial_num(struct device *dev, struct device_attribute *attr, char *buf)
{
    /* get access to the device private data */
    struct pcdev_private_data *dev_data = dev_get_drvdata(dev->parent);
    return sprintf(buf,"%s\n",dev_data->pdata.serial_number);
}

ssize_t store_max_size(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    /* get access to the device private data */
    struct pcdev_private_data *dev_data = dev_get_drvdata(dev->parent);
    long result;
    int ret;
    
    if(ret = kstrtol(buf,10,&result))
        return ret;
    dev_data->pdata.size = result;
    dev_data->buffer = krealloc(dev_data->buffer,dev_data->pdata.size,GFP_KERNEL);
    dev_info(dev,"Re-allocated memory for the device %d\n",result);
    return count;
}

static int pcd_sysfs_create_files(struct device *pcd_dev)
{
    int ret = 0;
    if(ret = sysfs_create_file(&pcd_dev->kobj,&dev_attr_max_size.attr))
    {
        return ret;
    }
    return sysfs_create_file(&pcd_dev->kobj,&dev_attr_serial_num.attr);
}

/* local helper function to get platform data */
static struct pcdev_platform_data* pcdev_get_platdata_from_dt(struct device *dev)
{
    struct device_node *dev_node = dev->of_node;
    struct pcdev_platform_data *pdata;

    if (!dev_node)
    {
        /* This probe didn't happen because of device tree */
        return NULL;
    }

    pdata = devm_kzalloc(dev,sizeof(*pdata),GFP_KERNEL);
    if(!pdata)
    {
        dev_info(dev,"Cannot allocate memory\n");
        return ERR_PTR(-ENOMEM);
    }
    /* extract device serial number, permission, size etc */
    if(of_property_read_string(dev_node,"org,device-serial-num",&pdata->serial_number)){
        dev_info(dev,"Missing serial number property\n");
        return ERR_PTR(-EINVAL);
    }
    if(of_property_read_u32(dev_node,"org,size",&pdata->size)){
        dev_info(dev,"Missing size property\n");
        return ERR_PTR(-EINVAL);
    }
    if(of_property_read_u32(dev_node,"org,perm",&pdata-> perm)){
        dev_info(dev,"Missing permission property\n");
        return ERR_PTR(-EINVAL);
    }
    return pdata;
}

int pcd_platform_driver_probe(struct platform_device *pdev)
{
    struct pcdev_private_data *dev_data;
    struct pcdev_platform_data *pdata;
    int ret=0;
    int driver_data;
    const struct of_device_id* match; 
    struct device *dev = &pdev->dev;

    dev_info(dev,"A device is detected\n");

    /* 1. Get the platform device data */
    /* Match will be NULL if linux doesn't support CONFIG_OF */
    match = of_match_device(pdev->dev.driver->of_match_table,dev);
    if(match)
    {
        pdata = pcdev_get_platdata_from_dt(dev); //from device tree
        if(IS_ERR(pdata))
            return PTR_ERR(pdata); //extracts error from ptr 
        //driver_data=(int)of_device_get_match_data(dev);
        //OR
        driver_data=(int)match->data;
    }
    else
    {
        pdata = (struct pcdev_platform_data *)dev_get_platdata(dev); //returns void ptr. so casting is necessary
        driver_data = pdev->id_entry->driver_data;
    }
    if (!pdata)
    {
        dev_info(dev,"No platform data available\n");
        return -EINVAL;
    }

    /* 2. Dynamically allocate memory for the device private data */
    //dev_data = kzalloc(sizeof(*dev_data),GFP_KERNEL); //can use devm_kmalloc to avoid free. Refer: https://github.com/niekiran/linux-device-driver-1/blob/master/custom_drivers/004_pcd_platform_driver/pcd_platform_driver.c (line 302)
    dev_data = devm_kzalloc(dev,sizeof(*dev_data),GFP_KERNEL); //devm usage
    if(!dev_data)
    {
        dev_info(dev,"Cannot allocate memory\n");
        ret = -ENOMEM;
        goto out;
    }
    dev_set_drvdata(&pdev->dev,dev_data);
    dev_data->pdata.serial_number=pdata->serial_number;
    dev_data->pdata.size=pdata->size;
    dev_data->pdata.perm=pdata->perm;
    dev_info(dev,"Device serial number: %s\n",dev_data->pdata.serial_number);
    dev_info(dev,"Device size: %d bytes\n",dev_data->pdata.size);
    dev_info(dev,"Device permission: 0x%x\n",dev_data->pdata.perm);

    dev_info(dev,"Config Item 1: %d",pcdev_cfg[driver_data].config_item1);
    dev_info(dev,"Config Item 2: %d",pcdev_cfg[driver_data].config_item2);

    /* 3. Dynamically allocate memory for device buffer using size information from the platform data */
    //dev_data->buffer = kzalloc(dev_data->pdata.size,GFP_KERNEL);
    dev_data->buffer = devm_kzalloc(dev,dev_data->pdata.size,GFP_KERNEL); //devm usage
    if(!dev_data->buffer)
    {
        dev_info(dev,"Cannot allocate memory\n");
        ret = -ENOMEM;
        goto dev_data_free;
    }

    /* 4. Get the device number */
    dev_data->dev_num=pcdrv_data.device_num_base + pcdrv_data.total_devices;

    /* 5. Do cdev init and cdev add */
    cdev_init(&dev_data->cdev,&pcd_fops);
    dev_data->cdev.owner=THIS_MODULE;
    ret = cdev_add(&dev_data->cdev,dev_data->dev_num,1); //add 1 device per loop
    if (ret<0)
    {
        dev_err(dev,"cdev add failed\n");
        goto buffer_free;
    }

    /* 6. Create device file for the detected platform device */
    pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd,dev,dev_data->dev_num,NULL,"pcdev-%d",pcdrv_data.total_devices); //error handle later
    if(IS_ERR(pcdrv_data.device_pcd))
    {
        dev_err(dev,"device creation failed\n");
        ret = PTR_ERR(pcdrv_data.device_pcd);
        goto cdev_del; //Class already created in platform_driver init (this is in probe function)
    }

    pcdrv_data.total_devices++;

    ret = pcd_sysfs_create_files(pcdrv_data.device_pcd);
    if (ret < 0)
    {
        device_destroy(pcdrv_data.class_pcd,dev_data->dev_num);
        return ret;
    }

    dev_info(dev,"The probe was successful\n");
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
    dev_info(dev,"Device probe failed\n");
    return ret;
}
int pcd_platform_driver_remove(struct platform_device *pdev)
{
    struct pcdev_private_data *dev_data;
    struct device *dev = &pdev->dev;
    dev_data = dev_get_drvdata(dev);
    /* 1. Remove device that's created with device_create */
    device_destroy(pcdrv_data.class_pcd,dev_data->dev_num);
    /* 2. Remove a cdev entry from the system */
    cdev_del(&dev_data->cdev);
    /* 3. Free the memory held by the device */
    //kfree(dev_data->buffer); //N/R because devm function used in probe function
    //kfree(dev_data); //N/R because devm function used in probe function
    pcdrv_data.total_devices--;
    dev_info(dev,"A device is removed\n");
    return 0;
}

/*********************** Driver init/exit functions ***********************/

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
MODULE_DESCRIPTION("Pseudo char driver multiple device support module dt snd sysfs");
MODULE_INFO(board,"Beaglebone Black REV A5");
