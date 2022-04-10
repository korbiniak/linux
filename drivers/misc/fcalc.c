#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define FCALC_MAJOR        140
#define FCALC_MAX_MINORS   10

struct fcalc_data {
  struct cdev cdev;
};

struct fcalc_data devs[FCALC_MAX_MINORS];

static const struct of_device_id fcalc_dt_ids[] = {
  { .compatible = "uwr,fcalc" },
  {}
};

static int fcalc_open(struct inode *inode, struct file *file) {
  struct fcalc_data *data =
    container_of(inode->i_cdev, struct fcalc_data, cdev);
  unsigned int minor = MINOR(inode->i_cdev->dev);
  file->private_data = data;

  return 0;
}

static ssize_t fcalc_read(
    struct file *file,
    char __user *buf,
    size_t count,
    loff_t *offset) {
  uint8_t *data = "Hello from the kernel!\n";
  static bool first_call = true;
  size_t datalen = strlen(data);

  if (first_call == true) {
    first_call = false;
  } else {
    count = 0;
  }
  if (count > datalen) {
    count = datalen;
  }

  if (copy_to_user(buf, data, count)) {
    return -EFAULT;
  }

  return count;
}

static const struct file_operations fcalc_fops = {
  .owner = THIS_MODULE,
  .read = fcalc_read,
};

static int fcalc_probe(struct platform_device *pdev) {
  printk(KERN_ERR"Probing my awesome driver!! :DDDDD\n");

  int i, err;
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
  printk(KERN_ERR":(\n");

  int i;
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
