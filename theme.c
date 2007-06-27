/*
 * theme() applies a theme to a webpage
 *
 * page.theme looks like
 *
 *   html html html html <?theme?> html html html
 */
#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <sys/mman.h>

static char *path = 0;
static char marker[] = "<?theme ";

static int avec_headers = 1;		/* needs ``Content-Type:'' as cgi */
static char *meta = 0;
static int szmeta = 0;
static char *rc = 0;			/* path to the theme file */
static char *document = 0;		/* path to the document */
static char *url = 0;			/* the whole path, including username */
static char *username = 0;		/* the user */
static int doc = 0;

typedef struct variable {
    char *var;
    char *value;
    struct variable *next;
} Variable;

static Variable *variables = 0;

extern void putbody();
int process(char *template, void (*putbody)(FILE*), int justdump, FILE *output);

/*
 * save a variable
 */
void
stash(char *var, char *value)
{
    Variable *tmp;

    if (value == 0) value="";

    /*syslog(LOG_INFO, "stash(%s,%s)", var, value);*/
    for (tmp=variables; tmp; tmp = tmp->next)
	if (strcmp(var, tmp->var) == 0) {
	    free(tmp->value);
	    tmp->value = strdup(value);
	    return;
	}

    if (tmp = malloc(sizeof *tmp)) {
	tmp->var   = strdup(var);
	tmp->value = strdup(value);
	tmp->next = variables;
	variables = tmp;
    }
}


/*
 * return a pointer to a variable
 */
char *
fetch(char *name)
{
    Variable *p = variables;

    while (p)
	if (strcasecmp(p->var, name) == 0)
	    return p->value;
	else
	    p = p->next;
    return 0;
}


/*
 * <?theme $var?>
 */
static void
putvariable(char *variable, FILE *f)
{
    char *p = fetch(variable);

    if (p) fwrite(p, 1, strlen(p), f);
}


/*
 * <?theme title?>
 */
static void
putpagetitle(FILE *f)
{
    char *title = fetch("title");

    if (title)
	fprintf(f,"<title>%s</title>", title);
}


/*
 * <?theme meta?>
 */
static void
putpagemeta(FILE *f)
{
    if (meta) fwrite(meta, 1, strlen(meta), f);
}


/*
 * <?theme rc?>
 */
static void
putrc(FILE *f)
{
    char *eop = strrchr(rc, '/');
    char *p = fetch("_ROOT");

    if (p)
	fprintf(f, "/%s/", p);
    else
	fprintf(f, "/~%s/", username);
    if (eop)
	fwrite(rc, (int)(eop-rc), 1, f);
}


/*
 * <?theme cwd?>
 */
static void
putcwd(FILE *f)
{
    char *eop = strrchr(document, '/');
    char *p = fetch("_ROOT");

    if (p) 
	fprintf(f, "/%s", p);
    else
	fprintf(f, "/~%s/", username);

    if (eop) {
	fwrite(document, (int)(eop-document), 1, f);
	fputc('/', f);
    }
}

/*
 * <?theme root?>
 */
void
putroot(FILE *f)
{
    int up = 0;
    char *p;
    char *root = fetch("_ROOT");


    if (root == 0)
	root ="/";

    if (document && strncasecmp(root, document, strlen(root)) == 0) {
	for (p = document + strlen(root); *p; ++p)
	    if ( (*p == '/') && (p[1] != '/')) up++;

	while (up-- > 0)
	    fprintf(f, "../");
    }
}


/*
 * <?theme sccs?>
 */
static void
putsccs(FILE *f)
{
    putvariable("sccs", f);
}


/*
 * <?theme [file]?>
 */
static void
putinclude(char *include, void (*putbody)(FILE*), FILE *f )
{
    int len = strlen(include);

    if (len > 0 && include[len-1] == ']')
	include[len-1] = 0;

    if ( !process(include, putbody, 0, f) )
	fprintf(f, "<!-- cannot open [%s]: %s -->\n", include, strerror(errno));
}


/*
 * process a <?theme XXX?> command
 */
struct op {
    char *keyword;
    void (*action)(FILE*);
} operations[] = {
    { "title",   putpagetitle },
    { "meta",    putpagemeta  },
    { "body",    0 },
    { "rc",      putrc },
    { "cwd",     putcwd },
    { "root",    putroot },
    { "sccs",    putsccs },
};
#define NR_OPS	(sizeof operations/sizeof operations[0])

static void
action(FILE *input, void (*putbody)(FILE*), FILE *output)
{
    int i=0, st=0;
    char what[80];
    register c;

    while ((c = fgetc(input)) != EOF) {
	if (c == '?' && (st == 0) )
	    st = 1;
	else if ( (c == '>') && (st == 1) )
	    break;
	else
	    st = 0;
	if (i < sizeof(what)-2)
	    what[i++] = c;
    }
    what[--i] = 0;

    if (what[0] == '$')
	putvariable(1+what, output);
    else if (what[0] == '[')
	putinclude(1+what, putbody, output);
    else {
	for (i=0; i < NR_OPS; i++)
	    if (strcasecmp(what, operations[i].keyword) == 0) {
		if (operations[i].action)
		    (*operations[i].action)(output);
		else
		    (*putbody)(output);
		break;
	    }
    }
}


int
themeinit()
{
    document = fetch("_DOCUMENT");
    username = fetch("_USER");
}


int
process(char *template, void (*putbody)(FILE*), int justdump, FILE *output )
{
    int level = 0;
    int i;
    register c;
    FILE *input;

    themeinit();

    if ( input = fopen(template, "r") ) {
	while ( (c = fgetc(input)) != EOF ) {
	    if (c == marker[level]) {
		if (marker[++level] == 0) {
		    action(input, putbody, output);
		    level = 0;
		}
	    }
	    else {
		for (i=0; i < level; i++)
		    fputc(marker[i], output);
		level = 0;
		fputc(c, output);
	    }
	}
	fclose(input);
	return 1;
    }
    else if (justdump) {
	(*putbody)(output);
	return 1;
    }
    return 0;
}
