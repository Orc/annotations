#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>

#include <scew/scew.h>

#include "indexer.h"
#include "formatting.h"

void
bbs_error(int code, char *why)
{
    int err = errno;

    if (code == 503)
	syslog(LOG_ERR, "%s %m", why);
    else
	syslog(LOG_ERR, "%s", why);

    printf("HTTP/1.0 %03d Oops\r\n"
	   "Status: %03d/%s\r\n"
           "Content-Type: text/html\r\n"
	   "\r\n", code, code, why);
    exit(1);
}


char *text, *title;
static struct article *art;
int help;
char *p, *q;
int rows;
int complain = 0;
struct passwd *user;
int   didpara  = 0;
int   printenv = 0;
char *bbspath  = "";
char *bbsroot  = "";
char *username = 0;
char *script   = 0;
char *filename = 0;
int   editing  = 0;
int   preview  = 0;
int   comments_ok = 0;


/*
 * pick apart the handy scew/expat data structure; all
 * we care about are the title, content, and comment
 * entries.  There are some wonderful potentials for
 * namespace collisions, because we don't care about
 * just where the items show up, and given the extreme
 * mutability of all-things-web, it's more than possible
 * that someone will want to use <comment></comment> for
 * something completely different.
 */
static int
dexml(scew_element *p)
{
    char *item;
    scew_element *idx;
    int rc;

    if (p == 0) return 0;

    item = scew_element_name(p);

    if (strcasecmp(item, "title") == 0)
	title = scew_element_contents(p);
    else if (strcasecmp(item, "content") == 0)
	text = scew_element_contents(p);
    else if (strcasecmp(item, "comments") == 0) {
	char *val = scew_element_contents(p);
	if ( (strcasecmp(val, "on") == 0) || (strcasecmp(val, "yes") == 0) )
	    comments_ok = 1;
    }

    for (idx=0; idx = scew_element_next(p, idx); ) {
	rc = dexml(idx);
	if (rc != 0) return rc;
    }
    return 0;
} /* dexml */


main(int argc, char **argv)
{
    FILE *theme;
    char *themfile;
    char *author = 0;
    char *filetoedit = 0;
    register c;
    struct article art;
    char *p;
    int res, size;
    int gotten, get;
    char *buffer;
    scew_tree* tree = NULL;
    scew_parser* parser = NULL;

    openlog("xmlpost", LOG_PID, LOG_NEWS);

    initialize();
    comments_ok = 0;

    if ( (p = getenv("CONTENT_LENGTH")) == 0)
	bbs_error(400, "bad call to xmlpost: CONTENT_LENGTH not set");

    size = atoi(p);
    if ( (buffer = alloca(size)) == 0)
	bbs_error(503, "Out of memory!");

    for (gotten=0; gotten < size; ) {
	get = read(0, buffer+gotten, size-gotten);
	if (get <= 0) 
	    break;
	gotten += get;
    }
    if (gotten < size)
	bbs_error(400, "truncated xml request");

    /* start here */

    if ( (author = xgetenv("REMOTE_USER")) == 0)
	bbs_error(400, "I don't know who you are!");

    parser = scew_parser_create();
    if (!scew_parser_load_buffer(parser, buffer, size)) {
	char oops[500];
	int error = scew_error_code();

	sprintf(oops, "Unable to read xml document (error #%d: %s)<br>",
			error, scew_error_code(error));

	if (error == scew_error_expat) {
            enum XML_Error xml_error = scew_error_expat_code(parser);
	    sprintf(oops+strlen(oops), "Expat error #%d (line %d): %s",
				xml_error, scew_error_expat_line(parser),
				scew_error_expat_string(xml_error));
	}
	bbs_error(400, oops);
    }

    if (dexml(scew_tree_root(scew_parser_tree(parser))) != 0)
	bbs_error(400, "malformed xml document");


    memset(&art, 0, sizeof art);

    art.body = text;
    art.size = text ? strlen(text) : 0;

    art.author = author;
    art.title = title;
    time(&art.timeofday);
    art.comments_ok = comments_ok;

    if (res = post(&art, bbspath, 1)) {
	printf("HTTP/1.0 201 Ok\r\n"
	       "Status: 201/Ok\r\n"
	       "Location: %s/%s\r\n\r\n", fmt.url, art.url);
	exit(0);
    }
    bbs_error(500, "can't save article");
}
