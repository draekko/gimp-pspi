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

#ifndef __PSPI_H__
#define __PSPI_H__

void              query_8bf    (const gchar         *file,
                                const struct stat   *st);

GimpPDBStatusType pspi_about   (PSPlugInEntry *pspie);

GimpPDBStatusType pspi_params  (PSPlugInEntry *pspie);

GimpPDBStatusType pspi_prepare (PSPlugInEntry *pspie,
				GimpDrawable  *drawable);

GimpPDBStatusType pspi_apply   (PSPlugInEntry *pspie,
                                GimpDrawable  *drawable);

#endif /* __PSPI_H__ */
