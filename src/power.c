#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "power.h"

static int read_line(const char *dir, const char *name, char *buf, size_t n) {
	char path[512];
	FILE *f;
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	if (!(f = fopen(path, "re")))
		return -1;
	if (!fgets(buf, (int)n, f)) {
		fclose(f);
		return -1;
	}
	fclose(f);
	buf[strcspn(buf, "\n")] = '\0';
	return 0;
}

static long long read_num(const char *dir, const char *name) {
	char buf[32], *end;
	long long v;
	if (read_line(dir, name, buf, sizeof(buf)))
		return -1;
	v = strtoll(buf, &end, 10);
	return end == buf || *end || v < 0 ? -1 : v;
}

static int battery_of(const char *dir, long long *now, long long *full) {
	static const char *pairs[][2] = {{"energy_now", "energy_full"},
	                                 {"charge_now", "charge_full"}};
	for (int i = 0; i < 2; i++) {
		*now = read_num(dir, pairs[i][0]);
		*full = read_num(dir, pairs[i][1]);
		if (*now >= 0 && *full > 0)
			return i;
	}
	*now = read_num(dir, "capacity");
	*full = 100;
	return *now >= 0 ? 2 : -1;
}

static int percent(long long now, long long full) {
	long long p = (now * 100 + full / 2) / full;
	return p > 100 ? 100 : (int)p;
}

static int backlight_rank(const char *type) {
	return !strcmp(type, "raw")        ? 3
	       : !strcmp(type, "platform") ? 2
	       : !strcmp(type, "firmware") ? 1
	                                   : 0;
}

void power_read(const char *sys, struct power *p) {
	char base[256], dir[512], buf[32];
	long long sum_now[3] = {0}, sum_full[3] = {0}, sum_pct = 0;
	int n = 0, of_kind[3] = {0}, rank = 0;
	struct dirent *e;
	DIR *d;

	p->battery = p->brightness = -1;
	p->charging = 0;
	snprintf(base, sizeof(base), "%s/class/power_supply", sys);
	if ((d = opendir(base))) {
		while ((e = readdir(d))) {
			long long now, full;
			int kind;
			if (e->d_name[0] == '.')
				continue;
			snprintf(dir, sizeof(dir), "%s/%s", base, e->d_name);
			if (read_line(dir, "type", buf, sizeof(buf)) ||
			    strcmp(buf, "Battery") ||
			    (!read_line(dir, "scope", buf, sizeof(buf)) &&
			     !strcmp(buf, "Device")) ||
			    (kind = battery_of(dir, &now, &full)) < 0)
				continue;
			sum_now[kind] += now;
			sum_full[kind] += full;
			sum_pct += percent(now, full);
			of_kind[kind]++;
			n++;
			if (!read_line(dir, "status", buf, sizeof(buf)) &&
			    !strcmp(buf, "Charging"))
				p->charging = 1;
		}
		closedir(d);
	}
	for (int k = 0; k < 3 && n; k++)
		if (of_kind[k] == n)
			p->battery = percent(sum_now[k], sum_full[k]);
	if (n && p->battery < 0)
		p->battery = (int)((sum_pct + n / 2) / n);
	snprintf(base, sizeof(base), "%s/class/backlight", sys);
	if ((d = opendir(base))) {
		while ((e = readdir(d))) {
			long long now, max;
			int r;
			if (e->d_name[0] == '.')
				continue;
			snprintf(dir, sizeof(dir), "%s/%s", base, e->d_name);
			if (read_line(dir, "type", buf, sizeof(buf)))
				continue;
			r = backlight_rank(buf);
			now = read_num(dir, "brightness");
			max = read_num(dir, "max_brightness");
			if (r > rank && now >= 0 && max > 0) {
				rank = r;
				p->brightness = percent(now, max);
			}
		}
		closedir(d);
	}
}

const char *power_battery_state(const struct power *p) {
	if (p->battery < 0)
		return NULL;
	if (p->charging)
		return "charging";
	return p->battery <= 15   ? "empty"
	       : p->battery <= 35 ? "low"
	       : p->battery <= 60 ? "half"
	       : p->battery <= 85 ? "high"
	                          : "full";
}

const char *power_brightness_state(const struct power *p) {
	if (p->brightness < 0)
		return NULL;
	return p->brightness <= 25   ? "low"
	       : p->brightness <= 50 ? "half"
	       : p->brightness <= 75 ? "high"
	                             : "full";
}

int power_uevent(const char *msg, size_t len) {
	for (size_t i = 0; i < len; i += strnlen(msg + i, len - i) + 1)
		if (!strncmp(msg + i, "SUBSYSTEM=power_supply", len - i) ||
		    !strncmp(msg + i, "SUBSYSTEM=backlight", len - i))
			return 1;
	return 0;
}
