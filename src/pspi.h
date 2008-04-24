/* pspi -- a GIMP plug-in to interface to Photoshop plug-ins.
 *
 * Copyright (C) 2001 Tor Lillqvist
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
