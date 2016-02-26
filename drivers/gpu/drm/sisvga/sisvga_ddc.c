/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 *
 * Based on regular I2C algorithm as implemented in
 *
 * 	drivers/i2c/algos/i2c-algo-bit.c
 *
 * DO NOT COPY-AND-PASTE THIS FILE INTO YOUR DRM DRIVER! SiS graphics
 * cards use their own variant of the i2c bit-shift algorithm for DDC.
 * Using the SiS algorithm can damage your hardware.
 */

#include "sisvga_device.h"
#include <drm/drm_device.h>
#include <linux/timex.h>
#include "sisvga_reg.h"

/*
 * I2C helpers
 */

static int i2c_get(struct sisvga_device *sdev, u8 mask)
{
	u8 sr11;

	BUG_ON(!sdev);
	RREG_SR(0x11, sr11);

	return (sr11 & mask) != 0;
}

static void i2c_set(struct sisvga_device *sdev, u8 mask, int state)
{
	u8 sr11;

	BUG_ON(!sdev);
	RREG_SR(0x11, sr11);

	if (state)
		sr11 |= mask;
	else
		sr11 &= ~mask;

	WREG_SR(0x11, sr11);
}

/*
 * SiS DDC I2C algorithm
 */

static int getsda(struct sisvga_ddc *sis_ddc)
{
	struct sisvga_device *sdev = sis_ddc->dev->dev_private;

	return i2c_get(sdev, sis_ddc->sda_mask);
}

static void setsda(struct sisvga_ddc *sis_ddc, int state)
{
	struct sisvga_device *sdev = sis_ddc->dev->dev_private;

	i2c_set(sdev, sis_ddc->sda_mask, state);
}

static int getscl(struct sisvga_ddc *sis_ddc)
{
	struct sisvga_device *sdev = sis_ddc->dev->dev_private;

	return i2c_get(sdev, sis_ddc->scl_mask);
}

static void setscl(struct sisvga_ddc *sis_ddc, int state)
{
	struct sisvga_device *sdev = sis_ddc->dev->dev_private;

	i2c_set(sdev, sis_ddc->scl_mask, state);
}

static int setscl_validate(struct sisvga_ddc *sis_ddc, int state)
{
	unsigned long timeout;
	struct sisvga_device *sdev = sis_ddc->dev->dev_private;

	i2c_set(sdev, sis_ddc->scl_mask, state);

	timeout = jiffies + sis_ddc->timeout;
	while (getscl(sis_ddc) != state) {
		if (time_after(jiffies, timeout)) {
			if (getscl(sis_ddc) == state)
				break;
			DRM_ERROR("SCL state validation failed with timeout\n");
			return -ETIMEDOUT;
		}
		cpu_relax();
	}
	udelay(sis_ddc->udelay);

	return 0;
}

static int pre_xfer(struct sisvga_ddc *sis_ddc)
{
	int ret;
	struct sisvga_device *sdev = sis_ddc->dev->dev_private;

	/* enable display while probing EDID */
	RREG_SR(0x01, sis_ddc->sr01);
	if (!(sis_ddc->sr01 & 0x20))
		WREG_SR(0x01, sis_ddc->sr01 & ~0x20);

	/* raise SDA, SCL before transfer */
	setsda(sis_ddc, 1);
	udelay((sis_ddc->udelay + 1) / 2);
	ret = setscl_validate(sis_ddc, 1);
	if (ret < 0)
		return ret;

	return 0;
}

static void post_xfer(struct sisvga_ddc *sis_ddc)
{
	u8 sr01;
	struct sisvga_device *sdev = sis_ddc->dev->dev_private;

	/* restore register state */

	RREG_SR(0x01, sr01);
	if (sr01 != sis_ddc->sr01)
		WREG_SR(0x01, sis_ddc->sr01);
}

static int tx_start(struct sisvga_ddc *sis_ddc)
{
	/* expect SDA, SCL high */

	setsda(sis_ddc, 0);
	udelay(sis_ddc->udelay);
	setscl(sis_ddc, 0);

	return 0;
}

static int tx_repstart(struct sisvga_ddc *sis_ddc)
{
	int ret;

	setsda(sis_ddc, 1);
	ret = setscl_validate(sis_ddc, 1);
	if (ret < 0)
		return ret;

	return tx_start(sis_ddc);
}

static int tx_stop(struct sisvga_ddc *sis_ddc)
{
	int ret;

	/* expect SCL low */

	setsda(sis_ddc, 0);
	ret = setscl_validate(sis_ddc, 1);
	if (ret < 0)
		return ret;
	setsda(sis_ddc, 1);
	udelay(sis_ddc->udelay);

	return 0;
}

static int tx_ack(struct sisvga_ddc *sis_ddc, u8 ack)
{
	int ret;

	/* expect SCL low */

	setsda(sis_ddc, !ack); /* bit is invert to ack */
	udelay((sis_ddc->udelay + 1) / 2);
	ret = setscl_validate(sis_ddc, 1);
	if (ret < 0)
			return ret;
	setscl(sis_ddc, 0);

	return 0;
}

static int rx_ack(struct sisvga_ddc *sis_ddc)
{
	int ret, sda;

	/* expect SCL low */

	setsda(sis_ddc, 1);
	ret = setscl_validate(sis_ddc, 1);
	if (ret < 0)
		return ret;
	sda = getsda(sis_ddc);
	setscl(sis_ddc, 0);
	if (sda)
		return -EPROTO;

	return 0;
}

static int rx_byte(struct sisvga_ddc *sis_ddc, bool ack)
{
	int i, ret, sda;
	u8 byte = 0;

	/* expect SCL low */

	for (i = 0; i < 8; ++i) {
		byte <<= 1;
		/* In contrast to the regular I2C bit-transfer algorithm, we
		 * raise SDA before receiving each bit. Usually this would be
		 * done by the sender side.
		 *
		 * DO NOT COPY-AND-PASTE THIS CODE INTO YOUR DRM DRIVER!
		 */
		setsda(sis_ddc, 1);
		ret = setscl_validate(sis_ddc, 1);
		if (ret < 0)
			goto err;
		sda = getsda(sis_ddc);
		if (sda)
			byte |= 0x01;
		if (1) {
			setscl(sis_ddc, 0);
			udelay(i == 7 ? sis_ddc->udelay / 2 : sis_ddc->udelay);
		} else {
			struct sisvga_device *sdev = sis_ddc->dev->dev_private;
			i2c_set(sdev, sis_ddc->scl_mask, 0);
			udelay(i == 7 ? sis_ddc->udelay / 2 : sis_ddc->udelay);
		}
	}
	ret = tx_ack(sis_ddc, ack);
	if (ret < 0)
		return ret;

	return byte;

err:
	setscl(sis_ddc, 0);
	tx_ack(sis_ddc, !ack);
	return ret;
}

static int rx_buf(struct sisvga_ddc *sis_ddc, u8 *buf, u16 len)
{
	int l, ret;

	for (l = len; l--; ++buf) {
		ret = rx_byte(sis_ddc, l); /* no ack on final byte */
		if (ret < 0)
			return ret;
		*buf = ret;
	}

	return len;
}

static int tx_byte(struct sisvga_ddc *sis_ddc, u8 byte)
{
	int i, ret;

	/* expect SCL low */

	for (i = 0; i < 8; ++i) {
		setsda(sis_ddc, byte & 0x80);
		udelay((sis_ddc->udelay + 1) / 2);
		ret = setscl_validate(sis_ddc, 1);
		if (ret < 0)
			return ret;
		byte <<= 1;
		setscl(sis_ddc, 0);
	}
	ret = rx_ack(sis_ddc);
	if (ret < 0)
		return ret;

	return 0;
}

static int tx_buf(struct sisvga_ddc *sis_ddc, const u8 *buf, u16 len)
{
	int l, ret;

	for (l = len; l--; ++buf) {
		ret = tx_byte(sis_ddc, *buf);
		if (ret < 0)
			return ret;
	}

	return len;
}

static int tx_addr(struct sisvga_ddc *sis_ddc, u8 addr, bool rx)
{
	return tx_byte(sis_ddc, (addr << 1) | !!rx);
}

/*
 * I2C adapter funcs
 */

static int master_xfer(struct i2c_adapter *adapter,
		       struct i2c_msg *msgs, int num)
{
	int ret, i;
	struct sisvga_ddc *sis_ddc = i2c_get_adapdata(adapter);

	ret = pre_xfer(sis_ddc);
	if (ret < 0)
		return ret;

	ret = tx_start(sis_ddc);
	if (ret < 0)
		goto err_tx_start;

	for (i = 0; i < num; ++i) {
		if (i) {
			ret = tx_repstart(sis_ddc);
			if (ret < 0)
				goto err_msg;
		}
		if (msgs[i].flags & I2C_M_RD) {
			ret = tx_addr(sis_ddc, msgs[i].addr, true);
			if (ret < 0)
				goto err_msg;

			ret = rx_buf(sis_ddc, msgs[i].buf, msgs[i].len);
			if (ret < 0)
				goto err_msg;
		} else {
			ret = tx_addr(sis_ddc, msgs[i].addr, false);
			if (ret < 0)
				goto err_msg;

			ret = tx_buf(sis_ddc, msgs[i].buf, msgs[i].len);
			if (ret < 0)
				goto err_msg;
		}
	}

	tx_stop(sis_ddc);

	post_xfer(sis_ddc);

	return i; /* number of messages signals success */

err_msg:
	setscl(sis_ddc, 0);
	tx_stop(sis_ddc);
err_tx_start:
	post_xfer(sis_ddc);
	return ret;

}

static u32 functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C;
}

static const struct i2c_algorithm sisvga_i2c_algorithm = {
	.master_xfer = master_xfer,
	.functionality = functionality,
};

/*
 * struct sisvga_ddc
 */

int sisvga_ddc_init(struct sisvga_ddc *sis_ddc, struct drm_device *dev)
{
	int ret;

	sis_ddc->dev = dev;

	sis_ddc->sda_mask = 0x02;
	sis_ddc->scl_mask = 0x01;

	sis_ddc->udelay = 10;
	sis_ddc->timeout = usecs_to_jiffies(2200);

	sis_ddc->adapter.owner = THIS_MODULE;
	sis_ddc->adapter.class = I2C_CLASS_DDC;
	sis_ddc->adapter.dev.parent = &dev->pdev->dev;
	sis_ddc->adapter.algo = &sisvga_i2c_algorithm;
	sis_ddc->adapter.algo_data = sis_ddc;
	sis_ddc->adapter.retries = 3;
	i2c_set_adapdata(&sis_ddc->adapter, sis_ddc);
	snprintf(sis_ddc->adapter.name, sizeof(sis_ddc->adapter.name),
		 "sisvga DDC");

	ret = i2c_add_adapter(&sis_ddc->adapter);
	if (ret < 0)
		return ret;

	return 0;
}

void sisvga_ddc_fini(struct sisvga_ddc *sis_ddc)
{
	i2c_del_adapter(&sis_ddc->adapter);
}
