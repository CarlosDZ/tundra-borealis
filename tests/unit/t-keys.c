#include "keys.c"
#include "test.h"

static struct keys k;
static struct keys_result r;

#define S KEYS_SUPER
#define SH (KEYS_SUPER | KEYS_SHIFT)

static int press(uint32_t code, unsigned int mods) {
	return keys_feed(&k, code, 1, mods, &r);
}

static int lift(uint32_t code, unsigned int mods) {
	return keys_feed(&k, code, 0, mods, &r);
}

static int is(enum keys_action a, int h, int d) {
	return r.action == a && r.h == h && r.d == d;
}

static void binds_one(uint32_t code, unsigned int mods, enum keys_action a,
                      const char *desc) {
	keys_init(&k);
	int kept = press(code, mods);
	CHECK(kept && is(a, 0, 0), desc);
}

int main(void) {
	binds_one(KEY_TAB, S, KEYS_FOCUS_NEXT, "Super Tab moves the focus");
	binds_one(KEY_TAB, SH, KEYS_SWAP_NEXT, "Super Shift Tab moves the window");
	binds_one(KEY_H, S, KEYS_LEFT, "Super h goes left");
	binds_one(KEY_L, S, KEYS_RIGHT, "Super l goes right");
	binds_one(KEY_J, S, KEYS_DEEPER, "Super j goes to the next depth");
	binds_one(KEY_K, S, KEYS_SHALLOWER, "Super k goes to the previous depth");
	binds_one(KEY_H, SH, KEYS_SEND_LEFT, "Super Shift h sends left");
	binds_one(KEY_L, SH, KEYS_SEND_RIGHT, "Super Shift l sends right");
	binds_one(KEY_J, SH, KEYS_SEND_DEEPER, "Super Shift j sends deeper");
	binds_one(KEY_K, SH, KEYS_SEND_SHALLOWER, "Super Shift k sends up");
	binds_one(KEY_BACKSPACE, S, KEYS_CLOSE, "Super Backspace closes");
	binds_one(KEY_F, S, KEYS_FULLSCREEN, "Super f is fullscreen");
	binds_one(KEY_T, S, KEYS_TERMINAL, "Super t opens the terminal");
	binds_one(KEY_O, S, KEYS_OTHER_SCREEN, "Super o is the other screen");
	binds_one(KEY_R, S, KEYS_ROTATE, "Super r rotates the preference");
	binds_one(KEY_M, S, KEYS_POINTER_LOCK, "Super m locks the pointer");
	binds_one(KEY_A, S, KEYS_ALERT, "Super a attends a notice");
	binds_one(KEY_S, S, KEYS_LID_MODE, "Super s changes the lid mode");
	binds_one(KEY_G, S, KEYS_CAPTURE, "Super g captures");
	binds_one(KEY_G, SH, KEYS_RECORD, "Super Shift g records");
	binds_one(KEY_ESC, S, KEYS_LOCK, "Super Esc locks");
	binds_one(KEY_F, SH, KEYS_NONE, "Super Shift f is kept but does nothing");
	binds_one(KEY_X, S, KEYS_NONE, "an unbound key with Super is kept");
	binds_one(KEY_CAPSLOCK, S, KEYS_NONE, "Super Caps Lock is not Super Esc");

	keys_init(&k);
	press(KEY_2, S);
	CHECK(is(KEYS_NONE, 0, 0), "a first digit waits");
	press(KEY_3, S);
	CHECK(is(KEYS_GOTO, 2, 3), "Super 2 3 goes to 23");

	keys_init(&k);
	press(KEY_2, SH);
	press(KEY_3, SH);
	CHECK(is(KEYS_SEND_TO, 2, 3),
	      "Shift 2 is still 2: Super Shift 2 3 sends to 23");

	keys_init(&k);
	press(KEY_2, SH);
	press(KEY_3, S);
	CHECK(is(KEYS_SEND_TO, 2, 3), "Shift at the first digit decides");

	keys_init(&k);
	press(KEY_2, S);
	lift(KEY_2, S);
	lift(KEY_LEFTMETA, S);
	press(KEY_3, S);
	CHECK(is(KEYS_NONE, 0, 0), "releasing Super after one digit drops it");
	press(KEY_1, S);
	CHECK(is(KEYS_GOTO, 3, 1), "and the next pair starts again");

	keys_init(&k);
	press(KEY_2, S);
	press(KEY_5, S);
	CHECK(is(KEYS_NONE, 0, 0), "a digit out of 1 to 4 does nothing");
	press(KEY_3, S);
	CHECK(is(KEYS_NONE, 0, 0), "and drops the first one");
	press(KEY_4, S);
	CHECK(is(KEYS_GOTO, 3, 4), "the next two digits work");

	keys_init(&k);
	press(KEY_0, S);
	press(KEY_1, S);
	press(KEY_1, S);
	CHECK(is(KEYS_GOTO, 1, 1), "0 is out of range too");

	keys_init(&k);
	press(KEY_2, S);
	press(KEY_H, S);
	CHECK(is(KEYS_LEFT, 0, 0), "another key drops the digit and acts");
	press(KEY_3, S);
	CHECK(is(KEYS_NONE, 0, 0), "so the next digit is a first one");

	keys_init(&k);
	CHECK(press(KEY_H, KEYS_SUPER | KEYS_CTRL) && is(KEYS_NONE, 0, 0),
	      "Super Ctrl h is kept and does nothing");
	CHECK(press(KEY_H, KEYS_SUPER | KEYS_ALT) && is(KEYS_NONE, 0, 0),
	      "Super Alt h is kept and does nothing");

	keys_init(&k);
	CHECK(!press(KEY_H, 0), "h without Super goes to the client");
	CHECK(!lift(KEY_H, 0), "and so does its release");
	CHECK(!press(KEY_LEFTMETA, 0), "Super itself goes to the client");
	CHECK(!press(KEY_LEFTSHIFT, S), "Shift with Super goes to the client");
	CHECK(press(KEY_H, S), "Super h is kept");
	CHECK(lift(KEY_H, 0), "its release is kept even after Super is up");
	CHECK(!lift(KEY_H, 0), "a second release goes on");
	CHECK(!press(KEY_MAX + 1, S), "a code past KEY_MAX goes on");

	return test_summary();
}
