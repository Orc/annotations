#! /bin/sh

# local options:  ac_help is the help message that describes them
# and LOCAL_AC_OPTIONS is the script that interprets them.  LOCAL_AC_OPTIONS
# is a script that's processed with eval, so you need to be very careful to
# make certain that what you quote is what you want to quote.

ac_help='
--wwwdir=DIR		system htdocs directory (@prefix@/htdocs)
--cgidir=DIR		system cgi script directory (@WWWDIR@/cgi-bin)
--userdir=DIR		where user htdocs live (~user/htdocs)
--webuser		the user that the web server runs as (nobody)'

LOCAL_AC_OPTIONS='
case X"$1" in
X--exec-suffix=*)
	    EXEC_SUFFIX=`echo "$1" | sed -e 's/^[^=]*=//'`
	    EXEC_SUFFIX_SET=1
	    shift 1
	    ;;
X--exec-suffix)
	    EXEC_SUFFIX=$2
	    EXEC_SUFFIX_SET=1
	    shift 2
	    ;;
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
X--webuser)
	    WEBUSER=$2
	    shift 2
	    ;;
X--webuser=*)
	    WEBUSER=`echo "$1" | sed -e 's/^[^=]*=//'`
	    shift 1
	    ;;
*)	    ac_error=1
	    ;;
esac'

# load in the configuration file
#
TARGET=annotations

. ./configure.inc

AC_INIT $TARGET

AC_PROG_CC

MARKDOWN=`acLookFor markdown`

if [ -z "$MARKDOWN" ]; then
    AC_FAIL "Cannot find markdown";
fi

MVERSION=`$MARKDOWN -V | awk '{print $3}' `

if [ -z "$MVERSION" ]; then
    AC_FAIL "$MARKDOWN -V does not return sensible output?"
fi
expr "$MVERSION" '>=' '1.2.0' || AC_FAIL "markdown must be >= version 1.2.0"

AC_CHECK_FIELD dirent d_namlen sys/types.h dirent.h


[ -z "$AC_WWWDIR" ] && AC_WWWDIR="$AC_PREFIX/htdocs"
[ -z "$AC_CGIDIR" ] && AC_CGIDIR="$AC_WWWDIR/cgi-bin"
[ -z "$AC_USERDIR" ] && AC_USERDIR="htdocs"

if AC_ID_OF ${WEBUSER:-nobody}; then
    LOG "WWW user      : ${WEBUSER:-nobody}, uid $av_UID, gid $av_GID"
    AC_DEFINE WEB_UID	${av_UID}
    AC_DEFINE WEB_GID	${av_GID}
else
    LOG "cannot find web user ${WEBUSER:-nobody}!"
    LOG "You need to set webuser to the user that your web server runs as."
    AC_FAIL "Sorry!"
fi
rm -f uid

if AC_LIBRARY uncgi -luncgi; then
    AC_SUB UNCGI ""
else
    AC_SUB UNCGI uncgi.o
fi

AC_CONFIG CGIDIR  "$AC_CGIDIR"
AC_CONFIG WWWDIR  "$AC_WWWDIR"
AC_CONFIG USERDIR "$AC_USERDIR"

LOG "WWW directory : $AC_WWWDIR"
LOG "CGI directory : $AC_CGIDIR"
LOG "user directory: $AC_USERDIR"

if AC_LIBRARY scew_parser_create '-lscew -lexpat'; then
    LOG "scew and expat found; building xmlpost"
    AC_SUB XMLPOST xmlpost
else
    AC_SUB XMLPOST ''
fi

# Check for the existance of the MAP_FAILED mmap() return value
#
cat - > $$.c << EOF
#include <sys/types.h>
#include <sys/mman.h>

int badmap = MAP_FAILED;
EOF

if $AC_CC -c -o $$.o $$.c; then
    LOG "MAP_FAILED exists.  Good."
else
    LOG "Need to define MAP_FAILED."
    AC_DEFINE MAP_FAILED -1
fi
rm -f $$.c $$.o

AC_CHECK_HEADERS libgen.h

test -d "$AC_WWWDIR" || LOG "Cannot find web directory $AC_WWWDIR"
test -d "$AC_CGIDIR" || LOG "Cannot find CGI directory $AC_CGIDIR"

AC_SUB EXEC_PREFIX ${EXEC_PREFIX}
AC_SUB EXEC_SUFFIX ${EXEC_SUFFIX}

AC_OUTPUT Makefile setup.weblog annotations.7 weblog.conf.5 \
                   setup.weblog.8 reindex.8
