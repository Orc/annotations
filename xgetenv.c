#include <stdlib.h>

char *
xgetenv(char *s)
{
    char *val = getenv(s);

    if (val && val[0])
	return val;
    return 0;
}
