. tests/wl/lib.sh

export TUNDRA_BOREALIS_DIR=$WORK/fixed
mkdir -p "$TUNDRA_BOREALIS_DIR"

program() {
	printf '%s {\n\tcommand = "exec %s list >%s/list.%s"\n\tplace = %s\n\tsize = 30 30\n\tprivileges = %s\n}\n' \
		"$1" "$CLIENT" "$WORK" "$1" "$2" "$3"
}

{
	program cap "0 0" capture
	program vin "50 0" virtual-input
	program none "0 50" none
} >"$TUNDRA_BOREALIS_DIR/canvas-21"

if ! start; then
	not_ok "starts"
	finish
	exit 1
fi
"$CLIENT" list >"$WORK/list.normal" 2>&1 &
CLIENTS="$CLIENTS $!"

listed() {
	for f in cap vin none normal; do
		grep -q '^listed' "$WORK/list.$f" 2>/dev/null || return 1
	done
}
i=0
until listed || [ "$i" -gt 50 ]; do
	i=$((i + 1))
	sleep 0.1
done

sees() {
	grep -q "^global $2\$" "$WORK/list.$1"
}

check "every client sees the public globals" sees normal wl_compositor
refute "a client of its own does not see capture" sees normal zwlr_screencopy_manager_v1
refute "nor a virtual keyboard" sees normal zwp_virtual_keyboard_manager_v1
refute "nor a virtual pointer" sees normal zwlr_virtual_pointer_manager_v1
refute "a fixed program without privileges does not see capture" \
	sees none zwlr_screencopy_manager_v1
refute "nor virtual input" sees none zwp_virtual_keyboard_manager_v1
check "one given capture sees it" sees cap zwlr_screencopy_manager_v1
refute "and only it" sees cap zwp_virtual_keyboard_manager_v1
check "one given virtual-input sees the virtual keyboard" \
	sees vin zwp_virtual_keyboard_manager_v1
check "and the virtual pointer" sees vin zwlr_virtual_pointer_manager_v1
refute "but not capture" sees vin zwlr_screencopy_manager_v1

check "SIGTERM ends it with status 0" stop
finish
