
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

extern int dirent_nsort(struct dirent **a, struct dirent **b);
extern int dirent_is_good(struct dirent *e);


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
               "<rss"
               "  xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
               "  xmlns:content=\"http://purl.org/rss/1.0/modules/content/\""
	       "  version=\"2.0\">\n"
	       "<channel>\n"
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

    timep = (art->modified != art->timeofday) ? &(art->modified)
					      : &(art->timeofday);
    strftime(tod, 80, "%a, %d %b %Y %H:%M:%S %Z", localtime(timep));

    fprintf(f, "\n"
	       "  <item>\n"
	       "    <title>");

    mkd_text(art->title, strlen(art->title), f, MKD_NOLINKS|MKD_NOIMAGE);
    fprintf(f,"</title>\n"
	       "    <link>%s/%s</link>\n"
	       "    <guid isPermaLink=\"true\">%s/%s</guid>\n"
	       "    <pubDate>%s</pubDate>\n", fmt.url, art->url,
					      fmt.url, art->url,
					      tod);
    fprintf(f, "    <description><![CDATA[");
    switch ( art->format ) {
    case MARKDOWN:
	markdown(mkd_string(art->body, art->size, MKD_NOHEADER), f, 0);
	break;
    default:
	ffilter(f, art->body, art->size);
	break;
    }
    fprintf(f, "]]></description>\n");
    fprintf(f, "  </item>\n");
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
	       "<feed version=\"0.3\" xmlns=\"http://purl.org/atom/ns#\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
	       "<title type=\"text/plain\">%s</title>\n"
	       "<tagline type=\"text/plain\">%s</tagline>\n"
	       "<link rel=\"alternate\" type=\"text/html\" href=\"%s\" />\n"
	       "<modified>%s</modified>\n"
	       "<generator>Annotations</generator>\n",
		    fmt.name, fmt.name, fmt.url, tod);
}


static int
atompost(FILE *f, struct article *art)
{
    char tod[80], mod[80];
    char *q;
    int print = 1;
    int size;

    strftime(tod, sizeof tod,
		"%Y-%m-%dT%H:%M:%SZ", gmtime(&(art->timeofday)));
    strftime(mod, sizeof mod,
		"%Y-%m-%dT%H:%M:%SZ", gmtime(&(art->modified)));

    fprintf(f, "\n"
	       "  <entry>\n"
	       "    <id>%s/%s</id>\n"
	       "    <title>",
		    fmt.url, art->url);
    mkd_text(art->title, strlen(art->title), f, MKD_NOLINKS|MKD_NOIMAGE);
    fprintf(f,"</title>\n"
	       "    <link rel=\"alternate\" type=\"text/html\" href=\"%s/%s\" />\n",
		    fmt.url, art->url);
    fprintf(f, "    <content type=\"text/html\" mode=\"escaped\" xml:lang=\"en-us\"  xml:base=\"%s\">\n", fmt.url);
    fprintf(f, "    <![CDATA[");
    switch ( art->format ) {
    case MARKDOWN:
	markdown(mkd_string(art->body, art->size, MKD_NOHEADER), f, 0);
    default:
	ffilter(f, art->body, art->size);
	break;
    }
    fprintf(f, "]]>\n"
	       "    </content>\n");
    fprintf(f, "    <issued>%s</issued>\n"
	       "    <modified>%s</modified>\n"
	       "    <author><name>%s</name></author>\n", tod, mod, art->author);
    fprintf(f, "    <dc:source>%s</dc:source>\n"
	       "    <dc:creator>%s</dc:creator>\n"
	       "    <dc:subject>",
		    fmt.name, art->author);
    mkd_text(art->title, strlen(art->title), f, MKD_NOLINKS|MKD_NOIMAGE);
    fprintf(f, "</dc:subject>\n"
	       "    <dc:format>text/html</dc:format>\n"
	       "  </entry>\n");
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
}
