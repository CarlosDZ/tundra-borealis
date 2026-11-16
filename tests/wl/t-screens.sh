. tests/wl/lib.sh

export WLR_HEADLESS_OUTPUTS=2
export TUNDRA_BOREALIS_DIR=$WORK/fixed
mkdir -p "$TUNDRA_BOREALIS_DIR"
printf 'p {\n\tcommand = "exec %s list >/dev/null"\n\tplace = 0 0\n\tsize = 100 100\n\tprivileges = none\n}\n' \
	"$CLIENT" >"$TUNDRA_BOREALIS_DIR/canvas-42"

if ! start; then
	not_ok "starts with two screens"
	finish
	exit 1
fi

screens() {
	ask screens | awk '$1 == "screens"'
}

check "two screens start extended, the second on the left" \
	[ "$(screens)" = "screens extended main 1280 0 1280 720 second 0 0 1280 720" ]
check "11 has the keyboard, on the second screen" [ "$(canvas)" = 11 ]

open a
wait_window a
check "a window on 11 lies on the second screen" [ "$(field a 6)" -lt 1280 ]
chord 125 24
check "Super + O moves the keyboard to 21, on the main screen" [ "$(canvas)" = 21 ]
check "and the pointer with it" [ "$(ask pointer | awk '$1 == "pointer" { print $2 }')" -ge 1280 ]
open b
wait_window b
check "a window on 21 lies on the main screen" [ "$(field b 6)" -ge 1280 ]
check "and a keeps its place on the second" [ "$(field a 6)" -lt 1280 ]

move_to a
check "crossing to the second screen gives it the keyboard" [ "$(canvas)" = 11 ]
check "and the window there" [ "$(focused)" = a ]

super_digits 1 3
check "13 and 23 show the same pair" [ "$(canvas)" = 13 ]
super_digits 2 3
check "23 has the main screen" [ "$(canvas)" = 23 ]
super_digits 4 2
open c
wait_window c
check "a window for the fixed 42 goes to 32, on the other screen" \
	[ "$(field c 4)" = 32 ]
check "with the keyboard" [ "$(canvas)" = 32 ]

check "tundra-borealis screens says the mode" \
	[ "$("$BOREALIS" screens)" = extended ]
"$BOREALIS" screens mirror >/dev/null
check "tundra-borealis screens mirror shows one canvas on both" \
	[ "$(screens)" = "screens mirror main 0 0 1280 720 second 0 0 1280 720" ]
check "and says so" [ "$("$BOREALIS" screens)" = mirror ]
"$BOREALIS" screens extended >/dev/null
check "and back to extended" [ "$("$BOREALIS" screens)" = extended ]
refute "a wrong mode is refused" "$BOREALIS" screens sideways

ask unplug >/dev/null
check "with the second unplugged, one screen" \
	[ "$(screens | cut -d' ' -f2)" = one ]
super_digits 1 1
check "and its canvases are still there" [ "$(field a 4)" = 11 ]
check "shown on the main screen" [ "$(field a 6)" -lt 1280 ]
ask plug >/dev/null
check "plugged in again, extended" [ "$(screens | cut -d' ' -f2)" = extended ]

check "SIGTERM ends it with status 0" stop
rm -f "$TUNDRA_BOREALIS_DIR/canvas-42"
mkdir -p "$XDG_CONFIG_HOME/tundra/borealis"
printf 'mode = mirror\nsecond = right\n' >"$XDG_CONFIG_HOME/tundra/borealis/screens"
start
check "the screens file can start in mirror" [ "$(screens | cut -d' ' -f2)" = mirror ]
"$BOREALIS" screens extended >/dev/null
check "and put the second on the right" \
	[ "$(screens)" = "screens extended main 0 0 1280 720 second 1280 0 1280 720" ]
check "SIGTERM ends it with status 0" stop

finish
