#!/bin/sh

RED="\x1b[31m"
GREEN="\x1b[32m"
RESET="\x1b[0m"

ret=0

# Files with errors
for f in err/*.kts; do
    ./mktc "$f"

    exit_code=$?
    if [ $exit_code = 1 ]; then
        echo "$GREEN" "✔ " "$f" "$RESET"
    else
        echo "$RED" "✘ " "$f" "$RESET"
        ret=1
    fi
done

exit $ret
