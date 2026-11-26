. tests/wl/lib.sh

wait_line() {
	i=0
	until grep -q "$2" "$WORK/client.$1"; do
		i=$((i + 1))
		[ "$i" -gt 50 ] && return 1
		sleep 0.1
	done
}

if ! start; then
	not_ok "starts for the primary selection"
	finish
	exit 1
fi

open a primary=hola
wait_count 1
wait_settled 1
move_to a
ask button down >/dev/null
ask button up >/dev/null
check "a click in a client sets the primary selection" wait_line a '^selected'
open b paste-primary
wait_count 2
wait_settled 2
move_to b
ask button down middle >/dev/null
ask button up middle >/dev/null
check "the middle button pastes it in another" \
	wait_line b '^pasted-primary hola$'
refute "and the first one keeps it" \
	grep -q "^primary cancelled" "$WORK/client.a"

check "SIGTERM ends it with status 0" stop
finish
