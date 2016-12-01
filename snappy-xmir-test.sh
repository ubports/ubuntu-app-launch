#!/bin/bash

set -ex

BUILDDIR=`pwd`
SOURCEDIR=`pwd`

# Test the test harness

${SOURCEDIR}/snappy-xmir-test-helper.sh appid ${SOURCEDIR}/snappy-xmir-test-check.sh

# Test our pass through

export UBUNTU_APP_LAUNCH_SNAPPY_XMIR_HELPER="${SOURCEDIR}/snappy-xmir-test-helper.sh"
export UBUNTU_APP_LAUNCH_LIBERTINE_LAUNCH="${SOURCEDIR}/snappy-xmir-test-libertine-launch.sh"

${BUILDDIR}/snappy-xmir appid ${SOURCEDIR}/snappy-xmir-test-check.sh

