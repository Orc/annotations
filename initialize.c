#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

extern char *bbspath, *bbsroot;

/*
 * initialize() finds out who we are and what directories we should be
 *                                             poking around inside of.
 *
 * there is some magic for being called from a webserver;   if the
 * program is run by the web userID+web groupID, initialize looks
 * at the SCRIPT_NAME variable and tries to pick a /~user/ off the
 * front of it.   It then setuid's to that ~user and continues to
 * set up like it would normally
 */
void
initialize()
{
    char *p;
    static char username[41];
    struct passwd *pwd;

    if ( (getuid() == WEB_UID) && (getgid() == WEB_GID) ) {
	/* called by the web user; use SCRIPT_NAME to find
	 * out who we should be.
	 */
	if ( (p = getenv("SCRIPT_NAME")) == 0)
	    bbs_error(500, "run from the webserver, but SCRIPT_NAME not set");

	if ( (p[0] == '/') && (p[1] == '~') ) {
	    p += 2;
	    strncpy(username, p, sizeof username - 1);
	    if ( p = strchr(username, '/') )
		*p = 0;
	}
	else
	    bbs_error(500, "can't determine the user from SCRIPT_NAME");

	if ( pwd = getpwnam(username) ) {
	    if (pwd->pw_uid == 0 || pwd->pw_gid == 0)
		bbs_error(500, "will not set user to superuser");

	    if (setgid(pwd->pw_gid) != 0 || setuid(pwd->pw_uid) != 0)
		bbs_error(503, "can't give up permissions");
	}
    }
    else {
	/* if we're setuid, stop the madness.
	 */
	if ( (getegid() != getgid()) && (setgid(getgid()) != 0) )
	    bbs_error(503, "can't give up permissions");
	if ( (geteuid() != getuid()) && (setuid(getuid()) != 0) )
	    bbs_error(503, "can't give up permissions");

	if (getuid() == 0 || getgid() == 0)
	    bbs_error(503, "won't run as root");

	if ( pwd = getpwuid(getuid()) )
	    strncpy(username, pwd->pw_name, sizeof username - 1);
    }

    if (pwd == 0)
	bbs_error(503, "cannot find user in auth table");

    if ( ! (bbsroot = malloc(strlen(username) + 4)) )
	bbs_error(503, "out of memory");
    sprintf(bbsroot, "/~%s/", username);

    if ( ! (bbspath = malloc(strlen(pwd->pw_dir) + strlen(PATH_USERDIR) + 4)) )
	bbs_error(503, "out of memory");

    sprintf(bbspath, "%s/%s/", pwd->pw_dir, PATH_USERDIR);

    stash("weblog",bbsroot);
    stash("_ROOT", bbsroot);
    stash("_USER", username);

    if (chdir(bbspath) != 0)
	bbs_error(503, bbspath);

    readconfig(bbspath);
} /* initialize */
