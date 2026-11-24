. tests/wl/lib.sh

export TUNDRA_BOREALIS_DIR=$WORK/fixed
mkdir -p "$TUNDRA_BOREALIS_DIR"
printf 'gone {\n\tcommand = "exit 1"\n\tplace = 0 0\n\tsize = 50 50\n\tprivileges = none\n}\n' \
	>"$TUNDRA_BOREALIS_DIR/canvas-12"

if ! start; then
	not_ok "starts with a fixed canvas"
	finish
	exit 1
fi

fallen() {
	[ "$(ask fixed | awk '$3 == "gone" { print $4 }')" = covered ]
}
i=0
until fallen || [ "$i" -gt 50 ]; do
	i=$((i + 1))
	sleep 0.1
done
check "the fixed program falls and its place is covered" fallen

open a
open b
wait_settled 2
before=$(windows)

ask unplug all >/dev/null
sleep 0.2
check "without screens, as while another console holds the session, it lives" \
	kill -0 "$PID"
check "and still answers" ask windows

ask plug >/dev/null
wait_settled 2
check "with the screen back, the windows are where they were" \
	[ "$(windows)" = "$before" ]
check "and the place of the fallen program is still covered" fallen
check "SIGTERM ends it with status 0" stop

finish
