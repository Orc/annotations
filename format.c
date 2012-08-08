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

char *fetch(char*);

void
byline(FILE *f, struct article *art, int with_url)
{
    char *r, *p;

    if (!art->author)
	return;

    if ( (r = fmt.byline) == 0 )
	r = "&mdash;%A %D";

    fprintf(f, "<p class=\"byline\">");

    for ( r = fmt.byline; *r; ++r ) {
	if ( *r == '%' ) {
	    switch (*++r) {
	    case 'A': 
		fputs(art->author, f);
		break;
	    case 'D':
		if ( with_url && (p = fetch("_ROOT")) && art->url && art->timeofday)
		    fprintf(f, "<a href=\"%s%s\">%s</a>\n", p, art->url, ctime(&art->timeofday));
		else
	    case 'd':
		    fputs(ctime(&art->timeofday), f);
		break;
	    case '%':
		putc('%', f);
		break;
	    }
	}
	else if ( *r == '\\' )
	    switch ( *++r ) {
	    case '\n':  putc('\n', f); break;
	    case '\\':  putc('\\', f); break;
	    case ' ':   fputs("&nbsp;", f); break;
	    }
	else
	    putc(*r, f);
    }
    fprintf(f, "</p>\n");
}


void
subject(FILE *f, struct article *art, int oktolink)
{
    char *p;

    if (art->title) {
	fputs(fmt.title.start, f);
	if (oktolink && fmt.linktitle && (p = fetch("_ROOT")) )
	    fprintf(f,"<a href=\"%s%s\">\n", p, art->url);
	mkd_text(art->title, strlen(art->title), f, MKD_NOLINKS|MKD_NOIMAGE);
	if (oktolink && fmt.linktitle && p)
	    fprintf(f, "</a>\n");
	fputs(fmt.title.end, f);
    }
}


void
article(FILE *f, struct article *art, int flags)
{
    MMIOT *doc;

    if (art->body) {
	subject(f, art, 0);
	if (fmt.topsig)
	    byline(f, art, 0);

	fputs(fmt.body.start, f);
	switch ( art->format ) {
	case MARKDOWN:
		doc = mkd_string(art->body,art->size, MKD_NOHEADER);
		if ( fmt.base )
		    mkd_basename(doc, fmt.base);
		markdown(doc, f, 0);
		break;
	default:
		fwrite(art->body,art->size,1,f);
		break;
	}
	fputs(fmt.body.end, f);

	if (!fmt.topsig)
	    byline(f, art, 0);
    }
}
