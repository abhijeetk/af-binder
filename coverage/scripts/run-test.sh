#!/bin/sh

export R=$(realpath $(dirname $0)/..)
export PATH="$R/bin:$R/scripts:$PATH"

$R/bin/afb-daemon-cov --help > /dev/null

$R/bin/afb-daemon-cov --version > /dev/null

$R/bin/afb-daemon-cov --fake-option > /dev/null

valgrind \
	--log-file=$R/valgrind.out \
	--trace-children=no \
	--track-fds=yes \
	--leak-check=full \
$R/bin/afb-daemon-cov \
	--verbose \
	--verbose \
	--verbose \
	--verbose \
	--quiet \
	--quiet \
	--quiet \
	--quiet \
	--quiet \
	--quiet \
	--log error,warning,notice,info,debug,critical,alert-error,warning,notice,info,debug,critical,alert+error,warning,notice,info,debug,critical,alert \
	--foreground \
	--name binder-cov \
	--roothttp $R/www \
	--rootbase /opx \
	--rootapi /api \
	--alias /icons:$R/www \
	--apitimeout 90 \
	--cntxtimeout 3600 \
	--cache-eol 200 \
	--workdir . \
	--uploaddir . \
	--rootdir . \
	--ldpaths $R/ldpath/strong \
	--binding $R/bin/demat.so \
	--weak-ldpaths $R/ldpath/weak \
	--auto-api $R/apis/auto \
	--token HELLO \
	--random-token \
	--session-max 1000 \
	--tracereq all \
	--traceapi all \
	--traceses all \
	--traceevt all \
	--call demat/ping:true \
	--ws-server unix:$R/apis/ws/hello \
	--ws-server unix:$R/apis/ws/salut \
	--exec $R/scripts/run-parts.sh @p @t

exit 0

