#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <dirent.h>
#include <sys/file.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "formatting.h"
#include "mapfile.h"
#include "indexer.h"
#include "syndicate.h"

char *pgm;


static char *bbspath, *bbsroot, *bbsdir;
static char *yrdir = 0;


static int
nsort(struct dirent **a, struct dirent **b)
{
    return atoi( (*a)->d_name ) - atoi( (*b)->d_name );
}


static int 
yselect(const struct dirent *e)
{
    int i;

    if (e->d_namlen < 1)
	return 0;
    for (i=0; i < e->d_namlen; i++)
	if (!isdigit(e->d_name[i]))
	    return 0;
    return 1;
}


static int
mselect(const struct dirent *e)
{
    char ifile[80];
    struct stat st;

    if (yrdir == 0 || yselect(e) == 0)
	return 0;
    snprintf(ifile, sizeof ifile, "%s/%s/index.html", yrdir, e->d_name);

    return (stat(ifile, &st) == 0) && ((st.st_mode & S_IFREG) != 0);
}


void
archivepage()
{
    struct dirent **year;
    struct dirent **month;
    int years, months;
    int count;
    int total=0;
    struct tm tm;
    char ftime[80];
    FILE *f;

    if ( (f = fopen("archive.html", "w")) == 0) {
	perror("archive.html");
	exit(1);
    }

    memset(&tm, 0, sizeof tm);
    years = scandir(".", &year, yselect, nsort);

    while (years-- > 0) {
	yrdir = year[years]->d_name;
	months = scandir(year[years]->d_name, &month, mselect, nsort);

	fprintf(stderr, "months for %s = %d\n", years[year]->d_name, months);

	for (count=0; months-- > 0; ++count) {
	    tm.tm_year = atoi(year[years]->d_name)-1900;
	    tm.tm_mon  = atoi(month[months]->d_name)-1;
	    if (count ==0) {
		if ( total++ == 0 )
		    fprintf(f, "%s\n", fmt.archive.start);

		fprintf(f, "<h3 class=archive>%s</h3>\n", year[years]->d_name);
		fprintf(f, "<ul class=archive>\n");
	    }
	    strftime(ftime, sizeof ftime, "%B %Y", &tm);
	    fprintf(f, "  <li><A HREF=\"%s%04d/%02d/index.html\">%s</A>\n",
		    bbsroot, tm.tm_year+1900, tm.tm_mon+1, ftime);
	}
	if (count > 0)
	    fprintf(f, "</ul>\n");
    }
    fclose(f);
}



main(int argc, char **argv)
{
    char *username = 0;
    struct passwd *user;
    int levels;
    time_t now = time(0);
    struct tm *today;
    struct tm *tm;
    int buildarchivepage = 0;
    int buildhomepage = 0;
    int buildsyndicate = 0;
    int full_rebuild = 0;
    register opt;

    pgm = basename(argv[0]);


    opterr = 1;
    while ( (opt=getopt(argc, argv, "afv")) != EOF )
	switch (opt) {
	case 'v':   stash("_VERBOSE", "T");
		    break;
	case 'f':   full_rebuild = 1;
		    break;
	case 'a':   buildarchivepage = 1;
		    buildhomepage = 1;
		    buildsyndicate = 1;
		    break;
	default:    exit(1);
	}

    argc -= optind;
    argv += optind;



    if (argc < 1 || strlen(*argv) < 1) {
	fprintf(stderr, "usage: %s {document-root}\n", pgm);
	exit(1);
    }

    if ( (bbsdir = strdup(*argv)) == 0 || (bbsroot = malloc(strlen(*argv)+2)) == 0) {
	perror(pgm);
	exit(1);
    }
    strcpy(bbsroot, *argv);
    if (bbsroot[strlen(bbsroot)-1] != '/')
	strcat(bbsroot, "/");

    if ( (bbsdir[0] == '/') && (bbsdir[1] == '~') ) {
	char *r = strchr(bbsdir+2, '/');

	username = bbsdir+2;

	if (r) {
	    *r++ = 0;
	    bbsdir = r;
	}
	else
	    bbsdir = "";
    }

    if (username) {
	if ( (user = getpwnam(username)) == 0) {
	    fprintf(stderr, "%s: user %s does not exist\n", pgm, username);
	    exit(1);
	}
    }
    else if (getuid() == 0) {
	fprintf(stderr, "%s: I will not run with uid=0 inless you supply a user webpage\n", pgm);
	exit(1);
    }
    else if ( (user = getpwuid(getuid())) == 0) {
	fprintf(stderr, "%s: No user record for user id %d\n", pgm, getuid());
	exit(1);
    }
    if ( username) {
	bbspath = malloc(strlen(user->pw_dir) +
			 strlen(PATH_USERDIR) +
			 strlen(bbsdir) + 4);

	if (bbspath == 0) {
	    perror(pgm);
	    exit(1);
	}

	if (setgid(user->pw_gid) || setuid(user->pw_uid)) {
	    perror("reindex: set permissions");
	    exit(1);
	}

	sprintf(bbspath, "%s/%s/%s", user->pw_dir, PATH_USERDIR, bbsdir);
    }
    else {
	bbspath = malloc(strlen(PATH_WWWDIR) + strlen(bbsdir) + 3);
	if (bbspath == 0) {
	    perror(pgm);
	    exit(1);
	}

	sprintf(bbspath, "%s/%s", PATH_WWWDIR, bbsdir);
    }
    if (chdir(bbspath) != 0) {
	perror(bbspath);
	exit(1);
    }
    readconfig(".");

    stash("weblog", bbsroot);
    stash("_USER", user->pw_name);
    stash("_ROOT", bbsroot);


    if (buildarchivepage)
	archivepage();

    if (argc > 1) {
	struct tm x;

	tm = &x;
	levels = sscanf(argv[1], "%d/%d/%d", &x.tm_year, &x.tm_mon, &x.tm_mday);

	if (levels < 1) {
	    fprintf(stderr, "usage: %s year[/mon[/day]]\n", pgm);
	    exit(1);
	}
	x.tm_year -= 1900;

	today = localtime(&now);

	if (levels >= 1)
	    buildhomepage = (today->tm_year == tm->tm_year);

	if (levels >= 2)
	    buildhomepage = buildhomepage && (today->tm_mon == tm->tm_mon);
    }
    else {
	buildhomepage = 1;
	levels = 3;
	tm = localtime(&now);
    }

    if (levels == 1) {
	int mon;

	for (mon=1; mon < 12; mon++) {
	    tm->tm_mon = mon;
	    generate(tm, ".", full_rebuild, PG_ARCHIVE);
	}
    }
    else {
	generate(tm, ".", full_rebuild, PG_ARCHIVE);
    }
    if (buildhomepage)
	buildpages(today, PG_HOME|PG_POST, 0);

    if (buildsyndicate) {
	syndicate(tm, bbsroot, &rss2feed);
	syndicate(tm, bbsroot, &atomfeed);
    }

    exit(0);
}
