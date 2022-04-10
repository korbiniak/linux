#ifndef _UAPI_MISC_FCALC_H
#define _UAPI_MISC_FCALC_H

#include <linux/ioctl.h>
#include <linux/types.h>

struct fcalc_ioctl_data {
	int value;
};

#define FCALC_IOCTL_IN _IOW('w', 1, struct fcalc_ioctl_data)


#endif
