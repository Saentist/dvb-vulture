#ifndef __rcfile_h
#define __rcfile_h
#include <stdio.h>              //FILE structure

/**\file rcfile.h
 *\brief simple config file parser
 *
 *Can be used to parse a simple two level config file with sections and key-value pairs contained therein.
 *functions that return pointers will return zero on error, functions that return integers will return values unequal zero on error unless specified differently.
 */

 /**\brief holds a key-value pair
  */
typedef struct
{
  ///the key
  char *key;
  ///the corresponding value
  char *value;
} KvPair;


/**\brief a section
 *
 *stores all key-value pairs of a section
 */
typedef struct
{
  ///number of pairs
  int kv_count;
  ///name of a section
  char *name;
  ///array (dynamic) of pairs
  KvPair *pairs;
} Section;

/**\brief top level structure
  *
  *stores all secions in the file
  */
typedef struct
{
  ///number of sections
  int section_count;
  ///arrau of sections (dynamic)
  Section *sections;
} RcFile;

/**\brief is used to parse a config file
 *\param data pointer to structure to fill
 *\param cfg filename of input file.
 */
int rcfileParse (char *cfg, RcFile * data);
/**
 *\brief deallocates the datastructure's internal memory
 *
 *\param data pointer to structure to clear
 */
int rcfileClear (RcFile * data);
/**
 *\brief finds a section by name
 *
 *\param data pointer to structure to search
 *\param section name of the section without the []
 *\return pointer to a section
 */
Section *rcfileFindSec (RcFile * data, char *section);
/**
 *\brief finds a value inside section by name
 *
 *\param data pointer to the section to search
 *\param key name of the key
 *\return pointer to a key value pair.
 */
KvPair *rcfileFindPair (Section * data, char *key);

/**
 *\brief finds value by keyname and sectionname
 *
 *\param data pointer to structure to search
 *\param section name of the section
 *\param key name of the key
 *\param value is used to return the value
 *\return 0 on success
 */
int rcfileFindVal (RcFile * data, char *section, char *key, char **value);
/**
*\brief finds integer value by keyname and sectionname
*
*\param data pointer to structure to search
*\param section name of the section
*\param key name of the key
*\param value is used to return the value
*\return 0 on success
*/
int rcfileFindValInt (RcFile * data, char *section, char *key, long *value);
/**
*\brief finds double value by keyname and sectionname
*
*\param data pointer to structure to search
*\param section name of the section
*\param key name of the key
*\param value is used to return the value
*\return 0 on success
*/
int rcfileFindValDouble (RcFile * data, char *section, char *key,
                         double *value);

/**
*\brief finds value inside a section by keyname
*
*\param value is used to return the value
*\param section section to search
*\param key name of the key
*\return 0 on success
*/
int rcfileFindSecVal (Section * section, char *key, char **value);
/**
*\brief finds integer value inside a section by keyname
*
*\param value is used to return the value
*\param section section to search
*\param key name of the key
*\return 0 on success
*/
int rcfileFindSecValInt (Section * section, char *key, long *value);
/**
 *\brief finds double value inside a section by keyname
 *
 *\param section section to search
 *\param key name of the key
 *\param value is used to return the value
 *\return 0 on success
 */
int rcfileFindSecValDouble (Section * section, char *key, double *value);

/**
 *\brief check if rcfile exists
 *
 *either as a file(LINUX)
 *or as registry key(WIN32)
 *\param cfg filename of input file.
 *\return 1 on success, 0 on error(does not exist or is not readable)
 */
int rcfileExists (char *cfg);

/**
 *\brief creates empty rcfile
 *
 *The registry key/file is created
 *(empty)
 *\param cfg filename of input file.
 *\return 0 on success, 1 on error
 */
int rcfileCreate (char *cfg);

#endif
