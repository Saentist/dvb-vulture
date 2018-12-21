#include <stdio.h>
#include "utl.h"
#include "btre.h"
#include "transponder.h"
#include "dmalloc.h"

int
write_transponder (void *x NOTUSED, BTreeNode * v, FILE * f)
{
  Transponder *t = btreeNodePayload (v);

  fwrite (t, sizeof (Transponder) - 2 * sizeof (Table *), 1, f);
  tbl_write1 (t->tbl[0], f);
  tbl_write1 (t->tbl[1], f);
  return 0;
}

//int _verify_btree(BTreeNode * top);

/**
 *\brief write transponders to file
 *
 */
int
tp_node_write (BTreeNode * root, FILE * f)
{
  return btreeWrite (root, NULL, write_transponder, f); //||_verify_btree(root));
}

BTreeNode *
read_transponder (void *x NOTUSED, FILE * f)
{
  Transponder *t;
  BTreeNode *n;
  n = utlCalloc (1, btreeNodeSize (sizeof (Transponder)));
  if (!n)
    return NULL;

  t = btreeNodePayload (n);

  fread (t, sizeof (Transponder) - 2 * sizeof (Table *), 1, f);

  t->tbl[0] = tbl_read1 (f);
  t->tbl[1] = tbl_read1 (f);
  return n;
}

/**
 *\brief read transponders from file
 *
 *\return the top Node of tree
 */
BTreeNode *
tp_node_read (FILE * f)
{
  BTreeNode *n = NULL;
  btreeRead (&n, NULL, read_transponder, f);
  return n;
}
