/*
 *  rewrite()/rclose() wrappers for posting
 */

#include <stdio.h>
#include "config.h"
#include "formatting.h"


FILE *
rewrite(char *f)
{
    if (fmt.filter) {
	char cmd[10000];	/* horrid kludge */

	sprintf(cmd, "%s > %s", fmt.filter, f);
	return popen(cmd, "w");
    }
    else
	return fopen(f, "w");
}


void
rclose(FILE *f)
{
    if (fmt.filter)
	pclose(f);
    else
	fclose(f);
}
