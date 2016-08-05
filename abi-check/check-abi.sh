#!/bin/sh

set -e

LIBRARY_NAME=ubuntu-app-launch
LIBRARY_VERSION=2

rm -rf abi_dumps
rm -rf installed_dumps
rm -rf build_dumps

abi-compliance-checker -l ${LIBRARY_NAME} -dump installed.xml
mv abi_dumps installed_dumps

abi-compliance-checker -l ${LIBRARY_NAME} -dump build.xml
mv abi_dumps build_dumps

abi-compliance-checker \
	-l ${LIBRARY_NAME} \
	-old installed_dumps/${LIBRARY_NAME}/${LIBRARY_NAME}_${LIBRARY_VERSION}.abi.tar.gz \
	-new build_dumps/${LIBRARY_NAME}/${LIBRARY_NAME}_${LIBRARY_VERSION}.abi.tar.gz

