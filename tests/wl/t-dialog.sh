. tests/wl/lib.sh

if ! start; then
	not_ok "starts"
	finish
	exit 1
fi

open p dialog=200x100
wait_window p-dialog
check "a window with a parent is a dialog, on the canvas of its parent" \
	[ "$(field p-dialog 5)$(field p-dialog 4)" = D11 ]
check "it takes no place: its parent fills the canvas alone" wait_settled 1
check "it keeps the size it chose" wait_configure p dlg- 0 0
check "it shows at that size" [ "$(field p-dialog 8)x$(field p-dialog 9)" = 200x100 ]
check "it is centred over its parent" centred_on p-dialog p
check "it has the keyboard" [ "$(focused)" = p-dialog ]
check "and its client knows" grep -q '^kbd-enter dlg' "$WORK/client.p"

open q
wait_settled 2
check "when its parent moves, it stays centred over it" centred_on p-dialog p

move_to p
check "the pointer over the parent gives the keyboard to its dialog" \
	[ "$(focused)" = p-dialog ]
chord 125 14
check "Super + Backspace closes the dialog, not its parent" \
	grep -q '^close dlg' "$WORK/client.p"
refute "the parent is not asked to close" grep -q '^close$' "$WORK/client.p"
check "the keyboard goes back to the parent" [ "$(focused)" = p ]

open big dialog=2000x2000
wait_window big-dialog
check "a dialog bigger than its parent stays inside it" inside big-dialog big
check "and is asked to shrink" grep -q '^dlg-configure [1-9]' "$WORK/client.big"

open m menu
wait_window m
i=0
until ask popups | grep -q '^popup m '; do
	i=$((i + 1))
	[ "$i" -gt 50 ] && break
	sleep 0.1
done
menu_inside() {
	set -- $(ask popups | awk '$2 == "m" { print $3, $4, $5, $6 }') $(box m)
	[ "$#" -eq 8 ] && [ "$1" -ge "$5" ] && [ "$2" -ge "$6" ] &&
		[ $(($1 + $3)) -le $(($5 + $7)) ] && [ $(($2 + $4)) -le $(($6 + $8)) ]
}
check "a menu that would hang past its window is moved inside it" menu_inside

check "SIGTERM ends it with status 0" stop
finish
