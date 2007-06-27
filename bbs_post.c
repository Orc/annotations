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

#include "indexer.h"
#include "formatting.h"

static struct article *art;
int help;
char *p, *q;
int rows;
int complain = 0;
struct passwd *user;
int   didpara  = 0;
int   printenv = 0;
char *author   = 0;
char *category = 0;
char *bbspath  = "";
char *bbsroot  = "";
char *username = 0;
char *script   = "dunno";
char *filename = 0;
int   editing  = 0;
int   preview  = 0;
int   comments_ok = 0;


extern char *xgetenv(char*);


static int
boolenv(char *s)
{
    return getenv(s) != 0;
}


static void
showvalue(char *text, FILE *f)
{
    if (text) {
	fprintf(f, " VALUE=\"");
	for ( ;*text; ++text) {
	    switch (*text) {
	    case '&':
		fprintf(f, "&amp;");
		break;
	    case '"':
		fprintf(f, "&quot;");
		break;
	    default:
		fputc(*text, f);
		break;
	    }
	}
	fputc('"', f);
    }
    fputs(">\n", f);
}


void
bbs_error(int code, char *why)
{
    int err = errno;

    if (code/100 == 5)
	syslog(LOG_ERR, "%m");
    else
	syslog(LOG_ERR, "%d: %s", code, why);

    printf("Content-Type: text/html; charset=iso-8859-1\r\n"
	   "Connection: close\r\n"
	   "Server: %s\r\n"
	   "Cache-Control: no-cache\r\n"
	   "\r\n", script);

    puts("<HTML>\n"
	 "<HEAD><TITLE>Aaaaiieee!</TITLE></HEAD>\n"
	 "<BODY BGCOLOR=black>\n"
         "<CENTER><FONT COLOR=RED>OH, NO!<BR>");
    printf("ERROR CODE %s<BR>\n", code);
    if (code/100 == 5)
	puts(strerror(err));
    else
	puts(why);
    puts("</FONT></CENTER>\n"
         "</BODY></HTML>");
    exit(1);
}


char *text, *title;


static void
putbody(FILE *f)
{
    char *t;

    fputs("<DIV CLASS=\"postwindow\">\n", f);
    if ( (editing||preview) && (art->size > 1)) {
	fputs("<DIV CLASS=\"previewbox\">\n", f);
	article(f, art, editing ? (FM_COOKED|FM_NOFF) : (FM_PREVIEW|FM_IMAGES));
	fputs("</DIV>\n"
	      "<HR>\n", f);
	rows=10;
    }
    else {
	rows=24;
    }

    fprintf(f, "<FORM METHOD=POST ACTION=\"%s\">\n", script);
    fputs("<DIV align=left CLASS=\"subjectbox\">Subject <INPUT TYPE=TEXT NAME=\"title\" SIZE=80 MAXLENGTH=180", f);
    showvalue(art->title, f);

    if (complain && !art->title)
	fputs("<font class=\"alert\">Please enter a subject</font>\n", f);
    fputs("<BR></DIV>\n", f);

    fprintf(f, "<DIV CLASS=\"inputbox\">\n"
	       "<TEXTAREA NAME=_text ROWS=%d COLS=80 WRAP=SOFT>\n",rows);

    if (art->body)
	for (p=art->body; *p; ++p)
	    if (*p == '&')
		fprintf(f, "&amp;");
	    else if (*p == '<')
		fprintf(f, "&lt;");
	    else if (*p == '\f')
		fprintf(f, "&lt;!more!&gt;");
	    else
		fputc(*p, f);
    
    fputs("</TEXTAREA></FONT>\n", f);
    if (complain && !art->body)
	fputs("<BR><font class=\"alert\">Please enter a message</font>\n", f);
    fputs("</DIV>\n", f);

    fputs("<DIV CLASS=\"tags\">Category <INPUT TYPE=TEXT NAME=\"category\" SIZE=80 MAXLENGTH=180", f);
    showvalue(art->category, f);

    fputs("<DIV ALIGN=LEFT CLASS=\"checkbox\">\n"
	  "Allow&nbsp;comments"
	  "<INPUT TYPE=\"checkbox\" NAME=\"commentsok\"",f);
    if (comments_ok)
	fputs(" checked", f);
    fputs("></DIV>\n",f);

    fputs("<DIV ALIGN=LEFT CLASS=\"controls\">\n", f);
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
    char *filetoedit = 0;
    register c;
    struct article scratch;
    char *p;

    openlog("bbs_post", LOG_PID, LOG_NEWS);

    if ( (argc > 1) && (strcmp(argv[1], "-e")) == 0) {
	argc--;
	argv++;
	printenv=1;
    }

    initialize();

    uncgi();

    script = xgetenv("SCRIPT_NAME");

    if ( (author = xgetenv("REMOTE_USER")) == 0)
	bbs_error(401, "I don't know who you are!");

    if ( (text = xgetenv("WWW_text")) && (*text == 0) )
	text = 0;

    preview = boolenv("WWW_previewing") | boolenv("WWW_preview");
    title = xgetenv("WWW_title");
    category = xgetenv("WWW_category");

    comments_ok = 0;
    if ( editing = boolenv("WWW_edit") ) {
	char *filetoedit;

	if ( filetoedit = xgetenv("WWW_url")) {
	    if ( (art = openart(filetoedit)) == 0)
		bbs_error(404, filetoedit);
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

	art->category = category;
	art->author = author;
	art->title = title;
	art->url = xgetenv("WWW_url");
	time(&art->timeofday);
    }
    art->comments_ok = comments_ok;

    if (p = xgetenv("WWW_post")) {
	if (art->author && art->title && art->body) {
	    int res ;

	    if (editing)
		res = edit(art, bbspath);
	    else
		res = post(art, bbspath, 0);

	    if (res) {
		printf("Content-Type: text/html; charset=iso-8859-1\r\n"
		       "Connection: close\r\n"
		       "Server: %s\r\n"
		       "Cache-Control: no-cache\r\n"
		       "\r\n", script);
		puts("<html>");
		printf("<meta http-equiv=\"Refresh\" Content=1; URL=%s/post/\">\n", bbsroot);
		printf("<html><head><title>article %s</title></head>",
			editing ? "updated" : "posted");
		printf("<body><p>The article has been %s.  If your web browser"
				"does not take you to the correct page, "
				"<a href=\"%s/post\">this is where you need to go</a></p></body>"
				"</html>\n", editing ? "updated" : "posted",
				bbsroot);
		exit(0);
	    }
	}
	complain = (strcmp(p, "New Message") != 0);
    }
    else if (xgetenv("WWW_cancel")) {
	printf("Content-Type: text/html; charset=iso-8859-1\r\n"
	       "Connection: close\r\n"
	       "Server: %s\r\n"
	       "Cache-Control: no-cache\r\n"
	       "\r\n", script);
	puts("<html>");
	printf("<meta http-equiv=\"Refresh\" Content=1; URL=%s/post/\">\n", bbsroot);
	printf("<head><title>%s cancelled</title></head>",
		editing ? "edit" : "post");
	printf("<body><p>%s cancelled.  If your web browser"
			"does not take you to the correct page, "
			"<a href=\"%s/post\">click here</a></p></body>"
			"</html>\n", editing ? "Edit" : "Post", bbsroot);
	exit(0);
    }

    help = boolenv("WWW_needhelp");


    if ( (themfile = alloca(strlen(bbspath) + 20)) == 0 )
	bbs_error(503, "Out of memory!");

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
