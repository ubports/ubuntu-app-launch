#!/bin/bash

UPSTARTVERSION=`initctl version | sed 's/[[:alpha:]\)|(|[:space:]]//g' | awk -F- '{print $1}' | awk -F. '{print $2}'`

# Only test on newer versions of Upstart, like not the
# versions on the builders
if [ ${UPSTARTVERSION} -gt 7 ] ; then
	dbus-test-runner --task init-checkconf --parameter "$1" --task-name init-checkconf
else
	echo "Upstart Version: $UPSTARTVERSION"
	echo "  ....Skipping Tests"
fi
