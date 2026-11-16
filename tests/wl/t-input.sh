. tests/wl/lib.sh

export TUNDRA_BOREALIS_TERMINAL="ls /proc/\$\$/fd >$WORK/term.fd; exec $CLIENT window term >$WORK/client.term 2>&1"

if ! start; then
	not_ok "starts"
	finish
	exit 1
fi

n=0
for a in a b c; do
	open "$a"
	n=$((n + 1))
	wait_count "$n"
done
wait_settled 3

check "the latest window has the keyboard" [ "$(focused)" = c ]
check "and its client knows" grep -q '^kbd-enter' "$WORK/client.c"

chord 125 15
check "Super + Tab after the last goes round to the first" [ "$(focused)" = a ]
chord 125 15
check "Super + Tab goes on clockwise" [ "$(focused)" = b ]
check "the pointer jumped to the centre of the new focus" at_centre b

chord 30
check "a key without Super reaches the focused client" \
	grep -q '^key 30 1' "$WORK/client.b"
refute "a key with Super never does" grep -q '^key 15 ' "$WORK/client.b"

move_to c
check "the pointer over a window gives it the focus" [ "$(focused)" = c ]

chord 125 50
check "Super + M locks the pointer" \
	[ "$(ask pointer | awk '$1 == "pointer" { print $4 }')" = 1 ]
here=$(ask pointer | awk '$1 == "pointer" { print $2, $3 }')
move_to a
check "a locked pointer does not move" pointer_at $here
check "nor does it take the focus" [ "$(focused)" = c ]
ask button down >/dev/null
ask button up >/dev/null
refute "nor does it click" grep -q '^button' "$WORK/client.c"
chord 125 50
move_to a
check "unlocked, it moves and focuses again" [ "$(focused)" = a ]

ask scroll 15 >/dev/null
check "two fingers scroll the client under the pointer" \
	grep -q '^axis 0 15' "$WORK/client.a"

ask swipe 3 0 -200 >/dev/null
check "three fingers up go to the next depth" [ "$(canvas)" = 12 ]
check "an empty canvas leaves the keyboard nowhere" [ "$(focused)" = - ]
ask swipe 3 0 200 >/dev/null
check "three fingers down come back" [ "$(canvas)" = 11 ]
check "and the canvas keeps its focus" [ "$(focused)" = a ]
ask swipe 3 200 0 >/dev/null
check "three fingers right go right" [ "$(canvas)" = 21 ]
ask swipe 3 -200 0 >/dev/null
ask swipe 3 0 -20 >/dev/null
check "left comes back, and a short swipe does nothing" [ "$(canvas)" = 11 ]

super_digits 2 3
check "Super, 2 and 3 go to 23" [ "$(canvas)" = 23 ]
super_digits 1 1
check "Super, 1 and 1 come back to 11" [ "$(canvas)" = 11 ]

chord 125 42 38
check "Super + Shift + l takes the view right" [ "$(canvas)" = 21 ]
check "with the window, focused" [ "$(focused)" = a ]
check "the window is on 21" [ "$(field a 4)" = 21 ]

chord 125 14
check "Super + Backspace asks the client to close" wait_count 2
check "and it heard it" grep -q '^close' "$WORK/client.a"

chord 125 20
check "Super + T starts the terminal of the configuration" wait_count 3
check "on the canvas in view" [ "$(field term 4)" = 21 ]
check "it inherits no blocked or ignored signal" \
	[ "$(grep -c '^Sig[A-Z][a-z]*:[[:space:]]*0*$' "$WORK/client.term")" = 2 ]
check "nor any descriptor but 0, 1 and 2" \
	[ "$(tr '\n' ' ' <"$WORK/term.fd")" = "0 1 2 " ]

check "SIGTERM ends it with status 0" stop
finish
