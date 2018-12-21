#include <winbase.h>
#include "utl.h"
#include "wordexp.h"
int wordexp(const char *s, wordexp_t *p, int flags)
{
	DWORD nSize=4096;
	int rv;
	LPTSTR lpDst=utlCalloc(sizeof(TCHAR),nSize+1);
	rv=(int)ExpandEnvironmentStrings(s,lpDst,nSize);
	p->we_wordc=1;
	p->we_wordv=utlCalloc(sizeof(LPTSTR),2);
	p->we_wordv[0]=lpDst;
	p->we_wordv[1]=NULL;
	p->we_offs=0;
	return (rv==0);
}

void wordfree(wordexp_t *p)
{
	size_t i;
	for(i=0;i<p->we_wordc;i++)
	{
		utlFAN(p->we_wordv[i]);
	}
	utlFAN(p->we_wordv);
}


