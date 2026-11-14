#ifndef TUNDRA_KEYS_H
#define TUNDRA_KEYS_H

#include <linux/input-event-codes.h>
#include <stdint.h>

#define KEYS_SUPER 1u
#define KEYS_SHIFT 2u
#define KEYS_CTRL 4u
#define KEYS_ALT 8u

enum keys_action {
	KEYS_NONE,
	KEYS_FOCUS_NEXT,
	KEYS_SWAP_NEXT,
	KEYS_LEFT,
	KEYS_RIGHT,
	KEYS_DEEPER,
	KEYS_SHALLOWER,
	KEYS_SEND_LEFT,
	KEYS_SEND_RIGHT,
	KEYS_SEND_DEEPER,
	KEYS_SEND_SHALLOWER,
	KEYS_GOTO,
	KEYS_SEND_TO,
	KEYS_CLOSE,
	KEYS_FULLSCREEN,
	KEYS_TERMINAL,
	KEYS_OTHER_SCREEN,
	KEYS_ROTATE,
	KEYS_POINTER_LOCK,
	KEYS_ALERT,
	KEYS_LID_MODE,
	KEYS_CAPTURE,
	KEYS_RECORD,
	KEYS_LOCK,
};

struct keys_result {
	enum keys_action action;
	int h, d; /* KEYS_GOTO and KEYS_SEND_TO: the coordinate, 1 to 4 */
};

struct keys {
	int first; /* first digit typed with Super held, 0 if none */
	int first_shift;
	uint8_t kept[(KEY_MAX + 8) / 8]; /* presses the client never saw */
};

void keys_init(struct keys *k);

/* Feeds one key event by its evdev code, so the layout does not matter. mods
 * are the KEYS_ bits held before this event. 1 if the event is borealis's and
 * must not reach the client, 0 if it goes on. *out says what to do. */
int keys_feed(struct keys *k, uint32_t code, int pressed, unsigned int mods,
              struct keys_result *out);

#endif
