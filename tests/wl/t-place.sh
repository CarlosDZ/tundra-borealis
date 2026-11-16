. tests/wl/lib.sh

prefs <<EOF
v = v1
v = v2
v = v3
EOF

v_in_order() {
	[ "$(field v1 6)" -lt "$(field v2 6)" ] && [ "$(field v2 6)" -lt "$(field v3 6)" ]
}

if ! start; then
	not_ok "starts with a windows file"
	finish
	exit 1
fi

n=0
for a in v1 h1 v2 h2 v3; do
	open "$a"
	n=$((n + 1))
	wait_count "$n"
done

if wait_settled 5; then
	ok "five windows, each drawn at the size of its place"
else
	not_ok "five windows, each drawn at the size of its place"
	windows | sed 's/^/# /'
fi

check "the windows file makes v1, v2 and v3 V" \
	[ "$(windows | awk '$5 == "V" { print $3 }' | sort | tr '\n' ' ')" = "v1 v2 v3 " ]
check "the rest are H" \
	[ "$(windows | awk '$5 == "H" { print $3 }' | sort | tr '\n' ' ')" = "h1 h2 " ]
check "all of them on canvas 11" \
	[ "$(windows | awk '$4 != "11"' | wc -l)" -eq 0 ]
check "no two windows overlap" apart

check "3V + 2H: both H in one column" [ "$(field h1 6)" = "$(field h2 6)" ]
check "3V + 2H: h1 above h2, in opening order" [ "$(field h1 7)" -lt "$(field h2 7)" ]
check "3V + 2H: the V to the right of the H" \
	[ "$(field v1 6)" -gt "$(($(field h1 6) + $(field h1 8)))" ]
check "3V + 2H: the V in opening order, left to right" v_in_order
check "3V + 2H: the V are taller than the H" \
	[ "$(field v1 9)" -gt "$(field h1 9)" ]

kill "$(echo $CLIENTS | cut -d' ' -f2)"
if wait_settled 4; then
	ok "closing h1 leaves four windows laid out again"
else
	not_ok "closing h1 leaves four windows laid out again"
fi
check "3V + 1H: the H at the bottom, under the V" \
	[ "$(field h2 7)" -gt "$(($(field v1 7) + $(field v1 9)))" ]

check "SIGTERM ends it with status 0" stop
finish
