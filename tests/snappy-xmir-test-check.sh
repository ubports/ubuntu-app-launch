#!/bin/bash

set -ex

if [ ${DISPLAY} != "foo" ] ; then
	echo Display is not set to 'foo'
	exit 1
fi

if [ ! -z ${MIR_SOCKET} ] ; then
	echo Mir variables are leaking in
	exit 1
fi
