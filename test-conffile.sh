#!/bin/bash

UPSTARTVERSION=`initctl version | sed 's/[[:alpha:]\)|(|[:space:]]//g' | awk -F- '{print $1}' | awk -F. '{print $2}'`

if [ ${UPSTARTVERSION} -lt 8 ] ; then
# Old upstart on builders, don't test
	true
else
	init-checkconf $1
fi
