#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
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
	fprintf(f, " value=\"");
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
    fputs("/>\n", f);
}


void
bbs_error(int code, char *why)
{
    int err = errno;

    if (code/100 == 5)
	syslog(LOG_ERR, "%m: %s", why);
    else
	syslog(LOG_ERR, "%d: %s", code, why);

    printf("Content-Type: text/html; charset=iso-8859-1\r\n"
	   "Connection: close\r\n"
	   "Server: %s\r\n"
	   "Cache-Control: no-cache\r\n"
	   "\r\n", script);

    puts("<html>\n"
	 "<head><title>Aaaaiieee!</title></head>\n"
	 "<body bgcolor=black>\n"
         "<center><font color=red>OH, NO!<br>");
    printf("error code %d<br>\n", code);
    if (code/100 == 5)
	puts(strerror(err));
    else
	puts(why);
    puts("</font></center>\n"
         "</body></html>");
    exit(1);
}


char *text, *title;


static void
putbody(FILE *f)
{
    char *t;

    fputs("<div class=\"postwindow\">\n", f);
    if ( (editing||preview) && (art->size > 1)) {
	fputs("<div class=\"previewbox\">\n", f);
	article(f, art);
	fputs("</div>\n"
	      "<hr/>\n", f);
	rows=10;
    }
    else {
	rows=24;
    }

    fprintf(f, "<form method=post action=\"%s\">\n", script);
    fputs("<div align=left class=\"subjectbox\"/>Subject <input type=text name=\"title\" size=80 maxlength=180", f);
    showvalue(art->title, f);

    if (complain && !art->title)
	fputs("<font class=\"alert\">Please enter a subject</font>\n", f);
    fputs("<br></div>\n", f);

    fprintf(f, "<div class=\"inputbox\">\n"
	       "<textarea name=\"_text\" rows=\"%D\" cols=\"80\" wrap=\"soft\">\n",rows);

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
    
    fputs("</textarea></font>\n", f);
    if (complain && !art->body)
	fputs("<br><font class=\"alert\">Please enter a message</font>\n", f);
    fputs("</div>\n", f);

    fputs("<div class=\"tags\"/>Category <input type=text name=\"category\" size=80 maxlength=180", f);
    showvalue(art->category, f);

    fputs("<div align=\"left\" class=\"checkbox\">\n"
	  "Allow&nbsp;comments"
	  "<input type=\"checkbox\" name=\"commentsok\"",f);
    if (comments_ok)
	fputs(" checked", f);
    fputs("/></div>\n",f);

    fputs("<div align=\"left\" class=\"controls\">\n", f);
    fprintf(f, "<input type=\"hidden\" name=\"comments\" value=\"%d\" />\n", art->comments);

    if (preview || editing)
	fprintf(f,"<input type=\"hidden\" name=\"previewing\" value=\"T\" />\n");

    if (editing) {
	fprintf(f, "<input type=\"hidden\" name=\"edit\" value=\"edit\" />\n");
	fprintf(f, "<input type=\"hidden\" name=\"url\" value=\"%s\" />\n", art->url);
    }
    fprintf(f, "<input type=\"hidden\" name=\"more\" value=\"more\"/>\n");

    fprintf(f, "<input type=\"submit\" name=\"preview\" value=\"Preview\"/>\n"
	       "<input type=\"submit\" name=\"post\" value=\"%s\"/>\n"
	       "<input type=\"submit\" name=\"cancel\" value=\"Cancel\"/>\n",
	       editing ? "Save" : "Post");
    fputs("</div>", f);

    if (editing)
	help = 0;
    else
	fprintf(f, "<input type=\"submit\" name=\"%s\" value=\"%s\"/>\n",
			    help ? "nohelp" : "needhelp",
			    help ? "Hide Help" : "Show Help");

    fputs("</form>\n", f);

    if (help)
	fputs("<div align=left><hr/>\n"
	      "<ul>"
	      "<li>paragraphs are separated by blank lines;</li>\n"
	      "<li>to blockquote, start the line with &gt;</li>"
	      "<li>*text* <i>italicizes</i> text;</li>"
	      "<li>**text** <b>boldfaces</b> text;</li>"
	      "<li>`text` becomes &lt;code>text&lt;/code></li>"
	      "<li>[title][url] makes &lt;a href=url>title&gt;/a></li>"
	      "<li>![title][img] makes &lt;img src=img alt=title></li>"
	      "</ul>"
	      "</div>", f);
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

    syslog(LOG_ERR, "ZERO");

    if ( (argc > 1) && (strcmp(argv[1], "-e")) == 0) {
	argc--;
	argv++;
	printenv=1;
    }

    initialize();

    uncgi();

    syslog(LOG_ERR, "ONE");
    script = xgetenv("SCRIPT_NAME");

    if ( (author = xgetenv("REMOTE_USER")) == 0)
	bbs_error(401, "I don't know who you are!");

    if ( (text = xgetenv("WWW_text")) && (*text == 0) )
	text = 0;

    syslog(LOG_ERR, "TWO");
    preview = boolenv("WWW_previewing") | boolenv("WWW_preview");
    title = xgetenv("WWW_title");
    category = xgetenv("WWW_category");

    comments_ok = 0;
    if ( editing = boolenv("WWW_edit") ) {

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
	art->format = MARKDOWN;
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
		       "\r\n", script ? script :"core.dump");
		puts("<html>");
		printf("<meta http-equiv=\"Refresh\" content=\"0; url=%spost\">\n", bbsroot ? bbsroot : "core.dump");
		printf("<html><head><title>article %s</title></head>",
			editing ? "updated" : "posted");
		printf("<body><p>The article has been %s.  If your web browser "
				"does not take you to the correct page, "
				"<a href=\"%s/post\">this is where you need to go</a></p></body>"
				"</html>\n", editing ? "updated" : "posted",
				bbsroot);
		fflush(stdout);
		if ( fork() )
		    exit(0);
		if ( fork() )
		    exit(0);
		chdir(bbspath);
		execl("post/after.post", "after.post", editing ? "EDIT" : "POST", art->url, 0);
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
	printf("<meta http-equiv=\"Refresh\" content=\"0; url=%spost\">\n", bbsroot);
	printf("<head><title>%s cancelled</title></head>",
		editing ? "edit" : "post");
	printf("<body><p>%s cancelled.  If your web browser\n"
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
