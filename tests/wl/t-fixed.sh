. tests/wl/lib.sh

export TUNDRA_BOREALIS_DIR=$WORK/fixed
mkdir -p "$TUNDRA_BOREALIS_DIR"

program() {
	printf '%s {\n\tcommand = "%s"\n\tplace = %s\n\tsize = %s\n\tlevel = %s\n\tprivileges = none\n}\n' "$@"
}

{
	program fa "exec $CLIENT window fa >$WORK/client.fa 2>&1" "0 0" "60 100" 0
	program fb "exec $CLIENT window fb >$WORK/client.fb 2>&1" "40 0" "60 50" 1
} >"$TUNDRA_BOREALIS_DIR/canvas-12"
{
	program a true "0 0" "60 100" 0
	program b true "40 0" "60 50" 0
} >"$TUNDRA_BOREALIS_DIR/canvas-13"

if ! start; then
	not_ok "starts with fixed canvases"
	finish
	exit 1
fi

check "a wrong file is reported" \
	grep -q 'canvas-13: a and b overlap on level 0' "$WORK/borealis.log"
wait_window fa
wait_window fb
check "both programs are on canvas 12, as fixed windows" \
	[ "$(field fa 4)$(field fa 5) $(field fb 4)$(field fb 5)" = "12F 12F" ]
over_fa() {
	[ "$(field fb 6)" -gt "$(field fa 6)" ] &&
		[ "$(field fb 6)" -lt "$(($(field fa 6) + $(field fa 8)))" ]
}
check "fb lies over the right of fa" over_fa

super_digits 1 2
ask move $(($(field fb 6) + 20)) $(($(field fb 7) + 20)) >/dev/null
check "where both are, the upper one gets the pointer" [ "$(focused)" = fb ]
ask move $(($(field fa 6) + 20)) $(($(field fa 7) + $(field fa 9) - 20)) >/dev/null
check "below it, the lower one" [ "$(focused)" = fa ]

chord 125 14
sleep 0.2
refute "Super + Backspace does not close a fixed program" \
	grep -q '^close' "$WORK/client.fa"

open intruder
i=0
until grep -q '^close' "$WORK/client.intruder" 2>/dev/null || [ "$i" -gt 50 ]; do
	i=$((i + 1))
	sleep 0.1
done
check "a new window on a fixed canvas is refused" \
	grep -q '^close' "$WORK/client.intruder"
refute "and never shows" wait_window intruder

window_of() {
	ask fixed | awk -v n="$1" '$3 == n { print $5 }'
}

state_of() {
	ask fixed | awk -v n="$1" '$3 == n { print $4 }'
}

again() {
	old=$(window_of fa)
	pkill -f "client window fa"
	i=0
	until [ "$(window_of fa)" != "$old" ] && [ "$(window_of fa)" != 0 ]; do
		i=$((i + 1))
		[ "$i" -gt 50 ] && return 1
		sleep 0.1
	done
}

check "a program that dies starts again in its place" again
check "on its canvas" [ "$(field fa 4)" = 12 ]
again
pkill -f "client window fa"
i=0
until [ "$(state_of fa)" = covered ] || [ "$i" -gt 50 ]; do
	i=$((i + 1))
	sleep 0.1
done
check "after three deaths within 10 s its place is covered" \
	[ "$(state_of fa)" = covered ]
refute "and nothing runs it" pgrep -f "client window fa"
check "fb goes on" [ "$(state_of fb)" = running ]

super_digits 1 3
open x
check "a canvas with a wrong file stays a working one" wait_window x
check "SIGTERM ends it with status 0" stop

for h in 1 2 3 4; do
	for d in 1 2 3 4; do
		program p true "0 0" "100 100" 0 >"$TUNDRA_BOREALIS_DIR/canvas-$h$d"
	done
done
start
check "sixteen fixed canvases are refused" \
	grep -q 'all 16 canvases fixed' "$WORK/borealis.log"
open w
check "and every canvas works" wait_window w
check "SIGTERM ends it with status 0" stop

finish
