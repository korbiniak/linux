#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <uapi/misc/fcalc.h>

#define FCALC_MAJOR	   140
#define FCALC_MAX_MINORS   10

struct fcalc_data {
	struct cdev cdev;
	long buffer;
	enum fcalc_calc_type calc_type;
};

struct fcalc_data devs[FCALC_MAX_MINORS];

static const struct of_device_id fcalc_dt_ids[] = {
	{ .compatible = "uwr,fcalc" },
	{}
};

static int fcalc_open(struct inode *inode, struct file *file) {
	struct fcalc_data *data =
		container_of(inode->i_cdev, struct fcalc_data, cdev);
	// unsigned int minor = MINOR(inode->i_cdev->dev);
	file->private_data = data;

	data->buffer = 0;
	data->calc_type = FCALC_ADDITION;

	return 0;
}

/* Internal function for conversion from long to str 
 * Assumes that buf_size is of suitable size.
 */
static ssize_t ltostr(long value, char *buf) {
	return sprintf(buf, "%ld", value);	
}

static ssize_t fcalc_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
	char data[30]; 
	ssize_t datalen;
	struct fcalc_data *dev_data = (struct fcalc_data *)file->private_data;		

	if (!ltostr(dev_data->buffer, data)) {
		return 0;
	}

	datalen = strlen(data);

	if (count > datalen) {
		count = datalen;
	}

	if (copy_to_user(buf, data, count)) {
		return -EFAULT;
	}

	return count;
}

static void modify_buffer(struct fcalc_data *data, long value) {
	switch(data->calc_type) {
	case FCALC_ADDITION:
		data->buffer += value;
		break;	
	case FCALC_MULTIPLICATION:
		data->buffer *= value;
		break;
	default:
		printk(KERN_ERR"WTF? Default modify_buffer, this \
				shouldn't ever happen.");
		break;
	}
}

static ssize_t fcalc_write(struct file *file, const char __user *buf, size_t count, loff_t *offset) {
	struct fcalc_data *data = (struct fcalc_data*)file->private_data;
	long value;
	
	if (kstrtol_from_user(buf, count, 10, &value)) {
		return -EFAULT;
	}
	
	modify_buffer(data, value);

	return count;
}

static int fcalc_release(struct inode *inode, struct file *file) {
	return 0;
}

/* Checks if type given in FCALC_IOCTL_CALC_TYPE is a correct calc type */
static bool check_ioctl_calc_type(int x) {
	const int E[] = {FCALC_ADDITION, FCALC_MULTIPLICATION};
	int i;
	
	for (i = 0; i < sizeof(E) / sizeof(*E); i++) {
		if (E[i] == x)
			return true;
	}

	return false;
}

static long fcalc_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	struct fcalc_data *data = (struct fcalc_data *)file->private_data;
	union fcalc_ioctl_data data_in;

	switch (cmd) {
	case FCALC_IOCTL_RESET:
		data->buffer = 0;
		break;
	case FCALC_IOCTL_GET_CALC_TYPE:
		if (copy_to_user((union fcalc_ioctl_data *)arg, &data->calc_type,
				sizeof(data->calc_type))) {
			return -EFAULT;
		}
		break;
	case FCALC_IOCTL_CALC_TYPE:
		if (copy_from_user(&data_in, (union fcalc_ioctl_data *)arg,
					sizeof(union fcalc_ioctl_data))) {
			return -EFAULT;
		}
		if (!check_ioctl_calc_type(data_in.calc_type)) {
			return -EINVAL;
		}
		printk(KERN_ERR "Calc type successful!");
		data->calc_type = data_in.calc_type;
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

static int fcalc_probe(struct platform_device *pdev) {
	int i, err;

	printk(KERN_ERR"Probing my awesome driver!! :DDDDD\n");
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
	}

	return 0;
}

static int fcalc_remove(struct platform_device *pdev) {
	int i;

	printk(KERN_ERR":(\n");
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
	printk(KERN_ERR"Hello World!\n");
	return platform_driver_register(&fcalc);
}

static void __exit fcalc_exit(void)
{
	pr_info("Goodbye!\n");
	platform_driver_unregister(&fcalc);
}

module_init(fcalc_init);
module_exit(fcalc_exit);
MODULE_LICENSE("GPL");
