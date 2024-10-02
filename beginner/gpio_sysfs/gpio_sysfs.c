#include "gpio_sysfs.h"


//************************* FUNCTION DECLARATIONS *****************************//

/* Probe / Remove functions */
int gpio_sysfs_probe(struct platform_device *pdev);
int gpio_sysfs_remove(struct platform_device *pdev);

//************************* GLOBALS *****************************//


struct gpiodrv_private_data gpio_drv_data;

struct of_device_id gpio_device_match[]= 
{
    {.compatible = "org,bone-gpio-sysfs"},
    { }
};

struct platform_driver gpio_sysfs_platform_driver = 
{
    .probe = gpio_sysfs_probe,
    .remove = gpio_sysfs_remove,
    .driver = {
        .name = "bone-gpio-sysfs",
        .of_match_table = of_match_ptr(gpio_device_match)
    }
};

#if 0
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
#endif

/* Create device_attribute */
static DEVICE_ATTR_RW(direction);
static DEVICE_ATTR_RW(value);
static DEVICE_ATTR_RO(label);

static struct attribute *gpio_attrs[] = //Attribute list is null terminated
{
    &dev_attr_direction.attr,
    &dev_attr_value.attr,
    &dev_attr_label.attr,
    NULL
};

static struct attribute_group gpio_attr_group = 
{
    .attrs = gpio_attrs
};

static const struct attribute_group *gpio_attr_groups[] = //tricky. because list of attribute groups to be used for device_create_with_groups function
{
    &gpio_attr_group,
    NULL
};

//************************* FUNCTIONS *****************************//

ssize_t direction_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
    int dir=0;
    char *direction;
    dir = gpiod_get_direction(dev_data->desc);
    if(dir<0)
    {
        return dir;
    }
    /* if dir=0, show "out", else show "in" */
    direction = (dir==0)? "out":"in";
    return sprintf(buf,"%s\n",direction);
}

ssize_t direction_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
    int ret = 0;
    //strcmp(buf,"in") // don't use regular strcmp when dealing with sysfs files. use below fn
    if(sysfs_streq(buf,"in"))
    {
        ret = gpiod_direction_input(dev_data->desc);
    }
    else if (sysfs_streq(buf,"out"))
    {
        ret = gpiod_direction_output(dev_data->desc,0);
    }
    else
    {
        ret = -EINVAL;
    }
    return ret ? : count;
}

ssize_t value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
    int value;
    value = gpiod_get_value(dev_data->desc);
    return sprintf(buf,"%d\n",value);
}

ssize_t value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
    int ret;
    long value;

    ret = kstrtol(buf,0,&value);
    if (ret)
    {
        return ret;
    }
    gpiod_set_value(dev_data->desc,value);
    return count;
}

ssize_t label_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct gpiodev_private_data *dev_data = dev_get_drvdata(dev);
    return sprintf(buf,"%s\n",dev_data->label);
}

int gpio_sysfs_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    const char *name; //of_property_read_string accepts const char* only
    /* parent device node */
    struct device_node *parent = pdev->dev.of_node;
    struct device_node *child = NULL;
    struct gpiodev_private_data *dev_data;
    int i=0,ret=0;

    gpio_drv_data.total_devices = of_get_child_count(parent);
    if(!gpio_drv_data.total_devices)
    {
        dev_err(dev,"No devices found\n");
        return -EINVAL;
    }
    dev_info(dev,"Total devies found = %d\n",gpio_drv_data.total_devices);
    gpio_drv_data.dev = devm_kzalloc(dev,sizeof(struct device *)*gpio_drv_data.total_devices, GFP_KERNEL);
    for_each_available_child_of_node(parent,child)
    {
        dev_data = devm_kzalloc(dev,sizeof(*dev_data),GFP_KERNEL);
        if(!dev_data){
            dev_err(dev,"Cannot allocate memory\n");
            return -ENOMEM;
        }

        /*query for label (read string property)*/
        if(of_property_read_string(child,"label",&name)) //name requires double ptr, so it is &
        {
            dev_warn(dev,"missing lavel information\n");
            snprintf(dev_data->label,sizeof(dev_data->label),"unkngpio%d",i);
        }
        else
        {
            strcpy(dev_data->label,name);
            dev_info(dev,"GPIO label = %s\n",dev_data->label);
        }

        /* Acquire GPIO */
        dev_data->desc = devm_fwnode_get_gpiod_from_child(dev,\
						"bone", &child->fwnode,GPIOD_ASIS,dev_data->label);
        if(IS_ERR(dev_data->desc))
        {
            ret=PTR_ERR(dev_data->desc);
            if(ret == -ENOENT)
            {
                dev_err(dev,"No GPIO has been assigned to the requested function and/or index\n");
            }
            return ret;
        }

        /* set gpio direction to output - check consumer.h/gpiolib.c */
        ret = gpiod_direction_output(dev_data->desc,0); //active_low, so send 0 to give high signal
        if (ret){
            dev_err(dev,"GPIO direction set failed\n");
            return ret;
        }

        /* Create devices under /sys/class/bone_gpios */
        gpio_drv_data.dev[i] = device_create_with_groups(gpio_drv_data.class_gpio,dev,0,dev_data,gpio_attr_groups,dev_data->label); //0 for devt to not create device under '/dev' directory
        if(IS_ERR(gpio_drv_data.dev[i]))
        {
            dev_err(dev, "Error in device create\n");
            return PTR_ERR(gpio_drv_data.dev[i]);
        }
        i++;
    }
    return 0;
}
int gpio_sysfs_remove(struct platform_device *pdev)
{   
    int i;
    struct device *dev = &pdev->dev;
    dev_info(dev,"remove called\n");
    for(i=0;i<gpio_drv_data.total_devices;i++)
    {
        device_unregister(gpio_drv_data.dev[i]);
    }
    return 0;
}

/*********************** Driver init/exit functions ***********************/

static int __init gpio_sysfs_init(void)
{   
    int ret=0;

    /* 1. Create device class under /sys/class */
    #if ( LINUX_VERSION_CODE >= KERNEL_VERSION( 6, 4, 0 ) ) // Hack to support newer kernel versions (host linux is newer currently)
    gpio_drv_data.class_gpio=class_create("bone_gpios"); //error handle later
    #else
    gpio_drv_data.class_gpio=class_create(THIS_MODULE,"bone_gpios"); //error handle later
    #endif
    if(IS_ERR(gpio_drv_data.class_gpio))
    {
        pr_err("class creation failed\n");
        ret = PTR_ERR(gpio_drv_data.class_gpio);
        return ret;
    }

    /* 2. Register a platform driver */
    platform_driver_register(&gpio_sysfs_platform_driver); //Error handle later

    pr_info("GPIO platform driver Module loaded\n");
    return 0;
}

static void __exit gpio_sysfs_cleanup(void)
{
    /* 1. Unregister platform driver */
    platform_driver_unregister(&gpio_sysfs_platform_driver);

    /* 2. Class destroy */
    class_destroy(gpio_drv_data.class_gpio);

    pr_info("GPIO platform driver Module unloaded\n");
}

module_init(gpio_sysfs_init);
module_exit(gpio_sysfs_cleanup);

//************************* MODULE INFO UPDATE *****************************//

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NANI_LINUX_DRIVERS");
MODULE_DESCRIPTION("Pseudo char driver multiple device support module dt snd sysfs");
MODULE_INFO(board,"Beaglebone Black REV A5");
