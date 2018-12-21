#ifndef __opt_h
#define __opt_h
/**
 *\file opt.h
 *\brief cheap cmdline handling
 */
char *find_opt_arg (int argc, char *argv[], char *key);
int find_opt (int argc, char *argv[], char *key);

#endif
