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
#include <sys/stat.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "interface.h"
#include "main.h"
#include "plugin-intl.h"

static void
pspi_settings_dialog_ok_callback (GtkWidget *widget,
		    gpointer   data)
{
  gchar     **path;
  GtkObject  *path_editor;
  
  path        = gtk_object_get_data (GTK_OBJECT (data), "path");
  path_editor = gtk_object_get_data (GTK_OBJECT (data), "path_editor");

  *path = gimp_path_editor_get_path (GIMP_PATH_EDITOR (path_editor));

  pspi_settings_dialog_ok = TRUE;

  gtk_widget_destroy (GTK_WIDGET (data));
}

gboolean
pspi_settings_dialog (gchar **search_path)
{
  GtkWidget *dlg;
  GtkWidget *main_vbox;
  GtkWidget *path_editor;
  GtkWidget *label;
  gchar     *new_path = NULL;

  gimp_ui_init (PLUGIN_NAME, FALSE);

  dlg = gimp_dialog_new (_("Photoshop Plug-in Settings"), PLUGIN_NAME,
			 gimp_standard_help_func, PLUGIN_NAME".html",
			 GTK_WIN_POS_MOUSE,
			 FALSE, TRUE, FALSE,

			 _("OK"), pspi_settings_dialog_ok_callback,
			 NULL, NULL, NULL, TRUE, FALSE,
			 _("Cancel"), gtk_widget_destroy,
			 NULL, 1, NULL, FALSE, TRUE,

			 NULL);

  gtk_signal_connect (GTK_OBJECT (dlg), "destroy",
		      GTK_SIGNAL_FUNC (gtk_main_quit),
		      NULL);

  /*  Initialize tooltips  */
  gimp_help_init ();

  main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dlg)->vbox), main_vbox);

  path_editor = gimp_path_editor_new (_("Directories with Photoshop Plug-ins"), *search_path);
  gtk_container_set_border_width (GTK_CONTAINER (path_editor), 6);
  gtk_widget_set_usize (GTK_WIDGET (path_editor), -1, 240);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), 
		      path_editor, TRUE, TRUE, 0);

  label = gtk_label_new (_("You may specify multiple directories here."));
  gtk_misc_set_padding (GTK_MISC (label), 6, 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), 
		      label, FALSE, FALSE, 0);

  gtk_object_set_data (GTK_OBJECT (dlg), "path", &new_path);
  gtk_object_set_data (GTK_OBJECT (dlg), "path_editor", path_editor);

  gtk_widget_show_all (dlg);

  gtk_main ();

  g_print (G_STRLOC ": new path: %s, old path: %s\n", (new_path ? new_path : "NULL"), *search_path);

  if (new_path &&
      (strcmp (*search_path, new_path) != 0))
    {
      g_free (*search_path);
      *search_path = new_path;
    }

  /*  Free tooltips  */
  gimp_help_free ();

  return pspi_settings_dialog_ok;
}

