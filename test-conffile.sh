#!/bin/bash

if [ -x /sbin/initctl.REAL ] ; then
	init-checkconf --initctl-path=/sbin/initctl.REAL $1
else
	init-checkconf $1
fi
