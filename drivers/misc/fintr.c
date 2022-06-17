#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/mmio.h>
#include <uapi/misc/fintr.h>

#define FINTR_MAJOR		141

#define GPIO_STATE		0x0
#define INTERRUPT_STATUS	0x0c
#define INTERRUPT_PENDING_REG	0x10
#define INTERRUPT_ENABLE_REG	0x14

/*****************************************************************************/
/* Private data - one instance per minor *************************************/

typedef struct fintr_data {
	struct cdev cdev;
	void *mem_base;
	spinlock_t cnt_lock;
	struct completion new_press;
	uint32_t counter;
} fintr_data_t;

/* Only one minor this time */
fintr_data_t dev[1];

/*****************************************************************************/
/* Auxilary functions ********************************************************/

inline static void write_to_register(uint32_t value, void *base, int reg_off) {
	writel((u32 __force)cpu_to_le32(value), base + reg_off);
}

inline static void enable_interrupts(fintr_data_t *data) {
	write_to_register(1, data->mem_base, INTERRUPT_ENABLE_REG);
}

inline static void clear_pending_interrupts(fintr_data_t *data) {
	write_to_register(1, data->mem_base, INTERRUPT_PENDING_REG);
}

/*****************************************************************************/
/* File operations ***********************************************************/

static int fintr_open(struct inode *inode, struct file *file) {
	/* Actually there is only one static fintr_data_t, but this is
	 * more generic. */
	fintr_data_t *data =
		container_of(inode->i_cdev, fintr_data_t, cdev);
	file->private_data = data;

	return 0;
}

static ssize_t fintr_read(struct file *file, 
			  char __user *buf, 
			  size_t count, 
			  loff_t *off) {
	char data[30];
	ssize_t datalen;
	uint32_t counter;
	unsigned long flags;
	/* Actually there is only one static fintr_data_t, but this is
	 * more generic. */
	fintr_data_t *dev_data = file->private_data;

	/* Wait interruptibly for a press */
	if (wait_for_completion_interruptible(&dev_data->new_press)) {
		return -EINTR;
	}
	/* I thought that we should wait if we issue next read immidietly
	 * without new button press */
	reinit_completion(&dev_data->new_press);

	spin_lock_irqsave(&dev_data->cnt_lock, flags);
	counter = dev_data->counter;
	spin_unlock_irqrestore(&dev_data->cnt_lock, flags);

	sprintf(data, "%u", counter);
	datalen = strlen(data);

	if (count > datalen) {
		count = datalen;
	}
	if (copy_to_user(buf, data, count)) {
		return -EFAULT;
	}

	return count;
}

static long fintr_ioctl(struct file *file, uint cmd, unsigned long arg) {
	fintr_data_t *data = (fintr_data_t *)file->private_data;
	uint32_t new_counter;
	unsigned long flags;
	
	switch(cmd) {
	case FINTR_IOCTL_RESET_COUNTER:
		if (copy_from_user(&new_counter, (uint32_t *)arg, (sizeof(uint32_t)))) {
			return -EFAULT;	
		}
		spin_lock_irqsave(&data->cnt_lock, flags);
		data->counter = new_counter;
		spin_unlock_irqrestore(&data->cnt_lock, flags);
		break;

	default:
		return -ENOTTY;	
	}

	return 0;
}

static const struct file_operations fintr_fops = {
	.owner = THIS_MODULE,
	.open = fintr_open,
	.read = fintr_read,
	.unlocked_ioctl = fintr_ioctl,
};

/*****************************************************************************/
/* Driver functions **********************************************************/

irqreturn_t fintr_intr_handler_irq(int irq, void *priv_data) {
	fintr_data_t *data = (fintr_data_t *)priv_data;
	unsigned long flags;

	clear_pending_interrupts(priv_data);
	spin_lock_irqsave(&data->cnt_lock, flags);
	data->counter++;
	spin_unlock_irqrestore(&data->cnt_lock, flags);
	complete(&data->new_press);

	return IRQ_HANDLED;
}

static int fintr_probe(struct platform_device *pdev) {
	int err, irq;
	struct resource *res;
	void *base;

	printk(KERN_ERR"Probing fintr driver.\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(base))
		return PTR_ERR(base);

	err = register_chrdev_region(MKDEV(FINTR_MAJOR, 0), 1, "fintr");
	if (err != 0) {
		printk(KERN_ERR"Error %d\n", err);
		return err;
	}

	/* Initialization of cdev */
	cdev_init(&dev->cdev, &fintr_fops);
	err = cdev_add(&dev->cdev, MKDEV(FINTR_MAJOR, 0), 1);
	if (err) {
		printk(KERN_ERR "Error %d adding fintr driver.\n", err);
		return err;
	}
	dev->mem_base = base;
	dev->counter = 0;
	spin_lock_init(&dev->cnt_lock);
	init_completion(&dev->new_press);

	/* Irq initialization */
	irq = platform_get_irq(pdev, 0);
	printk("Got interrupt: %d\n", irq);
	if (irq < 0) {
		printk(KERN_ERR"Failed to get interrupt\n");
		return irq;
	}

	err = devm_request_irq(
		&pdev->dev,
		irq,
		fintr_intr_handler_irq,
		IRQF_SHARED,
		pdev->name,
		dev
	);

	if (err) {
		printk(KERN_ERR"Failed to request interrupt\n");	
		return err;
	}

	enable_interrupts(dev);

	return 0;
} 

static int fintr_remove(struct platform_device *pdev) {
	printk(KERN_ERR"Removing fcalc driver.\n");
	cdev_del(&dev->cdev);
	unregister_chrdev_region(MKDEV(FINTR_MAJOR, 0), 1);

	return 0;
}

static const struct of_device_id fintr_dt_ids[] = {
	{ .compatible = "uwr,fintr" },
	{}
};

static struct platform_driver fintr = {
	.driver = {
		.name = "fintr",
		.of_match_table = fintr_dt_ids,
	},
	.probe = fintr_probe,
	.remove = fintr_remove,
};

static int __init fintr_init(void)
{
	printk(KERN_ERR"Initializing fintr driver.\n");
	return platform_driver_register(&fintr);
}

static void __exit fintr_exit(void)
{
	pr_info("Exiting fintr driver.\n");
	platform_driver_unregister(&fintr);
}

module_init(fintr_init);
module_exit(fintr_exit);
MODULE_LICENSE("GPL");
