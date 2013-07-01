#!/bin/bash

if [ -x /sbin/initctrl.REAL ] ; then
	init-checkconf --initctl-path=/sbin/initctrl.REAL $1
else
	init-checkconf $1
fi
