#!/bin/sh

candidate="$1"

tar=`which $candidate`
if [ ! -x "$tar" ]; then
	tar=`which tar`
	if [ ! -x "$tar" ]; then
		# echo nothing: no suitable tar found
		exit 1
	fi
fi

# Output of 'tar --version' examples:
# tar (GNU tar) 1.15.1
# tar (GNU tar) 1.25
# bsdtar 2.8.3 - libarchive 2.8.3
version=`$tar --version | head -n 1 | sed 's/^.*\s\([0-9]\+\.\S\+\).*$/\1/'`
major=`echo "$version" | cut -d. -f1`
minor=`echo "$version" | cut -d. -f2`
bugfix=`echo "$version" | cut -d. -f3`
version_bsd=`$tar --version | grep 'bsdtar'`

# BSD tar does not have all the command-line options
if [ -n "${version_bsd}" ] ; then
    # echo nothing: no suitable tar found
    exit 1
fi

# Minimal version = 1.27 (previous versions do not correctly unpack archives
# containing hard-links if the --strip-components option is used or create
# different gnu long link headers for path elements > 100 characters).
major_min=1
minor_min=27

# There used to be a maximum version of 1.29 here (1.30 changed --numeric-owner
# output for filenames > 100 characters). Upstream buildroot has since dropped
# the upper bound entirely (see upstream commit b11956fb66, "support/dependencies:
# require tar >= 1.35"), because building host-tar 1.29 no longer compiles against
# modern libacl headers. Dropping the cap lets a modern host tar be used directly.

if [ $major -lt $major_min ]; then
	# echo nothing: no suitable tar found
	exit 1
fi

if [ $major -eq $major_min -a $minor -lt $minor_min ]; then
	# echo nothing: no suitable tar found
	exit 1
fi

# valid
echo $tar
