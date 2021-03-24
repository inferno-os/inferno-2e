// TouchScreen Calibration data for most DynaPro screens
// (generally used for Brutus, Caesar, and SWoRD boards--
// however with the LCD screen used on most SWoRD boards,
// it is necessary to roughly double all the parameters
// due to lots of interference)

#include <u.h>
#include "ucbtouch.h"

int	touch_read_delay = 50;	/* usec between setup and first reading */
int	touch_l2nreadings = 2;	/* log2 of number of readings to take */
int	touch_minpdelta = 20;	/* minimum pressure difference to 
			   detect a change during calibration */
int	touch_filterlevel = 512; /* -1024(off) to 1024(full): 1024*cos(angle) */


TouchCal touchcal = {
	{
		{10, 10}, {630, 10}, {630, 470}, {10, 470},
	},
        {
            {   {119, 923}, {109, 76}, {890, 73}, {894, 926}, },
            {   {120, 75}, {109, 923}, {890, 926}, {893, 73}, },
            {   {857, 923}, {868, 76}, {87, 73}, {83, 926}, },
            {   {856, 75}, {867, 922}, {88, 925}, {83, 73}, },
        },
        {
            {   -184, -47967, 38596, -455, 45094048, -3399586 },
            {   -184, 47916, 38600, 500, -2797228, -3859260 },
            {   184, -47972, -38600, -501, 44918994, 34333815 },
            {   184, 47965, -38693, 502, -2980438, 33857856 },
        },
        {3, 3},
        {2, 2},
        276, 198,
};

