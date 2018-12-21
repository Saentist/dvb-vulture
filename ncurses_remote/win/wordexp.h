#ifndef __wordexp_h
#define __wordexp_h
#include <stdlib.h>
typedef struct
{
	size_t we_wordc;
	char **we_wordv;
	size_t we_offs;
}wordexp_t;
int wordexp(const char *s, wordexp_t *p, int flags);
void wordfree(wordexp_t *p);

#define WRDE_APPEND 1
#define WRDE_DOOFFS 2
#define WRDE_NOCMD 4
#define WRDE_REUSE 8
#define WRDE_SHOWERR 16
#define WRDE_UNDEF 32
#endif

