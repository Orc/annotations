
#include "config.h"
#include "formatting.h"
#include "indexer.h"
#include "syndicate.h"

#include <stdio.h>
#include <time.h>
#include <malloc.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>

extern int dirent_nsort(struct dirent **a, struct dirent **b);
extern int dirent_is_good(struct dirent *e);



/* Userlands RSS format */


static int
rss2header(FILE *f)
{
    time_t now = time(0);
    char tod[80];

    strftime(tod, 80, "%a, %d %b %Y %H:%M:%S %Z", localtime(&now));

    fprintf(f, "<?xml version=\"1.0\"?>\n"
	       "<rss version=\"2.0\">\n"
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

    strftime(tod, 80, "%a, %d %b %Y %H:%M:%S %Z", localtime(&(art->timeofday)));

    fprintf(f, "\n"
	       "  <item>\n"
	       "    <title>%s</title>\n"
	       "    <link>%s/%s</link>\n"
	       "    <guid permalink=\"true\">%s/%s</guid>\n"
	       "    <pubDate>%s</pubDate>\n", art->title,
					      fmt.url, art->url,
					      fmt.url, art->url,
					      tod);
    fprintf(f, "    <description>");

    size = art->size;
    for ( q=art->body; (size > 0) && isspace(*q); ++q, --size)
	;

    if (size > 200) size = 200;

    while (size-- > 0) {
	if (*q == '<') print = 0;
	if (print)
	    if (*q == ']')
		fprintf(f, "&#%02x;", ']');
	    else
		fputc( (*q & 0x80) ? '?' : *q , f);
	if (print == 0  && *q == '>') print = 1;
    }
    fprintf(f, "</description>\n"
	       "  </item>\n");
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

    fprintf(f, "<?xml version=\"1.0\"?>\n"
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
	       "    <title type=\"text/plain\">%s</title>\n"
	       "    <link rel=\"alternate\" type=\"text/html\" href=\"%s/%s\" />\n",
		    fmt.url, art->url,
		    art->title,
		    fmt.url, art->url);
    fprintf(f, "    <content type=\"text/html\" xml:base=\"%s\">\n", fmt.url);
    fprintf(f, "<![CDATA[\n\n");
    size = (art->size > 200) ? 200 : art->size;
    for ( q=art->body; size-- > 0; ++q) {
	if (*q == '<') print = 0;
	if (print)
	    if (*q == ']')
		fprintf(f, "&#%02x;", ']');
	    else
		putc(*q, f);
	if (print == 0  && *q == '>') print = 1;
    }
    fprintf(f, "\n\n]]>\n"
	       "    </content>\n");
    fprintf(f, "    <issued>%s</issued>\n"
	       "    <modified>%s</modified>\n"
	       "    <author><name>%s</name></author>\n", tod, mod, art->author);
    fprintf(f, "    <dc:source>%s</dc:source>\n"
	       "    <dc:creator>%s</dc:creator>\n"
	       "    <dc:subject>%s</dc:subject>\n"
	       "    <dc:format>text/html</dc:format>\n"
	       "  </entry>\n",
		    fmt.name, art->author, art->title);
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

    FILE *rssf;
    int fd;
    struct dirent **days, **each;
    int dmax, emax;
    struct dirent **dp, **ap;
    char **files;
    int nrfiles, count;
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


    strftime(mo, sizeof mo, "%Y/%m", tm);

    files = malloc(sizeof *files * (nrfiles = 1000));
    count=0;

    if ( (dmax=scandir(mo, &days, dirent_is_good, dirent_nsort)) > 0) {
	for (j=dmax; j-- > 0; ) {

	    sprintf(dydir, "%s/%s", mo, days[j]->d_name);
	    if ( (emax=scandir(dydir, &each, dirent_is_good, dirent_nsort)) < 1)
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
