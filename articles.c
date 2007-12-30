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

typedef int (*desorter)(const void *, const void *);

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

static int
numericsort(char *a, char *b)
{
    char *ap, *bp;
    int av, bv;

    if ( !a ) return b ? -1 : 0;

    while ( *a && *b ) {
	if ( isdigit(*a) ) {
	    av = strtol(a, &ap, 10);
	    bv = strtol(b, &bp, 10);

	    if ( av != bv ) return av - bv;
	    a = ap;
	    b = bp;
	}
	else if ( *a == *b ) {
	    ++a;
	    ++b;
	}
	else
	    break;
    }
    return *a - *b;
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


static int
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


static void
pathname(char *dest, char *prefix, struct dirent *e)
{
#ifdef HAVE_D_NAMLEN
    if ( prefix )
	sprintf(dest, "%s/%.*s", prefix, e->d_namlen, e->d_name);
    else {
	strncpy(dest, e->d_name, e->d_namlen);
	dest[e->d_namlen] = 0;
    }
#else
    if ( prefix )
	sprintf(dest, "%s/%s", prefix, e->d_name);
    else
	strcpy(dest, e->d_name);
#endif
}

int
foreach(char *path, int count, void *context, chooser func)
{
    struct dirent **files = 0;
    int nrfiles;
    int i, ret;
    int added = 0;
    char *filepath;

    nrfiles = scandir(path ? path : ".", &files, choosede, (desorter)sortde);

    filepath = alloca( (path?strlen(path):0) + 1 + longest(files,nrfiles) + 1);

    for ( i=0; i < nrfiles; i++ ) {
	pathname(filepath, path, files[i]);

	if ( (ret = (*func)(filepath,count,context)) > 0 ) {
	    added += ret;
	    if ( count && ((count -= ret) <= 0) ) break;
	}
    }
    return added;
}


static int
added_okay(char *path, int ignored_count, Articles *p)
{
    char *file = alloca(strlen(path) + strlen("/message.ctl") + 1);

    sprintf(file, "%s/message.ctl", path);

    if ( access(file, R_OK) != 0 )
	return 0;

    EXPAND( *p ) = strdup(path);
    return 1;
}


/*
 * return all the articles for a given day
 */
int
dailyposts(char *path, int count, Articles *list)
{
    return foreach(path, count, list, (chooser)added_okay);
}


/*
 * return all the articles for a given month
 */
int
monthlyposts(char *path, int count, Articles *list)
{
    return foreach(path, count, list, (chooser)dailyposts);
}

/*
 * return all the articles for a given year
 */
int
yearlyposts(char *path, int count, Articles *list)
{
    return foreach(path, count, list, (chooser)monthlyposts);
}

/*
 * return every article
 */
int
everypost(int count, Articles *list)
{
    return foreach(0, count, list, (chooser)yearlyposts);
}


int
intersects(Articles a, Articles b)
{
    int cmp;

    if ( !( S(a) && S(b) ) ) return 0;

    if ( numericsort(T(a)[0], T(b)[0]) <= 0 ) 
	return numericsort(T(a)[0], T(b)[S(b)-1]) >= 0;

    return intersects(b,a);
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

    if ( everypost(count, &articles) == 0 )
	exit(0);

    printf("[`%s` ...", T(articles)[0]);
    printf(" `%s`]\n", T(articles)[S(articles)-1]);

    CREATE(A);
    CREATE(B);

    monthlyposts("2007/10", 0, &A);
    monthlyposts("2007/11", 0, &B);

    if ( S(A) && S(B) ) {
	if ( intersects(A,B) )
	    puts("[2007/10] intersects [2007/11] ?  No, this is wrong.");
	if ( intersects(B,A) )
	    puts("[2007/11] intersects [2007/10] ?  No, this is wrong.");
    }

    if ( S(A) ) {
	if ( !intersects(A,A) )
	    puts("[2007/10 does not intersect [2007/10] ?  No, this is wrong.");
    }

    if ( cmp = intersects(A,articles) ) 
	printf("[latest %d] intersects [2007/10]\n", count);

    if ( cmp = intersects(B,articles) )
	printf("[latest %d] intersects [2007/11]\n", count);
}
#endif
