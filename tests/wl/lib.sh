
ROOT=$(pwd)
BOREALIS=$ROOT/build/test/tundra-borealis
CLIENT=$ROOT/build/test/tools/client
ASK=$ROOT/build/test/tools/ask
WORK=$(mktemp -d /tmp/borealis-test.XXXXXX)
PASS=0
FAIL=0
PID=
CLIENTS=

export XDG_RUNTIME_DIR=$WORK
export XDG_CONFIG_HOME=$WORK/config
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

prefs() {
	mkdir -p "$XDG_CONFIG_HOME/tundra/borealis"
	cat >"$XDG_CONFIG_HOME/tundra/borealis/windows"
}

start() {
	"$BOREALIS" 2>"$WORK/borealis.log" &
	PID=$!
	i=0
	while [ ! -S "$WORK/$WAYLAND_DISPLAY" ] || [ ! -S "$WORK/borealis-test" ]; do
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

ask() {
	"$ASK" "$@"
}

open() {
	"$CLIENT" window "$@" >"$WORK/client.$1" 2>&1 &
	CLIENTS="$CLIENTS $!"
}

wait_count() {
	i=0
	until [ "$(windows | wc -l)" -eq "$1" ]; do
		i=$((i + 1))
		[ "$i" -gt 50 ] && return 1
		sleep 0.1
	done
}

windows() {
	ask windows | grep '^window '
}

settled() {
	[ "$(windows | wc -l)" -eq "$1" ] || return 1
	windows | while read -r _ _ app _ _ _ _ w h _ _; do
		last=$(grep '^configure ' "$WORK/client.$app" | tail -n 1)
		[ "$last" = "configure $w $h" ] || exit 1
	done
}

wait_settled() {
	i=0
	until settled "$1"; do
		i=$((i + 1))
		[ "$i" -gt 50 ] && return 1
		sleep 0.1
	done
}

field() {
	windows | awk -v a="$1" -v f="$2" '$3 == a { print $f }'
}

apart() {
	windows | awk '
		{ x[NR] = $6; y[NR] = $7; w[NR] = $8; h[NR] = $9 }
		END {
			for (i = 1; i <= NR; i++)
				for (j = i + 1; j <= NR; j++)
					if (x[i] < x[j] + w[j] && x[j] < x[i] + w[i] &&
					    y[i] < y[j] + h[j] && y[j] < y[i] + h[i])
						exit 1
		}'
}

finish() {
	for c in $CLIENTS; do
		kill "$c" 2>/dev/null
	done
	[ -n "$PID" ] && kill -KILL "$PID" 2>/dev/null
	rm -rf "$WORK"
	printf '# %d passed, %d failed\n' "$PASS" "$FAIL"
	[ "$FAIL" -eq 0 ]
}
