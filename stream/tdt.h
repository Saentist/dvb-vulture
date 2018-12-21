#ifndef __tdt_h
#define __tdt_h
#include <stdint.h>
/**
 *\file tdt.h
 *\brief Time and Date Table
 */

///convert a raw tdt to a UTC linux timestamp
time_t tdtParse (uint8_t * raw_tdt);

#endif //__tdt_h
