#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include "menu.h"
/*
 *\file menu.c
 *\brief Curses Menu module
 *
 *Partial reimplementation hacked to the semantics of LINUX ncurses manpages..
 *Many features missing.
 *
 *This code is in the public domain
 */
#define FLG_SEL 1

struct ITEM_
{
	char const* name;
	char const* desc;///<not used
	unsigned flg;
	MENU *menu;
	void *x;
};


#define FLG_POSTED 1

struct MENU_
{
	Menu_Options o;
	int num_items;
	ITEM **items;
	int maxlen;///<of an item...
	int item_idx;///<...of current item
	WINDOW *win;
	WINDOW *sub;
	int mnu_fmt[2];
	int mnu_sz[2];///<log dimensions of menu(maybe higher than sub_sz. counts menu entries)
	int sub_sz[2];///<actually visible (counts not char cells but menu entries)
	int toprow;
	unsigned flg;
};

static MENU default_menu={
.o=O_ONEVALUE|
       O_SHOWDESC|
       O_ROWMAJOR|
       O_IGNORECASE|
       O_SHOWMATCH|
       O_NONCYCLIC,
.num_items=0,
.items=NULL,
.maxlen=0,
.item_idx=0,
.win=NULL,
.sub=NULL,
.mnu_fmt={1,16},
.sub_sz={0,0},
.mnu_sz={0,0},
.toprow=0,
.flg=0
};

static void get_mxy(MENU *menu,int idx,int *xy)
{
	int i=!!(menu->o&O_ROWMAJOR);
	xy[1-i]=idx%menu->mnu_sz[1-i];
	xy[i]=idx/menu->mnu_sz[1-i];
}

//returned idx may be out of bounds if items do not fill menu completely
static int mxy2idx(MENU *menu,int *xy)
{
	int i=!!(menu->o&O_ROWMAJOR);
	return xy[i]*menu->mnu_sz[1-i]+xy[1-i];
}

//bound x/y coords or wrap if cyclic
//idx: 0 horz wrap/saturate 1: vertical
static void mbound(MENU *menu,int *xy,int idx)
{
	int ubound;
	/*
	first, find max extents of current row/column(dep on idx)
	lower is always zero. upper varies
with
O_ROWMAJOR
0 1 2 
3 4 5
6 7 8
9 1011
(mnu_rows=4, mnu_cols=3)

without
O_ROWMAJOR
0 3 6
1 4 7
2 5 8


num_items=7
mnu_sz={3,3}
idx=0

0 3 6 ubound 3
1 4   ubound 2
2 5   ubound 2

with
O_ROWMAJOR
0 1 2 ubound 3
3 4 5 ubound 3
6     ubound 1
*/
	
	if((!!(menu->o&O_ROWMAJOR))^idx)
	{
		ubound=menu->num_items%menu->mnu_sz[idx];
		if((ubound==0)//no empty cells
		||(xy[1-idx]!=(menu->mnu_sz[1-idx]-1)))//not last row
			ubound=menu->mnu_sz[idx];
	}
	else
	{
	/*
0 3 6 ubound 3
1 4   ubound 2
2 5   ubound 2

3 3 1
		*/
		ubound=menu->num_items%menu->mnu_sz[1-idx];
		if((ubound<=xy[1-idx])&&(ubound!=0))
			ubound=menu->mnu_sz[idx]-1;
		else
			ubound=menu->mnu_sz[idx];
	}

	if(xy[idx]<0)
	{
		if(menu->o&O_NONCYCLIC)
			xy[idx]=0;
		else
		{
			xy[idx]=ubound-1;
			assert(xy[idx]>=0);
		}
	}
	
	
	if(xy[idx]>=ubound)
	{
		if(menu->o&O_NONCYCLIC)
			xy[idx]=ubound-1;
		else
			xy[idx]=0;
	}
}

static int find_nb_item(MENU *menu,int where)
{
	int p[2]={0,0};
	get_mxy(menu,menu->item_idx,p);
	p[where/2]+=(2*(where%2))-1;
	mbound(menu,p,where/2);
	return mxy2idx(menu,p);
}

//make sure item is within subwin. (changes toprow)
static void make_vis(MENU *menu, int idx)
{
	int p[2]={0,0};
	get_mxy(menu,idx,p);
	if(p[1]<menu->toprow)
		menu->toprow=p[1];
	else if(p[1]>=(menu->toprow+menu->sub_sz[1]))
		menu->toprow=p[1]-menu->sub_sz[1]+1;
}

// 0-3: left right up down
static void mv_nb(MENU *menu,int where)
{
	int idx;
	idx=find_nb_item(menu,where);
	make_vis(menu,idx);
	menu->item_idx=idx;
}

//make sure item is in subwindow (changes cursor y coord)
static void bound2sub(MENU *menu,int *p)
{
	if(p[1]<menu->toprow)
		p[1]=menu->toprow;
	if(p[1]>=menu->toprow+menu->sub_sz[1])
		p[1]=menu->toprow+menu->sub_sz[1]-1;
}

//make sure toprow is within bounds
static int boundtop(MENU *menu,int topr)
{
	if(topr>(menu->mnu_sz[1]-menu->sub_sz[1]))
		topr=menu->mnu_sz[1]-menu->sub_sz[1];//may end up wrong so...
	if(topr<0)//..no else here
		topr=0;
	return topr;
}

//scroll n lines
//0:up 1: down
static void scr_nln(MENU *menu,int where,int dist)
{
	int topr=menu->toprow+((2*where)-1)*dist;
	int p[2]={0,0};
	menu->toprow=boundtop(menu,topr);
	get_mxy(menu,menu->item_idx,p);
	bound2sub(menu,p);
	menu->item_idx=mxy2idx(menu,p);
	//make sure item exists and is onscreen
	while(menu->item_idx>=menu->num_items)
	{
		p[0]--;
		assert(p[0]>=0);
		menu->item_idx=mxy2idx(menu,p);
	}
}

//0:up 1: down
static void scr_ln(MENU *menu,int where)
{
	scr_nln(menu,where,1);
}
//0:up 1: down
static void scr_pg(MENU *menu,int where)
{
	scr_nln(menu,where,menu->sub_sz[1]);
}

static void move_around(MENU *menu,int c)
{
	switch(c)
	{
		case REQ_LEFT_ITEM:
		case REQ_RIGHT_ITEM:
		case REQ_UP_ITEM:
		case REQ_DOWN_ITEM:
		mv_nb(menu,c-1);
		break;
		case REQ_SCR_ULINE:
		case REQ_SCR_DLINE:
		scr_ln(menu,c-REQ_SCR_ULINE);
		break;
		case REQ_SCR_UPAGE:
		case REQ_SCR_DPAGE:
		scr_pg(menu,c-REQ_SCR_UPAGE);
		break;
		case REQ_FIRST_ITEM:
		menu->toprow=0;
		menu->item_idx=0;
		break;
		case REQ_LAST_ITEM:
		menu->item_idx=menu->num_items-1;
		make_vis(menu,menu->item_idx);
		break;
		case REQ_NEXT_ITEM://this may have to wrap like mbound...
		menu->item_idx++;
		if(menu->item_idx>=menu->num_items)
		{
			if(menu->o&O_NONCYCLIC)
				menu->item_idx=menu->num_items-1;
			else
				menu->item_idx=0;
		}
		make_vis(menu,menu->item_idx);
		break;
		case REQ_PREV_ITEM:
		menu->item_idx--;
		if(menu->item_idx<0)
		{
			if(menu->o&O_NONCYCLIC)
				menu->item_idx=0;
			else
				menu->item_idx=menu->num_items-1;
		}
		make_vis(menu,menu->item_idx);
		break;
		case REQ_TOGGLE_ITEM:
		if(!(menu->o&O_ONEVALUE))
			menu->items[menu->item_idx]->flg^=FLG_SEL;
		break;
		default:
		abort();
		break;
	}
}

static int is_selected(MENU *menu,int idx)
{
	return (menu->item_idx==idx)||(menu->items[menu->item_idx]->flg&FLG_SEL);
}

static int m_redraw(MENU * menu)
{
	int i;
	int idx[2];
	int start[2];
	int end[2];
	char * buf=calloc(1,sizeof(char)*(menu->maxlen+1)*3);
	if(NULL==buf)
		return ERR;
	start[1]=menu->toprow;
	start[0]=0;
	end[1]=menu->toprow+menu->sub_sz[1];
	end[0]=menu->mnu_sz[0];
	i=!!(menu->o&O_ROWMAJOR);
	idx[1]=1-i;
	idx[0]=i;
	for(i=start[idx[0]];i<end[idx[0]];i++)
	{
		int j;
		for(j=start[idx[1]];j<end[idx[1]];j++)
		{
			int p[2];
			int k;
			int y,x;
			int len;
			p[idx[1]]=j;
			p[idx[0]]=i;
			k=mxy2idx(menu,p);
			y=p[1]-menu->toprow;
			x=p[0]*menu->maxlen;
			
			len=getmaxx(menu->sub)-x;
			if(menu->maxlen<len)
				len=menu->maxlen;
			if(len>0)
			{
				mvwhline(menu->sub,y,x,' ',len);
				mvwchgat(menu->sub,y,x,len-1,A_NORMAL,0,NULL);
				
				if(k<menu->num_items)
				{
					snprintf(buf,len,"%c%s",
					is_selected(menu,k)?'-':' ',menu->items[k]->name);
					mvwaddnstr(menu->sub,y,x,buf,len);
				
					if(is_selected(menu,k))
						mvwchgat(menu->sub,y,x,len-1,A_REVERSE,0,NULL);
	/*			if(menu->item_idx==k)
				{
					curp[0]=x+menu->maxlen-1;
					curp[1]=y;
				}*/
				}
			}
		}
	}
//	wmove(menu->sub,curp[1],curp[0]);
	touchwin(menu->win);
	free(buf);
	return E_OK;
}

int menu_driver(MENU *menu, int c)
{
	move_around(menu,c);
	m_redraw(menu);
	return E_OK;
}

int set_menu_opts(MENU *menu, Menu_Options opts)
{
	if(menu->flg&FLG_POSTED)
		return E_POSTED;
	menu->o=opts;
	return E_OK;
}

int set_current_item(MENU *menu, const ITEM *item)
{
	int i;
	int idx=-1;
	if((NULL==menu)||(NULL==item)||(NULL==menu->items))
		return E_BAD_ARGUMENT;
	for(i=0;i<menu->num_items;i++)
	{
		if(item==menu->items[i])
		{
			idx=i;
		}
	}
	if(idx==-1)
		return E_NOT_CONNECTED;
	if(menu->flg&FLG_POSTED)
		make_vis(menu,idx);
	menu->item_idx=idx;
	return E_OK;
}

ITEM *current_item(const MENU *menu)
{
	if((NULL==menu)||(0>menu->item_idx)||(NULL==menu->items))//default menu/settings?? ...
		return NULL;
	return menu->items[menu->item_idx];
}

int item_index(const ITEM *item)
{
	int i;
	MENU * menu=item->menu;
	int idx=-1;
	if((NULL==menu)||(NULL==item)||(NULL==menu->items))
		return ERR;
	for(i=0;i<menu->num_items;i++)
	{
		if(item==menu->items[i])
		{
			idx=i;
		}
	}
	if(idx==-1)
		return ERR;
	return idx;
}

ITEM *new_item(const char *name, const char *description)
{
	ITEM * rv;
	if(name==NULL)
	{
		errno=E_BAD_ARGUMENT;
		return NULL;
	}
	rv=calloc(1,sizeof(ITEM));
	if(NULL==rv)
	{
		errno=E_SYSTEM_ERROR;
		return rv;
	}
	rv->name=name;
	rv->desc=description;
	rv->flg=0;
	rv->menu=NULL;
	return rv;
}

int free_item(ITEM *item)
{
	if(NULL==item)
		return E_BAD_ARGUMENT;
	if(item->menu)
		return E_CONNECTED;
	free(item);
	return E_OK;
}

MENU *new_menu(ITEM **items)
{
	MENU * m;
	if(NULL==items)
	{
		errno=E_NOT_CONNECTED;
		return NULL;
	}
	m=calloc(1,sizeof(MENU));
	if(NULL==m)
	{
		errno=E_SYSTEM_ERROR;
		return NULL;
	}
	*m=default_menu;
	m->sub=m->win=stdscr;
	m->items=items;
	for(m->num_items=0;(*items)!=NULL;m->num_items++,(*items)->menu=m,items++);//do nothing
	
	if(0==m->num_items)
	{
		free(m);
		errno=E_NOT_CONNECTED;
		return NULL;
	}
	return m;
}

int free_menu(MENU *menu)
{
	int i;
	if((NULL==menu)||(NULL==menu->items))
		return E_BAD_ARGUMENT;
	if(menu->flg&FLG_POSTED)
		return E_POSTED;
	for(i=0;i<menu->num_items;i++)
	{
		menu->items[i]->menu=NULL;
	}
	memset(menu,0,sizeof(MENU));
	free(menu);
	return E_OK;
}

static int no_room(MENU* menu)
{
	int y,x;
	getmaxyx(menu->sub,y,x);
	//allow truncation if it's just one column(it will always fit unless too long)
	//if it's multiple items wide, fail if they do not all fit
	return ((menu->sub_sz[0]>1)&&(menu->sub_sz[0]*menu->maxlen>x))||(menu->sub_sz[1]>y);
}


int post_menu(MENU *menu)
{
	int i;
	if(menu->flg&FLG_POSTED)
		return E_POSTED;
	if((NULL==menu->items)||(0==menu->num_items))
		return E_NOT_CONNECTED;
	if(NULL==menu->sub)
		return E_SYSTEM_ERROR;
	menu->maxlen=0;
	
	for(i=0;i<menu->num_items;i++)
	{
		menu->items[i]->flg&=~FLG_SEL;
		if(NULL==menu->items[i]->name)
			return E_SYSTEM_ERROR;
		if(strlen(menu->items[i]->name)>menu->maxlen)
			menu->maxlen=strlen(menu->items[i]->name);
	}
	menu->maxlen+=2;//one marker '-' , one separator ' ' ?
	
	menu->sub_sz[0]=menu->mnu_fmt[0];
	menu->sub_sz[1]=menu->mnu_fmt[1];

	i=!(menu->o&O_ROWMAJOR);
	if(menu->num_items<=menu->sub_sz[i])
	{//just one row/column or less
		menu->sub_sz[i]=menu->num_items;
		menu->sub_sz[1-i]=1;
	}

	if(no_room(menu))
		return E_NO_ROOM;
	menu->mnu_sz[0]=menu->sub_sz[0];
	menu->mnu_sz[1]=(menu->num_items+menu->mnu_sz[0]-1)/menu->mnu_sz[0];
	menu->flg|=FLG_POSTED;
	make_vis(menu,menu->item_idx);
	return m_redraw(menu);
}

int unpost_menu(MENU *menu)
{
	if(!(menu->flg&FLG_POSTED))
		return E_NOT_POSTED;
	menu->flg&=~FLG_POSTED;
	return E_OK;
}


int set_menu_win(MENU *menu, WINDOW *win)
{
	if(NULL==win)
		win=stdscr;
	if(NULL==menu)
		menu=&default_menu;
	if(menu->flg&FLG_POSTED)
		return E_POSTED;
	menu->win=win;
	return E_OK;
}

WINDOW *menu_win(const MENU *menu)
{
	if(NULL==menu)
		menu=&default_menu;
	return menu->win;
}

int set_menu_sub(MENU *menu, WINDOW *sub)
{
	if(NULL==sub)
		sub=stdscr;
	if(NULL==menu)
		menu=&default_menu;
	if(menu->flg&FLG_POSTED)
		return E_POSTED;
	menu->sub=sub;
	return E_OK;
}
WINDOW *menu_sub(const MENU *menu)
{
	if(NULL==menu)
		menu=&default_menu;
	return menu->sub;
}

int set_menu_format(MENU *menu, int rows, int cols)
{
	if(NULL==menu)
		menu=&default_menu;
	if(menu->flg&FLG_POSTED)
		return E_POSTED;
	if((cols<0)||(rows<0))
		return E_BAD_ARGUMENT;
	if(cols!=0)
		menu->mnu_fmt[0]=cols;
	if(rows!=0)
		menu->mnu_fmt[1]=rows;
	return E_OK;
}

void menu_format(const MENU *menu, int *rows, int *cols)
{
	if(NULL==menu)
		menu=&default_menu;
	*rows=menu->mnu_fmt[1];
	*cols=menu->mnu_fmt[0];
}


const char *item_name(const ITEM *item)
{
	return item->name;
}
const char *item_description(const ITEM *item)
{
	return item->desc;
}

int set_item_userptr(ITEM *item, void *userptr)
{
	item->x=userptr;
	return E_OK;
}
void *item_userptr(const ITEM *item)
{
	return item->x;
}


int set_item_value(ITEM *item, bool value)
{
	if(!(item->menu->o&O_ONEVALUE))
	{
		if(value)
			item->flg|=FLG_SEL;
		else
			item->flg&=~FLG_SEL;
	}
	else
		return E_REQUEST_DENIED;
	return E_OK;
}

bool item_value(const ITEM *item)
{
	return !!(item->flg&FLG_SEL);
}
