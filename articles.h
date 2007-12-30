#ifndef _ARTICLES_D
#define _ARTICLES_D 1

#include <time.h>
#include "cstring.h"

typedef STRING(char*) Articles;

typedef int (*chooser)(char*,int,void*);

extern int foreach(char*,int,void*,chooser);

extern int dailyposts(char*,int,Articles*);	/* all articles in a day */
extern int monthlyposts(char*,int,Articles*);	/* all articles in a month */
extern int yearlyposts(char*,int,Articles*);	/* all articles in a year */
extern int everypost(int,Articles*);		/* all articles */
extern int intersects(Articles,Articles);	/* articles in common? */

typedef struct article {
    int    fd_text;	/* fd of body file (for mmap() purposes) */
    char  *body;	/* -> article body text */
    long   size;	/* size of article body */
    char  *author;	/* author (from control file) */
    time_t timeofday;	/* when written ( " " ) */
    time_t modified;	/* when last changed ( " " ) */
    char  *path;
    char  *title;
    char  *category;
    char  *url;
    char  *preview;
    int    comments;	/* how many comments */
    int    comments_ok;	/* ok to post comments? */
    int    moderated;	/* okay to publish comments? */
    int    format;	/* preformatted or not? */
#define HTML 0
#define MARKDOWN 1
    int    flags;
#define N_BODY	0x01	/* body has been updated */
#define N_TITLE	0x02	/* title has been updated */
#define N_AUTH	0x04	/* author has been updated */
#define N_CAT	0x08	/* category has been updated */
#define N_FLAGS	0x10	/* comments_ok/moderated have been updated */
} ARTICLE;

ARTICLE *openart(char *);		/* open an existing article */
ARTICLE *newart();			/* create a new article */
int body(ARTICLE*, char*, int);		/* set the body of an article */
int bodyf(ARTICLE*, FILE*);		/* set the body of an article */
int title(ARTICLE*,char*);		/* set the title of an article */
int author(ARTICLE*,char*);		/* set the author of an article */
int category(ARTICLE*,char*);		/* set the categories of an article */
int comments(ARTICLE*,int);		/* set comments_ok (1/0) */
int format(ARTICLE*,int);		/* set article format */
int saveart(ARTICLE*);			/* save and close an article */
void freeart(ARTICLE*);			/* discard an article */

#endif/*_ARTICLES_D*/
