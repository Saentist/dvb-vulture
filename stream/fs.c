/*
 * fs.c
 *
 * file system interactions
 * files get stored under a directory named after their carousel ID
 * the filename includes the module ID and the object key
 * when we download a directory we create a directory in our carousel directory
 * and fill it with sym links to the actual files and dirs it contains
 * (the sym links maybe dangling if we have not downloaded all the files yet)
 *
 * we also create a symlink that points to the Service Gateway dir
 */

/*
 * Copyright (C) 2005, Simon Kilvington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>

#include "fs.h"
#include "biop.h"
#include "utils.h"
#include "config.h"
#include "dmalloc.h"


static char *
make_mheg_root (void)
{
  char *_carousel_root = calloc (sizeof (char), PATH_MAX);
  if (NULL == _carousel_root)
    return NULL;
  /* make the hierarchy */
  snprintf (_carousel_root, PATH_MAX, "%s", (char const *) DEFAULT_MHEG);
  if (mkdir (_carousel_root, 0755) < 0 && errno != EEXIST)
    fatal ("Unable to create mheg directory '%s': %s", _carousel_root,
           strerror (errno));
  return _carousel_root;
}

static char *
make_tsid_root (uint16_t tsid)
{
  char *_carousel_root = make_mheg_root ();
  if ((NULL == _carousel_root) || (strlen (_carousel_root) >= PATH_MAX))
  {
    free (_carousel_root);
    return NULL;
  }
  snprintf (_carousel_root + strlen (_carousel_root),
            PATH_MAX - strlen (_carousel_root), "/%" PRIu16, tsid);
  if (mkdir (_carousel_root, 0755) < 0 && errno != EEXIST)
    fatal ("Unable to create carousel directory '%s': %s", _carousel_root,
           strerror (errno));
  return _carousel_root;
}

static char *
make_carousel_root (uint16_t tsid)
{
  char *_carousel_root = make_tsid_root (tsid);

  snprintf (_carousel_root + strlen (_carousel_root),
            PATH_MAX - strlen (_carousel_root), "/%s",
            (char const *) CAROUSELS_DIR);
  if (mkdir (_carousel_root, 0755) < 0 && errno != EEXIST)
    fatal ("Unable to create carousel directory '%s': %s", _carousel_root,
           strerror (errno));
  return _carousel_root;
}

static char *
make_carousel_pid (uint16_t tsid, uint16_t elementary_pid)
{
  char *_carousel_root = make_carousel_root (tsid);
  snprintf (_carousel_root + strlen (_carousel_root),
            PATH_MAX - strlen (_carousel_root), "/%" PRIu16, elementary_pid);
  if (mkdir (_carousel_root, 0755) < 0 && errno != EEXIST)
    fatal ("Unable to create carousel directory '%s': %s", _carousel_root,
           strerror (errno));
  return _carousel_root;
}

static char *
make_carousel_id (uint16_t tsid, uint16_t elementary_pid,
                  uint32_t carousel_id)
{
  char *_carousel_root = make_carousel_pid (tsid, elementary_pid);
  snprintf (_carousel_root + strlen (_carousel_root),
            PATH_MAX - strlen (_carousel_root), "/%" PRIu32, carousel_id);
  if (mkdir (_carousel_root, 0755) < 0 && errno != EEXIST)
    fatal ("Unable to create carousel directory '%s': %s", _carousel_root,
           strerror (errno));

  return _carousel_root;
}

void
save_file (char *kind, uint16_t tsid, uint16_t elementary_pid,
           uint32_t carousel_id, uint16_t module_id, char *key,
           uint32_t key_size, unsigned char *file, uint32_t file_size)
{
  char *root;
  char *ascii_key;
  char filename[PATH_MAX];
  FILE *f;

  /* make sure the carousel directory exists */
  root = make_carousel_id (tsid, elementary_pid, carousel_id);

  /* BBC use numbers as object keys, so convert to a text value we can use as a file name */
  ascii_key = convert_key (key, key_size);

  /* construct the file name */
  snprintf (filename, sizeof (filename), "%s/%s-%" PRIu16 "-%s", root, kind,
            module_id, ascii_key);
  free (ascii_key);
  free (root);
  if ((f = fopen (filename, "wb")) == NULL)
    fatal ("Unable to create file '%s': %s", filename, strerror (errno));
  if (fwrite (file, 1, file_size, f) != file_size)
    fatal ("Unable to write to file '%s'", filename);

  fclose (f);

  verbose ("Created file '%s'", filename);

  return;
}

/*
 * creates the symlink services/<service_id>
 * it points to the given Service Gateway object
 */

void
make_service_root (uint16_t tsid, uint16_t service_id, char *kind,
                   uint16_t elementary_pid, uint32_t carousel_id,
                   uint16_t module_id, char *key, uint32_t key_size)
{
  char *dirname = make_tsid_root (tsid);
//  char dirname[PATH_MAX];
  char *ascii_key;
  char realfile[PATH_MAX];
  char linkfile[PATH_MAX];

  /* make sure the services directory exists */
  snprintf (dirname + strlen (dirname),
            PATH_MAX - strlen (dirname), "/%s", (char const *) SERVICES_DIR);
  if (mkdir (dirname, 0755) < 0 && errno != EEXIST)
  {
    fatal ("Unable to create services directory '%s': %s", dirname,
           strerror (errno));
    free (dirname);
    return;
  }
  /* BBC use numbers as object keys, so convert to a text value we can use as a file name */
  ascii_key = convert_key (key, key_size);

/*

Carousels shall be considered identical if, in the PMTs of the services, all the following hold:
Either:
a)
Both services are delivered within the same transport stream; and
- Both services list the boot component of the carousel on the same PID.
- The carousel_identifier_descriptor for the carousel are identical in both services (so the carousels have
 the same carousel_Id and boot parameters).
- All association tags used in the carousel map to the same PIDs in both services.
In this case, the carousel is transmitted over a single path, but the services are allowed to reference the carousel via a
number of routes, including deferral to a second PMT via deferred association tags.
Or:
b)
Both services are delivered over multiple transport streams; and
- The carousel_id in the carousel_identifier_descriptor is in the range of 0x100 - 0xffffffff (containing the
 broadcaster's organisation_id in the most significant 24 msbs of carousel_id).
 - The carousel_identifier_descriptor for the carousel are identical in both services (so the carousels have
  the same carousel Id and boot parameters).
*/
  /* create a symlink to the Service Gateway dir */
  snprintf (realfile, sizeof (realfile),
            "../%s/%" PRIu16 "/%" PRIu32 "/%s-%" PRIu16 "-%s",
            (char const *) CAROUSELS_DIR, elementary_pid, carousel_id, kind,
            module_id, ascii_key);
  free (ascii_key);
  snprintf (linkfile, sizeof (linkfile), "%s/%" PRIu16, dirname, service_id);
  free (dirname);

  /*
   * linkfile may already exist if we get an update to an existing module
   * if linkfile already exists, symlink will not update it
   * so delete it first to make sure the link gets updated
   */
  unlink (linkfile);
  if (symlink (realfile, linkfile) < 0 && errno != EEXIST)
    fatal ("Unable to create link '%s' to '%s': %s", linkfile, realfile,
           strerror (errno));

  verbose ("Added service root '%s' -> '%s'", linkfile, realfile);

  return;
}

/*
 * returns a ptr to the directory name
 * the name will be overwritten by the next call to this routine
 */


char *
make_dir (char *kind, uint16_t tsid, uint16_t elementary_pid,
          uint32_t carousel_id, uint16_t module_id, char *key,
          uint32_t key_size)
{
  char _dirname[PATH_MAX];
  char *root;
  char *ascii_key;

  /* make sure the carousel directory exists */
  root = make_carousel_id (tsid, elementary_pid, carousel_id);

  /* BBC use numbers as object keys, so convert to a text value we can use as a file name */
  ascii_key = convert_key (key, key_size);

  snprintf (_dirname, sizeof (_dirname), "%s/%s-%" PRIu16 "-%s", root, kind,
            module_id, ascii_key);
  free (ascii_key);
  free (root);
  /* may already exist if we get an updated version of it */
  if (mkdir (_dirname, 0755) < 0 && errno != EEXIST)
    fatal ("Unable to create directory '%s': %s", _dirname, strerror (errno));

  verbose ("Created directory '%s'", _dirname);

  return strdup (_dirname);
}

/*
 * adds a sym link to the real file to the given directory
 * the real file may not exist if it has not been downloaded yet
 */

void
add_dir_entry (char *dir, char *entry, int entry_size, char *kind,
               uint16_t elementary_pid, uint32_t carousel_id,
               uint16_t module_id, char *key, uint32_t key_size)
{
  char *ascii_key;
  char realfile[PATH_MAX];
  char linkfile[PATH_MAX];

  /* BBC use numbers as object keys, so convert to a text value we can use as a file name */
  ascii_key = convert_key (key, key_size);
//BUG: number of .. is wrong in most cases.. have to keep track of those to get back to mheg root.. links have to stay relative... directory depth is all wrong.. perhaps dl without generating symlinks..
  snprintf (realfile, sizeof (realfile),
            "../../../%" PRIu16 "/%" PRIu32 "/%s-%" PRIu16 "-%s",
            elementary_pid, carousel_id, kind, module_id, ascii_key);
  free (ascii_key);
  snprintf (linkfile, sizeof (linkfile), "%s/%.*s", dir, entry_size, entry);

  /*
   * linkfile may already exist if we get an update to an existing module
   * if linkfile already exists, symlink will not update it
   * so delete it first to make sure the link gets updated
   */
  unlink (linkfile);
  if (symlink (realfile, linkfile) < 0)
    fatal ("Unable to create link '%s' to '%s': %s", linkfile, realfile,
           strerror (errno));

  verbose ("Added directory entry '%s' -> '%s'", linkfile, realfile);

  return;
}

/*
 * BBC use numbers as object keys, ITV/C4 use ascii strings
 * we want an ascii string we can use as a filename
 * returns a static string that will be overwritten by the next call to this function
 */


char *
convert_key (char *key, uint32_t size)
{
  char _ascii_key[PATH_MAX];
  uint32_t i;

  /* assert */
  if (sizeof (_ascii_key) < (size * 2) + 1)
    fatal ("objectKey too long");

  /* convert to a string of hex byte values */
  for (i = 0; i < size; i++)
  {
    _ascii_key[i * 2] = hex_digit ((key[i] >> 4) & 0x0f);
    _ascii_key[(i * 2) + 1] = hex_digit (key[i] & 0x0f);
  }
  _ascii_key[size * 2] = '\0';

  return strdup (_ascii_key);
}
