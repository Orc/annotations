
#include "config.h"
#include "formatting.h"
#include "indexer.h"
#include "syndicate.h"

#include <stdio.h>
#include <time.h>
#include <string.h>

#include <sys/types.h>
#include <stdlib.h>
#include <dirent.h>

#include <mkdio.h>

#ifndef MKD_CDATA
#define CANNOT_CDATA
#define MKD_CDATA 0
#endif

extern int dirent_nsort(struct dirent **a, struct dirent **b);
extern int dirent_is_good(struct dirent *e);

extern char VERSION[];

/* pick a few sentences off the start of an article
 */
#define EX_HTML		0x01	/* allow html */
#define EX_ENTITY	0x02	/* allow character entities */
#define CLIPPING	400
#define MORE		200

char *
excerpt(char *text, int textsize, int flags)
{
    static char bfr[CLIPPING+MORE];
    char *p, *e;
    int print = 1;
    int clip, size;
    int dotdotdot;

    for (p=text, size=clip=0; (size < textsize) && (size < CLIPPING+MORE); ++p, ++size) {
	if (*p == '<') print = 0;
	if (print) {
	    clip++;
	    if ((clip > CLIPPING) && (*p == '.' || *p == '?' || *p == '!')) {
		++p, ++size;
		break;
	    }
	}
	if (*p == '>') print = 1;
    }

    print = 1;
    dotdotdot = (size < textsize);
    for (e=bfr, p=text; size > 0; ++p, --size) {
	if ((*p == '<') && !(flags&EX_HTML) ) print = 0;
	if (print) {
	    if ( (*p == '&') && !(flags&EX_ENTITY) ) {
		while (size && (*p != ';'))
		    ++p, --size;
	    }
	    else if (0x80 & *p) {
		*e++ = '?';
	    }
	    else
		*e++ = *p;
	}
	if ((*p == '>')) print = 1;
    }
    if (dotdotdot) {
	*e++ = '.';
	*e++ = '.';
	*e++ = '.';
    }
    *e = 0;
    return bfr;
}


void
ffilter(FILE *f, char *bfr, int len)
{
    for (; len-- > 0; ++bfr) {
	if (*bfr & 0x80)
	    fprintf(f, "&#%03d;", (unsigned char)(*bfr));
	    /*putc('?', f);*/
	else if (*bfr != '\f')
	    putc(*bfr, f);
    }
}



/* The Userland RSS format */


static int
rss2header(FILE *f)
{
    time_t now = time(0);
    char tod[80];

    strftime(tod, 80, "%a, %d %b %Y %H:%M:%S %Z", localtime(&now));

    fprintf(f, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
               "<rss version=\"2.0\">\n");
    fprintf(f, "<channel>\n"
	       "  <title>%s</title>\n", fmt.name);
    fprintf(f, "  <link>%s</link>\n", fmt.url);
    fprintf(f, "  <description>%s</description>\n",
		    fmt.about ? fmt.about : fmt.name);

    fprintf(f, "  <lastBuildDate>%s</lastBuildDate>\n", tod);
    fprintf(f, "  <docs>http://blogs.law.harvard.edu/tech/rss</docs>\n");
    fprintf(f, "  <generator>Annotations</generator>\n");
    if (fmt.author)
	fprintf(f, "  <managingEditor>%s</managingEditor>\n", fmt.author);
}


static int
rss2fini(FILE *f)
{
    fprintf(f, "\n</channel>\n"
	       "</rss>\n");
}


static int
rss2ok()
{
    return fmt.author && fmt.url && fmt.name;
}


static int
rss2post(FILE *f, struct article *art)
{
    char tod[80];
    char *q;
    int print = 1;
    int size;
    time_t *timep;
    MMIOT *doc;
    mkd_flag_t *flags = mkd_flags();

    mkd_set_flag_num(flags, MKD_NOLINKS);
    mkd_set_flag_num(flags, MKD_NOIMAGE);
    mkd_set_flag_num(flags, MKD_CDATA);
    mkd_set_flag_num(flags, MKD_DLEXTRA);
    mkd_set_flag_num(flags, MKD_FENCEDCODE);

    timep = (art->modified != art->timeofday) ? &(art->modified)
					      : &(art->timeofday);
    strftime(tod, 80, "%a, %d %b %Y %H:%M:%S %Z", localtime(timep));

    fprintf(f, "\n"
	       "  <item>\n"
	       "    <title>");

    mkd_text(art->title, strlen(art->title), f, flags);
    fprintf(f,"</title>\n"
	       "    <link>%s/%s</link>\n"
	       "    <guid isPermaLink=\"true\">%s/%s</guid>\n"
	       "    <pubDate>%s</pubDate>\n", fmt.url, art->url,
					      fmt.url, art->url,
					      tod);
    /*fprintf(f, "    <description><![CDATA[");*/
    fprintf(f, "    <description>");
    switch ( art->format ) {
    case MARKDOWN:
	mkd_clr_flag_num(flags, MKD_NOLINKS);
	mkd_clr_flag_num(flags, MKD_NOIMAGE);
	doc = mkd_string(art->body, art->size, flags);
	if ( fmt.base )
	    mkd_basename(doc, fmt.base);
	markdown(doc, f, flags);
	break;
    default:
	ffilter(f, art->body, art->size);
	break;
    }
    /*fprintf(f, "]]></description>\n");*/
    fprintf(f, "</description>\n");
    fprintf(f, "  </item>\n");

    mkd_free_flags(flags);
}


struct syndicator rss2feed = {
    "index.rss.xml",
    rss2ok,
    rss2header,
    rss2post,
    rss2fini
};


/* ~Userland's ATOM format */


static int
atomheader(FILE *f)
{
    char tod[80];
    time_t now = time(0);

    strftime(tod, sizeof tod, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    fprintf(f, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
	       "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n"
	       "<id>%s</id>\n"
	       "<title type=\"text\">%s</title>\n"
	       "<subtitle type=\"text\">%s</subtitle>\n"
	       "<link rel=\"alternate\" type=\"text/html\" href=\"%s\" />\n"
	       "<link rel=\"self\" type=\"application/atom+xml\" href=\"%s/%s\" />\n"
	       "<updated>%s</updated>\n"
	       "<generator uri=\"http://www.pell.portland.or.us/~orc/Code/annotations\" version=\"%s\">Annotations</generator>\n",
		    fmt.url, fmt.name, fmt.about, fmt.url,
		    fmt.url, atomfeed.filename,
		    tod, VERSION);
}


static int
atompost(FILE *f, struct article *art)
{
    char tod[80], mod[80];
    char *q;
    int print = 1;
    int size;
    MMIOT *doc;
    mkd_flag_t *flags;

    mkd_set_flag_num(flags, MKD_NOLINKS);
    mkd_set_flag_num(flags, MKD_NOIMAGE);
    mkd_set_flag_num(flags, MKD_DLEXTRA);
    mkd_set_flag_num(flags, MKD_FENCEDCODE);
    mkd_set_flag_num(flags, MKD_CDATA);

    strftime(tod, sizeof tod,
		"%Y-%m-%dT%H:%M:%SZ", gmtime(&(art->timeofday)));
    strftime(mod, sizeof mod,
		"%Y-%m-%dT%H:%M:%SZ", gmtime(&(art->modified)));

    fprintf(f, "\n"
	       "  <entry>\n"
	       "    <title type=\"html\">");
    mkd_text(art->title, strlen(art->title), f, flags);
    fprintf(f,"</title>\n"
	       "    <link rel=\"alternate\" type=\"text/html\" href=\"%s/%s\" />\n",
		    fmt.url, art->url);
    fprintf(f, "    <id>%s/%s</id>\n", fmt.url, art->url);
    fprintf(f, "    <content type=\"html\" xml:lang=\"en-us\"");
    if ( fmt.base )
	fprintf(f, " xml:base=\"%s/\"", fmt.base);
    fprintf(f, ">\n");
    switch ( art->format ) {
    case MARKDOWN:
	mkd_clr_flag_num(flags, MKD_NOLINKS);
	mkd_clr_flag_num(flags, MKD_NOIMAGE);
	mkd_clr_flag_num(flags, MKD_CDATA);
	mkd_set_flag_num(flags, MKD_NOHEADER);
	doc = mkd_string(art->body, art->size, flags);
	if ( fmt.base ) 
	    mkd_basename(doc, fmt.base);
	mkd_clr_flag_num(flags, MKD_NOLINKS);
	mkd_clr_flag_num(flags, MKD_NOIMAGE);
	markdown(doc, f, flags);
	break;
    default:
	fprintf(f, "    <![CDATA[");
	ffilter(f, art->body, art->size);
	fprintf(f, "]]>\n");
	break;
    }
    fprintf(f, "    </content>\n");
    fprintf(f, "    <published>%s</published>\n"
	       "    <updated>%s</updated>\n"
	       "    <author>\n"
	       "        <name>%s</name>\n"
	       "        <uri>%s</uri>\n"
	       "    </author>\n", tod, mod, art->author, fmt.url);
    fprintf(f, "  </entry>\n");

    mkd_free_flags(flags);
}


static int
atomfini(FILE *f)
{
    fprintf(f, "</feed>\n");
}


struct syndicator atomfeed = {
    "index.atom.xml",
    rss2ok,
    atomheader,
    atompost,
    atomfini,
};


int
syndicate(struct tm *tm, char *bbspath, struct syndicator *dsw)
{
    char mo[20];

    struct tm m;
    FILE *rssf;
    int fd;
    struct dirent **days, **each;
    int dmax, emax;
    struct dirent **dp, **ap;
    char **files;
    int nrfiles, count;
    int tries = 2;
    char scratch[20];
    char tod[80];
    int even = 0;
    int i;
    int j, k;
    char dydir[25];


#ifdef CANNOT_CDATA
    return 0;	/* syndication formats have changed. Again. */
#else

    if (! (*dsw->ok)() )
	return 0;

    if ( (rssf = fopen(dsw->filename, "w")) == 0) {
	perror("index.rss");
	return 0;
    }
    (*dsw->header)(rssf);


    m = *tm;

    files = malloc(sizeof *files * (nrfiles = 1000));
    count=0;

    while (tries--) {
	strftime(mo, sizeof mo, "%Y/%m", &m);
	dmax = scandir(mo, &days, dirent_is_good, (stfu)dirent_nsort);

	for (j=dmax; j-- > 0; ) {

	    sprintf(dydir, "%s/%s", mo, days[j]->d_name);
	    if ( (emax=scandir(dydir, &each, dirent_is_good, (stfu)dirent_nsort)) < 1)
		continue;

	    for (k = emax; k-- > 0; ) {
		char nmbuf[200];

		sprintf(nmbuf, "%s/%s/message.ctl", dydir, each[k]->d_name);
		if (count >= nrfiles-5)
		    files = realloc(files, sizeof *files * (nrfiles += 1000));
		files[count++] = strdup(nmbuf);
		/*free(*ap);*/
	    }
	    /*free(*dp);*/
	}
	if (count > fmt.nrposts)
	    break;
	if ( --m.tm_mon < 0) {
	    m.tm_mon = 11;
	    m.tm_year--;
	}
    }

    for (j = i = 0; i < count; i++) {
	struct article *art;
	int print=1;
	char *q;

	if (art = openart(files[i])) {

	    (*dsw->article)(rssf, art);

	    freeart(art);
	}
	else
	    perror(files[i]);

	free(files[i]);
    }
    free(files);

    (*dsw->fini)(rssf);
    fclose(rssf);

    return 1;
#endif
}
