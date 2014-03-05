/* mekstatus, a status provider thing for i3bar.
 * copyleft (CC-0) 2013 Melker "meklu" Narikka
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

/* battery id
 * if this doesn't work, take a look in /sys/class/power_supply/
 * desktop users need not worry, it won't show up on your rigs */
#define _mekstatus_battery_id "BAT0"
/* the number of load elements, see getloadavg(3)
 * the maximum value is usually 3 */
#define _mekstatus_load_nelem 3

/* some color constants */
const char *color_red = "#ff0000";
const char *color_cyan = "#00ffff";
const char *color_green = "#00ff00";
const char *color_yellow = "#ffff00";
const char *color_grey = "#696969";

static inline double mstatclamp(double val, double min, double max) { return (val >= min) ? ((val <= max) ? val : max) : min; }

const char *battstatuscolor(const char *status) {
	if (strncmp(status, "-", 2) == 0) {
		return color_red;
	} else if (strncmp(status, "+", 2) == 0) {
		return color_green;
	} else if (strncmp(status, "~", 2) == 0) {
		return color_cyan;
	}
	return color_yellow;
}

typedef struct {
	double r;
	double g;
	double b;
} color;

double getdeltapos(double abspos, double rangestart, double rangeend) {
	double len = rangeend - rangestart;
	double relpos = abspos - rangestart;
	return mstatclamp(relpos / len, 0.0, 1.0);
}

color blendcolor(color start, color end, double pos) {
	color retcol = { 0.0, 0.0, 0.0 };
	retcol.r = ((1.0 - pos) * start.r) + (pos * end.r);
	retcol.g = ((1.0 - pos) * start.g) + (pos * end.g);
	retcol.b = ((1.0 - pos) * start.b) + (pos * end.b);
	return retcol;
}

char *percentagecolor(double percentage) {
	char *buf = (char *) malloc(8); /* 6 hex chars + hash + \0 */
	color red = { 1.0, 0.0, 0.0 };
	color yellow = { 1.0, 1.0, 0.0 };
	color green = { 0.0, 1.0, 0.0 };
	color cyan = { 0.0, 1.0, 1.0 };
	color retcol = { 0.0, 0.0, 0.0 };
	if (percentage <= 50.0) {
		retcol = blendcolor(red, yellow, getdeltapos(percentage, 0.0, 50.0));
	} else if (percentage <= 75.0) {
		retcol = blendcolor(yellow, green, getdeltapos(percentage, 50.0, 75.0));
	} else { /* if (percentage <= 100.0) { */
		retcol = blendcolor(green, cyan, getdeltapos(percentage, 75.0, 100.0));
	}
	snprintf(buf, 8, "#%.2x%.2x%.2x",
		(int) (retcol.r * 255.0),
		(int) (retcol.g * 255.0),
		(int) (retcol.b * 255.0)
	);
	return buf;
}

void colorprint(const char *str, const char *color) {
	printf(",{\"full_text\":\"%s\"", str);
	if (strlen(color) > 0) {
		printf(",\"color\":\"%s\"", color);
	}
	printf("}");
}

/* sets up a file handle for a battery property
 * features a convoluted filter system */
int checkbatteryproperty(const char *identifier, const char *property, int (*filter)(FILE *, void *), void *userbuf) {
	FILE *batt_f;
	const char *batt_prefix = "/sys/class/power_supply/";
	char *buf = (char *) malloc(strlen(batt_prefix) + strlen(identifier) + 1 + strlen(property) + 1);
	int filtret;

	sprintf(buf, "%s%s/%s", batt_prefix, identifier, property);
	batt_f = fopen(buf, "r");
	if (batt_f == NULL) {
		return 0;
	}
	filtret = filter(batt_f, userbuf);
	fclose(batt_f);
	free(buf);
	return filtret;
}

/* some of the aforementioned filters */
int getstrfilt(FILE *f, void *buf) {
	int ret = 0;
	/* 32 bytes ought to be enough for anybody */
	if (fscanf(f, "%31s", (char *) buf) > 0) {
		return ret;
	}
	return 0;
}

/* this and the filter below it return their value as well, making the buffer
 * indesirable in most cases; hence 'ignorebuf' */
int getnumfilt(FILE *f, void *ignorebuf) {
	int ret = 0;
	if (fscanf(f, "%d", &ret) > 0) {
		if (ignorebuf != NULL) {
			*(int *) ignorebuf = ret;
		}
		return ret;
	}
	if (ignorebuf != NULL) {
		*(int *) ignorebuf = 0;
	}
	return 0;
}

int isbatteryfilt(FILE *f, void *ignorebuf) {
	const char *bat_s = "Battery";
	char *buf = (char *) malloc(64);
	if (fscanf(f, "%s", buf) > 0) {
		if (strncmp(bat_s, buf, sizeof(bat_s) * sizeof(char)) == 0) {
			free(buf);
			if (ignorebuf != NULL) {
				*(int *) ignorebuf = 1;
			}
			return 1;
		}
	}
	free(buf);
	if (ignorebuf != NULL) {
		*(int *) ignorebuf = 0;
	}
	return 0;
}

/* gets the full charge value */
int getbatteryfull(const char *identifier) {
	return checkbatteryproperty(identifier, "charge_full", getnumfilt, NULL);
}

/* gets the current charge */
int getbatterycharge(const char *identifier) {
	return checkbatteryproperty(identifier, "charge_now", getnumfilt, NULL);
}

/* gets a nice looking character representing battery status */
int getbatterystatus(const char *identifier, char *buf) {
	char fbuf[32];
	int ret;
	strncpy(fbuf, buf, sizeof(fbuf) - sizeof(fbuf[0]));
	fbuf[31] = '\0';
	ret = checkbatteryproperty(identifier, "status", getstrfilt, (void *) buf);
	/* gotta get dem funroll oops, hence only comparing 4 bytes */
	if (strncmp(buf, "Discharging", 4) == 0) {
		strcpy(buf, "-");
	} else if (strncmp(buf, "Charging", 4) == 0) {
		strcpy(buf, "+");
	} else if (strncmp(buf, "Full", 4) == 0) {
		strcpy(buf, "~");
	} else {
		/* if we only had a single character in the previous buffer,
		 * use it instead of '?' because it might look nicer */
		if (fbuf[1] == '\0') {
			strcpy(buf, fbuf);
		} else {
			strcpy(buf, "?");
		}
	}
	return ret;
}

/* checks whether the battery exists or not */
int hasbattery(const char *identifier) {
	return checkbatteryproperty(identifier, "type", isbatteryfilt, NULL);
}

int main() {
	/* battery stack */
	const char *batt_name = _mekstatus_battery_id;
	int batt_use = hasbattery(batt_name);
	int batt_charge_full;
	int batt_charge_curr;
	char batt_charge_status[32];
	char *batt_color;
	double batt_charge_prc;
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
		colorprint(buf, color_grey);
		/* load */
		getloadavg(load, _mekstatus_load_nelem);
		for (i = 0; i < _mekstatus_load_nelem - 1; i += 1) {
			snprintf(buf, sizeof(buf), "%.2f", load[i]);
			colorprint(buf, percentagecolor(100.0 - 20.0 * mstatclamp(load[i], 0.0, 5.0)));
		}
		snprintf(buf, sizeof(buf), "%.2f", load[_mekstatus_load_nelem - 1]);
		colorprint(buf, percentagecolor(100.0 - 20.0 * mstatclamp(load[i], 0.0, 5.0)));
		/* battery, if any exists */
		if (batt_use) {
			/* update values */
			batt_charge_full = getbatteryfull(batt_name);
			batt_charge_curr = getbatterycharge(batt_name);
			getbatterystatus(batt_name, batt_charge_status);
			batt_charge_prc = (double) 100 * (double) batt_charge_curr / (double) batt_charge_full;
			batt_color = percentagecolor(batt_charge_prc);
			/* print it */
			snprintf(buf, sizeof(buf), "%.2f%%", batt_charge_prc);
			colorprint(buf, batt_color);
			colorprint(batt_charge_status, battstatuscolor(batt_charge_status));
			free(batt_color);
		}
		printf("]\n");
		fflush(stdout);
		/* aligning to second */
		ts_time.tv_sec += 1;
		ts_time.tv_nsec = 0;
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts_time, NULL);
	} while (1);
}
