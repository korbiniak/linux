#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <uapi/misc/si7021.h>

#define SI7021_MAJOR 142
#define SI7021CMD_RESET 0xFE
#define SI7021CMD_RH_HOLD 0xE5
#define SI7021CMD_TEMP_HOLD 0xE3

typedef struct si7021_data {
	struct cdev cdev;
	struct i2c_client *client;	
	enum measurement_type m_type;	
} si7021_data_t;

si7021_data_t si7021_data[1];

static inline int si7021_send_byte(struct i2c_client *client, uint8_t byte) {
	return i2c_master_send(client, &byte, 1);
}

static inline int si7021_read_measurements(struct i2c_client *client, 
					   uint16_t *measurements) {
	return i2c_master_recv(client, (char *)measurements, 2);
}

static int si7021_open(struct inode *inode, struct file *file) {
	file->private_data = si7021_data;

	return 0;
}

static int si7021_read(struct file *file,
		       char __user *buf,
		       size_t count,
		       loff_t *off) {
	char data[30];
	size_t datalen;
	uint16_t measurement;
	uint32_t val;
	uint8_t cmd;
	int err;

	/* Assumes the m_type is set to a proper value */
	cmd = (si7021_data->m_type == SI7021_TEMPERATURE ? 
		SI7021CMD_TEMP_HOLD : SI7021CMD_RH_HOLD);
	
	err = si7021_send_byte(si7021_data->client, cmd);
	if (err < 0) {
		return -EBUSY;
	}

	err = si7021_read_measurements(si7021_data->client, &measurement);
	if (err < 0) {
		return -EBUSY;
	}

	val = ntohs(measurement);
	
	if (si7021_data->m_type == SI7021_TEMPERATURE) {
		val *= 17572;
		val -= 4685 * 65536;
		val /= 65536;
	}
	else {
		val *= 12500;
		val -= 6 * 65536;
		val /= 65536;
	}

	sprintf(data, "%u", val);
	datalen = strlen(data);

	if (count > datalen) {
		count = datalen;
	}
	if (copy_to_user(buf, data, count)) {
		return -EFAULT;
	}

	return count;
}

static long si7021_ioctl(struct file *file, uint cmd, unsigned long arg) {
	enum measurement_type new_type;
	switch (cmd) {
	case SI7021_IOCTL_SET_TYPE:
		if (copy_from_user(&new_type, (uint32_t *)arg, sizeof(uint32_t))) {
			return -EFAULT;
		}
		if (new_type != SI7021_TEMPERATURE && 
		    new_type != SI7021_HUMIDITY) {
			return -EINVAL;
		}
		si7021_data->m_type = new_type;
		break;
	default:
		return -ENOTTY;
	}
	return 0;
}

struct file_operations si7021_fops = {
	.owner = THIS_MODULE,
	.open = si7021_open,
	.read = si7021_read,
	.unlocked_ioctl = si7021_ioctl,
};

static int si7021_init_cdev(void) {
	int err;
	err = register_chrdev_region(MKDEV(SI7021_MAJOR, 0), 1, "si7021");
	if (err < 0) {
		printk(KERN_ERR"Registering chrdev failed: %d\n", err);
		return err;
	}

	cdev_init(&si7021_data->cdev, &si7021_fops);
	err = cdev_add(&si7021_data->cdev, MKDEV(SI7021_MAJOR, 0), 1);
	if (err < 0) {
		printk(KERN_ERR"Adding cdev failed: %d\n", err);
		return err;
	}
	si7021_data->m_type = SI7021_TEMPERATURE;

	return err;
}

static int si7021_probe(struct i2c_client *client,
			const struct i2c_device_id *id) {
	int err;

	printk(KERN_ERR"Probing si7201 I2C driver.\n");

	err = si7021_send_byte(client, SI7021CMD_RESET);
	if (err < 0) {
		printk(KERN_ERR"Reseting device failed: %d\n", err);
		return err;
	}
	msleep(15);

	si7021_data->client = client;
	
	err = si7021_init_cdev();
		
	return err;
}

static const struct i2c_device_id si7021_id[] = {
	{ "si7021", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, si7021_id);

static const struct of_device_id si7021_dt_ids[] = {
	{ .compatible = "silabs,si7021" },
	{ }
};
MODULE_DEVICE_TABLE(of, si7021_dt_ids);

static struct i2c_driver si7021_driver = {
	.driver = {
		.name = "si7021",
		.of_match_table = si7021_dt_ids,
	},
	.probe = si7021_probe,
	.id_table = si7021_id,
};
module_i2c_driver(si7021_driver);
MODULE_LICENSE("GPL");
