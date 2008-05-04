#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>

#include <mkdio.h>

#include "articles.h"

#define CTLFILE	"/message.ctl"
#define TXTFILE	"/message.txt"
#define HTMLFIL	"/index.html"


static char *
argument(char *s)
{
    while ( *s != ':' ) 
	if ( *s++ == 0 ) 
	    return 0;

    for ( ++s; isspace(*s); ++s )
	;
    return s;
}


char *
fgetcs(Cstring *line, FILE *f)
{
    int c;

    S(*line) = 0;
    while ( ((c = fgetc(f)) != EOF) && (c != '\n') )
	EXPAND(*line) = c;

    if ( (c == EOF) && (S(*line) == 0) ) return 0;

    EXPAND(*line) = 0;
    return T(*line);
}


static void
unmapbody(ARTICLE *a)
{
    if ( !a ) return;

    if ( a->fd_text != -1 ) {
	if ( a->body && (a->body != MAP_FAILED) )
	    munmap(a->body, a->size);

	flock(a->fd_text, LOCK_UN);
	close(a->fd_text);
	a->fd_text = -1;
    }
    a->body = 0;
    a->size = 0;
}


static int
mapbody(ARTICLE *a, char *file)
{
    if ( !a ) return -1;

    if ( (a->fd_text = open(file, O_RDONLY)) != -1 ) {
	if ( flock(a->fd_text, LOCK_SH) == -1 ) {
	    close(a->fd_text);
	    return -1;
	}
	a->size = lseek(a->fd_text, 0, SEEK_END);
	a->body = mmap(0,a->size,PROT_READ,MAP_SHARED,a->fd_text,0L);

	if ( a->body == MAP_FAILED )
	    unmapbody(a);
    }
    return a->fd_text;
}


ARTICLE *
openart(char *pathto)
{
    char *txt, *ctl;
    char *opt;
    Cstring line;
    struct tm *tm;
    FILE *f;
    ARTICLE *ret = newart();

    if ( !ret ) return 0;

    ctl = alloca(strlen(pathto) + sizeof CTLFILE + 1);
    txt = alloca(strlen(pathto) + sizeof TXTFILE + 1);

    sprintf(ctl, "%s" CTLFILE, pathto);
    sprintf(txt, "%s" TXTFILE, pathto);

    if (  (ret->path = strdup(pathto)) == 0 ) {
	freeart(ret);
	return 0;
    }

    if ( mapbody(ret,txt) == -1 ) {
	freeart(ret);
	return 0;
    }

    if ( !(f = fopen(ctl, "r")) ) {
	freeart(ret);
	return 0;
    }

    CREATE(line);
    while ( fgetcs(&line,f) ) {
	opt = argument(T(line));

	switch (T(line)[0]) {
	case 'A':   ret->author = strdup(opt);
		    break;
	case 'W':   ret->timeofday = atol(opt);
		    if ( tm = localtime(&ret->timeofday) )
			ret->ctime = *tm;
		    break;
	case 'M':   ret->modified = atol(opt);
		    if ( tm = localtime(&ret->modified) )
			ret->mtime = *tm;
		    break;
	case 'T':   ret->title = strdup(opt);
		    break;
	case 'U':   ret->url = strdup(opt);
		    break;
	case 'C':   ret->comments_ok = atoi(opt);
		    break;
	case 'F':   ret->comments = atoi(opt);
		    break;
	case 'Z':   switch ( toupper(*opt) ) {
		    case 'M':   ret->format = MARKDOWN;
				break;
		    case 'H':   ret->format = HTML;
				break;
		    }
		    break;
	case 'P':   ret->preview = strdup(opt);
		    break;
	case 'B':   ret->moderated = atoi(opt);
		    break;
	case 'X':   ret->category = strdup(opt);
		    break;
	default:    break;
	}
    }
    DELETE(line);
    fclose(f);

    return ret;
}


ARTICLE *
newart()
{
    ARTICLE *ret;

    if ( ret = calloc(sizeof *ret, 1) ) {
	ret->format = HTML;
	ret->fd_text = -1;
	ret->body = MAP_FAILED;
    }
    return ret;
}


int
body(ARTICLE *a, char *text, int size)
{
    FILE *f;

    if ( !a ) return -1;

    unmapbody(a);

    a->body = text;
    a->size = size;
    a->flags |= N_BODY;

    return 0;
}


int
bodyf(ARTICLE *a, FILE *f)
{
    FILE *q;

    if ( !a )
	return -1;

    unmapbody(a);

    if ( (q = tmpfile()) == 0 )
	return -1;

    a->fd_text = dup(fileno(q));
    fclose(q);

    a->size = lseek(a->fd_text, 0, SEEK_END);
    a->body = mmap(0, a->size, PROT_READ, MAP_SHARED, a->fd_text, 0L);

    if ( a->body == MAP_FAILED ) {
	unmapbody(a);
	return -1;
    }
    a->flags |= N_BODY;
    return 0;
}


int
title(ARTICLE *a, char *subj)
{
    if ( !a ) return -1;

    if ( a->title ) free(a->title);

    a->title = strdup(subj);
    a->flags |= N_TITLE;
    return 0;
}


int
author(ARTICLE *a, char *author)
{
    if ( !a ) return -1;
    if ( a->author ) free(a->author);

    a->author = strdup(author);
    a->flags |= N_AUTH;
    return 0;
}


int
category(ARTICLE *a, char *category)
{
    if ( !a ) return -1;

    if ( a->category ) free(a->category);

    a->category = strdup(category);
    a->flags |= N_CAT;
    return 0;
}


int
comments(ARTICLE *a, int comments_ok)
{
    if ( !a ) return -1;

    a->comments_ok = comments_ok;
    a->flags |= N_FLAGS;
    return 0;
}


int
format(ARTICLE *a, int format)
{
    if ( !a ) return -1;

    a->format = format;
    a->flags |=N_FLAGS;
    return 0;
}


void
freeart(ARTICLE* a)
{
    if ( a ) {
	if ( a->category ) free(a->category);
	if ( a->title ) free(a->title);
	if ( a->author ) free(a->author);
	if ( a->path ) free(a->path);
	if ( a->url ) free(a->url);
	if ( a->preview ) free(a->preview);
	unmapbody(a);
	free(a);
    }
}


int saveart(ARTICLE*);			/* save and close an article */


char *
get_article_theme()
{
    FILE *fd;
    char *q = 0;
    char *text;
    int len;
    MMIOT *doc;

    if ( fd = fopen("article.theme", "r") ) {
	doc = mkd_in(fd, 0);
	fclose(fd);
	if ( doc ) {
	    if ( mkd_compile(doc, 0) && (len = mkd_document(doc, &text)) )
		if ( q = calloc(1, len+1) )
		    memcpy(q, text, len);
	    mkd_cleanup(doc);
	}

    }

    if ( !q ) 
	q =  strdup("<h2><a href=\"$url\">$title</a></h2>\n"
		    "<p>Posted $date by $author "
			    "(<a href=\"$url\">permalink</a></p>\n"
		    "$text\n");
    assert(q != 0);
    return q;
}


void
discard_article_theme(char *theme)
{
    if ( theme )
	free(theme);
}


static int
match(char **input, char *pat)
{
    int len = strlen(pat);

    if ( (strncmp(*input,pat,len) == 0) && !isalnum((*input)[len]) ) {
	*input += len;
	return 1;
    }
    return 0;
}

int
pdate(struct tm *tm, char *format, FILE *out, int embed)
{
    char bfr[200];

    strftime(bfr, sizeof bfr, format, tm);
    if ( embed ) 
	mkd_text(bfr, strlen(bfr), out, 0);
    else
	markdown(mkd_string(bfr, strlen(bfr), 0), out, 0);
}

int
printheader(ARTICLE *a, int first, FILE *f)
{
    static int dy;

    if ( first ) dy = -1;

    if ( dy != a->ctime.tm_mday ) {
	dy = a->ctime.tm_mday;
	pdate(&a->ctime, "#%A, %B %d %Y#", f, 0);
    }
    else
	markdown(mkd_string("---",3,0), f, 0);
}


int
printart(ARTICLE *a, char *theme, FILE *f)
{
    char *time;
    MMIOT *doc;
    FILE *th = fopen("article.theme", "r");
    char bfr[80];
    int i, c;

    fprintf(f, "<div class=article>\n");
    while ( (c = *theme++) ) {
	if ( ((c == '%') && (strncmp(theme, "24", 2) == 0)) || (c == '$') ) {
	    if ( c == '%' )
		theme += 2;

	    if ( match(&theme, "title") )
		mkd_text(a->title, strlen(a->title), f, 0);
	    else if ( match(&theme, "text") )
		markdown(mkd_string(a->body, a->size, MKD_NOHEADER),f,0);
	    else if ( match(&theme, "date") )
		pdate(&a->ctime, "%I:%m %p %A, %B %d %Y", f, 1);
	    else if ( match(&theme, "url") )
		fwrite(a->url, strlen(a->url), 1, f);
	    else if ( match(&theme, "author") )
		mkd_text(a->author, strlen(a->author), f, 0);
	    else if ( match(&theme, "comments") ) {
		char bfr[80];

		if ( a->comments ) {
		    snprintf(bfr, sizeof bfr, "%d comment%s", a->comments, (a->comments!=1) ? "s" : "");
		    mkd_text(bfr, strlen(bfr), f, 0);
		}
		else
		    mkd_text("Comment?", 8, f, 0);
	    }
	    else
		fputs( (c=='$') ? "$" : "%24", f);
	}
	else
	    fputc(c, f);
    }
    fprintf(f, "</div>\n");
}


#if TESTING
main(argc,argv)
char **argv;
{
    char *theme;
    char *webroot = ".";
    int count = 10;
    Articles list;
    ARTICLE *art;
    int i, first;

    switch ( argc ) {
    default:	count = atoi(argv[2]);
    case 2:	webroot = argv[1];
    case 1:
    case 0:	break;
    }

    if ( chdir(webroot) == -1 ) {
	perror(webroot);
	exit(1);
    }

    theme = get_article_theme();

    CREATE(list);
    everypost(count,&list);

    theme = get_article_theme();

    for (i=0, first=1; i < S(list); i++) {
	if ( art=openart(T(list)[i]) ) {
	    printheader(art, first, stdout);
	    printart(art, theme, stdout);
	    freeart(art);
	    first=0;
	}
	else
	    perror(T(list)[i]);
    }

    DELETE(list);

    discard_article_theme(theme);
}
#endif
