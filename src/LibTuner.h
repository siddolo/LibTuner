/* TODO: move to .c and leave headers here */

#include <Wire.h>		// Arduino I2C
#include "tuner_r82xx.c"

#define FIR_LEN 16

/* two raised to the power of n */
#define TWO_POW(n)		((double)(1ULL<<(n)))


enum rtlsdr_tuner {
	RTLSDR_TUNER_UNKNOWN = 0,
	RTLSDR_TUNER_R820T,
	RTLSDR_TUNER_R828D,
	RTLSDR_TUNER_R842T
};

enum rtlsdr_ds_mode {
	RTLSDR_DS_IQ = 0,	/* I/Q quadrature sampling of tuner output */
	RTLSDR_DS_I,		/* 1: direct sampling on I branch: usually not connected */
	RTLSDR_DS_Q,		/* 2: direct sampling on Q branch: HF on rtl-sdr v3 dongle */
	RTLSDR_DS_I_BELOW,	/* 3: direct sampling on I branch when frequency below 'DS threshold frequency' */
	RTLSDR_DS_Q_BELOW	/* 4: direct sampling on Q branch when frequency below 'DS threshold frequency' */
};


typedef struct rtlsdr_tuner_iface {
	/* tuner interface */
	int (*init)(void *);
	int (*exit)(void *);
	int (*set_freq)(void *, uint32_t freq /* Hz */);
	int (*set_bw)(void *, uint32_t bw /* Hz */, uint32_t *applied_bw /* configured bw in Hz */, int apply /* 1 == configure it!, 0 == deliver applied_bw */);
	int (*set_gain)(void *, int gain /* tenth dB */);
	int (*set_if_gain)(void *, int stage, int gain /* tenth dB */);
	int (*set_gain_mode)(void *, int manual);
} rtlsdr_tuner_iface_t;

struct rtlsdr_dev {
	/* rtl demod context */
	uint32_t rate; /* Hz */
	uint32_t rtl_xtal; /* Hz */
	/* tuner context */
	enum rtlsdr_tuner tuner_type;
	rtlsdr_tuner_iface_t *tuner;
	uint32_t tun_xtal; /* Hz */
	uint32_t freq; /* Hz */
	uint32_t bw;
	uint32_t offs_freq; /* Hz */
	int corr; /* ppm */
	int gain; /* tenth dB */
	enum rtlsdr_ds_mode direct_sampling_mode;
	uint32_t direct_sampling_threshold; /* Hz */
	struct r82xx_config r82xx_c;
	struct r82xx_priv r82xx_p;
};

typedef struct rtlsdr_dev rtlsdr_dev_t;

/* Arduino I2C Read */
int rtlsdr_read_array(rtlsdr_dev_t *dev, uint16_t addr, uint8_t *array, uint8_t len)
{
	#if DEBUG > 5
		fprintf(stderr, "Entered in %s()\n", __FUNCTION__);
	#endif
	#if DEBUG > 6
		fprintf(stderr, "I2C read to addr=0x%02x, len=%d, data=", addr, len);
	#endif

	int r = 0;

 	Wire.requestFrom((uint8_t) addr, (uint8_t) len);
	while (Wire.available()) {
	//for (;r<len;r++) {
		array[r] = Wire.read();
		//memcpy(array,  Wire.read(), 1);
		#if DEBUG > 6
			fprintf(stderr, "0x%02x ", r82xx_bitrev(array[r]));
		#endif
		r++;
	}
	#if DEBUG > 6
		fprintf(stderr, "\n");
	#endif

	//r = libusb_control_transfer(dev->devh, CTRL_IN, 0, addr, index, array, len, CTRL_TIMEOUT);
	return r;
}

/* Arduino I2C Write */
int rtlsdr_write_array(rtlsdr_dev_t *dev, uint16_t addr, uint8_t *array, uint8_t len)
{
	#if DEBUG > 5
		fprintf(stderr, "Entered in %s()\n", __FUNCTION__);
	#endif
	#if DEBUG > 6
		fprintf(stderr, "I2C write to addr=0x%02x, len=%d, data=", addr, len);
	#endif

	int r = 0;
	int i;

	Wire.beginTransmission((uint8_t) addr);
	for (i=0; i<len; i++) {
		#if DEBUG > 6
			fprintf(stderr, "0x%02x ", array[i]);
		#endif
		r += Wire.write((uint8_t) array[i]);
	}
  	Wire.endTransmission();
	#if DEBUG > 6
		fprintf(stderr, "\n");
	#endif

	//r = libusb_control_transfer(dev->devh, CTRL_OUT, 0, addr, index, array, len, CTRL_TIMEOUT);

	return r;
}

int rtlsdr_i2c_write(rtlsdr_dev_t *dev, uint8_t i2c_addr, uint8_t *buffer, int len)
{
	#if DEBUG > 5
		fprintf(stderr, "Entered in %s()\n", __FUNCTION__);
	#endif

	uint16_t addr = i2c_addr;

	if (!dev)
		return -1;

	return rtlsdr_write_array(dev, addr, buffer, len);
}

int rtlsdr_i2c_read(rtlsdr_dev_t *dev, uint8_t i2c_addr, uint8_t *buffer, int len)
{
	#if DEBUG > 5
		fprintf(stderr, "Entered in %s()\n", __FUNCTION__);
	#endif

	uint16_t addr = i2c_addr;

	if (!dev)
		return -1;

	return rtlsdr_read_array(dev, addr, buffer, len);
}

int rtlsdr_i2c_write_fn(void *dev, uint8_t addr, uint8_t *buf, int len)
{
	#if DEBUG > 5
		fprintf(stderr, "Entered in %s()\n", __FUNCTION__);
	#endif

	if (dev)
		return rtlsdr_i2c_write(((rtlsdr_dev_t *)dev), addr, buf, len);

	return -1;
}

int rtlsdr_i2c_read_fn(void *dev, uint8_t addr, uint8_t *buf, int len)
{
	#if DEBUG > 5
		fprintf(stderr, "Entered in %s()\n", __FUNCTION__);
	#endif

	if (dev)
		return rtlsdr_i2c_read(((rtlsdr_dev_t *)dev), addr, buf, len);

	return -1;
}

int rtlsdr_get_xtal_freq(rtlsdr_dev_t *dev, uint32_t *rtl_freq, uint32_t *tuner_freq)
{
	if (!dev)
		return -1;

	#define APPLY_PPM_CORR(val,ppm) (((val) * (1.0 + (ppm) / 1e6)))

	if (rtl_freq)
		*rtl_freq = (uint32_t) APPLY_PPM_CORR(dev->rtl_xtal, dev->corr);

	if (tuner_freq)
		*tuner_freq = (uint32_t) APPLY_PPM_CORR(dev->tun_xtal, dev->corr);

	return 0;
}

int rtlsdr_set_center_freq(rtlsdr_dev_t *dev, uint32_t freq)
{
	int r = -1;

	if (!dev || !dev->tuner)
		return -1;

	/* More code on original implementation of librtlsdr.
	   On this arduino port experiment i want to support 
	   only rafael micro chips */

	if (dev->tuner && dev->tuner->set_freq) {
		r = dev->tuner->set_freq(dev, freq - dev->offs_freq);
	}

	if (!r)
		dev->freq = freq;
	else
		dev->freq = 0;

	return r;
}

int r820t_init(void *dev) {
	rtlsdr_dev_t* devt = (rtlsdr_dev_t*)dev;
	devt->r82xx_p.rtl_dev = dev;

	if (devt->tuner_type == RTLSDR_TUNER_R828D) {
		devt->r82xx_c.i2c_addr = R828D_I2C_ADDR;
		devt->r82xx_c.rafael_chip = CHIP_R828D;
	} else if (devt->tuner_type == RTLSDR_TUNER_R820T) {
		devt->r82xx_c.i2c_addr = R820T_I2C_ADDR;
		devt->r82xx_c.rafael_chip = CHIP_R820T;
	} else {
		devt->r82xx_c.i2c_addr = R842T_I2C_ADDR;
		//devt->r82xx_c.rafael_chip = CHIP_R828D; // ???
		devt->r82xx_c.rafael_chip = CHIP_R820T;
	}

	rtlsdr_get_xtal_freq(devt, NULL, &devt->r82xx_c.xtal);

	devt->r82xx_c.max_i2c_msg_len = 8;
	devt->r82xx_c.use_predetect = 0;
	devt->r82xx_p.cfg = &devt->r82xx_c;

	return r82xx_init(&devt->r82xx_p);
}

int r820t_exit(void *dev) {
	rtlsdr_dev_t* devt = (rtlsdr_dev_t*)dev;
	return r82xx_standby(&devt->r82xx_p);
}

int r820t_set_freq(void *dev, uint32_t freq) {
	rtlsdr_dev_t* devt = (rtlsdr_dev_t*)dev;
	return r82xx_set_freq(&devt->r82xx_p, freq);
}

int r820t_set_bw(void *dev, uint32_t bw, uint32_t *applied_bw, int apply) {
	#ifdef DEBUG
		fprintf(stderr, "Entered in %s( bw=%lu, applied_bw=%lu, apply=%d )\n", __FUNCTION__, (unsigned long)bw, (unsigned long)applied_bw, apply);
	#endif

	int r;
	rtlsdr_dev_t* devt = (rtlsdr_dev_t*)dev;

	r = r82xx_set_bandwidth(&devt->r82xx_p, bw, devt->rate, applied_bw, apply);
	if(!apply)
			return 0;
	if(r < 0)
			return r;

	return rtlsdr_set_center_freq(devt, devt->freq);
}

int r820t_set_gain(void *dev, int gain) {
	rtlsdr_dev_t* devt = (rtlsdr_dev_t*)dev;
	return r82xx_set_gain(&devt->r82xx_p, 1, gain, 0, 0, 0, 0);
}

int r820t_set_gain_ext(void *dev, int lna_gain, int mixer_gain, int vga_gain) {
	rtlsdr_dev_t* devt = (rtlsdr_dev_t*)dev;
	return r82xx_set_gain(&devt->r82xx_p, 0, 0, 1, lna_gain, mixer_gain, vga_gain);
}

int r820t_set_gain_mode(void *dev, int manual) {
	rtlsdr_dev_t* devt = (rtlsdr_dev_t*)dev;
	return r82xx_set_gain(&devt->r82xx_p, manual, 0, 0, 0, 0, 0);
}















int rtlsdr_set_offset_tuning(rtlsdr_dev_t *dev, int on)
{
	int r = 0;
	int bw;

	if (!dev)
		return -1;

	if (
		(dev->tuner_type == RTLSDR_TUNER_R820T) ||
		(dev->tuner_type == RTLSDR_TUNER_R828D) ||
		(dev->tuner_type == RTLSDR_TUNER_R842T)
		)
		return -2;

	/* More code on original implementation of librtlsdr.
	   On this arduino port experiment i want to support 
	   only rafael micro chips */
	return -1;
}

int rtlsdr_set_sample_rate(rtlsdr_dev_t *dev, uint32_t samp_rate)
{
	int r = 0;
	uint16_t tmp;
	uint32_t rsamp_ratio, real_rsamp_ratio;
	double real_rate;

	if (!dev)
		return -1;

	/* check if the rate is supported by the resampler */
	if ((samp_rate <= 225000) || (samp_rate > 3200000) ||
		 ((samp_rate > 300000) && (samp_rate <= 900000))) {
		fprintf(stderr, "Invalid sample rate: %lu Hz\n", (unsigned long)samp_rate);
		return -22; /* EINVAL (22) Invalid argument */
	}

	rsamp_ratio = (dev->rtl_xtal * TWO_POW(22)) / samp_rate;
	rsamp_ratio &= 0x0ffffffc;

	real_rsamp_ratio = rsamp_ratio | ((rsamp_ratio & 0x08000000) << 1);
	real_rate = (dev->rtl_xtal * TWO_POW(22)) / real_rsamp_ratio;

	if ( ((double)samp_rate) != real_rate )
		fprintf(stderr, "Exact sample rate is: %f Hz\n", real_rate);

	dev->rate = (uint32_t)real_rate;

	if (dev->tuner && dev->tuner->set_bw) {
		uint32_t applied_bw = 0;
		dev->tuner->set_bw(dev, dev->bw > 0 ? dev->bw : dev->rate, &applied_bw, 1);
	}

	/* recalculate offset frequency if offset tuning is enabled */
	if (dev->offs_freq)
		rtlsdr_set_offset_tuning(dev, 1);

	return r;
}

int rtlsdr_set_and_get_tuner_bandwidth(rtlsdr_dev_t *dev, uint32_t bw, uint32_t *applied_bw, int apply_bw )
{
	int r = 0;

		*applied_bw = 0;		/* unknown */

	if (!dev || !dev->tuner)
		return -1;

	if (dev->tuner->set_bw) {
		r = dev->tuner->set_bw(dev, bw > 0 ? bw : dev->rate, applied_bw, apply_bw);
		if (r)
			return r;
		dev->bw = bw;
	}
	return r;
}

int rtlsdr_set_tuner_gain_mode(rtlsdr_dev_t *dev, int mode)
{
	int r = 0;

	if (!dev || !dev->tuner)
		return -1;

	if (dev->tuner->set_gain_mode) {
		r = dev->tuner->set_gain_mode((void *)dev, mode);
	}

	return r;
}

int rtlsdr_set_tuner_gain(rtlsdr_dev_t *dev, int gain)
{
	int r = 0;

	if (!dev || !dev->tuner)
		return -1;

	if (dev->tuner->set_gain) {
		r = dev->tuner->set_gain((void *)dev, gain);
	}

	if (!r)
		dev->gain = gain;
	else
		dev->gain = 0;

	return r;
}

int rtlsdr_get_tuner_gains(rtlsdr_dev_t *dev, int *gains)
{
	/* More code on original implementation of librtlsdr.
	   On this arduino port experiment i want to support 
	   only rafael micro chips */

	/* all gain values are expressed in tenths of a dB */
	const int r82xx_gains[] = { 0, 9, 14, 27, 37, 77, 87, 125, 144, 157,
								166, 197, 207, 229, 254, 280, 297, 328,
						 		338, 364, 372, 386, 402, 421, 434, 439,
						 		445, 480, 496 };
	const int unknown_gains[] = { 0 /* no gain values */ };

	const int *ptr = NULL;
	int len = 0;

	if (!dev)
		return -1;

	switch (dev->tuner_type) {
	case RTLSDR_TUNER_R820T:
	case RTLSDR_TUNER_R828D:
	case RTLSDR_TUNER_R842T:
		ptr = r82xx_gains; len = sizeof(r82xx_gains);
		break;
	default:
		ptr = unknown_gains; len = sizeof(unknown_gains);
		break;
	}

	if (!gains) { /* no buffer provided, just return the count */
		return len / sizeof(int);
	} else {
		if (len)
			memcpy(gains, ptr, len);

		return len / sizeof(int);
	}
}

int rtlsdr_set_freq_correction(rtlsdr_dev_t *dev, int ppm)
{
	int r = 0;

	if (!dev)
		return -1;

	if (dev->corr == ppm)
		return -2;

	dev->corr = ppm;

	//r |= rtlsdr_set_sample_freq_correction(dev, ppm);

	/* read corrected clock value into r82xx structure */
	if (rtlsdr_get_xtal_freq(dev, NULL, &dev->r82xx_c.xtal))
		return -3;

	if (dev->freq) /* retune to apply new correction value */
		r |= rtlsdr_set_center_freq(dev, dev->freq);

	return r;
}












int verbose_set_frequency(rtlsdr_dev_t *dev, uint32_t frequency)
{
	int r;
	r = rtlsdr_set_center_freq(dev, frequency);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set center freq.\n");
	} else {
		fprintf(stderr, "Tuned to %lu Hz.\n", (unsigned long)frequency);
	}
	return r;
}

int verbose_set_sample_rate(rtlsdr_dev_t *dev, uint32_t samp_rate)
{
	int r;
	r = rtlsdr_set_sample_rate(dev, samp_rate);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set sample rate.\n");
	} else {
		fprintf(stderr, "Sampling at %lu S/s.\n", (unsigned long)samp_rate);
	}
	return r;
}


int verbose_set_bandwidth(rtlsdr_dev_t *dev, uint32_t bandwidth)
{
	int r;
	uint32_t applied_bw = 0;
	/* r = rtlsdr_set_tuner_bandwidth(dev, bandwidth); */
	r = rtlsdr_set_and_get_tuner_bandwidth(dev, bandwidth, &applied_bw, 1 /* =apply_bw */);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set bandwidth.\n");
	} else if (bandwidth > 0) {
		if (applied_bw)
			fprintf(stderr, "Bandwidth parameter %lu Hz resulted in %lu Hz.\n", (unsigned long)bandwidth, (unsigned long)applied_bw);
		else
			fprintf(stderr, "Set bandwidth parameter %lu Hz.\n", (unsigned long)bandwidth);
	} else {
		fprintf(stderr, "Bandwidth set to automatic resulted in %lu Hz.\n", (unsigned long)applied_bw);
	}
	return r;
}


// int verbose_offset_tuning(rtlsdr_dev_t *dev)
// {
// 	int r;
// 	r = rtlsdr_set_offset_tuning(dev, 1);
// 	if (r != 0) {
// 		if ( r == -2 )
// 			fprintf(stderr, "WARNING: Failed to set offset tuning: tuner doesn't support offset tuning!\n");
// 		else if ( r == -3 )
// 			fprintf(stderr, "WARNING: Failed to set offset tuning: direct sampling not combinable with offset tuning!\n");
// 		else
// 			fprintf(stderr, "WARNING: Failed to set offset tuning.\n");
// 	} else {
// 		fprintf(stderr, "Offset tuning mode enabled.\n");
// 	}
// 	return r;
// }

int verbose_auto_gain(rtlsdr_dev_t *dev)
{
	int r;
	r = rtlsdr_set_tuner_gain_mode(dev, 0);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
	} else {
		fprintf(stderr, "Tuner gain set to automatic.\n");
	}
	return r;
}

int verbose_gain_set(rtlsdr_dev_t *dev, int gain)
{
	int r;
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	r = rtlsdr_set_tuner_gain(dev, gain);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
	} else {
		fprintf(stderr, "Tuner gain set to %0.2f dB.\n", gain/10.0);
	}
	return r;
}

int verbose_ppm_set(rtlsdr_dev_t *dev, int ppm_error)
{
	int r;
	if (ppm_error == 0) {
		return 0;}
	r = rtlsdr_set_freq_correction(dev, ppm_error);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to set ppm error.\n");
	} else {
		fprintf(stderr, "Tuner error set to %i ppm.\n", ppm_error);
	}
	return r;
}

// int verbose_reset_buffer(rtlsdr_dev_t *dev)
// {
// 	int r;
// 	r = rtlsdr_reset_buffer(dev);
// 	if (r < 0) {
// 		fprintf(stderr, "WARNING: Failed to reset buffers.\n");}
// 	return r;
// }

int nearest_gain(rtlsdr_dev_t *dev, int target_gain)
{
	int i, r, err1, err2, count, nearest;
	int* gains;
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	count = rtlsdr_get_tuner_gains(dev, NULL);
	if (count <= 0) {
		return 0;
	}
	gains = malloc(sizeof(int) * count);
	count = rtlsdr_get_tuner_gains(dev, gains);
	nearest = gains[0];
	for (i=0; i<count; i++) {
		err1 = abs(target_gain - nearest);
		err2 = abs(target_gain - gains[i]);
		if (err2 < err1) {
			nearest = gains[i];
		}
	}
	free(gains);
	return nearest;
}

/* definition order must match enum rtlsdr_tuner */
static rtlsdr_tuner_iface_t tuners[] = {
	{
		NULL, NULL, NULL, NULL, NULL, NULL, NULL /* dummy for unknown tuners */
	},
	{
		r820t_init, r820t_exit,
		r820t_set_freq, r820t_set_bw, r820t_set_gain, NULL,
		r820t_set_gain_mode
	},
	{
		r820t_init, r820t_exit,
		r820t_set_freq, r820t_set_bw, r820t_set_gain, NULL,
		r820t_set_gain_mode
	},
	{
		r820t_init, r820t_exit,
		r820t_set_freq, r820t_set_bw, r820t_set_gain, NULL,
		r820t_set_gain_mode
	},
};





