/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include "inc/barometer_factory.h"

struct baro_factory_private {
	uint32_t gain;
	uint32_t sensitivity;
	struct baro_factory_fops *fops;
};

static struct baro_factory_private baro_factory;

static int baro_factory_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int baro_factory_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long baro_factory_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long err = 0;
	void __user *ptr = (void __user *)arg;
	int data = 0;
	uint32_t flag = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		BARO_PR_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case BAROMETER_IOCTL_INIT:
		if (copy_from_user(&flag, ptr, sizeof(flag)))
			return -EFAULT;
		if (baro_factory.fops != NULL && baro_factory.fops->enable_sensor != NULL) {
			err = baro_factory.fops->enable_sensor(flag, 200);
			if (err < 0) {
				BARO_LOG("BAROMETER_IOCTL_INIT fail!\n");
				return -EINVAL;
			}
			BARO_LOG("BAROMETER_IOCTL_INIT, enable: %d, sample_period:%dms\n", flag, 200);
		} else {
			BARO_PR_ERR("BAROMETER_IOCTL_INIT NULL\n");
			return -EINVAL;
		}
		return 0;
	case BAROMETER_GET_PRESS_DATA:
		if (baro_factory.fops != NULL && baro_factory.fops->get_data != NULL) {
			err = baro_factory.fops->get_data(&data);
			if (err < 0) {
				BARO_LOG("BAROMETER_GET_PRESS_DATA read data fail!\n");
				return -EINVAL;
			}
			if (copy_to_user(ptr, &data, sizeof(data)))
				return -EFAULT;
		} else {
			BARO_LOG("BAROMETER_GET_PRESS_DATA NULL\n");
			return -EINVAL;
		}
		return 0;
	case BAROMETER_IOCTL_ENABLE_CALI:
		if (baro_factory.fops != NULL && baro_factory.fops->enable_calibration != NULL) {
			err = baro_factory.fops->enable_calibration();
			if (err < 0) {
				BARO_LOG("BAROMETER_IOCTL_ENABLE_CALI fail!\n");
				return -EINVAL;
			}
		} else {
			BARO_LOG("BAROMETER_IOCTL_ENABLE_CALI NULL\n");
			return -EINVAL;
		}
		return 0;
	case BAROMETER_GET_TEMP_DATA:
		return 0;
	default:
		BARO_PR_ERR("unknown IOCTL: 0x%08x\n", cmd);
		return -ENOIOCTLCMD;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_baro_factory_unlocked_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		BARO_PR_ERR("compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_BAROMETER_IOCTL_INIT:
	/* case COMPAT_BAROMETER_IOCTL_READ_CHIPINFO: */
	case COMPAT_BAROMETER_GET_PRESS_DATA:
	case COMPAT_BAROMETER_GET_TEMP_DATA:
	case COMPAT_BAROMETER_IOCTL_ENABLE_CALI: {
		BARO_LOG("compat_ion_ioctl : BAROMETER_IOCTL_XXX command is 0x%x\n", cmd);
		return filp->f_op->unlocked_ioctl(filp, cmd,
			(unsigned long)compat_ptr(arg));
	}
	default:
		BARO_PR_ERR("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
		return -ENOIOCTLCMD;
	}
}
#endif
/*----------------------------------------------------------------------------*/
static const struct file_operations baro_factory_fops = {
	.open = baro_factory_open,
	.release = baro_factory_release,
	.unlocked_ioctl = baro_factory_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_baro_factory_unlocked_ioctl,
#endif
};

static struct miscdevice baro_factory_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "barometer",
	.fops = &baro_factory_fops,
};

int baro_factory_device_register(struct baro_factory_public *dev)
{
	int err = 0;

	if (!dev || !dev->fops)
		return -1;
	baro_factory.gain = dev->gain;
	baro_factory.sensitivity = dev->sensitivity;
	baro_factory.fops = dev->fops;
	err = misc_register(&baro_factory_device);
	if (err) {
		BARO_LOG("baro_factory_device register failed\n");
		err = -1;
	}
	return err;
}

int baro_factory_device_deregister(struct baro_factory_public *dev)
{
	baro_factory.fops = NULL;
	misc_deregister(&baro_factory_device);
	return 0;
}

