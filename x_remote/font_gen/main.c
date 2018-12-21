#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <inttypes.h>

static void skip_comment(FILE *f)
{
	int v;
	do
	{
		v=fgetc(f);
	}while((v!='\n')&&(v!=EOF));
}

static int get_num(FILE * f)
{
	int v;
	char buf[20];
	int i;
	do
	{
		v=fgetc(f);
		if(EOF==v)
			return -1;
		if('#'==v)
		{
			skip_comment(f);
			v=' ';
		}
	}while(isspace(v));
	fseek(f,-1,SEEK_CUR);
	
	i=0;
	do
	{
		v=fgetc(f);
		if(EOF==v)
			return -1;
		if('#'==v)
		{
			skip_comment(f);
			v=' ';
		}
	}while(!isdigit(v));
	fseek(f,-1,SEEK_CUR);

	i=0;
	do
	{
		v=fgetc(f);
		buf[i]=v;
		i++;
	}while(isdigit(v)&&(EOF!=v)&&(i<11));
	i--;
	buf[i]='\0';
	return atoi(buf);
}

static int get_pix(uint8_t * map,int width,int x,int y)
{
	int bit=width*y+x;
	return (map[bit/8]>>(bit%8))&1;
}

static void set_pix(uint8_t * map,int width,int x,int y,uint8_t val)
{
	int bit=width*y+x;
	map[bit/8]=(map[bit/8]&(~(1<<(bit%8))))|((!!val)<<(bit%8));
}

/*
whole pic: 615 x 2304 pixels (xdim x ydim)
border width h: 2-3pixels(not exact...)
border width v: 2-3pixels
bitmap 77x101 pixels
gap horiz: 22 pixel
gap vert: 40 pixels

pixel vert: 13 pixels
pixel hrz: 6 pixels
*/

static float lerp(float a,float b,float t)
{
	return (a*(1.0-t))+(b*t);
}

static void get_char(uint8_t *map,int xdim,int ydim,int col,int row)
{
	int i,j;
	float cx,cy;
	cx=lerp(0,532,((float)col)/5.0);
	cy=lerp(0,2197,((float)row)/15.0);
	
	for(i=0;i<10;i++)
	{
		uint16_t r=0;
		printf("\t\t");
		
		for(j=0;j<12;j++)
		{
			float x=lerp(cx+4,cx+78,j/11.0);
			float y=lerp(cy+6,cy+102,i/9.0);
//			int p=get_pix(map,xdim,(col*(6+157+6+44)+6+4+160*j/12)*xdim/1230,(row*(3+101+2+40)+3+7+105*i/10)*ydim/2304);
			int p=get_pix(map,xdim,x+0.5,y+0.5);

			if(p)
				printf("0");
			else
				printf("1");

			if(!p)
				r|=1<<j;
		}

		if(i!=9)
			printf("b,\n");
		else
			printf("b\n");
/*
		if(i!=9)
			printf("0x%4.4hx,\n",r);
		else
			printf("0x%4.4hx\n",r);
*/
	}
}

static void get_ochar(uint8_t *map,int xdim,int ydim,int col,int row)
{
	int i,j;
	
	for(i=0;i<10;i++)
	{
		uint16_t r=0;
		printf("\t\t");
		
		for(j=0;j<12;j++)
		{
			int p=get_pix(map,xdim,(col*(8+315+6+88)+12+14+303*j/12)*xdim/5336,(row*(3+92+2+40)+4+5+98*i/10)*ydim/1750);

/*			if(!p)
				printf("X");
			else
				printf("_");*/
			if(!p)
				r|=1<<j;
		}
/*
		if(i!=9)
			printf("\n");
		else
			printf("\n\n");
*/
		if(i!=9)
			printf("0x%4.4"PRIx16",\n",r);
		else
			printf("0x%4.4"PRIx16"\n",r);

	}
}

static void dump_charset(uint8_t *map,int xdim,int ydim)
{
	int i,j;
	printf("{\n");
	for(i=0;i<6;i++)
	{
		for(j=0;j<16;j++)
		{
			printf("\t{\n");
			get_char(map,xdim,ydim,i,j);
			if((i==5)&&(j==15))
			{
				printf("\t}\n");
			}
			else
			{
				printf("\t},\n");
			}
		}
	}
	printf("}\n");
}
//for option characters
static void dump_options(uint8_t *map,int xdim,int ydim)
{
	int i,j;
	printf("{\n");
	for(i=0;i<13;i++)
	{
		for(j=0;j<13;j++)
		{
			printf("\t{\n");
			get_ochar(map,xdim,ydim,j,i);
			if((i==12)&&(j==12))
			{
				printf("\t}\n");
			}
			else
			{
				printf("\t},\n");
			}
		}
	}
	printf("}\n");
}

int main(int argc,char * argv[])
{
	int xdim,ydim,scale,i,j;
	uint8_t * map;
	char mag[3]={'\0','\0','\0'};
	FILE * input_file;
	if(argc<2)
	{
		fprintf(stderr,"Error: need input filename as an argument (pgm ascii format)\n");
		return 1;
	}
	input_file=fopen(argv[1],"r");
	if(NULL==input_file)
	{
		fprintf(stderr,"Error opening %s\n",argv[1]);
		return 1;
	}
	fread(mag,1,2,input_file);
	if(strcmp("P2",mag))
	{
		fclose(input_file);
		fprintf(stderr,"Wrong magic: %s expected: P2\n",mag);
		return 1;
	}
	xdim=get_num(input_file);
	if(xdim<=0)
		return 1;
	ydim=get_num(input_file);
	if(ydim<=0)
		return 1;
	scale=get_num(input_file);
	if(scale<=0)
		return 1;
	
	map=calloc(1,((ydim*xdim)+7)/8);
	for(i=0;i<ydim;i++)
	{
		for(j=0;j<xdim;j++)
		{
			int v;
			v=get_num(input_file);
			if(v<0)
			{
				fprintf(stderr,"something went wrong\n");
				fclose(input_file);
				return 1;
			}
			set_pix(map,xdim,j,i,v?1:0);
		}
	}
	fclose(input_file);
//	dump_options(map,xdim,ydim);
	dump_charset(map,xdim,ydim);
	free(map);
	return 0;
}
