#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/file.h>

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
format(FILE *f, char *text, int flags)
{
    char *p, *q;
    int newpara = 1;
    int didpara = 0;
    char *para;
    int bold = 0;	/* 0 = not bold, 1 = *bold*, 2 = _bold_ */
    int quote = 0;

    if (flags & FM_COOKED) {
	fputs(text, f);
	return;
    }

    if ( (flags & FM_BLOCKED) ) {
	para = isspace(text[0]) ? "blockquote" : "p";
	fprintf(f, "<%s>\n", para);
	didpara=1;
    }

    for (p = text; *p; ++p) {
	if ( (*p == '\n') && (p[1] == '\n') ) {
	    ++p;
	    if (!newpara) {
		if (didpara)
		    fprintf(f, "</%s>\n", para);
		para = (p[1] == '\t' || p[1] == ' ') ? "blockquote" : "p";
		fprintf(f, "<%s>\n", para);
		newpara = 1;
		didpara = 1;
		continue;
	    }
	}

	newpara = 0;
	if (*p == '*') {
	    if ( (bold & 1) && !isspace(p[-1]) ) {
		fputs("</B>", f);
		bold &= ~1;
	    }
	    else if ( ((bold &1) == 0) && isalnum(p[1])) {
		fputs("<B>", f);
		bold |= 1;
	    }
	    else
		putchar(*p);
	}
	else if (*p == '_') {
	    if ( (bold & 2) && !isspace(p[-1]) ) {
		fputs("</I>", f);
		bold &= ~2;
	    }
	    else if ( ((bold & 2) == 0) && isalnum(p[1])) {
		fputs("<I>", f);
		bold |= 2;
	    }
	    else
		fputc(*p, f);
	}
	else if ( (*p == '{') && (flags & FM_IMAGES) ) {
	    int start;
	    char *align;

	    if (strncasecmp(p, "{pic:", 5) == 0) {
		start = 5;
		align="";
	    }
	    else if (strncasecmp(p, "{left:", 6) == 0) {
		start = 6;
		align=" align=left";
	    }
	    else if (strncasecmp(p, "{right:", 7) == 0) {
		start = 7;
		align=" align=right";
	    }
	    else if (strncasecmp(p, "{http:", 6) == 0) {
		fputs("<A HREF=\"", f);
		++p;
		while (*p && *p != '}')
		    fputc(*p++, f);
		fputc('"', f);
		fputc('>', f);
		if (*p) ++p;
		if (*p == '{') {
		    ++p;
		    while (*p && *p != '}')
			fputc(*p++, f);
		}
		else
		    fprintf(f, "link");
		fputs("</A>", f);
		continue;
	    }
	    else {
		fputc(*p, f);
		continue;
	    }

	    fprintf(f, "\n<IMG SRC=\"");
	    p += start;
	    if (strncasecmp(p, "http://", 7) != 0)
		fputs(fetch("weblog"), f);

	    while ( *p && (*p != '}') )
		fputc(*p++, f);
	    fputc('"', f);
	    if (p[1] == '{') {
		p += 2;
		fprintf(f, " width=");
		while ( *p && (*p != '}') )
		    fputc(*p++, f);
	    }
	    fprintf(f, "%s>", align);
	}
	else if (*p == '<' && !(isalpha(p[1]) || p[1] == '/') )
	    fputs("&lt;",f);
	else if (*p == '&' && isspace(p[1]))
	    fputs("&amp;",f);
	else if (*p == '\\' && 1[p])
	    fputc(*++p, f);
	else
	    fputc(*p, f);
    }
    if (didpara)
	fprintf(f, "</%s>\n", para);
}


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
subject(FILE *f, char *title)
{
    if (title) {
	fputs(fmt.title.start, f);
	format(f, title, 0);
	fputs(fmt.title.end, f);
    }
}

void
body(FILE *f, char *text, int flags)
{
    fputs(fmt.body.start, f);
    format(f, text, flags|FM_BLOCKED);
    fputs(fmt.body.end, f);
}

void
article(FILE *f, struct article *art, int flags)
{
    if (art->body) {
	subject(f,art->title);
	if (fmt.topsig)
	    byline(f, art, !(flags & FM_PREVIEW) );
	body(f, art->body, flags);
	if (!fmt.topsig)
	    byline(f, art, !(flags & FM_PREVIEW) );
    }
}
