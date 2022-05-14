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
#include <errno.h>

#include <mkdio.h>

#include "indexer.h"
#include "formatting.h"
#include "comment.h"

extern char *xgetenv();

struct passwd *user;
char *bbspath  = "";
char *bbsroot  = "";

int fillin      = 0;
int printenv    = 0;
int preview     = 0;
int publish_mail= 1;
int help        = 0;
char *text      = 0;
char *script    = 0;
char *name      = 0;
char *email     = 0;
char *website   = 0;
char *url       = 0;

static void
prefix(char *script)
{
    printf("Content-Type: text/html; charset=iso-8859-1\r\n"
	   "Connection: close\r\n"
	   "Server: %s\r\n"
	   "Cache-Control: no-cache\r\n"
	   "\r\n", script);
}


void
bbs_error(int code, char *why)
{
    int err = errno;

    syslog(LOG_ERR, "%m");

    prefix(script);
    puts("<html>\n"
	 "<meta name=\"robots\" content=\"noindex,nofollow\">"
	 "<head><title>Aaaaiieee!</title></head>\n"
	 "<body bgcolor=black>\n"
         "<center><font color=red>OH, NO!<br>\n");
    printf("ERROR CODE %d<br>\n", code);
    if (code == 503) {
	puts(strerror(err));
	puts("<br/>");
    }
    puts(why);
    puts("</font></center>\n"
         "</body></html>");
    exit(1);
}


static void
putbody(FILE *f)
{
    int rows;
    char *p;
    mkd_flag_t *flags = mkd_flags();


    mkd_set_flag_num(flags, MKD_NOHEADER);

    fputs("<div class=\"postwindow\">\n", f);

    if (fillin) {
	fprintf(f, "<p>Please enter your name, email or website,\n");
	fprintf(f, "and some text.</p>\n");
    }

    fprintf(f, "<!-- preview = %d, |text| = %d -->\n", preview,
		    text ? strlen(text) : 0);

    if ( preview && text && (strlen(text) > 1) ) {
	fputs("<div class=\"previewbox\">\n", f);
	markdown(mkd_string(text,strlen(text), flags),f,0);
	fputs("</div>\n"
	      "<hr/>\n", f);
	rows=10;
    }
    else {
	rows=24;
    }

    fprintf(f, "<form method=\"post\" action=\"%s\">\n", script);
    fprintf(f, "<input type=\"hidden\" name=\"url\" value=\"%s\"/>\n", url);
    fprintf(f, "<div align=\"left\" class=\"Name\">Name"
	       "<input type=\"text\" name=\"name\" size=40 maxlength=180"
	       " value=\"%s\">\n", name ? name : "");
    if (fillin && !name)
	fprintf(f, " <font class=alert>Please enter your name</font>\n");
    fprintf(f, "<br><div align=\"left\" class=\"Email\">Email"
	       "<input type=\"text\" name=\"email\" size=\"40\" maxlength=\"180\""
	       " value=\"%s\">\n", email ? email : "");
    if (fillin && !email)
	fprintf(f, " <font class=alert>Please enter your email address</font>\n");
    fprintf(f, "<input type=\"checkbox\" name=\"publish_mail\" %s/>&nbsp;publish your email address?\n", publish_mail  ? "CHECKED" : "");
    fprintf(f, "<div align=\"left\" class=\"URL\">Website "
	       "<input type=\"text\" name=\"website\" size=\"40\" maxlength=\"180\""
	       " value=\"%s\">\n", website ? website : "");
    /*
    if (fillin && !email)
	fprintf(f, " <font class=alert>Please enter your website</font>\n");
     */

    if (fillin && !text)
	fprintf(f, "<br><p class=\"alert\">Please enter a message</p>\n");
    fprintf(f, "<br><div class=\"inputbox\">\n"
	       "<font bgcolor=\"silver\">\n"
	       "<textarea name=\"_text\" rows=\"%d\" cols=\"80\" wrap=\"soft\">\n",rows);

    if (text)
	for (p=text; *p; ++p)
	    if (*p == '&')
		fprintf(f, "&amp;");
	    else
		fputc(*p, f);
    
    fputs("</textarea></font></div>\n", f);

    fputs("<div align=\"left\" class=\"controls\">\n", f);

    if (preview)
	fprintf(f, "<input type=\"hidden\" name=\"preview\" value=\"Preview\"/>\n");
    fprintf(f, "<input type=\"submit\" name=\"preview\" value=\"Preview\"/>\n"
	       "<input type=\"submit\" name=\"post\" value=\"Post\"/>\n"
	       "<input type=\"submit\" name=\"cancel\" value=\"Cancel\"/>\n");

    if (help)
	fprintf(f, "<input type=\"hidden\" name=\"help\" value=\"Show\"/>\n");
    fprintf(f, "<input type=\"submit\" name=\"help\" value=\"%s\"/>\n",
		help ? "Hide Help" : "Show Help");

    fputs("</div>\n"
	  "</form>\n", f);

    if (help)
	fputs("<div class=\"helpbox\" align=\"left\"><hr/>\n"
	      " *text* <b>boldfaces</b> text; \n"
	      " _text_ <i>italicizes</i> text; \n"
	      " a blank line starts a new paragraph\n"
	      "</div>", f);

    mkd_free_flags(flags);
}


int
comment(char *from, char *email,
        int publish_mail, char *website,
	char *text, struct article *art)
{
    struct comment *cmt;
    FILE *out;
    time_t now;
    struct tm tm, *today;
    int buildflags = PG_ARCHIVE;
    char *ip;
    int yr,mn,dy,artno;
    int approved = 0;

    if ( cmt = newcomment(art) )  {
	cmt->author  = from;
	cmt->email   = email;
	cmt->publish_mail  = publish_mail;
	cmt->website = website;
	cmt->text    = text;
	approved     = cmt->approved;
	if ( !savecomment(cmt) )
	    cmt = 0;
    }

    if (cmt == 0) {
	syslog(LOG_ERR, "cannot post comments to [%s]: %m", art->title);
	return 0;
    }
    if (ip = xgetenv("REMOTE_ADDR"))
	syslog(LOG_INFO, "COMMENT TO %s%s FROM %s", bbsroot, art->url, ip);
    else
	syslog(LOG_INFO, "COMMENT TO %s%s", bbsroot, art->url);

    art->comments++;

    tm = *localtime( &(art->timeofday) );
    today = localtime( &now );

    writectl(art);
    writehtml(art);

    if (sscanf(art->url, "%d/%d/%d/%d", &yr, &mn, &dy, &artno) == 4) {
	if ( (tm.tm_year == today->tm_year) && (tm.tm_mon == today->tm_mon) )
	    buildflags |= (PG_HOME|PG_POST);
	generate(&tm, bbspath, 0, buildflags);
    }

    return approved ? 2 : 1;
}


main(int argc, char **argv, char **envp)
{
    FILE *theme;
    char *themfile;
    char *bbsdir = "/";
    register c;
    char *p;
    int status;
    char *title;
    struct article *art;

    static char *item[] = { "WWW_text", "WWW_name", "WWW_email",
                            "WWW_publish_mail", "WWW_website", "WWW_preview",
			    "WWW_url", "WWW_help", 0 };

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

    publish_mail  = getenv("WWW_publish_mail") != 0;
    preview = getenv("WWW_preview") != 0;
    url     = getenv("WWW_url");
    if ( (p = getenv("WWW_help")) != 0 && strncasecmp(p, "Show", 4) == 0)
	help = 1;

    if ( (url == 0) || ((art = openart(url)) == 0) )
	bbs_error(500, "Nothing to comment to?");
    else if ( art->comments_ok == 0)
	bbs_error(400, "Can't comment here");

    if ( (title = alloca(strlen(art->title) + 100)) == 0)
	bbs_error(503, "Out of memory");

    sprintf(title, "Commenting on %s", art->title);
    stash("title", title);

    if (getenv("WWW_post")) {
	if (text && name && (email||website) ) {

	    switch ( comment(name,email,publish_mail,website,text,art) ) {
	    case 2:
		prefix(script);
		puts("<html>");
		printf("<meta http-equiv=\"Refresh\" content=\"0; url=%s%s\">\n", bbsroot, url);
		puts("</html>");
		exit(0);
	    case 1:
		prefix(script);
		printf("<html>\n"
		       "<head>\n"
		       "<title>your comment is being held by "
		              "the moderator</title>\n"
		       "<meta http-equiv=refresh content=\"5; url=%s%s\">\n"
		       "</head>\n", bbsroot, url);
		printf("<body>\n"
		       "<p>Your comment is being held for moderation, "
			  "and will not show up until it is "
			  "approved</p>\n"
		       "<p>In a second or so, you should be redirected to "
			  "<a href=\"%s/%s\">the article</a> you commented "
			  "on.   If you are not, you can "
			  "<a href=\"%s/%s\">go there now</a></p>\n"
			"</body>\n", bbsroot, url, bbsroot, url);
		exit(0);
	    default:
		bbs_error(503, "Can't post this comment due to some "
			       "mysterious system error?");
	    }
	}
	else
	    fillin = 1;
	/* complain about missing items */
    }
    else if (getenv("WWW_cancel")) {
	prefix(script);
	puts("<html>");
	printf("<meta http-equiv=\"Refresh\" content=\"0; url=%s%s\">\n", bbsroot,url);
	puts("</html>");
	exit(0);
    }

    if ( (themfile = alloca(strlen(bbspath) + 20)) == 0 )
	bbs_error(503, "Out of memory!");

    prefix(script);
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
