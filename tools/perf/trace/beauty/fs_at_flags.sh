#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1

if [ $# -ne 1 ] ; then
	beauty_uapi_linux_dir=tools/perf/trace/beauty/include/uapi/linux/
else
	beauty_uapi_linux_dir=$1
fi

linux_fcntl=${beauty_uapi_linux_dir}/fcntl.h

printf "static const char *fs_at_flags[] = {\n"
regex='^[[:space:]]*#[[:space:]]*define[[:space:]]+AT_([^_]+[[:alnum:]_]+)[[:space:]]+(0x[[:xdigit:]]+)[[:space:]]*.*'
# AT_EACCESS is only meaningful to faccessat, so we will special case it there...
# AT_STATX_SYNC_TYPE is not a bit, its a mask of AT_STATX_SYNC_AS_STAT, AT_STATX_FORCE_SYNC and AT_STATX_DONT_SYNC
# AT_RENAME_* flags are just aliases of RENAME_* flags and we don't need to include them.
# AT_HANDLE_* flags are only meaningful for name_to_handle_at, which we don't support.

grep -E $regex ${linux_fcntl} | \
	grep -v AT_EACCESS | \
	grep -v AT_STATX_SYNC_TYPE | \
	grep -Ev "AT_RENAME_(NOREPLACE|EXCHANGE|WHITEOUT)" | \
	grep -Ev "AT_HANDLE_(FID|MNT_ID_UNIQUE)" | \
	sed -r "s/$regex/\2 \1/g"	| \
	xargs printf "\t[ilog2(%s) + 1] = \"%s\",\n"
printf "};\n"
