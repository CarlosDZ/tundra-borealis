. tests/wl/lib.sh

if ! start; then
	not_ok "starts"
	finish
	exit 1
fi

open small
wait_count 1
open big 2000 2000

if wait_settled 2; then
	ok "two windows settle"
else
	not_ok "two windows settle"
	windows | sed 's/^/# /'
fi

check "the big one gets the same place as the small one" \
	[ "$(field big 8)x$(field big 9)" = "$(field small 8)x$(field small 9)" ]
check "what shows of the big one is cut to its box" \
	[ "$(field big 10)x$(field big 11)" = "$(field big 8)x$(field big 9)" ]
check "the big one draws at its minimum anyway" \
	[ "$(grep -c '^configure' "$WORK/client.big")" -ge 1 ]
check "no two windows overlap" apart

check "SIGTERM ends it with status 0" stop
finish
