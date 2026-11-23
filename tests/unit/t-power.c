#include "power.c"
#include "test.h"

#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

static char root[64];

static void put(const char *path, const char *text) {
	char p[256];
	FILE *f;
	snprintf(p, sizeof(p), "%s/%s", root, path);
	for (char *c = p + strlen(root) + 1; *c; c++)
		if (*c == '/') {
			*c = '\0';
			mkdir(p, 0700);
			*c = '/';
		}
	if ((f = fopen(p, "w"))) {
		fputs(text, f);
		fclose(f);
	}
}

static void drop(const char *dir) {
	char cmd[160];
	snprintf(cmd, sizeof(cmd), "rm -rf '%s/%s'", root, dir);
	if (system(cmd) != 0)
		printf("# cannot remove %s\n", dir);
}

static int states(int battery, int charging, const char *want) {
	struct power p = {battery, charging, -1};
	const char *got = power_battery_state(&p);
	return got && !strcmp(got, want);
}

static int moon(int brightness, const char *want) {
	struct power p = {-1, 0, brightness};
	const char *got = power_brightness_state(&p);
	return got && !strcmp(got, want);
}

int main(void) {
	const char *tmp = getenv("TMPDIR");
	struct power p;
	static const char plugged[] = "change@/devices/LNXSYSTM:00/ACPI0003:00\0"
	                              "ACTION=change\0SUBSYSTEM=power_supply\0";
	static const char light[] = "change@/devices/pci/backlight\0"
	                            "SUBSYSTEM=backlight\0SEQNUM=12\0";
	static const char other[] = "add@/devices/usb1\0SUBSYSTEM=usb\0"
	                            "DEVTYPE=SUBSYSTEM=power_supply_x\0";

	snprintf(root, sizeof(root), "%s/borealis-sys-XXXXXX", tmp ? tmp : "/tmp");
	if (!mkdtemp(root)) {
		perror("mkdtemp error");
		return 1;
	}
	power_read(root, &p);
	CHECK(p.battery == -1 && p.brightness == -1 && !p.charging &&
	          !power_battery_state(&p) && !power_brightness_state(&p),
	      "without a battery or a backlight, nothing to show");

	put("class/power_supply/BAT0/type", "Battery\n");
	put("class/power_supply/BAT0/status", "Discharging\n");
	put("class/power_supply/BAT0/energy_now", "30000000\n");
	put("class/power_supply/BAT0/energy_full", "60000000\n");
	put("class/power_supply/BAT1/type", "Battery\n");
	put("class/power_supply/BAT1/energy_now", "10000000\n");
	put("class/power_supply/BAT1/energy_full", "40000000\n");
	put("class/power_supply/AC/type", "Mains\n");
	put("class/power_supply/AC/online", "1\n");
	put("class/power_supply/mouse/type", "Battery\n");
	put("class/power_supply/mouse/scope", "Device\n");
	put("class/power_supply/mouse/capacity", "5\n");
	power_read(root, &p);
	CHECK(p.battery == 40 && !p.charging,
	      "two batteries add up what they hold; a mouse's is not counted");
	put("class/power_supply/BAT1/status", "Charging\n");
	power_read(root, &p);
	CHECK(p.charging && !strcmp(power_battery_state(&p), "charging"),
	      "one charging is the system charging");
	drop("class/power_supply/BAT1");
	put("class/power_supply/BAT2/type", "Battery\n");
	put("class/power_supply/BAT2/capacity", "90\n");
	power_read(root, &p);
	CHECK(p.battery == 70, "of mixed kinds, their percentages are averaged");
	drop("class/power_supply/BAT2");
	put("class/power_supply/BAT0/energy_now", "bad\n");
	put("class/power_supply/BAT0/charge_now", "1500\n");
	put("class/power_supply/BAT0/charge_full", "2000\n");
	power_read(root, &p);
	CHECK(p.battery == 75, "charge when there is no energy");

	put("class/backlight/acpi_video0/type", "firmware\n");
	put("class/backlight/acpi_video0/brightness", "100\n");
	put("class/backlight/acpi_video0/max_brightness", "100\n");
	put("class/backlight/intel_backlight/type", "raw\n");
	put("class/backlight/intel_backlight/brightness", "300\n");
	put("class/backlight/intel_backlight/max_brightness", "1200\n");
	power_read(root, &p);
	CHECK(p.brightness == 25 && !strcmp(power_brightness_state(&p), "low"),
	      "the panel's backlight goes before the firmware's");

	CHECK(states(0, 0, "empty") && states(15, 0, "empty") &&
	          states(16, 0, "low") && states(35, 0, "low") &&
	          states(36, 0, "half") && states(60, 0, "half") &&
	          states(61, 0, "high") && states(85, 0, "high") &&
	          states(86, 0, "full") && states(100, 0, "full") &&
	          states(5, 1, "charging"),
	      "the battery's states end at 15, 35, 60 and 85 (DR-181)");
	CHECK(moon(0, "low") && moon(25, "low") && moon(26, "half") &&
	          moon(50, "half") && moon(51, "high") && moon(75, "high") &&
	          moon(76, "full") && moon(100, "full"),
	      "the moon grows by quarters");
	CHECK(power_uevent(plugged, sizeof(plugged) - 1) &&
	          power_uevent(light, sizeof(light) - 1) &&
	          !power_uevent(other, sizeof(other) - 1),
	      "uevents of power supplies and backlights, and no others");
	drop("");
	return test_summary();
}
