/*
 * read configuration information for the weblog
 */
#include "config.h"
#include "formatting.h"
#include "mapfile.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>


extern char *restofline(char *, char *);

char *
eoln(char *p, char *end)
{

    for ( ;p < end; ++p)
	if (*p == '\n')
	    return p;
    return end;
}

struct fmt fmt;

enum kind { BOOL, NUMBER, SINGLE, DOUBLE };

#define IT(s,n,p)	{ s, (sizeof s)-1, n, &(fmt.p) }
static struct _keys {
    char *name;
    int   szname;
    enum kind results;
    void *ptr;
} keys[] = {
    IT("Name", SINGLE, name),
    IT("HomePage", SINGLE, homepage),
    IT("Filter", SINGLE, filter),
    IT("URL", SINGLE, url),
    IT("Author",SINGLE, author),
    IT("About", SINGLE, about),
    IT("nrposts", NUMBER, nrposts),
    IT("topsig", BOOL, topsig),
    IT("Chapter", DOUBLE, chapter),
    IT("Article", DOUBLE, article),
    IT("Title", DOUBLE, title),
    IT("Byline", DOUBLE, byline),
    IT("Body", DOUBLE, body),
    IT("Post", DOUBLE, post),
    IT("Edit", DOUBLE, edit),
    IT("LinkTitle", BOOL, linktitle),
    IT("SimpleArchive", BOOL, simplearchive),
    IT("Comment", DOUBLE, comment),
    IT("Archive", DOUBLE, archive),
    IT("Separator", SINGLE, separator),
    IT("CommentSep", SINGLE, commentsep),
    IT("ReadMore", SINGLE, readmore),
};
#define NRKEY	(sizeof keys/sizeof keys[0])


void
readconfig(char *path)
{
    char *cf = alloca(strlen(path) + 20);
    char *text;
    long size;
    char *q, *end;
    char *r;
    struct markup *m;
    struct _keys *p;

    memset(&fmt, 0, sizeof fmt);
    fmt.name          = "";
    fmt.homepage      = "index.html";
    fmt.filter        = 0;
    fmt.chapter.start = "<H2 CLASS=chapter>";
    fmt.chapter.end   = "</H2>";
    fmt.article.start = "<div CLASS=article>";
    fmt.article.end   = "</div>";
    fmt.title.start   = "<H3 CLASS=title>";
    fmt.title.end     = "</H3>";
    fmt.byline.start  = "<p CLASS=byline>--";
    fmt.byline.end    = "</p>";
    fmt.body.start    = "<div CLASS=body>";
    fmt.body.end      = "</div>";
    fmt.post.start    = "<h4 CLASS=post>";
    fmt.post.end      = "</h4>";
    fmt.edit.start    = "<p CLASS=edit>";
    fmt.edit.end      = "</p>";
    fmt.archive.start = "<hr CLASS=archive><h2 CLASS=archive>Archives</h2>";
    fmt.comment.start = "<p CLASS=commentbutton>";
    fmt.comment.end   = "</p>";
    fmt.commentsep    = "<hr>";
    fmt.separator     = "";
    fmt.nrposts       = 10;
    fmt.readmore      = "Read More...";

    if (cf == 0)
	return;

    sprintf(cf, "%s/weblog.conf", path);

    if (text = mapfile(cf, &size)) {
	char *nextline;
	int i;

	q = text;
	end = q + size;

	for ( ;q < end; q = nextline+1) {
	    nextline = eoln(q, end);

	    for (p = &keys[0], i=NRKEY; i-- > 0; p++) {

		if (strncasecmp(q, p->name, p->szname) != 0
				    || isalnum(q[p->szname]))
		    continue;

		q += p->szname;

		switch (p->results) {
		case DOUBLE:
		    m = p->ptr;
		    if (strncasecmp(q, ".start=", 7) == 0) {
			m->start = restofline(q+7, nextline);
			/*printf("%s.start=%s\n", p->name, m->start);*/
		    }
		    else if (strncasecmp(q, ".end=", 5) == 0) {
			m->end = restofline(q+5, nextline);
			/*printf("%s.end=%s\n", p->name, m->end);*/
		    }
		    break;
		case SINGLE:
		    if ( *q == '=' ) {
			*((char**)p->ptr) = restofline(q+1,nextline);
			/*printf("%s=%s\n", p->name, *(char**)p->ptr);*/
		    }
		    break;
		case NUMBER:
		case BOOL:
		    if ( *q == '=' ) {
			*((int*)p->ptr) = atoi(q+1);
			/*printf("%s=%d\n", p->name, *(int*)p->ptr);*/
		    }
		    break;
		}
		break;
	    }
	}
	munmap(text, size);
    }

    if (fmt.name && fmt.name[0])
	stash("NAME", fmt.name);
}
