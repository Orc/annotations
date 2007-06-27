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

#define IT(s,n,p)	{ s, sizeof s, n, &(fmt.p) }
static struct _keys {
    char *name;
    int   szname;
    enum kind results;
    void *ptr;
} keys[] = {
    IT("Name", SINGLE, name),
    IT("HomePage", SINGLE, homepage),
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
    IT("Comment", DOUBLE, comment),
    IT("Archive", DOUBLE, archive),
    IT("Separator", DOUBLE, separator),
    IT("CommentSep", DOUBLE, commentsep),
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

    memset(&fmt, 0, sizeof fmt);
    fmt.name          = "";
    fmt.homepage      = "index.html";
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
    fmt.comment.start = "<p CLASS=comment>";
    fmt.comment.end   = "</p>";
    fmt.commentsep    = "<hr>";
    fmt.separator     = "";
    fmt.nrposts       = 10;

    if (cf == 0)
	return;

    sprintf(cf, "%s/config", path);

    if (text = mapfile(cf, &size)) {
	char *nextline;
	int i;

	q = text;
	end = q + size;

	for ( ;q < end; q = nextline) {
	    nextline = 1+eoln(q, end);

	    if (*q != '[')
		continue;

	    for (i=NRKEY; i-- > 0; ) {
		if (strncasecmp(1+q, keys[i].name, keys[i].szname-1) != 0 || q[keys[i].szname] != ']')
		    continue;

		if (keys[i].results > 0)
		    nextline = eoln(q=nextline, end);

		switch (keys[i].results) {
		case DOUBLE:
		    m = keys[i].ptr;
		    m->start = restofline(q, nextline);
		    nextline = eoln(q=1+nextline, end); 
		    m->end = restofline(q, nextline);
		    ++nextline;
		    break;
		case SINGLE:
		    *((char**)keys[i].ptr) = restofline(q,nextline);
		    ++nextline;
		    break;
		case NUMBER:
		    *((int*)keys[i].ptr) = atoi(q);
		    break;
		case BOOL:
		    *((int*)keys[i].ptr) = 1;
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
