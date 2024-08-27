#include <linux/module.h>
#include <linux/platform_device.h>

#include "platform.h"

//************************* Function prototypes *****************************//
void pcdev_release(struct device *dev);

//************************* GLOBALS *****************************//

/* 1. Create two platform data*/
struct pcdev_platform_data pcdev_data[] = {
    [0] = {.size = 512,.perm = DEV_DRV_PERM_RDWR,.serial_number = "PCDEVABC111"},
    [1] = {.size = 1024,.perm = DEV_DRV_PERM_RDWR,.serial_number = "PCDEVXYZ222"},
    [2] = {.size = 128,.perm = DEV_DRV_PERM_RDONLY,.serial_number = "PCDEVDEF333"},
    [3] = {.size = 32,.perm = DEV_DRV_PERM_WRONLY,.serial_number = "PCDEVIJK444"}
};

/* 2. Create two platform devices */
struct platform_device platform_pcdev_1 = {
    .name = "pcdev-A1x",
    .id = 0,
    .dev = {
        .platform_data = &pcdev_data[0],
        .release = pcdev_release
    }
};

struct platform_device platform_pcdev_2 = {
    .name = "pcdev-B1x",
    .id = 1,
    .dev = {
        .platform_data = &pcdev_data[1],
        .release = pcdev_release
    }
};

struct platform_device platform_pcdev_3 = {
    .name = "pcdev-C1x",
    .id = 2,
    .dev = {
        .platform_data = &pcdev_data[2],
        .release = pcdev_release
    }
};

struct platform_device platform_pcdev_4 = {
    .name = "pcdev-D1x",
    .id = 3,
    .dev = {
        .platform_data = &pcdev_data[3],
        .release = pcdev_release
    }
};

struct platform_device *platform_devices[]=
{
    &platform_pcdev_1,
    &platform_pcdev_2,
    &platform_pcdev_3,
    &platform_pcdev_4
};

//************************* FUNCTIONS *****************************//

void pcdev_release(struct device *dev)
{
    pr_info("Device release\n");
}

static int __init pcdev_platform_init(void) //init is int type.
{
    /*Register platform device*/
    //platform_device_register(&platform_pcdev_1); //Adding individually. Optimized way used below
    //platform_device_register(&platform_pcdev_2);
    //platform_device_register(&platform_pcdev_3);
    //platform_device_register(&platform_pcdev_4);
    platform_add_devices(platform_devices,ARRAY_SIZE(platform_devices)); //check how ARRAY_SIZE works and remember!
    pr_info("Device setup module loaded \n");
    return 0;
}

static void __exit pcdev_platform_exit(void) //exit is void type.
{
    /*Unregister platform device*/
    platform_device_unregister(&platform_pcdev_1);
    platform_device_unregister(&platform_pcdev_2);
    platform_device_unregister(&platform_pcdev_3);
    platform_device_unregister(&platform_pcdev_4);
    pr_info("Device setup module unloaded \n");
}


module_init(pcdev_platform_init);
module_exit(pcdev_platform_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NANI_LINUX_DRIVERS");
MODULE_DESCRIPTION("Module to register platform devices");
MODULE_INFO(board,"Beaglebone Black REV A5");

