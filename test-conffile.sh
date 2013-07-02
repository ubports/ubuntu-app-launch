#!/bin/bash

UPSTARTVERSION=`initctl version | sed 's/[[:alpha:]\)|(|[:space:]]//g' | awk -F- '{print $1}' | awk -F. '{print $2}'`

# Only test on newer versions of Upstart, like not the
# versions on the builders
if [ ${UPSTARTVERSION} -gt 7 ] ; then
	init-checkconf $1
else
	echo "Upstart Version: $UPSTARTVERSION"
	echo "  ....Skipping Tests"
fi
