#!/bin/bash

if [ $1 != "-rootless" ]; then
	echo "-rootless missing"
	exit 1
fi

if [ $2 != "-displayfd" ]; then
	echo "-displayfd missing"
	exit 1
fi

if [ $4 != "-mir" ]; then
	echo "-mir missing"
	exit 1
fi

if [ $5 != "com.mir.test_mirtest_1.2.3" ]; then
	echo "AppID wrong"
	exit 1
fi

echo "42" >&$3

# Ensure that our "XMir" runs longer than
# the test, if it exits first that's a failure
sleep 1
