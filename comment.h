#ifndef __COMMENT_D
#define __COMMENT_D

#include "indexer.h"
#include <time.h>

struct comment {
    struct article *art;
    char  *file;
    char  *author;
    char  *email;
    char  *website;
    char  *text;
    time_t when;
    int    approved;		/* approved for publication? */
    int    publish_mail;	/* publish the email address? */
};

struct comment *newcomment(struct article *);
struct comment *opencomment(char *f);
int             savecomment(struct comment *);
void            freecomment(struct comment *);

#endif/*__COMMENT_D*/
