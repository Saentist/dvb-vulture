#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
//does bresenham interpolation
void iplBres(int16_t x1,int16_t y1,int16_t x2,int16_t y2,uint8_t color)
{
	int16_t dec=0,step,dx,dy;
	int16_t* px,*py;

	if(clipLine(&x1,&y1,&x2,&y2))
		return;
	dx=abs(x2-x1);
	dy=abs(y2-y1);
	
	px=&x1;
	py=&y1;
	if(dx<dy)
	{        
		swap(x1,y1);
		swap(x2,y2);
		swap(dx,dy);
		px=&y1;
		py=&x1;
	}
	
	if(x2<x1)
	{
		swap(x1,x2);
		swap(y1,y2);
	}

	step=(y1<y2)? 1:-1;
	putPix(*px,*py,color);
	dy*=2;
	dec=dy+dx;
	dx*=2;
	while(x1<x2)
	{
		x1++;
		if(dec>dx)
		{
			y1+=step;
			dec-=dx;
		}
		dec+=dy;
		putPix(*px,*py,color);
	}
  return;
}
*/
static uint16_t PLAIN_VT_CHARS[(11*96+13*13)][10]={

#include "latin_g0_nn.c"
,
#include "latin_g2_nn.c"
,
#include "cyr_g0_1_nn.c"
,
#include "cyr_g0_2_nn.c"
,
#include "cyr_g0_3_nn.c"
,
#include "cyr_g2_nn.c"
,
#include "greek_g0_nn.c"
,
#include "greek_g2_nn.c"
,
#include "arab_g0_nn.c"
,
#include "arab_g2_nn.c"
,
#include "hebrew_g0_nn.c"
,
#include "latinsub_nn.c"
};


static void frob_embolden(int sz,uint16_t *dest,uint16_t *src)
{
	int i,j,k,l;
	for(i=0;i<sz;i++)
	{
		uint16_t * c=src+10*i;
		uint16_t * d=dest+10*i;
		for(j=0;j<10;j++)
		{//clear
			d[j]=0;
		}
		
		for(j=0;j<10;j++)
		{
			for(k=0;k<12;k++)
			{
				if(c[j]&(1<<k))
				{
					uint16_t tmp;
/*					if(j>0)
					{
						tmp=(c[j-1]<<1)|(3<<k);
						tmp=(tmp>>1)&0x0fff;
						d[j-1]|=tmp;
					}*/
					tmp=(c[j]<<1)|(3<<k);
					tmp=(tmp>>1)&0x0fff;
					d[j]|=tmp;
/*					if(j<9)
					{
						tmp=(c[j+1]<<1)|(3<<k);
						tmp=(tmp>>1)&0x0fff;
						d[j+1]|=tmp;
					}*/
				}
			}
		}
	}
}

static void frob_italicise(int sz,uint16_t *dest,uint16_t *src)
{
	int i,j,k,l;
	for(i=0;i<sz;i++)
	{
		uint16_t * c=src+10*i;
		uint16_t * d=dest+10*i;
		for(j=0;j<10;j++)
		{//clear
			d[j]=0;
		}
		for(j=0;j<3;j++)
		{
			d[j]=(c[j]<<1)&0x0fff;
		}
		for(j=3;j<6;j++)
		{
			d[j]=c[j];
		}
		for(j=6;j<10;j++)
		{
			d[j]=c[j]>>1;
		}
	}
}

static void file_gen(char * name,int sz,uint16_t *data)
{
	FILE * fp;
	int i,j,k;
	fp=fopen(name,"w");
	if(NULL==fp)
		return;
	for(i=0;i<sz;i++)
	{
		uint16_t * c=data+10*i;
		fputs("{\n",fp);
		for(j=0;j<10;j++)
		{
			fputs("\t0b0",fp);
			for(k=0;k<12;k++)
			{
				if(c[j]&(1<<(11-k)))
					fputc('1',fp);
				else
					fputc('0',fp);
			}
			if(j<9)
				fputs(",\n",fp);
			else
				fputc('\n',fp);
		}
		if(i<(sz-1))
			fputs("},\n",fp);
		else
			fputs("}\n",fp);
	}
	fclose(fp);
}

static void frob_attr(char* base,int sz,uint16_t *in_chars)
{
	uint16_t dest1[169][10];
	uint16_t dest2[169][10];
	char buffer[256];
	
	frob_embolden(sz,dest1[0],in_chars);
	strcpy(buffer,base);
	strcat(buffer,"_nb.c");
	file_gen(buffer,sz,dest1[0]);
	
	frob_italicise(sz,dest1[0],in_chars);
	strcpy(buffer,base);
	strcat(buffer,"_in.c");
	file_gen(buffer,sz,dest1[0]);
	
	frob_embolden(sz,dest2[0],dest1[0]);
	strcpy(buffer,base);
	strcat(buffer,"_ib.c");
	file_gen(buffer,sz,dest2[0]);
}

int main(int argc,char * argv[])
{
	frob_attr("latin_g0",96,PLAIN_VT_CHARS[0]);
	frob_attr("latin_g2",96,PLAIN_VT_CHARS[96]);
	frob_attr("cyr_g0_1",96,PLAIN_VT_CHARS[2*96]);
	frob_attr("cyr_g0_2",96,PLAIN_VT_CHARS[3*96]);
	frob_attr("cyr_g0_3",96,PLAIN_VT_CHARS[4*96]);
	frob_attr("cyr_g2",96,PLAIN_VT_CHARS[5*96]);
	frob_attr("greek_g0",96,PLAIN_VT_CHARS[6*96]);
	frob_attr("greek_g2",96,PLAIN_VT_CHARS[7*96]);
	frob_attr("arab_g0",96,PLAIN_VT_CHARS[8*96]);
	frob_attr("arab_g2",96,PLAIN_VT_CHARS[9*96]);
	frob_attr("hebrew_g0",96,PLAIN_VT_CHARS[10*96]);
	frob_attr("latinsub",13*13,PLAIN_VT_CHARS[11*96]);
	return 0;
}

