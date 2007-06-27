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

#define IT(s,n,p)	{ s, sizeof s, n, &(fmt.p) }
static struct _keys {
    char *name;
    int   szname;
    int   results;
    void *ptr;
} keys[] = {
    IT("Name", 1, name),
    IT("URL", 1, url),
    IT("Author",1, author),
    IT("About", 1, about),
    IT("Chapter", 2, chapter),
    IT("Article", 2, article),
    IT("Title", 2, title),
    IT("Byline", 2, byline),
    IT("Body", 2, body),
    IT("Post", 2, post),
    IT("Edit", 2, edit),
    IT("Comment", 2, comment),
    IT("Archive", 2, archive),
    IT("Separator", 1, separator),
    IT("CommentSep", 1, commentsep),
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
    fmt.chapter.start = "<H2 class=chapter>";
    fmt.chapter.end   = "</H2>";
    fmt.article.start = "<div id=article>";
    fmt.article.end   = "</div>";
    fmt.title.start   = "<H3 class=title>";
    fmt.title.end     = "</H3>";
    fmt.byline.start  = "<p class=byline>--";
    fmt.byline.end    = "</p>";
    fmt.body.start    = "<div id=body>";
    fmt.body.end      = "</div>";
    fmt.post.start    = "<h4 class=post>";
    fmt.post.end      = "</h4>";
    fmt.edit.start    = "<p class=edit>";
    fmt.edit.end      = "</p>";
    fmt.archive.start = "<hr class=archive><h2 class=archive>Archives</h2>";
    fmt.comment.start = "<p class=comment>";
    fmt.comment.end   = "</p>";
    fmt.commentsep    = "<hr>";
    fmt.separator     = "";

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

	    for (i=NRKEY; i-- > 0; )
		if (strncasecmp(1+q, keys[i].name, keys[i].szname-1) == 0 && q[keys[i].szname] == ']') {
		    if (keys[i].results > 0)
			nextline = eoln(q=nextline, end);

		    switch (keys[i].results) {
		    case 2:
			m = keys[i].ptr;
			m->start = restofline(q, nextline);
			nextline = eoln(q=1+nextline, end); 
			m->end = restofline(q, nextline);
			++nextline;
			goto more;
		    case 1:
			*((char**)keys[i].ptr) = restofline(q,nextline);
			++nextline;
			goto more;
		    case 0:
			*((int*)keys[i].ptr) = 1;
			goto more;
		    }
		}
	    more: 1;
	}
	munmap(text, size);
    }

    if (fmt.name && fmt.name[0])
	stash("NAME", fmt.name);
}
