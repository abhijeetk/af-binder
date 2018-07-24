#!/bin/sh

export R=$(realpath $(dirname $0)/..)
export PATH="$R/bin:$R/scripts:$PATH"

cd $R/bin

lcov -c -i -d $R/bin -o $R/lcov-out.info

mk() {
	echo
	echo "*******************************************************************"
	echo "** $*"
	echo "*******************************************************************"
	lcov -c -i -d $R/bin -o $R/fake.info
	"$@"
	lcov -c -d $R/bin -o $R/tmp.info
	mv $R/lcov-out.info $R/previous.info
	lcov -a $R/tmp.info -a $R/previous.info -o $R/lcov-out.info
	rm $R/previous.info $R/fake.info  $R/tmp.info
}

mkdir /tmp/ldpaths
export AFB_LDPATHS=/tmp/ldpaths
export AFB_TRACEAPI=no

##########################################################
# test to check options
##########################################################
mk $R/bin/afb-daemon-cov --help

mk $R/bin/afb-daemon-cov --version

mk $R/bin/afb-daemon-cov --no-httpd --fake-option

mk $R/bin/afb-daemon-cov --daemon --session-max

mk $R/bin/afb-daemon-cov --ws-client fake --session-max toto

mk $R/bin/afb-daemon-cov --foreground --port -55

mk $R/bin/afb-daemon-cov --foreground --port 9999999

mk $R/bin/afb-daemon-cov --no-ldpath --traceapi fake

mk $R/bin/afb-daemon-cov --traceditf all --tracesvc all --log error,alarm

LISTEN_FDNAMES=toto,demat LISTEN_FDS=5 mk $R/bin/afb-daemon-cov --no-ldpath --binding $R/bin/demat.so --ws-server sd:demat --call "demat/exit:0"

mk $R/bin/afb-daemon-cov --weak-ldpaths $R/ldpath/weak --binding $R/bin/demat.so --ws-server sd:demat --call "demat/exit:0"

##########################################################
# test of the bench
##########################################################
mk $R/bin/test-apiset

mk $R/bin/test-session

mk $R/bin/test-wrap-json

##########################################################
# true life test
##########################################################
mk \
valgrind \
	--log-file=$R/valgrind.out \
	--trace-children=no \
	--track-fds=yes \
	--leak-check=full \
	--show-leak-kinds=all \
	--num-callers=50 \
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
	--port 8888 \
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
	--auto-api $R/apis/auto \
	--token HELLO \
	--random-token \
	--session-max 1000 \
	--tracereq all \
	--traceapi all \
	--traceses all \
	--traceevt all \
	--monitoring \
	--call demat/ping:true \
	--ws-server unix:$R/apis/ws/hello \
	--ws-server unix:$R/apis/ws/salut \
	--ws-server localhost:9595/salut \
	--ws-client localhost:9595/salut2 \
	--exec $R/scripts/run-parts.sh @p @t

exit 0

