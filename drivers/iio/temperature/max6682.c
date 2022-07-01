#include <linux/module.h>
#include <linux/iio/iio.h>
#include <linux/spi/spi.h>

#define MAX6682_NAME "max6682"

struct max6682_data {
	struct spi_device *spi_dev;
};

static const struct iio_chan_spec max6682_channels[] = {
	{
		.type = IIO_TEMP,
		.info_mask_separate = 
			BIT(IIO_CHAN_INFO_RAW),
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 11,
			.storagebits = 16,
			.shift = 5,
			.endianness = IIO_BE,
		},
	},
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

static int max6682_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask) {
	struct max6682_data *priv_data = iio_priv(indio_dev);
	int ret;
	uint16_t buf;
	
	switch(mask) {
	case IIO_CHAN_INFO_RAW:
		ret = iio_device_claim_direct_mode(indio_dev);
		if (ret)
			return ret;
		spi_read(priv_data->spi_dev, (void *)&buf, 2);
		*val = be16_to_cpu(buf);
		iio_device_release_direct_mode(indio_dev);
		if (!ret)
			return IIO_VAL_INT;
		break;
	}

	return ret;
}

static const struct iio_info max6682_info = {
	.read_raw = max6682_read_raw,
};

static int max6682_probe(struct spi_device *spi_dev) {
	struct iio_dev *indio_dev;
	struct max6682_data *priv_data;

	indio_dev = devm_iio_device_alloc(&spi_dev->dev, 
					    sizeof(*priv_data));
	if (!indio_dev) {
		printk(KERN_ERR"????????!!!\n");
		return -ENOMEM;
	}

	dev_set_drvdata(&spi_dev->dev, (void*)indio_dev);
	
	priv_data = iio_priv(indio_dev);
	priv_data->spi_dev = spi_dev;

	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = max6682_channels;
	indio_dev->num_channels = ARRAY_SIZE(max6682_channels);
	indio_dev->info = &max6682_info;
	indio_dev->name = MAX6682_NAME;


	printk(KERN_ERR"aWESOME DRIVER PROBED!!!!\n");
	printk(KERN_ERR"aWESOME DRIVER PROBED!!!!\n");

	return devm_iio_device_register(&spi_dev->dev, indio_dev);
}


static const struct of_device_id max6682_dt_ids[] = {
	{ .compatible = "maxim,max6682" },	
	{ },
};
MODULE_DEVICE_TABLE(of, max6682_dt_ids);

static const struct spi_device_id max6682_id_table[] = {
	{ MAX6682_NAME },
	{},
};
MODULE_DEVICE_TABLE(spi, max6682_id_table);

static struct spi_driver max6682_driver = {
	.driver = {
		.name = MAX6682_NAME,
		.of_match_table = max6682_dt_ids,
	},
	.probe = max6682_probe,
	.id_table = max6682_id_table,
};
module_spi_driver(max6682_driver);

MODULE_LICENSE("GPL");
