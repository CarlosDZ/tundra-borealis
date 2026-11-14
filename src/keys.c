#include <string.h>

#include "keys.h"

struct bind {
	uint16_t code;
	uint8_t shift;
	uint8_t action;
};

static const struct bind binds[] = {
    {KEY_TAB, 0, KEYS_FOCUS_NEXT},  {KEY_TAB, 1, KEYS_SWAP_NEXT},
    {KEY_H, 0, KEYS_LEFT},          {KEY_H, 1, KEYS_SEND_LEFT},
    {KEY_L, 0, KEYS_RIGHT},         {KEY_L, 1, KEYS_SEND_RIGHT},
    {KEY_J, 0, KEYS_DEEPER},        {KEY_J, 1, KEYS_SEND_DEEPER},
    {KEY_K, 0, KEYS_SHALLOWER},     {KEY_K, 1, KEYS_SEND_SHALLOWER},
    {KEY_BACKSPACE, 0, KEYS_CLOSE}, {KEY_F, 0, KEYS_FULLSCREEN},
    {KEY_T, 0, KEYS_TERMINAL},      {KEY_O, 0, KEYS_OTHER_SCREEN},
    {KEY_R, 0, KEYS_ROTATE},        {KEY_M, 0, KEYS_POINTER_LOCK},
    {KEY_A, 0, KEYS_ALERT},         {KEY_S, 0, KEYS_LID_MODE},
    {KEY_G, 0, KEYS_CAPTURE},       {KEY_G, 1, KEYS_RECORD},
    {KEY_ESC, 0, KEYS_LOCK},
};

static int is_modifier(uint32_t code) {
	switch (code) {
	case KEY_LEFTSHIFT:
	case KEY_RIGHTSHIFT:
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
	case KEY_LEFTALT:
	case KEY_RIGHTALT:
	case KEY_LEFTMETA:
	case KEY_RIGHTMETA:
		return 1;
	}
	return 0;
}

static int digit_of(uint32_t code) {
	if (code >= KEY_1 && code <= KEY_9)
		return (int)(code - KEY_1) + 1;
	if (code == KEY_0)
		return 0;
	return -1;
}

static enum keys_action bound(uint32_t code, int shift) {
	for (size_t i = 0; i < sizeof(binds) / sizeof(binds[0]); i++)
		if (binds[i].code == code && binds[i].shift == shift)
			return binds[i].action;
	return KEYS_NONE;
}

static void keep(struct keys *k, uint32_t code) {
	k->kept[code / 8] |= (uint8_t)(1u << (code % 8));
}

static int release(struct keys *k, uint32_t code) {
	uint8_t bit = (uint8_t)(1u << (code % 8));
	if (!(k->kept[code / 8] & bit))
		return 0;
	k->kept[code / 8] &= (uint8_t)~bit;
	return 1;
}

void keys_init(struct keys *k) { memset(k, 0, sizeof(*k)); }

int keys_feed(struct keys *k, uint32_t code, int pressed, unsigned int mods,
              struct keys_result *out) {
	out->action = KEYS_NONE;
	out->h = out->d = 0;
	if (code > KEY_MAX)
		return 0;

	if (!pressed) {
		if (code == KEY_LEFTMETA || code == KEY_RIGHTMETA)
			k->first = 0;
		return release(k, code);
	}
	if (is_modifier(code) || !(mods & KEYS_SUPER))
		return 0;

	keep(k, code);
	if (mods & (KEYS_CTRL | KEYS_ALT)) {
		k->first = 0;
		return 1;
	}
	int shift = (mods & KEYS_SHIFT) != 0;
	int digit = digit_of(code);
	if (digit < 0) {
		k->first = 0;
		out->action = bound(code, shift);
		return 1;
	}
	if (digit < 1 || digit > 4) {
		k->first = 0;
		return 1;
	}
	if (!k->first) {
		k->first = digit;
		k->first_shift = shift;
		return 1;
	}
	out->action = k->first_shift ? KEYS_SEND_TO : KEYS_GOTO;
	out->h = k->first;
	out->d = digit;
	k->first = 0;
	return 1;
}
