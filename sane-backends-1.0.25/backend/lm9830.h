/*
  (c) 2001 Nathan Rutman  nathan@gordian.com  11/13/01

  National Semi LM9830 scanner-on-a-chip register defs
*/

#ifndef LM9830_H
#define LM9830_H


/* LM9830 registers (see lm9830 datasheet for a full description */
#define IMAGE_DATA_AVAIL 0x01
#define STATUS		0x02
#define DATAPORT_TARGET	0x03
#define DATAPORT_ADDR	0x04
#define DATAPORT	0x06
#define COMMAND		0x07
#define CLOCK_DIV	0x08	/* Master clock divider */
#define ACTIVE_PX_START 0x1e	/* Active pixel start */
#define LINE_END	0x20
#define DATA_PX_START	0x22
#define DATA_PX_END	0x24
#define COLOR_MODE	0x26
#define LAMP_R_ON	0x2c
#define LAMP_R_OFF	0x2e
#define LAMP_G_ON	0x30
#define LAMP_G_OFF	0x32
#define LAMP_B_ON	0x34
#define LAMP_B_OFF	0x36
#define PARALLEL_PORT	0x42	/* Parallel port settings */
#define MICROSTEP	0x45
#define STEP_SIZE	0x46
#define FAST_STEP	0x48
#define SKIP_STEPS	0x4a
#define BUFFER_LIMIT 	0x4e	/* Pause scanning when buffer is this full */
#define BUFFER_RESUME	0x4f
#define REVERSE_STEPS	0x50
#define STEP_PWM	0x57
#define PAPER_SENSOR	0x58

/* Register 02 flags */
#define	STATUS_HOME  	0x02
#define STATUS_PAUSE 	0x10
#define STATUS_POWER 	0x20

/* Register 03 flags */
#define	DP_GAMMA  	0x00
#define	DP_OFFSET  	0x01
#define	DP_R  	0x00
#define	DP_G  	0x02
#define	DP_B  	0x04

/* Register 04 flags */
#define DP_WRITE 	0x0000
#define DP_READ 	0x2000


#endif /* LM9830_H */
