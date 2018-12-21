#include <stdlib.h>
#include <string.h>
//#include <pthread.h>
#include "utl.h"
#include "btre.h"
#include "debug.h"
#include "dmalloc_loc.h"

int DF_BTREE;
#define FILE_DBG DF_BTREE

BTreeNode *
btreeNodeFind (BTreeNode * top, void *elem, void *x,
               int (*compar) (void *, const void *, const void *))
{
  BTreeNode *current;
  if (!elem)
    return 0;
  current = top;
  while (current)
  {
    int cmp = compar (x, btreeNodePayload (current), elem);
    if (!cmp)
    {
      return current;
    }
    else if (cmp < 0)
    {
      current = current->Child[0];
      continue;
    }
    else
    {
      current = current->Child[1];
      continue;
    }
    return 0;
  }
  return 0;
}

BTreeNode *
btreeNodeGetOrIns (BTreeNode ** root, void *cv, void *x,
                   int (*cmp) (void *, const void *, const void *),
                   size_t size)
{
  BTreeNode *rv;
  rv = btreeNodeFind (*root, cv, x, cmp);
  if (!rv)
  {
    void *p;
    rv = utlCalloc (btreeNodeSize (size), 1);
    if (!rv)
    {
      errMsg ("calloc failed\n");
      return NULL;
    }
    p = btreeNodePayload (rv);
    memmove (p, cv, size);
    btreeNodeInsert (root, rv, x, cmp);
  }
  return rv;                    //btreeNodePayload(rv);
}

#define MAXIMUM(_A,_B) ((_A)>(_B)?(_A):(_B))
///derive a node's height from subtrees
static void
btree_node_height_fix (BTreeNode * x)
{
  unsigned h[2];
  h[0] = x->Child[0] ? x->Child[0]->height : 0;
  h[1] = x->Child[1] ? x->Child[1]->height : 0;
  x->height = MAXIMUM (h[0], h[1]) + 1;
}

///left rotates with i==0, right with 1
static void
btree_rot (BTreeNode ** root, BTreeNode * x, int i)
{
  BTreeNode *y = x->Child[1 - i];
  x->Child[1 - i] = y->Child[i];
  if (y->Child[i])
    y->Child[i]->Parent = x;
  y->Parent = x->Parent;
  if (NULL == x->Parent)
    *root = y;
  else if (x == x->Parent->Child[i])
    x->Parent->Child[i] = y;
  else
    x->Parent->Child[1 - i] = y;
  y->Child[i] = x;
  x->Parent = y;

  btree_node_height_fix (x);
  btree_node_height_fix (y);
//      errMsg("\nrotfix x: %u\nrotfix y: %u\n",x->height,y->height);
}

static void
btree_node_balance (BTreeNode ** top, BTreeNode * prnt, int dir)
{
  if (prnt->Child[1 - dir]->Child[dir])
  {
    if ((!prnt->Child[1 - dir]->Child[1 - dir])
        || (prnt->Child[1 - dir]->Child[1 - dir]->height <
            prnt->Child[1 - dir]->Child[dir]->height))
    {
      btree_rot (top, prnt->Child[1 - dir], 1 - dir);
    }
  }
  btree_rot (top, prnt, dir);
  prnt = prnt->Parent;
}

//regenerate heights from point of insertion upwards and rotate if unbalanced
static void
btree_node_height (BTreeNode ** top, BTreeNode * prnt)
{
  unsigned h[2];
  while (prnt)
  {
    h[0] = prnt->Child[0] ? prnt->Child[0]->height : 0;
    h[1] = prnt->Child[1] ? prnt->Child[1]->height : 0;

    if (h[0] > (h[1] + 1))
    {
      btree_node_balance (top, prnt, 1);
      prnt = prnt->Parent;
    }
    else if (h[1] > (h[0] + 1))
    {
      btree_node_balance (top, prnt, 0);
      prnt = prnt->Parent;
    }
    else if (prnt->height == (MAXIMUM (h[0], h[1]) + 1))
      return;                   //nothing changes upwards
/*
		h[0]=prnt->Child[0]?prnt->Child[0]->height:0;
		h[1]=prnt->Child[1]?prnt->Child[1]->height:0;
		errMsg("new heights: %u,%u\n",h[0],h[1]);
*/
    btree_node_height_fix (prnt);       //enter new height
    prnt = prnt->Parent;        //go up
  }
}

int
btreeNodeInsert (BTreeNode ** top, BTreeNode * elem, void *x,
                 int (*compar) (void *, const void *, const void *))
{
  BTreeNode *current;
  void *e = btreeNodePayload (elem);
  if (!top)
    return 1;
  if (!elem)
    return 3;
  elem->height = 1;
  current = *top;
  if (!current)
  {
    *top = elem;
    elem->Parent = NULL;
  }
  else
    while (1)
    {
      int cmp = compar (x, btreeNodePayload (current), e);
      if (!cmp)
      {
        return 2;
      }
      else if (cmp < 0)
      {
        if (current->Child[0])
        {
          current = current->Child[0];
          continue;
        }
        current->Child[0] = elem;
        elem->Parent = current;
        btree_node_height (top, elem->Parent);
        return 0;
      }
      else
      {
        if (current->Child[1])
        {
          current = current->Child[1];
          continue;
        }
        current->Child[1] = elem;
        elem->Parent = current;
        btree_node_height (top, elem->Parent);
        return 0;
      }
    }
  return 0;
}


void *
btreeNodePayload (BTreeNode * b)
{
  return b + 1;
}

BTreeNode *
btreePayloadNode (void *payload)
{
  BTreeNode *p = payload;
  return p - 1;
}

static int
btree_walk (BTreeNode * top, void *x, int depth,
            void (*visit) (void *x, BTreeNode * this, int which, int depth))
{
  if (!top)
    return 0;

  if ((!top->Child[0]) && (!top->Child[1]))
    visit (x, top, 3, depth);
  else
  {
    visit (x, top, 0, depth);

    if (top->Child[0])
      btree_walk (top->Child[0], x, depth + 1, visit);

    visit (x, top, 1, depth);

    if (top->Child[1])
      btree_walk (top->Child[1], x, depth + 1, visit);

    visit (x, top, 2, depth);
  }
  return 0;
}

int
btreeWalk (BTreeNode * top, void *x,
           void (*visit) (void *x, BTreeNode * this, int which, int depth))
{
  return btree_walk (top, x, 0, visit);

}


/**
 * unlink the old node and hang the new one where the old one was
 * order must be preserved: if(old>parent) then new>parent and if(old<=parent) then new<=parent
 * new may be NULL,parent may be NULL
 */
static void
btree_node_relink (BTreeNode ** root, BTreeNode * parent, BTreeNode * old,
                   BTreeNode * new)
{
  if (NULL != new)
  {
    new->Parent = parent;
  }
  if (NULL == parent)
  {
    *root = new;
  }
  else if (old == parent->Child[0])
  {
    parent->Child[0] = new;
  }
  else
  {
    if (old == parent->Child[1])
    {
      parent->Child[1] = new;
    }
    else
    {
      errMsg ("can't relink, not a child\n");
    }
  }

  old->Child[0] = NULL;
  old->Child[1] = NULL;
  old->Parent = NULL;
}

/**
 * removes an element having only one child
 *@param e element to remove
 */
static void
btree_node_rmv_single (BTreeNode ** root, BTreeNode * e)
{
  BTreeNode *parent;
  int i, j;
  parent = e->Parent;           //elements' parent

  if (parent->Child[1] == e)    //which child are we?
    i = 1;
  else if (parent->Child[0] == e)
    i = 0;
  else
  {                             //oops
    errMsg ("error removing single node\n");
    return;
  }

  if (e->Child[1])              //any child to relink?
    j = 1;
  else if (e->Child[0])
    j = 0;
  else
  {
    parent->Child[i] = NULL;
    btree_node_height (root, parent);
    return;
  }

  parent->Child[i] = e->Child[j];
  e->Child[j]->Parent = parent;

  btree_node_height (root, parent);
}


/**
 *\brief find and remove the tree node before e
 */
static BTreeNode *
btree_node_pre (BTreeNode ** root, BTreeNode * e)
{
  e = e->Child[0];
  while (e->Child[1])
  {
    e = e->Child[1];
  }
  btree_node_rmv_single (root, e);
  return e;
}

#if 0
/**
 *\brief find and remove the tree node after e
 */

static BTreeNode *
btree_node_post (BTreeNode ** root, BTreeNode * e)
{
  e = e->Child[1];
  while (e->Child[0])
  {
    e = e->Child[0];
  }
  btree_node_rmv_single (root, e);
  return e;
}
#endif
///protects our rand_r seed
//static pthread_mutex_t btre_rand_mx=PTHREAD_MUTEX_INITIALIZER;
//static unsigned int btre_seed;

//a sentinel node. functions may write to it. we'd need a mutex. better avoid it. would've needed that for rbtree as in corman, leiserson, rivest, stein: Introduction to Algorithms
//BTreeNode const BTREE_SENTINEL={&BTREE_SENTINEL,&BTREE_SENTINEL,&BTREE_SENTINEL,1)
//BTreeNode const BTREE_NIL=&BTREE_SENTINEL;




int
btreeNodeRemove (BTreeNode ** root, BTreeNode * e)
{
  BTreeNode *tmp;
  if (!e->Child[0])
  {
    tmp = e->Parent;
    btree_node_relink (root, e->Parent, e, e->Child[1]);
    btree_node_height (root, tmp);
  }
  else if (!e->Child[1])
  {
    tmp = e->Parent;
    btree_node_relink (root, e->Parent, e, e->Child[0]);
    btree_node_height (root, tmp);
  }
  else
  {
//              int decision;
    debugMsg ("else\n");
/*		pthread_mutex_lock(&btre_rand_mx);
		decision=rand_r(&btre_seed);
		pthread_mutex_unlock(&btre_rand_mx);
		if(decision>(RAND_MAX/2))
		{
			tmp=btree_node_post(root,e);
		}
		else
		{*/
    tmp = btree_node_pre (root, e);
//              }
    //now insert tmp instead of e
    tmp->Parent = NULL;
    tmp->Child[0] = e->Child[0];
    tmp->Child[1] = e->Child[1];
    if (e->Child[0])
      e->Child[0]->Parent = tmp;
    if (e->Child[1])
      e->Child[1]->Parent = tmp;
    //subtrees now at new node
    tmp->height = e->height;    //replace height tag
    //reparent node
    btree_node_relink (root, e->Parent, e, tmp);
  }
  return 0;
}

size_t
btreeNodeSize (size_t size_of_payload)
{
  return size_of_payload + sizeof (BTreeNode);
}

#define FID_NULL '0'
#define FID_VALID 'X'

int
btreeWrite (BTreeNode * top, void *x,
            int (*write) (void *x, BTreeNode * this, FILE * f), FILE * f)
{
  if (!top)
    fputc (FID_NULL, f);
  else
  {
    fputc (FID_VALID, f);
    if (write (x, top, f))
      return 1;
    return (btreeWrite (top->Child[0], x, write, f) ||
            btreeWrite (top->Child[1], x, write, f));
  }
  return 0;
}

uint64_t
btreeWriteSize (BTreeNode * top, void *x,
                uint64_t (*size) (void *x, BTreeNode * this))
{
  if (!top)
    return sizeof (char);
  else
    return sizeof (char) + size (x, top) + btreeWriteSize (top->Child[0], x,
                                                           size) +
      btreeWriteSize (top->Child[1], x, size);
}

static int
btree_read (BTreeNode * parent, BTreeNode ** rtn, void *x,
            BTreeNode * (*read) (void *x, FILE * f), FILE * f)
{
  BTreeNode *n;
  int node_id = fgetc (f);
  int rv;
  if (node_id == FID_NULL)
  {
    *rtn = NULL;
    return 0;
  }
  else if (node_id != FID_VALID)
  {                             //this is much worse
    errMsg ("invalid BTree FID\n");
    *rtn = NULL;
    return 1;
  }
  n = read (x, f);
  if (!n)
  {
    *rtn = NULL;
    return 1;
  }
  *rtn = n;
  n->Parent = parent;
  rv = (btree_read (n, &n->Child[0], x, read, f) ||
        btree_read (n, &n->Child[1], x, read, f));
  btree_node_height_fix (n);    //enter height
  return rv;
}

/*
#define ABSOLUTE(_A) (((_A)>0)?(_A):-(_A))
int _verify_btree(BTreeNode *x)
{
	int h[2];
	if(!x)
		return 0;
	h[0]=x->Child[0]?x->Child[0]->height:0;
	h[1]=x->Child[1]?x->Child[1]->height:0;
//	errMsg("balance: %d\n", h[0]-h[1]);
	return ((ABSOLUTE(h[0]-h[1])>=2)||_verify_btree(x->Child[0])||_verify_btree(x->Child[1]));//fail if any of the subtrees is unbalanced or this node is unbalanced
}

static void destroy_btree(BTreeNode *x)
{
	if(NULL==x)
		return;
	destroy_btree(x->Child[0]);
	destroy_btree(x->Child[1]);
	utlFAN(x);
}

static int verify_btree(BTreeNode ** top)
{
	int rv;
	rv=_verify_btree(*top);
	if(rv)
	{
		errMsg("BTree Verification failed\n");
		destroy_btree(*top);
		*top=NULL;
	}
	return rv;
}
*/
int
btreeRead (BTreeNode ** top, void *x, BTreeNode * (*read) (void *x, FILE * f),
           FILE * f)
{
  return btree_read (NULL, top, x, read, f);    //||verify_btree(top);
}
