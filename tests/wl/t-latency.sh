. tests/wl/lib.sh

in_time() {
	[ -n "$1" ] && [ "$1" -ge 0 ] && [ "$1" -lt 2000000 ]
}

if ! start; then
	not_ok "starts for the latency of a key"
	finish
	exit 1
fi

refute "latency with no window is an error" ask latency 30

open echo echo
wait_count 1
out=$(ask latency 30)
us=$(echo "$out" | awk '$1 == "latency" { print $2 }')
check "a window that answers a key with a frame: its latency" in_time "$us"
check "and its client got the key" grep -q '^key 30 1' "$WORK/client.echo"

kill "$(echo $CLIENTS | cut -d' ' -f1)"
wait_count 0
open quiet
wait_count 1
check "a window that draws nothing after the key: no frame" \
	[ "$(ask latency 30)" = "error: no frame" ]

check "SIGTERM ends it with status 0" stop
finish
