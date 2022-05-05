#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/mmio.h>

#define FINTR_MAJOR		141

struct fintr_data {
	struct cdev cdev;
	void *mem_base;
};

/* Only one minor this time */
struct fintr_data dev[1];

static const struct of_device_id fintr_dt_ids[] = {
	{ .compatible = "uwr,fintr" },
	{}
};

static const struct file_operations fintr_fops = {
	.owner = THIS_MODULE,
};

static int fintr_probe(struct platform_device *pdev) {
	int err;
	struct resource *res;
	void *base;

	printk(KERN_ERR"Probing fintr driver.\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	err = register_chrdev_region(MKDEV(FINTR_MAJOR, 0), 1, "fintr");
	if (err != 0) {
		printk(KERN_ERR"Error %d\n", err);
		return err;
	}

	cdev_init(&dev->cdev, &fintr_fops);
	err = cdev_add(&dev->cdev, MKDEV(FINTR_MAJOR, 0), 1);
	if (err) {
		printk(KERN_ERR "Error %d adding fintr driver.\n", err);
	}
	dev->mem_base = base;

	return 0;
} 

static int fintr_remove(struct platform_device *pdev) {
	printk(KERN_ERR"Removing fcalc driver.\n");
	cdev_del(&dev->cdev);
	unregister_chrdev_region(MKDEV(FINTR_MAJOR, 0), 1);

	return 0;
}

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
