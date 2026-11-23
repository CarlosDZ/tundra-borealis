#include "head.c"
#include "test.h"

#include <string.h>

int main(void) {
	char b[HEAD_TEXT];
	struct tm tm = {
	    .tm_year = 126, .tm_mon = 10, .tm_mday = 3, .tm_hour = 9, .tm_min = 5};
	struct head_layout l;

	CHECK(head_target(720) == 16 && head_target(1080) == 24 &&
	          head_target(1440) == 32 && head_target(2160) == 48 &&
	          head_target(10) == 1,
	      "the line aims at 1/45 of the screen's height");
	CHECK(head_height(16) == 22 && head_height(24) == 32 &&
	          head_height(32) == 42 && head_height(48) == 64,
	      "the header is the line and a sixth of it above and below");
	head_place((struct grid_pos){2, 3}, b);
	CHECK(!strcmp(b, "H2 P3"), "the place names both axes");
	head_clock(&tm, b);
	CHECK(!strcmp(b, "2026-11-03  09:05"),
	      "the clock in ISO 8601, without seconds");
	CHECK(head_next_minute(1795424151) == 1795424160 &&
	          head_next_minute(1795424160) == 1795424220,
	      "the next tick is at the start of the next minute");
	head_layout(1920, 24, 60, 204, &l);
	CHECK(l.place_x == 12 && l.clock_x == 858 && l.y == 4,
	      "the place half a line from the left, the clock centred");
	head_layout(200, 24, 60, 204, &l);
	CHECK(l.clock_x == 84, "on a narrow screen the clock goes after the place");
	CHECK(head_icon_scale(24, 12) == 2 && head_icon_scale(16, 12) == 1 &&
	          head_icon_scale(48, 12) == 4 && head_icon_scale(10, 12) == 1 &&
	          head_icon_scale(24, 0) == 1,
	      "an icon at the whole scale that fits the line");
	{
		int iw[2] = {24, 24}, nw[2] = {48, 36}, ix[2], nx[2];
		head_indicators(1920, 24, 2, iw, nw, ix, nx);
		CHECK(nx[1] == 1872 && ix[1] == 1842 && nx[0] == 1770 && ix[0] == 1740,
		      "the indicators from the right margin, the first leftmost");
	}
	return test_summary();
}
