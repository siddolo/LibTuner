#include <Stdinout.h>	// Redirect stdin/out/err to Serial
#include <LibTuner.h>

#define DEBUG 1

#define DEF_RTL_XTAL_FREQ	28800000

#define DEFAULT_SAMPLE_RATE		2048000
#define DEFAULT_BANDWIDTH		0	/* automatic bandwidth */
#define DEFAULT_BUF_LENGTH		(16 * 16384)
#define MINIMAL_BUF_LENGTH		512
#define MAXIMAL_BUF_LENGTH		(256 * 16384)

void exit(int code) {
	/* simulate exit() avoiding Arduino loop() */
	fprintf(stderr, "exit(%d)\n", code);

	while(1) {
		/* do nothing */
		delay(100);
	}
}

void setup() {
	Serial.begin (115200);
	STDIO.open (Serial); // connect serial port to stdin/out/err
	//STDIO.close (); // release stdin/out/err when done with it
	
	#ifdef DEBUG
		fprintf(stderr, "\nArduino.Setup()\n");
	#endif

	int n_read;
	int r, opt;
	int gain = 0;
	int ppm_error = 0;
	int sync_mode = 0;
	FILE *file;
	uint8_t *buffer;
	int dev_index = 0;
	int dev_given = 0;
	//uint32_t frequency = 100000000;
	uint32_t frequency = 102000000;
	uint32_t bandwidth = DEFAULT_BANDWIDTH;
	uint32_t samp_rate = DEFAULT_SAMPLE_RATE;
	uint32_t out_block_size = DEFAULT_BUF_LENGTH;

	rtlsdr_dev_t *dev = NULL;

	dev = malloc(sizeof(rtlsdr_dev_t));
	memset(dev, 0, sizeof(rtlsdr_dev_t));

	dev->rtl_xtal = DEF_RTL_XTAL_FREQ;

	dev->tuner_type = RTLSDR_TUNER_R842T;
	//dev->tuner_type = RTLSDR_TUNER_R820T;

	/* use the rtl clock value by default */
	dev->tun_xtal = dev->rtl_xtal;
	dev->tuner = &tuners[dev->tuner_type];


	switch (dev->tuner_type) {
	case RTLSDR_TUNER_R828D:
		dev->tun_xtal = R828D_XTAL_FREQ;
		break;
	case RTLSDR_TUNER_R820T:
		break;
	case RTLSDR_TUNER_R842T:
		//dev->tun_xtal = R828D_XTAL_FREQ; // ???
		break;
	case RTLSDR_TUNER_UNKNOWN:
		fprintf(stderr, "No supported tuner found\n");
		break;
	default:
		break;
	}

	if (dev->tuner->init)
		r = dev->tuner->init(dev);
	
	if (r < 0) {
		fprintf(stderr, "Failed to init the tuner\n");
		exit(r);
	}

	/* Set the sample rate */
	verbose_set_sample_rate(dev, samp_rate);

	/* Set the tuner bandwidth */
	verbose_set_bandwidth(dev, bandwidth);

	/* Set the frequency */
	verbose_set_frequency(dev, frequency);

	if (gain == 0) {
	 	 /* Enable automatic gain */
	 	verbose_auto_gain(dev);
	} else {
		/* Enable manual gain */
		gain = nearest_gain(dev, gain);
		verbose_gain_set(dev, gain);
	}

	verbose_ppm_set(dev, ppm_error);

	/* Reset endpoint before we start reading from it (mandatory) */
	//	verbose_reset_buffer(dev);

	int i;
	for (i=1; i<=255; i++) {
		delay(2000);
		verbose_gain_set(dev, i);
	}


	#ifdef DEBUG
		fprintf(stderr, "End Setup\n");
	#endif

}

void loop() {  
}
