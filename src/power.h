#ifndef TUNDRA_POWER_H
#define TUNDRA_POWER_H

#include <stddef.h>

struct power {
	int battery;
	int charging;
	int brightness;
};

void power_read(const char *sys, struct power *p);

const char *power_battery_state(const struct power *p);
const char *power_brightness_state(const struct power *p);

int power_uevent(const char *msg, size_t len);

#endif
