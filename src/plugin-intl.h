/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * plugin-intl.h
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __PLUGIN_INTL_H__
#define __PLUGIN_INTL_H__

#include <locale.h>

#ifdef G_OS_WIN32
/* Don't use hardcoded path */
#undef LOCALEDIR
/* Assume the plug-in's message catalog is installed in the same location
 * as GIMP's.
 */
#define LOCALEDIR g_strconcat (gimp_toplevel_directory (), \
			       "\\lib\\locale", \
			       NULL)
#endif /* G_OS_WIN32 */

#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif

#define INIT_LOCALE(domain)	G_STMT_START{	\
	gtk_set_locale ();			\
	setlocale (LC_NUMERIC, "C");		\
	bindtextdomain (domain, LOCALEDIR);	\
	textdomain (domain);			\
				}G_STMT_END


#ifdef HAVE_LC_MESSAGES
#define INIT_I18N()	G_STMT_START{	        \
  setlocale(LC_MESSAGES, ""); 			\
  bindtextdomain("gimp-libgimp", LOCALEDIR);    \
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);	\
  textdomain(GETTEXT_PACKAGE);		      	\
  			}G_STMT_END
#else
#define INIT_I18N()	G_STMT_START{		\
  bindtextdomain("gimp-libgimp", LOCALEDIR);    \
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);	\
  textdomain(GETTEXT_PACKAGE);			\
  			}G_STMT_END
#endif

#define INIT_I18N_UI()	G_STMT_START{	\
  gtk_set_locale();			\
  setlocale (LC_NUMERIC, "C");		\
  INIT_I18N();				\
			}G_STMT_END

#endif /* __PLUGIN_INTL_H__ */
