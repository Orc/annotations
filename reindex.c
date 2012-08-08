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
#if HAVE_LIBGEN_H
#include <libgen.h>
#endif

#include "formatting.h"
#include "mapfile.h"
#include "indexer.h"
#include "syndicate.h"

#include <mkdio.h>

char *pgm;

extern char VERSION[];


char *bbspath, *bbsroot, *bbsdir;
static char *yrdir = 0;


static int
nsort(struct dirent **a, struct dirent **b)
{
    return atoi( (*a)->d_name ) - atoi( (*b)->d_name );
}


static int 
yselect(struct dirent *e)
{
    int i;

    if (e->d_reclen < 1 || e->d_name[0] == 0)
	return 0;
    for (i=0; (i < e->d_reclen) && e->d_name[i]; i++)
	if (!isdigit(e->d_name[i]))
	    return 0;
    return 1;
}


static int
mselect(struct dirent *e)
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
    years = scandir(".", &year, yselect, (stfu)nsort);

    while (years-- > 0) {
	yrdir = year[years]->d_name;
	months = scandir(year[years]->d_name, &month, mselect, (stfu)nsort);

	if (verbose)
	    fprintf(stderr, "months for %s = %d\n",years[year]->d_name, months);

	if ( fmt.calendararchive ) {
	    char monthtab[12];
	    int i;
	    
	    memset(monthtab, sizeof monthtab, 0);

	    for (i=0; i<months; i++)
		monthtab[atoi(month[months]->d_name)-1] = 1;
	    
	    fprintf(f, "<table class=\"archive\">\n");
	    fprintf(f, "<caption>%s</caption>\n", year[years]->d_name);

	    for ( count=0; count < 12; count++ ) {
		if ( count % 4 == 0 )
		    fprintf(f, "  <tr>\n");
		if ( monthtab[count] ) {
		    tm.tm_mon = count;
		    strftime(ftime, sizeof ftime, "%b", &tm);
		    fprintf(f, "<td><a href=\"%s%s/%02d/index.html\">%.3s</a>\n",
				bbsroot, year[years]->d_name, count+1, ftime);
		}
		else
		    fprintf(f, "<td>&nbsp;</td>\n");
		
		if ( count % 4 == 3 )
		    fprintf(f, "  </tr>\n");
	    }
	    fprintf(f, "</table>\n");
	}
	else {
	    for (count=0; months-- > 0; ++count) {
		tm.tm_year = atoi(year[years]->d_name)-1900;
		tm.tm_mon  = atoi(month[months]->d_name)-1;
		if (! (count || fmt.simplearchive) ) {
		    if ( total++ == 0 )
			fprintf(f, "%s\n", fmt.archive.start);

		    fprintf(f, "<h3 class=archive>%s</h3>\n",
					year[years]->d_name);
		    fprintf(f, "<ul class=archive>\n");
		}
		strftime(ftime, sizeof ftime, "%B %Y", &tm);
		fprintf(f, "  <li><A HREF=\"%s%04d/%02d/index.html\">%s</A>\n",
			bbsroot, tm.tm_year+1900, tm.tm_mon+1, ftime);
	    }
	    if (count > 0 && !fmt.simplearchive)
		fprintf(f, "</ul>\n");
	}
    }
    rclose(f);
}


int
do_one_page(char *page)
{
    struct article *art;

    if (art = openart(page)) {
	stash("LOCATION", page);
	writectl(art);
	writehtml(art);
	return 0;
    }
    return 1;
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
    int flags = 0;
    register opt;
    struct passwd *pw;

    pgm = basename(argv[0]);


    opterr = 1;
    while ( (opt=getopt(argc, argv, "asfvV")) != EOF )
	switch (opt) {
	case 'V':   printf("%s: annotations %s + discount %s\n",
			    pgm, VERSION, markdown_version);
		    exit(0);
	case 'v':   stash("_VERBOSE", "T");
		    break;
	case 'f':   flags = INDEX_FULL;
		    break;
	case 'a':   buildarchivepage = 1;
		    buildhomepage = 1;
		    break;
	case 's':   buildsyndicate = 1;
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
	    ++argv;
	    --argc;
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


    if (argc > 0) {
	struct tm x;

	tm = &x;
	levels = sscanf(argv[0], "%d/%d/%d", &x.tm_year, &x.tm_mon, &x.tm_mday);

	if (levels < 1) {
	    int rc = do_one_page(argv[0]);

	    if (rc != 0)
		fprintf(stderr, "usage: %s year[/mon[/day]]\n", pgm);
	    exit(rc);
	}
	x.tm_year -= 1900;
	x.tm_mon --;

	today = localtime(&now);

	if (levels >= 1)
	    buildhomepage = (today->tm_year == tm->tm_year);

	if (levels >= 2)
	    buildhomepage = buildhomepage && (today->tm_mon == tm->tm_mon);

	if (levels == 3)
	    flags |= INDEX_DAY;

	buildarchivepage = 1;
    }
    else {
	buildhomepage = 1;
	levels = 3;
	tm = localtime(&now);
    }

    if (buildarchivepage)
	archivepage();


    if (levels == 1) {
	int mon;

	for (mon=0; mon < 12; mon++) {
	    tm->tm_mon = mon;
	    generate(tm, ".", flags, PG_ARCHIVE);
	}
    }
    else {
	generate(tm, ".", flags, PG_ARCHIVE);
    }
    pagetime = *localtime(&now);

    if (buildhomepage)
	generate(&pagetime, ".", 1, PG_HOME|PG_POST);

    if (buildhomepage || buildsyndicate) {
	syndicate(&pagetime, bbsroot, &rss2feed);
	syndicate(&pagetime, bbsroot, &atomfeed);
    }

    exit(0);
}
