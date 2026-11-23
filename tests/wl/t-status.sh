. tests/wl/lib.sh

BAT=$TUNDRA_BOREALIS_SYS/class/power_supply/BAT0
LIGHT=$TUNDRA_BOREALIS_SYS/class/backlight/intel_backlight
mkdir -p "$BAT" "$LIGHT"
printf 'Battery\n' >"$BAT/type"
printf 'Discharging\n' >"$BAT/status"
printf '50\n' >"$BAT/energy_now"
printf '100\n' >"$BAT/energy_full"
printf 'raw\n' >"$LIGHT/type"
printf '40\n' >"$LIGHT/brightness"
printf '100\n' >"$LIGHT/max_brightness"

if ! start; then
	not_ok "starts"
	finish
	exit 1
fi

status() {
	ask status | awk '$1 == "status"'
}

colored() {
	[ "$(ask count "$1" | awk '$1 == "count" { print $2 }')" -gt 0 ]
}

check "battery and brightness are read, each in its state" \
	[ "$(status)" = "status battery 50 half 1 brightness 40 half 1" ]
check "their icons and numbers are on the right" \
	[ "$(ask ink 1000 280 | cut -d' ' -f2)" -gt 0 ]
check "half the battery is yellow" colored ffd2a408

printf '10\n' >"$BAT/energy_now"
ask uevent >/dev/null
check "a uevent reads it again: empty" \
	[ "$(status | cut -d' ' -f2-5)" = "battery 10 empty 1" ]
check "and red" colored ffff1712
printf 'Charging\n' >"$BAT/status"
ask uevent >/dev/null
check "charging, whatever the level" \
	[ "$(status | cut -d' ' -f2-5)" = "battery 10 charging 1" ]
printf 'Discharging\n' >"$BAT/status"
printf '70\n' >"$BAT/energy_now"
ask uevent >/dev/null

mkdir -p "$HOME/.config/tundra"
printf 'ok = 0000ff\n' >"$HOME/.config/tundra/theme"
i=0
until colored ff0000ff; do
	i=$((i + 1))
	[ "$i" -gt 50 ] && break
	sleep 0.1
done
check "a theme saved repaints the icons in its colors" colored ff0000ff

rm -r "$LIGHT"
ask uevent >/dev/null
check "a backlight gone leaves the battery alone" \
	[ "$(status | cut -d' ' -f6-8)" = "brightness -1 -" ]
rm -r "$BAT"
ask uevent >/dev/null
check "and without either, nothing on the right" \
	[ "$(ask ink 1000 280 | cut -d' ' -f2)" -eq 0 ]
check "stops" stop

mkdir -p "$BAT" "$XDG_CONFIG_HOME/tundra/borealis/art"
printf 'Battery\n' >"$BAT/type"
printf '10\n' >"$BAT/capacity"
printf 'not a toa\n' >"$XDG_CONFIG_HOME/tundra/borealis/art/battery.toa"
cp "$TUNDRA_BOREALIS_ART/brightness.toa" "$XDG_CONFIG_HOME/tundra/borealis/art/brightness.toa"
start
check "a broken drawing of the user leaves its place without one" \
	[ "$(status | cut -d' ' -f2-5)" = "battery 10 empty 0" ]
check "and says why" grep -q '^art: .*/battery.toa: ' "$WORK/borealis.log"
rm "$XDG_CONFIG_HOME/tundra/borealis/art/brightness.toa"
cp "$TUNDRA_BOREALIS_ART/brightness.toa" "$XDG_CONFIG_HOME/tundra/borealis/art/battery.toa"
check "stops" stop
start
check "one that opens is used" \
	[ "$(status | cut -d' ' -f2-5)" = "battery 10 empty 1" ]
check "and a state it has no animation for is said" \
	grep -q '^art: battery has no animation empty' "$WORK/borealis.log"
check "stops cleanly" stop
finish
