#ifndef __FORMATTING_D
#define __FORMATTING_D

#include <stdio.h>

struct markup {
    char *start;	/* markup to put at the start of an element */
    char *end;		/* markup to put at the end of an element */
} ;

struct fmt {
    char *name;			/* name of the weblog */
    char *homepage;		/* what page it lives at */
    char *filter;		/* postprocessing filter */
    char *base;			/* base url (machine-name part of url or nothing) */
    char *url;			/* url for the weblog */
    char *author;		/* author for the weblog */
    char *about;		/* one line description of the weblog */
    int nrposts;		/* # of articles on the homepage */
    int topsig;			/* signature at the start of a post? */
    int linktitle;		/* titles are also used as permlinks */
    int simplearchive;		/* do the archive as a list body */
    int calendararchive;	/* do the archive as month calendar */
    struct markup article;	/* markup around an article in index.html */
    struct markup body;		/* markup around the body of an article */
    struct markup title;	/*  "  "   " "       subject      " " */
    struct markup chapter;	/*  "  "   " "  the date header in index.html */
    struct markup post;
    struct markup edit;
    struct markup comment;
    struct markup archive;
    char * byline;		/* What's in the byline? */
    char *separator;		/* what to separate articles with */
    char *commentsep;		/* what to separate comments with */
    char *readmore;		/* ``Read more...'' or similar text */
};

void getconfig();
extern struct fmt fmt;

#define FMT_FLAGS	MKD_DLEXTRA|MKD_FENCEDCODE

#endif/*__FORMATTING_D*/
