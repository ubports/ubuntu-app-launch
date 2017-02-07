#!/bin/bash

set -ex

if [ $1 != "this-is-a-really-really-really_long_appid-that-we-shouldnt-reallyhave_13523432324235.234.234.234234+foo" ] ; then
	exit 1
fi

export DISPLAY=foo
export DBUS_SESSION_BUS_ADDRESS=bar

exec $2 $3 $4 $5
