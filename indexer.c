#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <dirent.h>
#include <sys/file.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <mkdio.h>

#include "formatting.h"
#include "indexer.h"
#include "comment.h"
#include "mapfile.h"

extern char *fetch(char*);
extern char *eoln(char*,char*);
extern FILE *rewrite(char*);

/*
 * rebuild $bbsroot/${HomePage}		{ all articles this month }
 *         $bbsroot/YYYY/MM/index.html	{ ditto }
 */
static char art[40];	/* YYYY/MM/DD/###/message.{txt|inf} */
static char dydir[40];	/* YYYY/MM/DD/### */
static char mo[40];	/* YYYY/MM/index.html */

int
dirent_is_good(struct dirent *e)
{
    char *p;

    for (p = e->d_name; *p; ++p)
	if (!isdigit(*p))
	    return 0;

    return 1;
}

int
dirent_nsort(struct dirent **a, struct dirent **b)
{
    return atoi((*a)->d_name) - atoi((*b)->d_name);
}


char *
restofline(char *p, char *q)
{
    char *a;
    int size = (q-p);

    a = malloc(size+2);
    memcpy(a, p, size);
    a[size] = 0;
    return a;
}

static FILE *iFb = 0;

void
putindex(FILE *f)
{
    register c;
    static char *text = 0;
    static long size;
    char *p, *end, *q;
    char *url;
    char *home;
    char *context;
    char *readmore = 0;
    unsigned int comments;
    unsigned int comments_ok;
    enum { HOME, POST, ARCHIVE } state = HOME;

    /* state == home; just the [comment] button
     *       == post; [comment], [edit], [post]
     *       == archive; [comment]
     */

    if (context = fetch("_CONTEXT")) {
	if (strcmp(context, "post") == 0) state = POST;
	else if (strcmp(context, "archive") == 0) state = ARCHIVE;
    }

    if (state == POST) {
	fprintf(f, "<form method=\"get\" action=\"post.cgi\">\n");
	fputs(fmt.post.start, f);
	fprintf(f, "<input type=\"submit\" name=\"post\" value=\"New Message\" />\n");
	fputs(fmt.post.end, f);
	fprintf(f, "</form>\n");
	fprintf(f, "<form method=\"get\" action=\"reindex.cgi\">\n");
	fprintf(f, "<input type=\"submit\" name=\"yes\" value=\"regenerate pages\" />\n");
	fprintf(f, "</form>\n");
	fputs(fmt.separator, f);
    }

    rewind(iFb);

    if (text = mapfd(fileno(iFb), &size)) {
	/* context format is:  rep [ text^@{settings}^@ ]
	 * where settings are ^Aurl
	 *                    ^Bwebroot
	 *                    ^C# of comments
	 *                    ^Dcomments ok
	 *                    ^Ebreak-mark for (more..)
	 */
	/*fwrite(text,size,1,f);*/
	p = text;
	end = p + size;

	for (; (p < end) && (q = memchr(p, 0, end-p)); p = 1+q) {
	    fwrite(p, (q-p), 1, f);

	    readmore = 0;
	    comments_ok = comments = 0;
	    while (*++q) {
		switch (*q) {
		case 1: url = 1+q;
			break;
		case 2: home = 1+q;
			break;
		case 3: comments=atoi(1+q);
			break;
		case 4: comments_ok = 1;
			break;
		case 5: readmore = 1+q;
			break;
		}
		while (*q) ++q;
	    }

	    if (readmore) {
		fprintf(f, "<p class=\"readmore\">"
			   "<a href=%s/%s>", home, url);
		fputs(fmt.readmore, f);
		fputs("</a></p>\n", f);
	    }

	    if ( (state == POST) && url ) {
		fprintf(f, "<form method=\"get\" action=\"post.cgi\">\n");
		fputs(fmt.edit.start, f);
		fprintf(f, "<input type=\"submit\" name=\"edit\" VALUE=\"Edit\" />\n");
		fprintf(f, "<input type=\"hidden\" name=\"url\" VALUE=\"%s\" />\n", url);
		fputs(fmt.edit.end, f);
		fprintf(f, "</form>\n");
	    }

	    if ( !(url && home) )
		continue;

	    if (comments_ok) {
		fprintf(f, "<form method=\"get\" action=\"%s", home);
		if ( comments > 0 )
		    fputs(url, f);
		else
		    fputs("comment", f);
		fprintf(f, "\">\n");
		fputs(fmt.comment.start, f);
		fprintf(f, "<input type=\"submit\" name=\"comment\" value=\"");
		if ( comments > 0 )
		    fprintf(f, "%d comment%s", comments, (comments!=1)?"s":"");
		else
		    fprintf(f, "Comment?");
		fprintf(f, "\" />\n");
		fprintf(f, "<input type=\"hidden\" name=\"url\" value=\"%s\" />\n", url);
		fputs(fmt.comment.end, f);
		fprintf(f, "</form>\n");
	    }
	    else if (comments > 0) {
		fputs(fmt.comment.start, f);
		fprintf(f, "<a href=\"%s%s\">%d comment%s</a>\n",
			    home, url, comments, (comments!=1)?"s":"");
		fputs(fmt.comment.end,f);
	    }
	}
	fwrite(p, end-p, 1, f);
	munmap(text,size);
    }
    else {
	fprintf(f, "<!-- mapfd failed: %s -->\n", strerror(errno));
	syslog(LOG_INFO, "mapfd failed: %m");
    }
}


char *
makefile(char *pathname, char *filepart)
{
    char *ret = malloc(strlen(pathname)+strlen(filepart) + 2);
    char *p;
    struct stat st;
    int len;

    if (ret) {
	strcpy(ret, pathname);
	len = strlen(ret);

	if ( (len > 0)  && (ret[len-1] == '/') )
	    ret[len-1] = 0;

	if ( (stat(ret, &st) == 0) && (st.st_mode & S_IFDIR) ) {
	    strcat(ret, "/");
	    strcat(ret, filepart);
	}
	else {
	    if (p = strrchr(ret, '/'))
		strcpy(p+1, filepart);
	    else
		strcpy(ret, filepart);
	}
    }
    return ret;
}


struct article *
openart(char *filename)
{
    struct article *ret = calloc(sizeof *ret, 1);
    char *base;
    char *p, *eol, *end;
    char field;
    char *fname;
    char *ctl;
    long size;

    if ( ret == 0 )
	return 0;

    ret->ctlfile = makefile(filename, "message.ctl");
    ret->msgfile = makefile(filename, "message.txt");
    ret->cmtfile = makefile(filename, "comment.txt");
    ret->cmtdir  = makefile(filename, "comments");

    if ( !(ret->ctlfile || ret->msgfile || ret->cmtfile || ret->cmtdir) ) {
	freeart(ret);
	return 0;
    }

    if ( (ctl = mapfile(ret->ctlfile, &size)) == 0 ) {
	freeart(ret);
	return 0;
    }

    end = ctl + size;

    for (p = ctl; p < end; p = 1+eol) {
	eol = eoln(p, end);

	if ( p[1] == ':' ) {
	    field = p[0];
	    p += 2;
	    switch (field) {
	    case 'A':	/* The (A)uthor */
		    ret->author = restofline(p,eol);
		    break;
	    case 'W':	/* W)hen was it posted */
		    ret->timeofday = atol(p);
		    break;
	    case 'M':	/* when it was (M)odified */
		    ret->modified = atol(p);
		    break;
	    case 'T':	/* T)itle of the article */
		    ret->title = restofline(p,eol);
		    break;
	    case 'U':	/* end of (U)rl */
		    ret->url = restofline(p,eol);
		    break;
	    case 'C':	/* comments ok? */
		    ret->comments_ok = atoi(p);
		    break;
	    case 'F':	/* # of (F)ollowups */
		    ret->comments = atoi(p);
		    break;
	    case 'Z':	/* article format */
		    if ( toupper(p[0]) == 'M' )
			ret->format = MARKDOWN;
		    break;
	    case 'P':	/* P)review (for rss syndication) */
		    ret->preview = restofline(p, eol);
		    break;
	    case 'B':	/* B)ig Brother is watching you */
		    ret->moderated = atoi(p);
		    break;
	    case 'X':	/* X)ref header (categories) */
		    ret->category = restofline(p, eol);
		    break;
	    default:
		    break;
	    }
	}
    }
    munmap(ctl, size);

    if (ret->modified == 0) ret->modified = ret->timeofday;

    if (ret->body = mapfile(ret->msgfile, &ret->size))
	return ret;

    freeart(ret);
    return 0;
}


int
newart(struct article *art)
{
    char path[40];
    char *end;
    int seq;
    struct tm *tm;

    if (art == 0)
	return 0;

    tm = localtime( &art->timeofday );

    strftime(path, sizeof path, "%Y", tm);
    if ( (mkdir(path, 0755) != 0) && (errno != EEXIST) ) return 0;
    strftime(path, sizeof path, "%Y/%m", tm);
    if ( (mkdir(path, 0755) != 0) && (errno != EEXIST) ) return 0;
    strftime(path, sizeof path, "%Y/%m/%d", tm);
    if ( (mkdir(path, 0755) != 0) && (errno != EEXIST) ) return 0;

    end = path + strlen(path);

    for (seq=0; seq< 1000; seq++) {
	sprintf(end, "/%03d", seq);
	if (mkdir(path, 0755) == 0) {
	    strcat(path, "/");
	    art->url     = makefile(path, "index.html");
	    art->ctlfile = makefile(path, "message.ctl");
	    art->msgfile = makefile(path, "message.txt");
	    art->cmtfile = makefile(path, "comment.txt");
	    art->cmtdir  = makefile(path, "comments");
	    art->format  = MARKDOWN;

	    return(art->ctlfile && art->msgfile && art->cmtfile && art->cmtdir);
	}
    }
    return 0;
}


void
freeartbody(struct article *art)
{
    if (art && art->body) {
	munmap(art->body, art->size);
	art->body = 0;
	art->size = 0;
    }
}


void
freeart(struct article *art)
{
    if (art) {
	freeartbody(art);

	if (art->author)  free(art->author);
	if (art->title)   free(art->title);
	if (art->url)     free(art->url);
	if (art->ctlfile) free(art->ctlfile);
	if (art->msgfile) free(art->msgfile);
	if (art->cmtfile) free(art->cmtfile);
	if (art->cmtdir)  free(art->cmtdir);
	memset(art,0,sizeof *art);
	free(art);
    }
}


int
writectl(struct article *art)
{
    FILE *f;

    errno = EINVAL;

    if (art && art->ctlfile && (f = fopen(art->ctlfile, "w")) ) {
	fprintf(f, "A:%s\n", art->author);
	fprintf(f, "W:%ld\n", art->timeofday);
	fprintf(f, "M:%ld\n", art->modified ? art->modified : art->timeofday);
	fprintf(f, "T:%s\n",art->title);
	if (art->category && art->category[0])
	    fprintf(f, "X:%s\n", art->category);
	if ( art->format == MARKDOWN )
	    fprintf(f, "Z:Markdown\n");
	fprintf(f, "U:%s\n", art->url);
	fprintf(f, "F:%d\n", art->comments);
	fprintf(f, "C:%d\n", art->comments_ok);
	fprintf(f, "B:%d\n", art->moderated);
	fclose(f);
	return 1;
    }
    return 0;
}


/* allocate a comment structure
 */
struct comment *
newcomment(struct article *art)
{
    struct comment *tmp;
    char *q;
    int seq;
    int f;

    if ( tmp = calloc(sizeof tmp[0], 1) ) {
	tmp->approved = !art->moderated;
	tmp->art = art;
	time( &(tmp->when) );

	if ( (tmp->file = malloc(strlen(art->cmtdir) + 60)) == 0 ) {
	    free(tmp);
	    return 0;
	}
	mkdir (art->cmtdir, 0700);

	sprintf(tmp->file, "%s/", art->cmtdir);
	q = tmp->file + strlen(tmp->file);


	for (seq = art->comments; seq >= 0; seq++) {
	    sprintf(q, "%04d", seq);
	    if ( (f = open(tmp->file, O_RDWR|O_CREAT|O_EXCL, 0600)) != -1) {
		close(f);
		return tmp;
	    }
	    else if (errno != EEXIST)
		break;
	}
	freecomment(tmp);
	errno = ENFILE;
    }
    return 0;
}


struct comment *
opencomment(char *filename)
{
    struct comment *ret = calloc(sizeof *ret, 1);
    char *base;
    char *p, *eol, *end;
    char field;
    char *ctl;
    long size;

    if ( ret == 0 )
	return 0;

    if ( (ret->file = strdup(filename)) == 0) {
	freecomment(ret);
	return 0;
    }

    if ( (ctl = mapfile(ret->file, &size)) == 0 ) {
	freecomment(ret);
	return 0;
    }

    end = ctl + size;

    for (p = ctl; p < end; p = 1+eol) {
	eol = eoln(p, end);

	if ( p[1] == ':' ) {
	    field = p[0];
	    p += 2;
	    switch (field) {
	    case 'A':	/* The (A)uthor */
		    ret->author = restofline(p,eol);
		    break;
	    case 'W':	/* their (W)ebsite */
		    ret->website = restofline(p,eol);
		    break;
	    case '@':	/* their email address */
		    ret->email = restofline(p,eol);
		    break;
	    case 'D':	/* comment (D)ate */
		    ret->when = atol(p);
		    break;
	    case 'O':
		    ret->approved = atoi(p);
		    break;
	    case 'L':
		    ret->linksok = atoi(p);
		    break;
	    case 'P':
		    ret->publish_mail = atoi(p);
		    break;
	    case 'T':
		    if ( ret->text = malloc(1+(end-p)) ) {
			strncpy(ret->text, p, (end-p));
			ret->text[end-p] = 0;
			munmap(ctl, size);
			return ret;
		    }
	    }
	}
    }
    munmap(ctl, size);
    freecomment(ret);
    return 0;
}

void
freecomment(struct comment *cmt)
{
    struct stat st;

    if (cmt->file) {
	if (stat(cmt->file, &st) != 0 || st.st_size == 0)
	    unlink(cmt->file);
	free(cmt->file);
    }
    free(cmt);
}


int
savecomment(struct comment *cmt)
{
    FILE *f = fopen(cmt->file, "w");
    int status;

    if (f) {
	if (cmt->email)
	    fprintf(f, "@:%s\n", cmt->email);
	if (cmt->website)
	    fprintf(f, "W:%s\n", cmt->website);
	fprintf(f, "A:%s\n"
		   "O:%d\n"
		   "P:%d\n"
		   "D:%ld\n"
		   "T:%s", cmt->author, cmt->approved, cmt->publish_mail,
			   cmt->when, cmt->text);
	fclose(f);
	status = 1;
    }
    else status = 0;

    freecomment(cmt);
    return status;
}


int
writemsg(struct article *art)
{
    FILE *f;
    errno = EINVAL;

    if (art && art->msgfile && (f = fopen(art->msgfile, "w")) ) {
	fwrite(art->body, art->size, 1, f);
	fclose(f);
	return 1;
    }
    return 0;
}


static void
alink(FILE *f, char *pfx, char *sfx, char *line, char *end)
{
    mkd_flag_t *flags = mkd_flags();

    mkd_set_flag_num(flags, MKD_NOLINKS);
    mkd_set_flag_num(flags, MKD_NOIMAGE);
    mkd_set_flag_num(flags, MKD_DLEXTRA);
    mkd_set_flag_num(flags, MKD_FENCEDCODE);

    
    fprintf(f, "%s<a href=\"%s/",pfx, fmt.url);
    while (*line != ':' && line < end)
	putc(*line++, f);
    putc('"', f);
    putc('>', f);
    if (*line == ':') {
	++line;
	mkd_text(line,end-line, f, flags);
    }
    fprintf(f, "</a>%s\n",sfx);

    mkd_free_flags(flags);
}


char *lastlast;

static void
navbar(FILE *f, char *url)
{
    static char *db = 0;
    static long size;
    static int usize;
    static int hasdb = 1;
    char *end;
    char *p, *low, *high;
    int ret, gap;

    lastlast = 0;

    if (fmt.url == 0)
	return;

    if ( hasdb && !db ) {
	db = mapfile("index.db", &size);
	hasdb = (db != 0);
    }
    if ( !hasdb )
	return;


    high = end = db + size;
    low = db;
    usize = strlen(url);

    while (low < high) {
	gap = high-low;
	p = low + (gap/2);

	while (p > low && p[-1] != '\n')
	    --p;

	ret = strncmp(url, p, usize);

	if (ret < 0)
	    high = p;
	else if (ret > 0) {
	    low = p;
	    while (low < high && *low != '\n')
		++low;
	    if (low < high) low++;
	}
	else {
	    char *next, *last;

	    fprintf(f, "<p class=\"navbar\">\n");

	    for (next=p; next < end && *next != '\n'; ++next)
		;
	    if (next < end-1) {
		++next;
		alink(f, "&laquo; ", " ", next, eoln(next,end));
	    }
	    else
		next=0;

	    if (p > db) {
		last = p-1;
		while (last > db && last[-1] != '\n')
		    --last;
		if (next)
		    fprintf(f, " &middot; ");
		lastlast = last;
		alink(f, " ", " &raquo;", last, p);
	    }
	    fprintf(f, "</p>\n");
	    return;
	}
    }
}


static struct article *htmlart = 0;

static int
puthtml(FILE *f)
{
    char *text;
    long  size, count;
    struct comment *c;
    DIR *cd;
    struct dirent *ce;
    char *cf;
    char *p;
    int firstcomment = 1;
    int cmax, i;
    struct dirent **say;
    MMIOT *doc;
    mkd_flag_t *flags = mkd_flags();

    mkd_set_flag_num(flags, MKD_NOHEADER);
    mkd_set_flag_num(flags, MKD_DLEXTRA);
    mkd_set_flag_num(flags, MKD_FENCEDCODE);

    navbar(f, htmlart->url);
    subject(f, htmlart, 0);
    if (fmt.topsig) byline(f, htmlart, 0);

    if (text = mapfile(htmlart->msgfile, &size)) {
	fprintf(f, "<!-- message -->\n");
	switch (htmlart->format) {
	case MARKDOWN:
	    doc = mkd_string(text,size, flags);
	    if ( fmt.base )
		mkd_basename(doc, fmt.base);
	    markdown(mkd_string(text, size, flags), f, 0);
	    break;
	default:
	    for (count=size, p = text; count>0; --count, ++p)
		if (*p != '\f')
		    putc(*p, f);
	    break;
	}
	munmap(text,size);
    }
    else
	fprintf(f, "<p class=\"Error\">Ooops! mmap %s</p>\n",strerror(errno));
    if (!fmt.topsig) byline(f,htmlart, 0);


    if (htmlart->comments > 0) {
	fprintf(f, "<p class=\"CommentHeader\">Comments<hr/></p>\n");
	if (text = mapfile(htmlart->cmtfile, &size)) {
	    fwrite(text,size,1,f);
	    munmap(text,size);
	    firstcomment = 0;
	}
	cf = alloca(strlen(htmlart->cmtdir) + 60);

	cmax = scandir(htmlart->cmtdir, &say, dirent_is_good, (stfu)dirent_nsort);
	for (i=0; i < cmax; i++) {
	    sprintf(cf, "%s/%s", htmlart->cmtdir, say[i]->d_name);
	    if ( c = opencomment(cf) ) {
		if (c->approved) {

		    mkd_flag_t *copy = mkd_copy_flags(flags);

		    if ( c->linksok ) {
			mkd_clr_flag_num(copy, MKD_NOLINKS);
			mkd_clr_flag_num(copy, MKD_NOIMAGE);
		    }
		    else {
			mkd_set_flag_num(copy, MKD_NOLINKS);
			mkd_set_flag_num(copy, MKD_NOIMAGE);
		    }
		    
		    fprintf(f, "<a name=\"%d\">\n</a><div class=\"comment\">\n", i);

		    if (firstcomment)
			firstcomment = 0;
		    else
			fputs(fmt.commentsep, f);

		    markdown(mkd_string(c->text, strlen(c->text), flags), f, copy);
		    fprintf(f, "</div>\n");
		    fprintf(f, "<div class=\"commentsig\">\n");

		    if (c->website) {
			if (!strncasecmp(c->website, "http://", 7))
			    fprintf(f, "<a href=\"%s\">%s</a>\n",
					c->website, c->author);
			else
			    fprintf(f, "<a href=\"http://%s\">%s</a>\n",
					c->website, c->author);
		    }
		    else if (c->email && c->publish_mail)
			fprintf(f, "<a href=\"mailto:%s\">%s</a>\n",
					c->email, c->author);
		    else
			fprintf(f, "%s ", c->author);
		    fputs(ctime( &(c->when) ), f);
		    fprintf(f, "</div>\n");
		    mkd_free_flags(copy);
		}
		freecomment(c);
	    }
	}
    }
    if (htmlart->comments_ok) {
	fprintf(f, "<!-- comment input -->\n");
	fprintf(f, "<form method=\"get\" action=\"%scomment\">\n",
		    fetch("_ROOT"));
	fputs(fmt.comment.start, f);
	if (htmlart->comments > 0)
	    fprintf(f, "<input type=\"submit\" name=\"comment\" value=\"Another Comment?\" />\n");
	else
	    fprintf(f, "<input type=\"submit\" name=\"comment\" value=\"Comment?\" />\n");
	fprintf(f, "<input type=\"hidden\" name=\"url\" value=\"%s\" />\n", htmlart->url);
	fputs(fmt.comment.end, f);
	fprintf(f, "</form>\n");
    }
    else if (htmlart->comments > 0)
	fprintf(f, "<p class=\"CommentHeader\">Comments are closed</p>\n");

    mkd_free_flags(flags);

    return 1;
}


int
writehtml(struct article *art)
{
    FILE *f;
    char *url, *root, *path;
    char *p;

    if ( art && art->url && (f = rewrite(art->url)) ) {
	flock(fileno(f), LOCK_EX);
	htmlart = art;

	root = fetch("_ROOT");

	url = alloca(strlen(art->url)+strlen(root)+10);
	sprintf(url, "%s%s", root, art->url);
	stash("_DOCUMENT", url);

	path = alloca(strlen(art->url)+1);
	strcpy(path, art->url);
	if ( (p = strrchr(path, '/')) == 0)
	    path = ".";
	else
	    *p = 0;
	stash("LOCATION", path);

	stash("_USER", art->author);
	stash("title", art->title);

	process("post.theme", puthtml, 1, f);
	fflush(f);
	ftruncate(fileno(f), ftell(f));
	flock(fileno(f), LOCK_UN);
	rclose(f);
    }
    htmlart = 0;
}


int
reindex(struct tm *tm, char *bbspath, int flags, int nrposts)
{
    char b1[20], b2[20];

    int fd;
    struct dirent **days, **each;
    int dmax, emax;
    struct dirent **dp, **ap;
    struct tm m;
    char **files;
    int nrfiles, count;
    char scratch[20];
    int even = 0;
    int more  = 0;
    int total = 0;
    int reindexme;
    int i;
    int c;
    int j, k;
    int verbose = fetch("_VERBOSE") != 0;
    char *webroot = fetch("_ROOT");
    char *q;
    MMIOT *doc;
    mkd_flag_t *markdown_flags = mkd_flags();

    mkd_set_flag_num(markdown_flags, MKD_DLEXTRA);
    mkd_set_flag_num(markdown_flags, MKD_FENCEDCODE);

    m = *tm;


    files = malloc(sizeof *files * (nrfiles = 1000));
    count=0;

    do {
	strftime(mo, sizeof mo, "%Y/%m", &m);

	dmax = scandir(mo, &days, dirent_is_good, (stfu)dirent_nsort);

	for (j=dmax; j-- > 0; ) {

	    sprintf(dydir, "%s/%s", mo, days[j]->d_name);

	    if ((emax=scandir(dydir, &each, dirent_is_good, (stfu)dirent_nsort)) < 1)
		continue;

	    if ( (flags & INDEX_DAY) && (atoi(days[j]->d_name) != tm->tm_mday) )
		strftime(b1, sizeof b1, "! %b %%s %Y", &m);
	    else
		strftime(b1, sizeof b1, "@ %b %%s, %Y", &m);

	    sprintf(b2, b1, days[j]->d_name);
	    files[count++] = strdup(b2);

	    if (verbose)
		printf("T %s\n", b2);

	    for (k = emax; k-- > 0; ) {
		more ++;
		total ++;
		sprintf(art, "%s/%s/message.ctl", dydir, each[k]->d_name);
		if (count >= nrfiles-5)
		    files = realloc(files, sizeof *files * (nrfiles += 1000));
		files[count++] = strdup(art);
		if (verbose)
		    printf("A %s\n", art);
		/*free(*ap);*/
	    }
	    /*free(*dp);*/

	    if (nrposts && (more > nrposts))
		break;
	}

	if (--m.tm_mon < 0) {
	    m.tm_mon = 11;
	    m.tm_year--;
	}

	/* bail out when it gets older than the oldest month the weblog
	 * program existed
	 */
	if (m.tm_year < 100)
	    break;
    } while (more < nrposts);

    if (iFb) fclose(iFb);
    iFb = tmpfile();

    for (reindexme = j = i = 0; i < count; i++) {

	if (files[i][0] == '@' || files[i][0] == '!') {
	    fprintf(iFb, "%s%s%s\n", fmt.chapter.start, 1+(files[i]),
				     fmt.chapter.end);
	    reindexme = (files[i][0] == '@');
	    j=0;
	}
	else {
	    struct article *art;


	    if (art = openart(files[i])) {

		if (reindexme /* || (flags & INDEX_FULL)*/ ) {
		    writehtml(art);
		    if (verbose)
			printf("H %s\n", files[i]);
		}

		if (j++ > 0)
		    fputs(fmt.separator, iFb);

		fprintf(iFb, fmt.article.start, even ? "even" : "odd");

		subject(iFb, art, 1);
		if (fmt.topsig)
		    byline(iFb, art, 1);

		switch (art->format) {
		case MARKDOWN:
		    mkd_set_flag_num(markdown_flags, MKD_NOHEADER);
		    doc = mkd_string(art->body, art->size, markdown_flags);
		    if ( fmt.base )
			mkd_basename(doc, fmt.base);
		    markdown(doc, iFb, 0);
		    break;
		default:
		    if (q=memchr(art->body, '\f', art->size)) {
			fwrite(art->body, (q - art->body), 1, iFb);
			fputc(0, iFb);
			fputc(5, iFb);
		    }
		    else
			fwrite(art->body, art->size, 1, iFb);
		    break;
		}
		if (!fmt.topsig)
		    byline(iFb, art, 1);

		/* url fragment to article */
		fputc(0, iFb);
		fputc(1, iFb);
		fputs(art->url, iFb);
		/* url root of weblog */
		fputc(0, iFb);
		fputc(2, iFb);
		fputs(webroot, iFb);
		if (art->comments > 0) {
		    fputc(0, iFb);
		    fputc(3, iFb);
		    fprintf(iFb, "%d", art->comments);
		}
		if (art->comments_ok) {
		    fputc(0, iFb);
		    fputc(4, iFb);
		    fprintf(iFb, "1");

		}
		fputc(0, iFb);
		fputc(0, iFb);

		fputs(fmt.article.end, iFb);

		even = !even;
		freeart(art);

		if ( nrposts && !--total )
		    break;
	    }
	    else
		perror(files[i]);
	}

	free(files[i]);
    }
done:
    while (i < count)
	free(files[i++]);
    /*fflush(iFb);*/
    rewind(iFb);
    free(files);


    mkd_free_flags(markdown_flags);

    return 1;
}


int
buildpages(struct tm *tm, int which)
{
    char post[80];
    char archive_for[80];
    FILE *f;
    struct stat st;
    char *bbsroot = fetch("_ROOT");
    char *archive;


    archive = alloca(strlen(bbsroot) + 1001);


    if (which & PG_ARCHIVE) {
	strftime(post, sizeof post, "%Y/%m/index.html", tm);

	strftime(archive_for, sizeof archive_for, "Archive for %b %Y", tm);

	/* set stdout to -> YYYY/MM/index.html */
	if ( f = rewrite(post) ) {
	    sprintf(archive, "%s%s",bbsroot, post);
	    stash("_DOCUMENT", archive);
	    stash("_CONTEXT", "archive");
	    stash("title", archive_for);
	    process("page.theme", putindex, 1, f);
	    rclose(f);
	}
    }
    else if (which & PG_HOME) {
	/* set stdout to -> {HomePage} */
	if ( f = rewrite(fmt.homepage) ) {
	    sprintf(archive, "%s%s", bbsroot, fmt.homepage);
	    stash("_DOCUMENT", archive);
	    stash("_CONTEXT", "home");
	    stash("title", fmt.name);
	    process("page.theme", putindex, 1, f);
	    rclose(f);
	}
    }
    else if (which & PG_POST) {
	/* set stdout to -> post/index.html */
	if ( f = rewrite("post/index.html") ) {
	    sprintf(archive, "%spost/index.html", bbsroot);
	    stash("_DOCUMENT", archive);
	    stash("_CONTEXT", "post");
	    stash("title", fmt.name);
	    if (stat("post/page.theme", &st) == 0)
		process("post/page.theme", putindex, 1, f);
	    else
		process("page.theme", putindex, 1, f);
	    rclose(f);
	}
    }
    return 1;
}


void
generate(struct tm *tm, char *bbspath, int indexflags, int buildflags)
{
    int nrposts = 0;
    unsigned int flag;

    for (flag = 0x01; flag; flag <<= 1)
	if (buildflags & flag) {
	    nrposts = (flag==PG_HOME || flag == PG_POST) ? fmt.nrposts : 0;

	    if (reindex(tm, bbspath, indexflags, nrposts))
		buildpages(tm, flag);
	}
}
