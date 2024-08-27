//*************************Pre-processor macros*****************************//

/* Read write permission macros */
#define DEV_DRV_PERM_RDONLY 0x1
#define DEV_DRV_PERM_WRONLY 0x10
#define DEV_DRV_PERM_RDWR   0x11

#define pr_fmt(fmt) "%s : "fmt,__func__ //WARNING EXPECTED. redefined pr_fmt. and it works because kernel builds these cases for pr_* cases

//*************************Struct declarations*****************************//
struct pcdev_platform_data
{
    int size;
    int perm;
    const char *serial_number;
};