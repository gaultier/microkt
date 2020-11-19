#!/bin/sh

RED="\x1b[31m"
GREEN="\x1b[32m"
RESET="\x1b[0m"

ret=0

for f in test/*.actual; do
    TEST="$f" 
    diff "${TEST/actual/expected}" "$f" > "${TEST/actual/diff}"
    exit_code=$?
    if [ $exit_code = 0 ]; then
        echo $GREEN "✔ " "${TEST/actual/kts}" $RESET
    else 
        echo $RED "✘ " "${TEST/actual/kts}" $RESET
        ret=1
    fi
done

exit $ret
