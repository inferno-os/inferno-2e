
int	touch_read_delay = 50;	/* usec between setup and first reading */
int	touch_l2nreadings = 2;	/* log2 of number of readings to take */
int	touch_minpdelta = 20;	/* minimum pressure difference to 
				   detect a change during calibration */
int	touch_filterlevel = 512; /* -1024(off) to 1024(full): 1024*cos(angle) */

