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


char *bbspath, *bbsroot, *bbsdir;
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
    int verbose = fetch("_VERBOSE") != 0;

    if ( (f = rewrite("archive.html")) == 0) {
	perror("archive.html");
	exit(1);
    }

    memset(&tm, 0, sizeof tm);
    years = scandir(".", &year, yselect, nsort);

    while (years-- > 0) {
	yrdir = year[years]->d_name;
	months = scandir(year[years]->d_name, &month, mselect, nsort);

	if (verbose)
	    fprintf(stderr, "months for %s = %d\n",years[year]->d_name, months);

	for (count=0; months-- > 0; ++count) {
	    tm.tm_year = atoi(year[years]->d_name)-1900;
	    tm.tm_mon  = atoi(month[months]->d_name)-1;
	    if (! (count || fmt.simplearchive) ) {
		if ( total++ == 0 )
		    fprintf(f, "%s\n", fmt.archive.start);

		fprintf(f, "<h3 class=archive>%s</h3>\n", year[years]->d_name);
		fprintf(f, "<ul class=archive>\n");
	    }
	    strftime(ftime, sizeof ftime, "%B %Y", &tm);
	    fprintf(f, "  <li><A HREF=\"%s%04d/%02d/index.html\">%s</A>\n",
		    bbsroot, tm.tm_year+1900, tm.tm_mon+1, ftime);
	}
	if (count > 0 && !fmt.simplearchive)
	    fprintf(f, "</ul>\n");
    }
    rclose(f);
}


void
bbs_error(int code, char *why)
{
    fprintf(stderr, "%s: %s (code %d)\n", pgm, why, code);
    exit(1);
}



main(int argc, char **argv)
{
    int levels;
    time_t now = time(0);
    struct tm *tm, *today, pagetime;
    int buildarchivepage = 0;
    int buildhomepage = 0;
    int buildsyndicate = 0;
    int full_rebuild = 0;
    register opt;
    struct passwd *pw;

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

    if ( (getuid() == 0) && (argc > 0) ) {
	/* if a username is passed in, run reindex as that user
	 * (offer only valid for root)
	 */
	if ( pw = getpwnam(argv[0]) ) {
	    setgid(pw->pw_gid);
	    setegid(pw->pw_gid);
	    setuid(pw->pw_uid);
	    seteuid(pw->pw_uid);
	}
	else
	    bbs_error(503, "cannot find user in auth table");
    }
    
    initialize();

    stash("weblog", bbsroot);


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

	for (mon=0; mon < 12; mon++) {
	    tm->tm_mon = mon;
	    generate(tm, ".", full_rebuild, PG_ARCHIVE);
	}
    }
    else {
	generate(tm, ".", full_rebuild, PG_ARCHIVE);
    }
    pagetime = *localtime(&now);

    if (buildhomepage)
	generate(&pagetime, ".", 1, PG_HOME|PG_POST);


    if (buildsyndicate) {
	syndicate(&pagetime, bbsroot, &rss2feed);
	syndicate(&pagetime, bbsroot, &atomfeed);
    }

    exit(0);
}
