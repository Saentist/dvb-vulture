#ifndef __sit_com_h
#define __sit_com_h
#include <stdint.h>
#include "bool2.h"
#include "trn_strm.h"
/**
 *\file sit_com.h
 *\brief Selection Information Compiler
 *
 *to inject selection information (SIT) in output streams.
 *includes DIT
 *
 *TODO: ETR 211 lists a few additional constraints regarding partial TS.
 *(someone else, not me)
 */

/**
 *\brief generates a discontinuity information section with the indicated changes
 *
 *\param ts_change specifies the transition_flag
 *\param section should point to buffer large enough for section(4096 bytes)
 *\return (total) section size
 */
int ditCompile (bool ts_change, uint8_t * section);

/**
 *\brief generates a selection information section with the indicated program numbers
 */
int sitCompile (uint16_t * pnrs, int num_pnrs, uint8_t vnr,
                uint8_t * section);
/**
 *\brief writes a continuity counter to a ts packet
 */
int ts_put_ctr (uint8_t * ts, uint8_t ctr);     //inserts continuity counter
/**
 *\brief packetizes a section into a sequence of ts packets
 */
uint8_t *sec_to_ts (uint8_t * raw_section, uint32_t len, uint16_t pid,
                    uint32_t * num_ts_ret);

#endif
