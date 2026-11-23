. tests/wl/lib.sh

FONT=/usr/share/fonts/TTF/DejaVuSansMono.ttf
mkdir -p "$HOME/.config/tundra"
[ -e "$FONT" ] && printf 'ui = %s\n' "$FONT" >"$HOME/.config/tundra/fonts"

if ! start; then
	not_ok "starts"
	finish
	exit 1
fi

header() {
	ask header | awk -F '\t' -v r="$1" '$1 ~ "^header " r " " { print $' "${2:-1}" ' }'
}

check "a 1280x720 screen has a header 22 high, for a line of 16" \
	[ "$(header main | cut -d' ' -f3-8)" = "0 0 1280 22 16 1" ]
check "it shows the place" [ "$(header main 2)" = "H1 P1" ]
either() {
	[ "$1" = "$2" ] || [ "$1" = "$3" ]
}

before=$(date '+%Y-%m-%d  %H:%M')
shown=$(header main 3)
after=$(date '+%Y-%m-%d  %H:%M')
check "and the date and time, in ISO 8601" either "$shown" "$before" "$after"

open a
wait_window a
check "a window lies below the header" [ "$(field a 7)" -ge 22 ]
super_digits 2 1
check "going to 21 changes the place" [ "$(header main 2)" = "H2 P1" ]

check "without a theme, the colors of Tundra" \
	[ "$(ask colors | awk '$1 == "colors" { print $5 }')" = ff1f2529 ]
check "the header is painted in them" [ "$(ask pixel 0 0)" = "pixel ff1f2529" ]
if [ -e "$FONT" ]; then
	check "it writes the place on the left" [ "$(ask ink 0 200 | cut -d' ' -f2)" -gt 0 ]
	check "and the clock in the middle" [ "$(ask ink 540 200 | cut -d' ' -f2)" -gt 0 ]
	check "and nothing else yet on the right" [ "$(ask ink 1000 280 | cut -d' ' -f2)" -eq 0 ]
else
	printf '# no %s: the text is not checked\n' "$FONT"
fi

themed() {
	i=0
	until [ "$(ask pixel 0 0)" = "pixel $1" ]; do
		i=$((i + 1))
		[ "$i" -gt 50 ] && return 1
		sleep 0.1
	done
}

printf 'surface = 123456\n' >"$HOME/.config/tundra/theme"
check "a theme saved is painted without a restart" themed ff123456
printf 'surface = 123456\nborealis.surface = 654321\n' >"$HOME/.config/tundra/theme"
check "borealis.surface goes before surface" themed ff654321
printf 'surface = 1234\n' >"$HOME/.config/tundra/theme"
sleep 0.3
check "a broken theme keeps the colors" [ "$(ask pixel 0 0)" = "pixel ff654321" ]
check "and says why" grep -q '^theme: .*theme:1:' "$WORK/borealis.log"

super_digits 1 1
chord 125 33
check "a fullscreen window covers the header" [ "$(box a)" = "0 0 1280 720" ]
chord 125 33

check "stops cleanly" stop
finish
