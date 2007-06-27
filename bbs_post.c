#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>

#include "indexer.h"
#include "formatting.h"

void
html_error(int code, char *why)
{
    int err = errno;

    syslog(LOG_ERR, "%m");

    printf("HTTP/1.0 %03d %s\r\n", code, why);
    puts("content-type: text/html\r\n"
           "\r\n"
         "<HTML>\n"
	 "<HEAD><TITLE>Aaaaiieee!</TITLE></HEAD>\n"
	 "<BODY BGCOLOR=black>\n"
         "<CENTER><FONT COLOR=RED>OH, NO!<BR>");
    if (code == 503)
	puts(strerror(err));
    else
	puts(why);
    puts("</FONT></CENTER>\n"
         "</BODY></HTML>");
    exit(1);
}


char *text, *title;
static struct article *art;
int help;
char *p, *q;
int rows;
struct passwd *user;
int   didpara  = 0;
int   printenv = 0;
char *author   = 0;
char *bbspath  = "";
char *bbsroot  = "";
char *username = 0;
char *script   = 0;
char *filename = 0;
int   editing  = 0;
int   preview  = 0;
int   comments_ok = 0;


static int
boolenv(char *s)
{
    return getenv(s) != 0;
}


static void
putbody(FILE *f)
{
    fputs("<DIV ID=\"postwindow\">\n", f);
    if ( (editing||preview) && (art->size > 1)) {
	fputs("<DIV ID=\"previewbox\">\n", f);
	article(f, art, editing ? FM_COOKED : (FM_PREVIEW|FM_IMAGES));
	fputs("</DIV>\n"
	      "<HR>\n", f);
	rows=10;
    }
    else {
	rows=24;
    }

    fprintf(f, "<FORM METHOD=POST ACTION=\"%s\">\n", script);
    fputs("<DIV align=left ID=\"subjectbox\">Subject <INPUT TYPE=TEXT NAME=\"title\" SIZE=80 MAXLENGTH=180", f);
    if (art->title) fprintf(f, " VALUE=\"%s\"", art->title);
    fputs("><BR></DIV>\n", f);

    fprintf(f, "<DIV ID=\"inputbox\">\n"
	       "<TEXTAREA NAME=_text ROWS=%d COLS=80 WRAP=SOFT>\n",rows);

    if (art->body)
	for (p=art->body; *p; ++p)
	    if (*p == '&')
		fprintf(f, "&amp;");
	    else
		fputc(*p, f);
    
    fputs("</TEXTAREA></FONT></DIV>\n", f);

    fputs("<DIV ALIGN=LEFT ID=\"checkbox\">\n"
	  "Allow&nbsp;comments"
	  "<INPUT TYPE=\"checkbox\" NAME=\"commentsok\"",f);
    if (comments_ok)
	fputs(" checked", f);
    fputs("></DIV>\n",f);

    fputs("<DIV ALIGN=LEFT ID=\"controls\">\n", f);
    fprintf(f, "<INPUT TYPE=HIDDEN NAME=comments value=%d>\n", art->comments);

    if (preview || editing)
	fprintf(f,"<INPUT TYPE=HIDDEN NAME=previewing VALUE=T>\n");

    if (editing) {
	fprintf(f, "<INPUT TYPE=HIDDEN NAME=edit VALUE=edit>\n");
	fprintf(f, "<INPUT TYPE=HIDDEN NAME=url VALUE=\"%s\"\n", art->url);
    }
    fprintf(f, "<INPUT TYPE=HIDDEN NAME=more VALUE=more>\n");

    fprintf(f, "<INPUT TYPE=SUBMIT NAME=preview VALUE=Preview>\n"
	       "<INPUT TYPE=SUBMIT NAME=post VALUE=%s>\n"
	       "<INPUT TYPE=SUBMIT NAME=cancel VALUE=Cancel>\n",
	       editing ? "Save" : "Post");
    fputs("</DIV>", f);

    if (editing)
	help = 0;
    else
	fprintf(f, "<INPUT TYPE=SUBMIT NAME=%s VALUE=\"%s\">\n",
			    help ? "nohelp" : "needhelp",
			    help ? "Hide Help" : "Show Help");

    fputs("</FORM>\n", f);

    if (help)
	fputs("<DIV ALIGN=LEFT><HR>\n"
	      " *text* <b>boldfaces</b> text; \n"
	      " _text_ <i>italicizes</i> text; \n"
	      " blank line starts a new paragraph; \n"
	      " {pic:url} displays the picture at url<br>\n"
	      " {url}{title} makes <b>title</b> a"
	      " link to <b>url</b><br>\n"
	      "</DIV>", f);
}


main(int argc, char **argv)
{
    FILE *theme;
    char *themfile;
    char *bbsdir = "/";
    char *filetoedit = 0;
    register c;
    struct article scratch;

    openlog("bbs_post", LOG_PID, LOG_NEWS);
    opterr = 0;
    while ( (c = getopt(argc, argv, "d:eu:A:S:")) != EOF) {
	switch (c) {
	case 'S':
	    script = optarg;
	    break;
	case 'A':
	    author = optarg;
	    break;
	case 'd':
	    bbsdir = optarg;
	    break;
	case 'u':
	    username = optarg;
	    break; 
	case 'e':
	    printenv=1;
	    break;
	}
    }

    bbsroot = malloc(strlen(bbsdir)+2);
    strcpy(bbsroot, bbsdir);
    if (bbsroot[0] && bbsroot[strlen(bbsroot)-1] != '/')
	strcat(bbsroot, "/");

    stash("weblog",bbsroot);
    stash("_ROOT", bbsroot);

    if ( (bbsdir[0] == '/') && (bbsdir[1] == '~') ) {
	char *r = strchr(bbsdir+2, '/');

	username = bbsdir+2;

	if (r) {
	    *r++ = 0;
	    bbsdir = r;
	}
	else
	    bbsdir = "";
    }
    if ( username) {
	if ( (user = getpwnam(username)) == 0 )
	    html_error(500, "User does not exist");

	if (user->pw_uid == 0 || user->pw_gid == 0)
	    html_error(500, "User cannot be root");

	bbspath = malloc(strlen(user->pw_dir) +
			 strlen(PATH_USERDIR) +
			 strlen(bbsdir) + 4);

	if (bbspath == 0)
	    html_error(503, "Out of memory!");

	if (setgid(user->pw_gid) || setuid(user->pw_uid))
	    html_error(503, "Privilege confusion");

	sprintf(bbspath, "%s/%s/%s", user->pw_dir, PATH_USERDIR, bbsdir);
    }
    else {
	bbspath = malloc(strlen(PATH_WWWDIR) + strlen(bbsdir) + 3);
	if (bbspath == 0)
	    html_error(503, "Out of memory!");

	sprintf(bbspath, "%s/%s", PATH_WWWDIR, bbsdir);
    }

    if (chdir(bbspath) != 0)
	html_error(503, bbspath);

    readconfig(bbspath);
    uncgi();

    if (!script)
	script = getenv("SCRIPT_NAME");

    if ( author == 0 && (author = getenv("REMOTE_USER")) == 0)
	html_error(500, "I don't know who you are!");

    if ( (text = getenv("WWW_text")) && (*text == 0) )
	text = 0;

    preview = boolenv("WWW_previewing") | boolenv("WWW_preview");
    title = getenv("WWW_title");

    comments_ok = 0;
    if ( editing = boolenv("WWW_edit") ) {
	char *filetoedit;

	if ( filetoedit = getenv("WWW_url")) {
	    if ( (art = openart(filetoedit)) == 0)
		html_error(404, filetoedit);
	    else if (text) {
		freeartbody(art);
		art->body = text;
		art->size = strlen(text);
	    }
	    comments_ok = art->comments_ok;
	}
	else
	    editing = 0;
    }

    if (boolenv("WWW_more"))
	comments_ok = boolenv("WWW_commentsok");

    if (!art) {
	char *p;
	memset(&scratch, 0, sizeof scratch);
	art = &scratch;

	art->body = text;
	art->size = text ? strlen(text) : 0;

	art->author = author;
	art->title = title;
	art->url = getenv("WWW_url");
	time(&art->timeofday);
    }
    art->comments_ok = comments_ok;

    if (boolenv("WWW_post")) {
	if (art->author && art->title && art->body) {
	    int res ;

	    if (editing)
		res = edit(art, bbspath);
	    else
		res = post(art, bbspath);

	    syslog(LOG_INFO, "res (%s) is %d", editing?"edit":"post", res);

	    if (res) {
		printf("HTTP/1.0 307 Ok\r\n"
		       "Location: %s/post\n"
		       "\n", bbsroot);
		exit(0);
	    }
	}
	/* complain about missing items */
    }
    else if (getenv("WWW_cancel")) {
	printf("HTTP/1.0 307 Ok\n"
	       "Location: %s/post\n"
	       "\n", bbsroot);
	exit(0);
    }

    help = boolenv("WWW_needhelp");


    if ( (themfile = alloca(strlen(bbspath) + 20)) == 0 )
	html_error(503, "Out of memory!");


    printf("Content-Type: text/html; charset=iso-8859-1\r\n"
	   "Connection: close\r\n"
	   "Server: %s\r\n"
	   "Cache-Control: no-cache\r\n"
	   "\r\n", script);

    stash("_DOCUMENT", script);
    stash("_USER", author);
    stash("title", editing ? "Edit Article" : "New Article");

    sprintf(themfile, "%s/post.theme", bbspath);
    process(themfile, putbody, 1, stdout);

    if (printenv) {
	printf("<!-- ENV:\n");
	fflush(stdout);
	system("printenv");
	printf("  -->\n");
    }

    exit(0);
}
