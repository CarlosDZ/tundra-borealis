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
export HOME=$WORK/home
export XDG_CONFIG_HOME=$HOME/.config
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

refute() {
	desc=$1
	shift
	if "$@" >/dev/null 2>&1; then
		not_ok "$desc"
	else
		ok "$desc"
	fi
}

prefs() {
	mkdir -p "$XDG_CONFIG_HOME/tundra/borealis"
	cat >"$XDG_CONFIG_HOME/tundra/borealis/windows"
}

start() {
	"$BOREALIS" >"$WORK/borealis.out" 2>"$WORK/borealis.log" &
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
	[ "$(windows | awk '$5 != "D"' | wc -l)" -eq "$1" ] || return 1
	windows | awk '$5 != "D"' | while read -r _ _ app _ _ _ _ w h _; do
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

wait_configure() {
	i=0
	until [ "$(grep "^$2configure " "$WORK/client.$1" | tail -n 1)" = "$2configure $3 $4" ]; do
		i=$((i + 1))
		[ "$i" -gt 50 ] && return 1
		sleep 0.1
	done
}

wait_window() {
	i=0
	until windows | awk -v a="$1" '$3 == a { f = 1 } END { exit !f }'; do
		i=$((i + 1))
		[ "$i" -gt 50 ] && return 1
		sleep 0.1
	done
}

box() {
	windows | awk -v a="$1" '$3 == a { print $6, $7, $8, $9 }'
}

centred_on() {
	set -- $(box "$1") $(box "$2")
	dx=$(($1 * 2 + $3 - $5 * 2 - $7))
	dy=$(($2 * 2 + $4 - $6 * 2 - $8))
	[ "$dx" -ge -4 ] && [ "$dx" -le 4 ] && [ "$dy" -ge -4 ] && [ "$dy" -le 4 ]
}

inside() {
	set -- $(box "$1") $(box "$2")
	[ "$1" -ge "$5" ] && [ "$2" -ge "$6" ] &&
		[ $(($1 + $3)) -le $(($5 + $7)) ] && [ $(($2 + $4)) -le $(($6 + $8)) ]
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

chord() {
	for c in "$@"; do
		ask key "$c" down >/dev/null
	done
	for c in $(printf '%s\n' "$@" | sed -n '1!G;h;$p'); do
		ask key "$c" up >/dev/null
	done
}

super_digits() {
	ask key 125 down >/dev/null
	for n in "$1" "$2"; do
		ask key $((n + 1)) down >/dev/null
		ask key $((n + 1)) up >/dev/null
	done
	ask key 125 up >/dev/null
}

focused() {
	ask focus | awk '$1 == "focus" { print $3 }'
}

canvas() {
	ask focus | awk '$1 == "focus" { print $4 }'
}

centre() {
	windows | awk -v a="$1" '$3 == a { print int($6 + $8 / 2), int($7 + $9 / 2) }'
}

move_to() {
	ask move $(centre "$1") >/dev/null
}

at_centre() {
	set -- $(centre "$1") $(ask pointer | awk '$1 == "pointer" { print $2, $3 }')
	[ $(($1 - $3)) -le 1 ] && [ $(($3 - $1)) -le 1 ] &&
		[ $(($2 - $4)) -le 1 ] && [ $(($4 - $2)) -le 1 ]
}

pointer_at() {
	[ "$(ask pointer | awk '$1 == "pointer" { print $2, $3 }')" = "$1 $2" ]
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
