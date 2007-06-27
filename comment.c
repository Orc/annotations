#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <syslog.h>

#include "indexer.h"
#include "formatting.h"

void
bbs_error(int code, char *why)
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


struct passwd *user;
char *bbspath  = "";
char *bbsroot  = "";

int fillin      = 0;
int printenv    = 0;
int preview     = 0;
int help        = 0;
char *text      = 0;
char *script    = 0;
char *name      = 0;
char *email     = 0;
char *website   = 0;
char *url       = 0;


static void
putbody(FILE *f)
{
    int rows;
    char *p;

    fputs("<DIV CLASS=\"postwindow\">\n", f);

    if (fillin) {
	fprintf(f, "<p>Please enter your name, email or website,\n");
	fprintf(f, "and some text.</p>\n");
    }

    fprintf(f, "<!-- preview = %d, |text| = %d -->\n", preview,
		    text ? strlen(text) : 0);

    if ( preview && text && (strlen(text) > 1) ) {
	fputs("<DIV CLASS=\"previewbox\">\n", f);
	format(f, text, FM_BLOCKED);
	fputs("</DIV>\n"
	      "<HR>\n", f);
	rows=10;
    }
    else {
	rows=24;
    }

    fprintf(f, "<FORM METHOD=POST ACTION=\"%s\">\n", script);
    fprintf(f, "<INPUT TYPE=HIDDEN NAME=url VALUE=\"%s\">\n", url);
    fprintf(f, "<DIV align=left CLASS=\"Name\">Name"
	       "<INPUT TYPE=TEXT NAME=\"name\" SIZE=40 MAXLENGTH=180"
	       " VALUE=\"%s\">\n", name ? name : "");
    if (fillin && !name)
	fprintf(f, " <font CLASS=alert>Please enter your name</font>\n");
    fprintf(f, "<br><DIV align=left CLASS=\"Email\">Email"
	       "<INPUT TYPE=TEXT NAME=\"email\" SIZE=40 MAXLENGTH=180"
	       " VALUE=\"%s\">\n", email ? email : "");
    if (fillin && !email)
	fprintf(f, " <font CLASS=alert>Please enter your email address</font>\n");
    fprintf(f, "<br><DIV align=left CLASS=\"URL\">Website "
	       "<INPUT TYPE=TEXT NAME=\"website\" SIZE=40 MAXLENGTH=180"
	       " VALUE=\"%s\">\n", website ? website : "");
    if (fillin && !email)
	fprintf(f, " <font CLASS=alert>Please enter your website</font>\n");

    if (fillin && !text)
	fprintf(f, "<br><p CLASS=alert>Please enter a message</p>\n");
    fprintf(f, "<br><DIV CLASS=\"inputbox\">\n"
	       "<FONT BGCOLOR=silver>\n"
	       "<TEXTAREA NAME=_text ROWS=%d COLS=80 WRAP=SOFT>\n",rows);

    if (text)
	for (p=text; *p; ++p)
	    if (*p == '&')
		fprintf(f, "&amp;");
	    else
		fputc(*p, f);
    
    fputs("</TEXTAREA></FONT></DIV>\n", f);

    fputs("<DIV ALIGN=LEFT CLASS=\"controls\">\n", f);

    if (preview)
	fprintf(f, "<INPUT TYPE=HIDDEN NAME=preview VALUE=Preview>\n");
    fprintf(f, "<INPUT TYPE=SUBMIT NAME=preview VALUE=Preview>\n"
	       "<INPUT TYPE=SUBMIT NAME=post VALUE=Post>\n"
	       "<INPUT TYPE=SUBMIT NAME=cancel VALUE=Cancel>\n");

    if (help)
	fprintf(f, "<INPUT TYPE=HIDDEN NAME=help VALUE=\"Show\">\n");
    fprintf(f, "<INPUT TYPE=SUBMIT NAME=help VALUE=\"%s\">\n",
		help ? "Hide Help" : "Show Help");

    fputs("</DIV>\n"
	  "</FORM>\n", f);

    if (help)
	fputs("<DIV CLASS=\"helpbox\" ALIGN=LEFT><HR>\n"
	      " *text* <b>boldfaces</b> text; \n"
	      " _text_ <i>italicizes</i> text; \n"
	      " a blank line starts a new paragraph\n"
	      "</DIV>", f);
}


int
comment(char *from, char *email, char *website, char *text, char *article)
{
    struct article *art;
    int len = strlen(article);
    char *base = strrchr(article, '/');
    FILE *out;
    time_t now;
    struct tm *tm, *today;
    int buildflags = PG_ARCHIVE;

    if ( (art = openart(article)) == 0) {
	syslog(LOG_ERR, "openart(%s): %m", article);
	return 0;
    }
    else if (art->comments_ok == 0) {
	syslog(LOG_INFO, "comment_ok is FALSE");
	freeart(art);
	return 0;
    }

    if ( (out = fopen(art->cmtfile, "a")) == 0) {
	syslog(LOG_ERR, "cannot open [%s]: %m", art->cmtfile);
	return 0;
    }
    flock(fileno(out), LOCK_EX);

    fprintf(out, "<DIV CLASS=\"comment\">\n");
    if (art->comments > 0)
	fputs(fmt.commentsep, out);
    format(out, text, FM_BLOCKED);
    fprintf(out, "</DIV>\n");
    fprintf(out, "<DIV CLASS=\"commentsig\">\n");

    if (email || website) {
	if (website) {
	    if (strncasecmp(website, "http://", 7) == 0)
		fprintf(out, "<a href=\"%s\">%s</a>\n", website, from);
	    else
		fprintf(out, "<a href=\"http://%s\">%s</a>\n", website, from);
	}
	else
	    fprintf(out, "<a href=\"mailto:%s\">%s</a>\n", email, from);
    }
    else
	fprintf(out, "%s ", from);

    time(&now);
    fputs(ctime(&now), out);
    fprintf(out, "</DIV>\n");
    fflush(out);

    art->comments++;

    tm = localtime( &(art->timeofday) );
    today = localtime( &now );


    writectl(art);
    writehtml(art);

    syslog(LOG_INFO, "COMMENT TO %s", art->url);

    generate(tm,bbspath,0,buildflags);
    if ( (tm->tm_year == today->tm_year) && (tm->tm_mon == today->tm_mon) ) {
	/*buildflags |= (PG_HOME|PG_POST);*/
	generate(tm, ".", 0, PG_ALL);
    }

    flock(fileno(out), LOCK_UN);
    fclose(out);

    return 1;
}


static char *
xgetenv(char *e)
{
    char *p = getenv(e);


    if (p && strlen(p)) {
	while (isspace(*p) && *p)
	    ++p;
	return (*p) ? p : 0;
    }
    return 0;
}

main(int argc, char **argv, char **envp)
{
    FILE *theme;
    char *themfile;
    char *bbsdir = "/";
    register c;
    char *p;

    static char *item[] = { "WWW_text", "WWW_name", "WWW_email", "WWW_website",
                            "WWW_preview", "WWW_url", "WWW_help", 0 };

    openlog("comment", LOG_PID, LOG_NEWS);

    fillin = 0;
    initialize();

    if ( (script = getenv("SCRIPT_NAME")) == 0 )
	script = "/bin/false";

    uncgi();

    text    = xgetenv("WWW_text");
    name    = xgetenv("WWW_name");
    email   = xgetenv("WWW_email");
    website = xgetenv("WWW_website");

    preview = getenv("WWW_preview") != 0;
    url     = getenv("WWW_url");
    if ( (p = getenv("WWW_help")) != 0 && strncasecmp(p, "Show", 4) == 0)
	help = 1;

    if (url == 0)
	bbs_error(500, "Nothing to comment to?");

    if (getenv("WWW_post")) {
	if (text && name && (email||website) ) {

	    if ( comment(name,email,website,text,url) ) {
		printf("HTTP/1.0 303 Ok\r\n"
		       "Location: %s%s\n"
		       "\n", bbsroot, url);
		exit(0);
	    }
	    syslog(LOG_ERR, "Oops#1");
	}
	else
	    fillin = 1;
	/* complain about missing items */
    }
    else if (getenv("WWW_cancel")) {
	printf("HTTP/1.0 303 Ok\n"
	       "Location: %s\n"
	       "\n", bbsroot);
	exit(0);
    }


    if ( (themfile = alloca(strlen(bbspath) + 20)) == 0 )
	bbs_error(503, "Out of memory!");

    printf("Content-Type: text/html; charset=iso-8859-1\r\n"
	   "Connection: close\r\n"
	   "Server: %s\r\n"
	   "Cache-Control: no-cache\r\n"
	   "\r\n", script);

    stash("_DOCUMENT", script);

    sprintf(themfile, "%s/post.theme", bbspath);
    process(themfile, putbody, 1, stdout);

    if (printenv) {
	int i;
	puts("<!-- WWW_vars:");
	for (i=0; item[i]; i++)
	    printf("%s=\"%s\"\n", item[i], getenv(item[i]));
	puts("  -->");
	puts("<!-- ENV:");
	for (i=0; envp[i]; i++)
	    puts(envp[i]);
	puts("  -->");
    }

    exit(0);
}
