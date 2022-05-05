#ifndef _UAPI_MISC_FCALC_H
#define _UAPI_MISC_FCALC_H

#include <linux/ioctl.h>
#include <linux/types.h>

enum fcalc_op_type {
	FCALC_ADDITION,
	FCALC_SUBSTRACTION,
	FCALC_MULTIPLICATION,
	FCALC_DIVISION,
};

enum fcalc_status {
	FCALC_OK,
	FCALC_DIV_BY_ZERO,
	FCALC_INVALID_OPERATION,
};

#define FCALC_IOCTL_RESET _IO('w', 1)
#define FCALC_IOCTL_SET_OP_TYPE _IOW('w', 2, enum fcalc_op_type)
#define FCALC_IOCTL_GET_STATUS _IOR('w', 3, enum fcalc_status)

#endif
