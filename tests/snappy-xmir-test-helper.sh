#!/bin/bash

set -ex

if [ $1 != "appid" ] ; then
	exit 1
fi

export DISPLAY=foo
export DBUS_SESSION_BUS_ADDRESS=bar

exec $2 $3 $4 $5
