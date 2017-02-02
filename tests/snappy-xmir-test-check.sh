#!/bin/bash

set -ex

if [ -z ${DISPLAY} ] ; then
	echo DISPLAY is not set
	exit 1
fi

if [ ${DISPLAY} != "foo" ] ; then
	echo DISPLAY is not set to 'foo'
	exit 1
fi

if [ -z ${DBUS_SESSION_BUS_ADDRESS} ] ; then
	echo DBUS_SESSION_BUS_ADDRESS is not set
	exit 1
fi

if [ ${DBUS_SESSION_BUS_ADDRESS} != "bar" ] ; then
	echo DBUS_SESSION_BUS_ADDRESS is not set to 'bar'
	exit 1
fi

if [ ! -z ${MIR_SOCKET} ] ; then
	echo Mir variables are leaking in
	exit 1
fi

if [ ! -z ${UBUNTU_APP_LAUNCH_SNAPPY_XMIR_ENVVARS_PID} ] ; then
	kill -TERM ${UBUNTU_APP_LAUNCH_SNAPPY_XMIR_ENVVARS_PID}
fi
