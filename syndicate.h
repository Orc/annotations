#ifndef SYNDICATE_D
#define SYNDICATE_D

#include <stdio.h>
#include <time.h>
#include "indexer.h"

struct syndicator {
    char *filename;		/* file we're writing to */
    int (*ok)();
    int (*header)(FILE *);
    int (*article)(FILE *, struct article *);
    int (*fini)(FILE *);
};

extern struct syndicator rss2feed;
extern struct syndicator atomfeed;

int syndicate(struct tm *, char *, struct syndicator *);

#endif/*SYNDICATE_D*/
