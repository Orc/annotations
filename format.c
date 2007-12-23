#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/file.h>
#include <syslog.h>

#include <mkdio.h>

#include "formatting.h"
#include "indexer.h"

/*
 * formatting:  strip out all <>'s (by quoting them), replace
 * *text* with <B>text</B>, replace _text_ with <B>text</B>,
 * replace {pic:text} with <IMG SRC="text">, replace [[ ... ]]
 * with <blockquote> .. </blockquote>
 */


char *fetch(char*);

void
byline(FILE *f, struct article *art, int with_url)
{
    char *p;

    if (!art->author)
	return;

    fputs(fmt.byline.start, f);
    fprintf(f, "%s&nbsp;", art->author);
    if ( with_url && (p = fetch("_ROOT")) && art->url && art->timeofday) {
	fprintf(f, "<A HREF=\"%s%s\">%s</a>\n",
		    p, art->url, ctime(&art->timeofday));
	/* put in an edit button. */
    }

    fputs(fmt.byline.end, f);
}

void
subject(FILE *f, struct article *art, int oktolink)
{
    char *p;

    if (art->title) {
	fputs(fmt.title.start, f);
	if (oktolink && fmt.linktitle && (p = fetch("_ROOT")) )
	    fprintf(f,"<A HREF=\"%s%s\">\n", p, art->url);
	mkd_text(art->title, strlen(art->title), f, MKD_NOLINKS|MKD_NOIMAGE);
	if (oktolink && fmt.linktitle && p)
	    fprintf(f, "</A>\n");
	fputs(fmt.title.end, f);
    }
}


void
article(FILE *f, struct article *art, int flags)
{
    if (art->body) {
	subject(f, art, 0);
	if (fmt.topsig)
	    byline(f, art, !(flags & FM_PREVIEW) );

	fputs(fmt.body.start, f);
	switch ( art->format ) {
	case MARKDOWN:
		markdown(mkd_string(art->body,art->size), f, 0);
		break;
	default:
		fwrite(art->body,art->size,1,f);
		break;
	}
	fputs(fmt.body.end, f);

	if (!fmt.topsig)
	    byline(f, art, !(flags & FM_PREVIEW) );
    }
}
