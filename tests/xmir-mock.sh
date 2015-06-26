#!/bin/bash

if [ $1 != "-displayfd" ]; then
	echo "-displayfd missing"
	exit 1
fi

if [ $3 != "-mir" ]; then
	echo "-mir missing"
	exit 1
fi

if [ $4 != "com.mir.test_mirtest_1.2.3" ]; then
	echo "AppID wrong"
	exit 1
fi

echo "42" >&$2
