. tests/wl/lib.sh

export TUNDRA_BOREALIS_DIR=$WORK/fixed
mkdir -p "$TUNDRA_BOREALIS_DIR"
printf 'gone {\n\tcommand = "exit 1"\n\tplace = 0 0\n\tsize = 50 50\n\tprivileges = none\n}\n' \
	>"$TUNDRA_BOREALIS_DIR/canvas-12"
printf 'v = v\n' | prefs

if ! start; then
	not_ok "starts"
	finish
	exit 1
fi

fills() {
	ask fills | awk -v c="$1" '$1 == "fill" && $2 == c { print $3, $4, $12 }'
}

frames() {
	ask fills | awk -v c="$1" -v n="$2" '$1 == "fill" && $2 == c && $4 == n { print $13; exit }'
}

fitted() {
	ask fills | awk -v c="$1" '$1 == "fill" && $2 == c {
		n++
		w = $9 * $11; h = $10 * $11
		if ($11 < 1 || w > $7 || h > $8) bad++
		if (w + $9 <= $7 && h + $10 <= $8) bad++
	} END { exit !(n > 0 && !bad) }'
}

grows() {
	before=$(frames "$1" "$2")
	sleep 1.2
	[ "$(frames "$1" "$2")" -gt "$before" ]
}

check "an empty canvas has one hole, with the drawing that fills it best" \
	[ "$(fills 11)" = "empty wide 1" ]
check "at the largest whole scale that fits" fitted 11
check "and it animates while it is seen" grows 11 wide

open a
wait_window a
open b
wait_window b
wait_settled 2
check "two H leave a tall hole at each side, for the tall drawing" \
	[ "$(fills 11 | sort -u)" = "free tall 1" ]
check "each at its scale" fitted 11
check "it animates too" grows 11 tall
chord 125 33
refute "under a fullscreen window it stops" grows 11 tall
chord 125 33
check "and goes on after it" grows 11 tall

super_digits 2 1
open v
wait_window v
open h
wait_window h
wait_settled 2
check "an H beside a V leaves holes above and below it, for the wide drawing" \
	[ "$(fills 21 | sort -u)" = "free wide 1" ]
refute "the canvas out of view does not animate" grows 11 tall

state() {
	ask fixed | awk '$3 == "gone" { print $4 }'
}

i=0
until [ "$(state)" = covered ] || [ "$i" -gt 100 ]; do
	i=$((i + 1))
	sleep 0.1
done
check "a fixed program that fell has a drawing of its own kind" \
	[ "$(fills 12 | cut -d' ' -f1-2)" = "fallen wide" ]
check "at its scale" fitted 12

check "stops cleanly" stop
finish
