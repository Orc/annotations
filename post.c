#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <dirent.h>
#include <sys/file.h>
#include <ctype.h>


#include "indexer.h"
#include "formatting.h"
#include "mapfile.h"
#include "syndicate.h"


void
addcomments(FILE *f, struct article *art)
{
    char *com;
    long size;

    if (com = mapfile(art->cmtfile, &size)) {
	fprintf(f, "<!-- comments -->\n");
	fwrite(com, size, 1, f);
	munmap(com, size);
    }
    if (art->comments_ok) {
	fprintf(f, "<form method=\"get\" action=\"%scomment\">\n",
		    fetch("_ROOT"));
	fputs(fmt.comment.start, f);
	fprintf(f, "<input type=\"submit\" name=\"comment\" value=\"Comment\" />\n");
	fprintf(f, "<input type=\"hidden\" name=\"url\" value=\"%s\" />\n", art->url);
	fputs(fmt.comment.end, f);
	fprintf(f, "</form>\n");
    }
}


int
post(struct article *art, char *bbspath, int cooked)
{
    struct tm tm;
    extern char *lastlast;
    FILE *f;


    if ( chdir(bbspath) != 0 || newart(art) == 0 || art->url == 0) return 0;

    tm = *localtime(&art->timeofday);
    art->modified = art->timeofday;

    if (f = fopen("index.db", "a")) {
	flock(fileno(f), LOCK_EX);
	fprintf(f, "%s:%s\n", art->url, art->title);
	fflush(f);
	flock(fileno(f), LOCK_UN);
	fclose(f);
    }

    writectl(art);
    writemsg(art);
    writehtml(art);

    if (lastlast) {
	struct article *l;
	char filename[80];
	char *p;

	strncpy(filename, lastlast, 80);
	if ( p = strchr(filename, ':') ) {
	    *p = 0;
	    if (l = openart(filename)) {
		writehtml(l);
		freeart(l);
	    }
	}
    }

    generate(&tm,  bbspath, 0, PG_ALL);
    syndicate(&tm, bbspath, &rss2feed);
    syndicate(&tm, bbspath, &atomfeed);
    return 1;
}


int
edit(struct article *art, char *bbspath)
{
    struct tm tm, *today;
    int buildflags = PG_ARCHIVE;

    if ( chdir(bbspath) != 0) return 0;

    art->modified = time(0);
    tm = *localtime( &art->timeofday );
    today = localtime( &art->modified );

    if ( (tm.tm_year == today->tm_year) && (tm.tm_mon == today->tm_mon) )
	buildflags |= PG_HOME|PG_POST;

    writectl(art);
    writemsg(art);
    writehtml(art);

    generate(&tm, bbspath, 0, buildflags);
    return 1;
}
