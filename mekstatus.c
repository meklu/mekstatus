/* mekstatus, a status provider thing for i3bar.
 * copyleft (CC-0) 2013 Melker "meklu" Narikka
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#define _mekstatus_load_nelem 3

const char *color_red = "#ff0000";
const char *color_cyan = "#00ffff";
const char *color_yellow = "#ffff00";
const char *color_lgrey = "#696969";
const char *color_dgrey = "#424242";
const char *color_white = "#ffffff";

const char *loadnum(float load) {
	if (load < 1) {
		return color_cyan;
	} else if (load < 4) {
		return color_yellow;
	} else {
		return color_red;
	}
}

void colorprint(char *str, const char *color) {
	printf(",{\"full_text\":\"%s\"", str);
	if (strlen(color) > 0) {
		printf(",\"color\":\"%s\"", color);
	}
	printf("}");
}

int main() {
	/* date/time stack */
	char buf[64];
	size_t buflen = 64;
	char *format_d = "%F";
	char *format_t = "%T";
	char *format_z = "%z";
	struct timespec ts_time;
	struct tm *s_time;
	/* load stack */
	double load[_mekstatus_load_nelem];
	/* iterator */
	int i = 0;
	printf("{\"version\":1}\n");
	fflush(stdout);
	printf("[\n");
	fflush(stdout);
	printf("[]\n");
	fflush(stdout);
	do {
		printf(",[{\"full_text\":\"\"}");
		/* date, time */
		clock_gettime(CLOCK_REALTIME, &ts_time);
		s_time = localtime(&ts_time.tv_sec);
		/* date */
		strftime(buf, buflen, format_d, s_time);
		colorprint(buf, color_yellow);
		/* time */
		strftime(buf, buflen, format_t, s_time);
		colorprint(buf, color_cyan);
		/* timezone */
		strftime(buf, buflen, format_z, s_time);
		colorprint(buf, color_lgrey);
		/* load */
		getloadavg(load, _mekstatus_load_nelem);
		for (i = 0; i < _mekstatus_load_nelem - 1; i += 1) {
			loadnum(load[i]);
			sprintf(buf, "%.2f", load[i]);
			colorprint(buf, loadnum(load[i]));
		}
		loadnum(load[_mekstatus_load_nelem - 1]);
		sprintf(buf, "%.2f", load[_mekstatus_load_nelem - 1]);
		colorprint(buf, loadnum(load[i]));
		printf("]\n");
		fflush(stdout);
		/* aligning to second */
		usleep(1000000 - (ts_time.tv_nsec / 1000));
	} while (1);
}
