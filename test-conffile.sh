#!/bin/bash

if [ -x /sbin/initctl.REAL ] ; then
# Old upstart on builders, don't test
	true
else
	init-checkconf $1
fi
