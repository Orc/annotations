#ifndef __INDEXER_D
#define __INDEXER_D

#include <time.h>

struct article {
    char  *ctlfile;
    char  *msgfile;
    char  *cmtfile;
    char  *cmtdir;
    char  *body;
    long   size;
    char  *author;
    time_t timeofday;
    time_t modified;
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
};

int buildpages(struct tm *, int);
#define PG_HOME		0x01
#define PG_ARCHIVE	0x02
#define PG_POST		0x04
#define	PG_ARTICLE	0x08
#define PG_SYNDICATE	0x10
#define PG_ALL		(PG_HOME|PG_ARCHIVE|PG_POST|PG_ARTICLE|PG_SYNDICATE)

int reindex(struct tm *, char *, int, int);

#define INDEX_FULL	0x01
#define INDEX_DAY	0x02

void putindex(FILE*);

char* makefile(char*, char*);

struct article *openart(char*);
int             newart(struct article*);
int             writectl(struct article*);
int             writemsg(struct article*, int);
void            freeart(struct article*);


typedef int (*stfu)(const void*,const void*);
FILE *rewrite(char*);

#endif/*__INDEXER_D*/
