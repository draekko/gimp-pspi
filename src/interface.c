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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "interface.h"
#include "main.h"
#include "plugin-intl.h"

gboolean
pspi_settings_dialog (gchar **search_path)
{
  GtkWidget *dlg;
  GtkWidget *main_vbox;
  GtkWidget *path_editor;
  GtkWidget *label;
  gboolean   run = FALSE;
  gchar     *new_path = NULL;

  gimp_ui_init (PLUGIN_NAME, TRUE);

  dlg = gimp_dialog_new (_("Photoshop Plug-in Settings"), PLUGIN_NAME,
			 NULL, 0, 
			 gimp_standard_help_func, PLUGIN_NAME,

			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 GTK_STOCK_OK,     GTK_RESPONSE_OK,

			 NULL);

  main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);

  path_editor = gimp_path_editor_new (_("Directories with Photoshop Plug-ins"), *search_path);
  gtk_container_set_border_width (GTK_CONTAINER (path_editor), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), 
		      path_editor, TRUE, TRUE, 0);

  label = gtk_label_new (_("Specify directories where to look for Photoshop plug-ins."));
  gtk_misc_set_padding (GTK_MISC (label), 6, 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), 
		      label, FALSE, FALSE, 0);

  gtk_widget_show_all (dlg);

  run = (gimp_dialog_run (GIMP_DIALOG (dlg)) == GTK_RESPONSE_OK);

  if (run)
    {
      new_path = gimp_path_editor_get_path (GIMP_PATH_EDITOR (path_editor));
      if (new_path && (strcmp (*search_path, new_path) != 0))
	{
	  g_free (*search_path);
	  *search_path = new_path;
	}
    }

  return run;
}
