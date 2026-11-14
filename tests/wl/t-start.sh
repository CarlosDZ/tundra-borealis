. tests/wl/lib.sh

out=$("$BOREALIS" --version)
check "--version prints the test version" \
	[ "$out" = "tundra-borealis $(grep '^version=' metapkg | cut -d= -f2)-test" ]

if start; then
	ok "starts without a screen and opens its socket"
	check "a client finds the core globals" "$CLIENT"
	check "SIGTERM ends it with status 0" stop
else
	not_ok "starts without a screen and opens its socket"
fi

finish
