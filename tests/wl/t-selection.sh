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
	not_ok "starts for the clipboard"
	finish
	exit 1
fi

open a copy=hola
wait_count 1
check "a client with the keyboard sets the clipboard" wait_line a '^copied'
open b paste
wait_count 2
check "the next one with the keyboard reads it from the first" \
	wait_line b '^pasted hola$'

check "SIGTERM ends it with status 0" stop
finish
