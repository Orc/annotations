#ifndef _ARTICLES_D
#define _ARTICLES_D 1

#include <time.h>
#include "cstring.h"

typedef struct article {
    int    fd_text;	/* fd of body file (for mmap() purposes) */
    char  *body;	/* -> article body text */
    long   size;	/* size of article body */
    char  *author;	/* author (from control file) */
    time_t timeofday;	/* when written ( " " ) */
    time_t modified;	/* when last changed ( " " ) */
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
} ARTICLE;

typedef STRING(char*) Articles;

typedef int (*chooser)(char*,int,void*);

extern int foreach(char*,int,void*,chooser);

extern int every_post(char*,int,Articles*);	/* all articles in a day */
extern int every_day(char*,int,Articles*);	/* all articles in a month */
extern int every_month(char*,int,Articles*);	/* all articles in a year */
extern int every_year(int,Articles*);		/* all articles */
extern int latest(int,Articles*);		/* latest <n> articles */
extern int intersects(Articles,Articles);	/* articles in common? */

#endif/*_ARTICLES_D*/
