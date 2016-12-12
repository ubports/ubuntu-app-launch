#!/bin/bash

set -ex

if [ ${DISPLAY} != "foo" ] ; then
	exit 1
fi

