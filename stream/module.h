/*
 * module.h
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

#ifndef __MODULE_H__
#define __MODULE_H__

#include <stdint.h>
#include "bool2.h"
#include <limits.h>
#include <sys/types.h>

#include "dsmcc.h"
#include "assoc.h"


/* functions */
struct module *find_module (Carousel *, uint16_t, uint8_t, uint32_t);
struct module *add_module (Carousel *, struct DownloadInfoIndication *,
                           struct DIIModule *);
void delete_module (Carousel *, uint32_t);
void free_module (struct module *);
void download_block (uint16_t tsid, uint16_t current_pid, Carousel *,
                     struct module *, uint16_t, unsigned char *, uint32_t);

int uncompress_module (struct module *);

#endif /* __MODULE_H__ */
