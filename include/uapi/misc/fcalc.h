#ifndef _UAPI_MISC_FCALC_H
#define _UAPI_MISC_FCALC_H

#include <linux/ioctl.h>
#include <linux/types.h>

enum fcalc_calc_type {
	FCALC_ADDITION,
	FCALC_MULTIPLICATION,
};

union fcalc_ioctl_data {
	long value;
	enum fcalc_calc_type calc_type;
};

#define FCALC_IOCTL_RESET _IO('w', 1)
#define FCALC_IOCTL_CALC_TYPE _IOW('w', 2, union fcalc_ioctl_data)
#define FCALC_IOCTL_GET_CALC_TYPE _IOR('w', 3, union fcalc_ioctl_data)

#endif
