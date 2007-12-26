#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "cstring.h"
#include "articles.h"

static int
freedea(struct dirent **dea, int count)
{
    int i;

    if ( dea ) {
	for (i=0; i < count; i++)
	    free(dea[i]);
	free(dea);
    }
}


static int
choosede(struct dirent *e)
{
    char *s;
    int i;

    if ( !(e && (s = e->d_name)) )
	return 0;

#if HAVE_D_NAMLEN
    if ( !e->d_namlen ) return 0;
    for ( i = 0; i < e->d_namlen; i++)
	if ( !isdigit(s[i]) ) return 0;
#else
    if ( !*s ) return 0;
    while (*s)
	if ( !isdigit(*s++) ) return 0;
#endif

    return 1;
}


static int
sortde(struct dirent **a, struct dirent **b)
{
    return atoi((*b)->d_name) - atoi((*a)->d_name);
}


static char*
mkfile(char *path, char *file)
{
    int len = strlen(path) + strlen(file) + 3;
    char *ret = malloc(len);

    assert(ret);

    sprintf(ret, "%s/%s", path, file);
    return ret;
}


int
longest(struct dirent **de, int count)
{
    int len, max = 0;

    while ( count-- > 0 )
#ifdef HAVE_D_NAMLEN
	if ( (len=(*de++)->d_namlen) > max )
	    max = len;
#else
	if ( (len=strlen((*de++)->d_name)) > max )
	    max = len;
#endif
    return max;
}


void
pathname(char *dest, char *prefix, struct dirent *e)
{
#ifdef HAVE_D_NAMLEN
    sprintf(dest, "%s/%.*s", prefix, e->d_namlen, e->d_name);
#else
    sprintf(dest, "%s/%s", prefix, e->d_name);
#endif
}


typedef int (*chooser)(char*,int,void*);


int
foreach(char *path, int count, void *context, chooser func)
{
    struct dirent **files = 0;
    int nrfiles;
    int i, ret;
    int added = 0;
    char *filepath;

    nrfiles = scandir(path, &files, choosede, (desorter)sortde);
    filepath = alloca(strlen(path) + 1 + longest(files,nrfiles) + 1);

    for ( i=0; i < nrfiles; i++ ) {
	pathname(filepath, path, files[i]);
	if ( (ret = (*func)(filepath,count,context)) > 0 ) {
	    added += ret;
	    if ( count && ((count -= ret) <= 0) ) break;
	}
    }
    return added;
}


int
added_okay(char *path, int ignored_count, Articles *p)
{
    struct entry *ret;
    char *file = mkfile(path, "message.ctl");

    if ( access(path, R_OK) != 0 ) {
	free(file);
	return 0;
    }
    ret = &EXPAND( *p );

    ret->ctl = file;
    ret->text = mkfile(path, "message.txt");
    ret->index = mkfile(path, "index.html");
    return 1;
}


int
every_post(char *path, int count, Articles *list)
{
    return foreach(path, count, list, (chooser)added_okay);
}


int
every_day(char *path, int count, Articles *list)
{
    return foreach(path, count, list, (chooser)every_post);
}

int
every_month(char *path, int count, Articles *list)
{
    return foreach(path, count, list, (chooser)every_day);
}

int
every_year(char *path, int count, Articles *list)
{
    return foreach(path, count, list, (chooser)every_month);
}


int
getlatest(char *path, int count, Articles *list)
{
    every_year(path, count, list);
}


int
intersects(Articles a, Articles b)
{
    int cmp;

    if ( !( S(a) && S(b) ) ) return 0;

    cmp = strcmp(T(a)[0].ctl, T(b)[0].ctl);

    if ( cmp > 0 ) {	/* A[0] < B[0]; if A[S(A)-1] < B[0] it doesn't match */
	cmp = strcmp(T(a)[S(a)-1].ctl, T(b)[0].ctl);
	return cmp <= 0;
    }

    if ( cmp < 0 )
	return intersects(b,a);

    return 1;
}


#if TESTING
main(argc, argv)
int argc;
char **argv;
{
    int i, cmp, expected;
    Articles articles;
    Articles A, B;
    int count = 10;
    char* webroot = ".";

    if ( argc )
	switch ( argc ) {
	default:
	case 3:  count = atoi(argv[2]);
	case 2:  webroot = argv[1];
	case 1:  break;
	}

    if ( chdir(webroot) != 0 ) {
	perror(webroot);
	exit(1);
    }
    CREATE(articles);

    getlatest(webroot, count, &articles);
    for ( i = 0; i < S(articles); i++ )
	printf("%d: %s\n", i, T(articles)[i].index);

    if ( !S(articles) )
	exit(0);

    CREATE(A);
    CREATE(B);

    every_day("2007/10", 0, &A);
    every_day("2007/11", 0, &B);


    if ( intersects(A,B) )
	puts("failure: [2007/10] should not intersect [2007/11], but does");
    if ( intersects(B,A) )
	puts("failure: [2007/11] should not intersect [2007/10], but does");

    if ( !intersects(A,A) )
	puts("failure: [2007/11] should intersect [2007/11], but does not");


    cmp = intersects(A,articles);
    expected = !!(strcmp(T(A)[0].ctl, T(articles)[S(articles)-1].ctl) > 0);

    if ( cmp ^ expected )
	printf("[latest %d] should%s intersect [2007/10], but does%s\n",
		    count, cmp ? "" : " not", cmp ? " not" :"" );
}
#endif
