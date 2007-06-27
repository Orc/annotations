#! /bin/sh

# local options:  ac_help is the help message that describes them
# and LOCAL_AC_OPTIONS is the script that interprets them.  LOCAL_AC_OPTIONS
# is a script that's processed with eval, so you need to be very careful to
# make certain that what you quote is what you want to quote.

ac_help='
--wwwdir=DIR		system htdocs directory (@prefix@/htdocs)
--cgidir=DIR		system cgi script directory (@WWWDIR@/cgi-bin)
--userdir=DIR		where user htdocs live (~user/htdocs)'

LOCAL_AC_OPTIONS='
case X"$1" in
X--exec-prefix=*)
	    EXEC_PREFIX=`echo "$1" | sed -e 's/^[^=]*=//'`
	    EXEC_PREFIX_SET=1
	    shift 1
	    ;;
X--exec-prefix)
	    EXEC_PREFIX=$2
	    EXEC_PREFIX_SET=1
	    shift 2
	    ;;
X--wwwdir=*)
	    AC_WWWDIR=`echo "$1" | sed -e 's/^[^=]*=//'`
	    shift 1
	    ;;
X--wwwdir)
	    AC_WWWDIR=$2
	    shift 2
	    ;;
X--cgidir=*)
	    AC_CGIDIR=`echo "$1" | sed -e 's/^[^=]*=//'`
	    shift 1
	    ;;
X--cgidir)
	    AC_CGIDIR=$2
	    shift 2
	    ;;
X--userdir=*)
	    AC_USERDIR=`echo "$1" | sed -e 's/^[^=]*=//'`
	    shift 1
	    ;;
X--userdir)
	    AC_USERDIR=$2
	    shift 2
	    ;;
*)	    ac_error=1
	    ;;
esac'

# load in the configuration file
#
TARGET=annotations

test -r configure.inc || test -d SCCS && get configure.inc
. ./configure.inc


AC_INIT $TARGET

AC_PROG_CC

[ -z "$AC_WWWDIR" ] && AC_WWWDIR="$AC_PREFIX/htdocs"
[ -z "$AC_CGIDIR" ] && AC_CGIDIR="$AC_WWWDIR/cgi-bin"
[ -z "$AC_USERDIR" ] && AC_USERDIR="htdocs"

if AC_LIBRARY uncgi -luncgi; then
    AC_CONFIG CGIDIR  "$AC_CGIDIR"
    AC_CONFIG WWWDIR  "$AC_WWWDIR"
    AC_CONFIG USERDIR "$AC_USERDIR"

    LOG "WWW directory : $AC_WWWDIR"
    LOG "CGI directory : $AC_CGIDIR"
    LOG "user directory: $AC_USERDIR"
else
    LOG "Cannot find libuncgi;  you need to have a copy of Steven Grimm's"
    LOG "uncgi program compiled as a library for $TARGET to work"
    LOG "You can get uncgi from http://www.midwinter.com/~koreth/uncgi.html"
    AC_FAIL "Sorry!"
fi

test -d "$AC_WWWDIR" || AC_FAIL "Cannot find web directory $AC_WWWDIR"
test -d "$AC_CGIDIR" || AC_FAIL "Cannot find CGI directory $AC_CGIDIR"

if [ "$EXEC_PREFIX_SET" ]; then
    AC_SUB EXEC_PREFIX ${EXEC_PREFIX}
else
    AC_SUB EXEC_PREFIX bbs-
fi

AC_OUTPUT Makefile

