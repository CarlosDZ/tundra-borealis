. tests/wl/lib.sh

if ! start; then
	not_ok "starts"
	finish
	exit 1
fi

open a
wait_count 1
open b
wait_count 2
wait_settled 2
a_box=$(box a)
b_box=$(box b)

chord 125 33
check "Super + F puts b over the whole screen" [ "$(box b)" = "0 0 1280 720" ]
check "and says so" [ "$(field b 12)" = 1 ]
check "the client draws it" wait_configure b "" 1280 720
check "a keeps its place underneath" [ "$(box a)" = "$a_box" ]
chord 125 15
check "Super + Tab does nothing in fullscreen" [ "$(focused)" = b ]
move_to a
check "nor does the pointer over a hidden window" [ "$(focused)" = b ]

chord 125 33
check "Super + F again gives b its place back" [ "$(box b)" = "$b_box" ]
check "and the client hears it" wait_configure b "" $(echo "$b_box" | cut -d' ' -f3,4)

open c full
wait_window c
check "a client that asks for fullscreen gets it" [ "$(field c 12)" = 1 ]
open d
wait_window d
check "a new window on its canvas ends it" [ "$(field c 12)" = 0 ]
check "and lays out with the rest" wait_settled 4

check "SIGTERM ends it with status 0" stop
finish
