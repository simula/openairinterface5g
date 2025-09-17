#!/bin/bash

function die() {
	echo $@
	exit 1
}

echo "deployment from script"

[ $# -gt 0 ] && die "failing"

exit 0
