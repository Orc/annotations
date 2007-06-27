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
    char *url;			/* url for the weblog */
    char *author;		/* author for the weblog */
    char *about;		/* one line description of the weblog */
    int nrposts;		/* # of articles on the homepage */
    int topsig;			/* signature at the start of a post? */
    int linktitle;		/* titles are also used as permlinks */
    int simplearchive;		/* do the archive as a list body */
    struct markup article;	/* markup around an article in index.html */
    struct markup body;		/* markup around the body of an article */
    struct markup title;	/*  "  "   " "       subject      " " */
    struct markup byline;	/*  "  "   " "       byline      " " */
    struct markup chapter;	/*  "  "   " "  the date header in index.html */
    struct markup post;
    struct markup edit;
    struct markup comment;
    struct markup archive;
    char *separator;		/* what to separate articles with */
    char *commentsep;		/* what to separate comments with */
};

void getconfig();
extern struct fmt fmt;
void format(FILE*, char *, int);

#define FM_IMAGES	0x01
#define	FM_COOKED	0x02
#define	FM_PREVIEW	0x04
#define FM_BLOCKED	0x08	/* should the format be wrapped in <p>? */
#define FM_STRIP	0x10
#define FM_ONELINE	0x20	/* format only one line */

#endif/*__FORMATTING_D*/
