#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/mmio.h>
#include <uapi/misc/fcalc.h>

#define FCALC_MAJOR	   140
#define FCALC_MAX_MINORS   10

/*****************************************************************************/
/* fcalc_periph.py device offsets ********************************************/

#define STATUS_REG_OFFSET		0x00
#define OPERATION_REG_OFFSET		0x04
#define DAT0_REG_OFFSET			0x08
#define DAT1_REG_OFFSET			0x0c
#define RESULT_REG_OFFSET		0x10
 
#define OPERATION_ADD			(1 << 0)
#define OPERATION_SUB			(1 << 1)
#define OPERATION_MULT			(1 << 2)
#define OPERATION_DIV			(1 << 3)

#define STATUS_OK			0	
#define STATUS_INVALID_OPERATION	(1 << 0)
#define STATUS_DIV_BY_ZERO		(1 << 1)

/*****************************************************************************/
/* Private data - one instance per minor *************************************/

struct fcalc_data {
	struct cdev cdev;
	void *mem_base;
	bool write_to_dat0;
};

struct fcalc_data devs[FCALC_MAX_MINORS];

static const struct of_device_id fcalc_dt_ids[] = {
	{ .compatible = "uwr,fcalc" },
	{}
};

/*****************************************************************************/
/* Auxilary functions ********************************************************/

static inline void write_device_register(u32 value, void __iomem *addr) {
	writel((u32 __force)cpu_to_le32(value), addr);
}

static inline u32 read_device_register(void __iomem *addr) {
	return le32_to_cpu((__le32 __force)readl(addr));
}

static void set_device_op(enum fcalc_op_type op_type, void __iomem *base) {
	write_device_register((1 << (int)op_type), base + OPERATION_REG_OFFSET);
}

static void set_dat0_reg(u32 value, void __iomem *base) {
	write_device_register(value, base+DAT0_REG_OFFSET);
}

static void set_dat1_reg(u32 value, void __iomem *base) {
	write_device_register(value, base+DAT1_REG_OFFSET);
}

static void clear_status_reg(void __iomem *base) {
	write_device_register(STATUS_INVALID_OPERATION | STATUS_DIV_BY_ZERO,
		       base + STATUS_REG_OFFSET);
}

static u32 get_result_reg(void __iomem *base) {
	return read_device_register(base + RESULT_REG_OFFSET);
}

static u32 get_status_reg(void __iomem *base) {
	return read_device_register(base + STATUS_REG_OFFSET);
}

static void reset_device(struct fcalc_data *data) {
	set_dat0_reg(0, data->mem_base);
	set_dat1_reg(0, data->mem_base);
	clear_status_reg(data->mem_base);
	data->write_to_dat0 = true;
}

/*****************************************************************************/
/* File operations ***********************************************************/

static int fcalc_open(struct inode *inode, struct file *file) {
	struct fcalc_data *data =
		container_of(inode->i_cdev, struct fcalc_data, cdev);
	file->private_data = data;

	return 0;
}

static ssize_t fcalc_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
	char data[30]; 
	ssize_t datalen;
	struct fcalc_data *dev_data = (struct fcalc_data *)file->private_data;		
	
	u32 result = get_result_reg(dev_data->mem_base);
	sprintf(data, "%ld", (long)result);
	datalen = strlen(data);

	if (count > datalen) {
		count = datalen;
	}

	if (copy_to_user(buf, data, count)) {
		return -EFAULT;
	}

	return count;
}

static ssize_t fcalc_write(struct file *file, const char __user *buf, size_t count, loff_t *offset) {
	struct fcalc_data *data = (struct fcalc_data*)file->private_data;
	long value;
	
	if (kstrtol_from_user(buf, count, 10, &value)) {
		return -EFAULT;
	}
	
	if (data->write_to_dat0) {
		set_dat0_reg(value, data->mem_base);
		data->write_to_dat0 = false;
	}
	else {
		set_dat1_reg(value, data->mem_base);
		data->write_to_dat0 = true;
	}

	return count;
}

static int fcalc_release(struct inode *inode, struct file *file) {
	return 0;
}

static long fcalc_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	struct fcalc_data *data = (struct fcalc_data *)file->private_data;
	enum fcalc_status status;
	enum fcalc_op_type op_type;

	switch (cmd) {
	case FCALC_IOCTL_RESET:
		reset_device(data);
		break;
	case FCALC_IOCTL_GET_STATUS:
		status = get_status_reg(data->mem_base);
		if (copy_to_user((enum fcalc_status *)arg, &status,
				sizeof(enum fcalc_status))) {
			return -EFAULT;
		}
		break;
	case FCALC_IOCTL_SET_OP_TYPE:
		if (copy_from_user(&op_type, (enum fcalc_op_type *)arg,
					sizeof(enum fcalc_op_type))) {
			return -EFAULT;
		}

		/* Set the operation only if the device is in OK state */
		if (get_status_reg(data->mem_base) == STATUS_OK) {
			set_device_op(op_type, data->mem_base);
		}
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations fcalc_fops = {
	.owner = THIS_MODULE,
	.read = fcalc_read,
	.open = fcalc_open,
	.write = fcalc_write,
	.release = fcalc_release,
	.unlocked_ioctl = fcalc_ioctl,
};

/*****************************************************************************/
/* Driver functions **********************************************************/

static int fcalc_probe(struct platform_device *pdev) {
	int i, err;
	struct resource *res;
	void *base;

	printk(KERN_ERR"Probing fcalc driver.\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	err = register_chrdev_region(MKDEV(FCALC_MAJOR, 0), FCALC_MAX_MINORS,
								"fcalc");
	if (err != 0) {
		printk(KERN_ERR"Error %d\n", err);
		return err;
	}

	for (i = 0; i < FCALC_MAX_MINORS; i++) {
		cdev_init(&devs[i].cdev, &fcalc_fops);
		err = cdev_add(&devs[i].cdev, MKDEV(FCALC_MAJOR, i), 1);
		if (err) {
			printk(KERN_ERR "Error %d adding driver %d\n", err, i);
		}
		devs[i].mem_base = base;
		devs[i].write_to_dat0 = true;
	}

	return 0;
}

static int fcalc_remove(struct platform_device *pdev) {
	int i;

	printk(KERN_ERR"Removing fcalc driver.\n");
	for (i = 0; i < FCALC_MAX_MINORS; i++) {
		cdev_del(&devs[i].cdev);
	}
	unregister_chrdev_region(MKDEV(FCALC_MAJOR, 0), FCALC_MAX_MINORS);

	return 0;
}

static struct platform_driver fcalc = {
	.driver = {
		.name = "fcalc",
		.of_match_table = fcalc_dt_ids,
	},
	.probe = fcalc_probe,
	.remove = fcalc_remove,
};

static int __init fcalc_init(void)
{
	printk(KERN_ERR"Initializing fcalc driver.\n");
	return platform_driver_register(&fcalc);
}

static void __exit fcalc_exit(void)
{
	pr_info("Exiting fcalc driver.\n");
	platform_driver_unregister(&fcalc);
}

module_init(fcalc_init);
module_exit(fcalc_exit);
MODULE_LICENSE("GPL");
