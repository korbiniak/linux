#ifndef _UAPI_MISC_SI7021_H
#define _UAPI_MISC_SI7021_H

enum measurement_type {
	SI7021_TEMPERATURE,
	SI7021_HUMIDITY,
};

#define SI7021_IOCTL_SET_TYPE _IOW('v', 1, uint32_t)

#endif /* _UAPI_MISC_SI7021_H */
