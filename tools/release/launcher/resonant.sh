#!/bin/sh
# Starts Resonant: the same as ./resonant (for desktops and scripts that
# expect a .sh file). Arguments and the exit status are passed through.
self=$0
while [ -L "$self" ]; do
    link=$(readlink -- "$self")
    case $link in
        /*) self=$link ;;
        *) self=$(dirname -- "$self")/$link ;;
    esac
done
here=$(cd -P -- "$(dirname -- "$self")" 2>/dev/null && pwd -P) || {
    echo "resonant.sh: cannot determine the package folder" >&2
    exit 1
}
exec "$here/resonant" "$@"
