#!/bin/sh

RED="\x1b[31m"
GREEN="\x1b[32m"
RESET="\x1b[0m"

ret=0

for f in test/*.actual; do
    EXPECTED=$(echo "$f" | sed 's/actual/expected/g')
    DIFF=$(echo "$f" | sed 's/actual/diff/g')
    KTS=$(echo "$f" | sed 's/actual/kts/g')

    diff "$EXPECTED" "$f" > "$DIFF"
    exit_code=$?
    if [ $exit_code = 0 ]; then
        echo "$GREEN" "✔ " "$KTS" "$RESET"
    else 
        echo "$RED" "✘ " "$KTS" "$RESET"
        ret=1
    fi
done

# Files with errors
for f in err/*.kts; do
    if ./microktc "$f"; then # Should return non zero
        ret=1
    fi
done

exit $ret
