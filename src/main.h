/* pspi -- a GIMP plug-in to interface to Photoshop plug-ins.
 *
 * Copyright (C) 2001 Tor Lillqvist
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __MAIN_H__
#define __MAIN_H__

typedef struct PIentrypoint_ PIentrypoint;

typedef struct {
  guint timestamp;
  gchar *location;
  gboolean present;
  GList *entries;
} PSPlugIn;

typedef struct {
  PSPlugIn *pspi;
  gchar *pdb_name;
  gchar *menu_path;
  gchar *image_types;
  gchar *entrypoint_name;
  PIentrypoint *entry;
} PSPlugInEntry;

extern gint debug_mask;

#define PSPI_DEBUG_ADVANCE_STATE	(1<<0)
#define PSPI_DEBUG_BUFFER_SUITE		(1<<1)
#define PSPI_DEBUG_CHANNEL_PORT_SUITE	(1<<2)
#define PSPI_DEBUG_HANDLE_SUITE		(1<<3)
#define PSPI_DEBUG_IMAGE_SERVICES_SUITE	(1<<4)
#define PSPI_DEBUG_PROPERTY_SUITE	(1<<5)
#define PSPI_DEBUG_RESOURCE_SUITE	(1<<6)
#define PSPI_DEBUG_COLOR_SERVICES_SUITE	(1<<7)
#define PSPI_DEBUG_SPBASIC_SUITE	(1<<8)
#define PSPI_DEBUG_DEBUGGER		(1<<9)
#define PSPI_DEBUG_PIPL			(1<<10)
#define PSPI_DEBUG_CALL			(1<<11)
#define PSPI_DEBUG_PSPIRC		(1<<12)
#define PSPI_DEBUG_MISC_CALLBACKS	(1<<30)
#define PSPI_DEBUG_ANY			(~0)
#define PSPI_DEBUG_ALL			PSPI_DEBUG_ANY
#define PSPI_DEBUG_VERBOSE		(PSPI_DEBUG_ALL & ~PSPI_DEBUG_DEBUGGER)

#ifdef PSPI_WITH_DEBUGGING
  #define PSPI_DEBUG(bit, stmt)				\
    G_STMT_START { 					\
      if (debug_mask & PSPI_DEBUG_##bit)		\
	stmt;						\
    } G_STMT_END
#else
 #define PSPI_DEBUG(bit, stmt)
#endif

#define FILTER_MENU_PREFIX "<Image>/Filters/"

extern GimpParamDef standard_args[];
extern gint standard_nargs;

void   install_pdb         (gchar       *pdb_name,
			    const gchar *file,
			    gchar       *menu_path,
			    gchar       *image_types);

gchar *make_pdb_name       (const gchar *file,
			    const gchar *entrypoint);

void   add_found_plugin    (PSPlugIn    *pspi);

void   add_entry_to_plugin (PSPlugIn    *pspi,
			    gchar       *pdb_name,
			    gchar       *menu_path,
			    gchar       *image_types,
			    gchar       *entrypoint);

#endif /* __MAIN_H__ */
