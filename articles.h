#ifndef _ARTICLES_D
#define _ARTICLES_D 1

#include "cstring.h"

typedef struct entry {
    char *ctl;
    char *text;
    char *index;
} Article;

typedef STRING(struct entry) Articles;

typedef int (*desorter)(const void *, const void *);

#endif/*_ARTICLES_D*/
