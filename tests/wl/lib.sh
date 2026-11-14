
ROOT=$(pwd)
BOREALIS=$ROOT/build/test/tundra-borealis
CLIENT=$ROOT/build/test/tools/client
WORK=$(mktemp -d /tmp/borealis-test.XXXXXX)
PASS=0
FAIL=0
PID=

export XDG_RUNTIME_DIR=$WORK
export WAYLAND_DISPLAY=wayland-0
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_LIBINPUT_NO_DEVICES=1

ok() {
	PASS=$((PASS + 1))
	printf 'ok - %s\n' "$1"
}

not_ok() {
	FAIL=$((FAIL + 1))
	printf 'not ok - %s\n' "$1"
}

check() {
	desc=$1
	shift
	if "$@" >/dev/null 2>&1; then
		ok "$desc"
	else
		not_ok "$desc"
	fi
}

start() {
	"$BOREALIS" 2>"$WORK/borealis.log" &
	PID=$!
	i=0
	while [ ! -S "$WORK/$WAYLAND_DISPLAY" ]; do
		i=$((i + 1))
		if [ "$i" -gt 50 ] || ! kill -0 "$PID" 2>/dev/null; then
			sed 's/^/# /' "$WORK/borealis.log"
			return 1
		fi
		sleep 0.1
	done
}

stop() {
	[ -n "$PID" ] || return 1
	kill -TERM "$PID" 2>/dev/null
	wait "$PID"
	rc=$?
	PID=
	return $rc
}

finish() {
	[ -n "$PID" ] && kill -KILL "$PID" 2>/dev/null
	rm -rf "$WORK"
	printf '# %d passed, %d failed\n' "$PASS" "$FAIL"
	[ "$FAIL" -eq 0 ]
}
