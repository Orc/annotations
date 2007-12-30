#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>

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
    while ( (c = fgetc(f)) != EOF && c != '\n' )
	EXPAND(*line) = c;

    EXPAND(*line) = 0;
    return T(*line);
}


static void
unmapfd(ARTICLE *a)
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
mapfd(ARTICLE *a, char *file)
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
	    unmapfd(a);
    }
    return a->fd_text;
}


ARTICLE *
openart(char *pathto)
{
    char *txt, *ctl;
    char *opt;
    Cstring line;
    FILE *f;
    ARTICLE *ret = newart();

    if ( !ret ) return 0;

    txt = alloca(strlen(pathto) + sizeof CTLFILE + 1);
    ctl = alloca(strlen(pathto) + sizeof TXTFILE + 1);

    sprintf(txt, "%s" CTLFILE, pathto);
    sprintf(ctl, "%s" TXTFILE, pathto);

    if (  (ret->path = strdup(pathto)) == 0 ) {
	freeart(ret);
	return 0;
    }

    if ( mapfd(ret,txt) == -1 ) {
	freeart(ret);
	return 0;
    }

    if ( !(f = fopen(ctl, "r")) ) {
	freeart(ret);
	return 0;
    }

    while ( fgetcs(&line,f) ) {
	opt = argument(T(line));

	switch (T(line)[0]) {
	case 'A':   ret->author = strdup(opt);
		    break;
	case 'W':   ret->timeofday = atol(opt);
		    break;
	case 'M':   ret->modified = atol(opt);
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

    unmapfd(a);

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

    unmapfd(a);

    if ( (q = tmpfile()) == 0 )
	return -1;

    a->fd_text = dup(fileno(q));
    fclose(q);

    a->size = lseek(a->fd_text, 0, SEEK_END);
    a->body = mmap(0, a->size, PROT_READ, MAP_SHARED, a->fd_text, 0L);

    if ( a->body == MAP_FAILED ) {
	unmapfd(a);
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


int author(ARTICLE*,char*);		/* set the author of an article */
int category(ARTICLE*,char*);		/* set the categories of an article */
int comments(ARTICLE*,int);		/* set comments_ok (1/0) */
int format(ARTICLE*,int);		/* set article format */
int saveart(ARTICLE*);			/* save and close an article */
int freeart(ARTICLE*);			/* discard an article */
