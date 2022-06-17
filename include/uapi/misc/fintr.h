#ifndef _UAPI_MISC_FINTR_H
#define _UAPI_MISC_FINTR_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define FINTR_IOCTL_RESET_COUNTER _IOW('w', 1, uint32_t)

#endif /* _UAPI_MISC_FINTR_H */
