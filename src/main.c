#define PING() g_print (G_STRLOC "\n")
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
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#ifdef G_OS_WIN32
#include <process.h>
#endif

#define STRICT
#include <windows.h>
#undef STRICT

#include "interface.h"
#include "main.h"
#include "pspi.h"

#include "plugin-intl.h"

/*  Constants  */

#define PSPI_PATH_TOKEN "pspi-path"
#define PSPIRC "pspirc"
#define DEBUGGER_SLEEP_TIME 20000000

#define PSPI_SETTINGS_NAME "pspi_settings"

#define HELP_ABOUT_PREFIX "help_about_"

/*  Function prototypes  */

static void   init  (void);
static void   query (void);
static void   run   (const gchar     *name,
		     gint             nparams,
		     const GimpParam *param,
		     gint      	     *nreturn_vals,
		     GimpParam 	    **return_vals);


gint debug_mask = 0;

GHashTable *handle_hash;

GimpParamDef standard_args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE, "image", "Input image (unused)" },
    { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
  };
gint standard_nargs = sizeof (standard_args) / sizeof (standard_args[0]);
  
/*  Local variables  */

static gchar *search_path = "";

static GHashTable *plug_in_hash;
static GHashTable *entry_hash;

static gboolean pspirc_values_modified;

static GimpPlugInInfo PLUG_IN_INFO =
{
  init,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

static GimpParamDef pspi_settings_args[] =
{
  { GIMP_PDB_INT32,    "run_mode",   "Interactive, non-interactive"       },
  { GIMP_PDB_INT32,    "path_n_entries",   "Number of path entries"       },
  { GIMP_PDB_STRINGARRAY, "search_path", "Search path"                    }
};
static gint pspi_settings_nargs =
  sizeof (pspi_settings_args) / sizeof (pspi_settings_args[0]);

MAIN ()

gchar *
make_pdb_name (const gchar *file,
	       const gchar *entrypoint)
{
  gchar *pdb_name, *tem;
  gchar *last_slash, *last_period;

  last_slash = strrchr (file, G_DIR_SEPARATOR);
  tem = g_strdup (last_slash + 1);
  last_period = strrchr (tem, '.');
  *last_period = '\0';

  pdb_name = g_strdup_printf ("photoshop_plug_in_%s_%s", tem, entrypoint);
  g_free (tem);

  tem = pdb_name;
  while (*tem)
    {
      if (isupper (*tem))
	*tem = tolower (*tem);
      else if (!isalnum (*tem) &&
	       *tem != '_')
	*tem = '_';
      tem++;
    }

  return pdb_name;
}

static void
add_plugin_to_hash_tables (PSPlugIn *pspi)
{
  GList *list;

  g_hash_table_insert (plug_in_hash, pspi->location, pspi);
  list = pspi->entries;
  while (list != NULL)
    {
      PSPlugInEntry *pspie = list->data;
      list = list->next;
      g_hash_table_insert (entry_hash, pspie->pdb_name, pspie);
    }
}

void
add_found_plugin (PSPlugIn *pspi)
{
  pspirc_values_modified = TRUE;

  add_plugin_to_hash_tables (pspi);
}

void
add_entry_to_plugin (PSPlugIn *pspi,
		     gchar    *pdb_name,
		     gchar    *menu_path,
		     gchar    *image_types,
		     gchar    *entrypoint)
{
  PSPlugInEntry *pspie;

  pspie = g_new (PSPlugInEntry, 1);

  pspie->pspi = pspi;
  pspie->pdb_name = g_strdup (pdb_name);
  pspie->menu_path = g_strdup (menu_path);
  pspie->image_types = g_strdup (image_types);
  pspie->entrypoint_name = g_strdup (entrypoint);
  pspie->entry = NULL;

  pspi->entries = g_list_append (pspi->entries, pspie);
}  

static gboolean
check_present (gpointer key,
	       gpointer value,
	       gpointer user_data)
{
  PSPlugIn *pspi = (PSPlugIn *) value;

  if (!pspi->present)
    {
      pspirc_values_modified = TRUE;
      return TRUE;
    }

  return FALSE;
}

static void
save_pspirc_entry (gpointer key,
		   gpointer value,
		   gpointer user_data)
{
  PSPlugIn *pspi = (PSPlugIn *) value;
  FILE *pspirc = (FILE *) user_data;
  PSPlugInEntry *pspie;
  GList *list = pspi->entries;

  fprintf (pspirc, "  <ps-plug-in path=\"%s\" timestamp=\"%d\">\n",
	   pspi->location, pspi->timestamp);

  PSPI_DEBUG (PSPIRC, g_print (" %s", pspi->location));
  while (list != NULL)
    {
      pspie = list->data;
      list = list->next;

      fprintf (pspirc, "    <entrypoint menu-path=\"%s\" image-types=\"%s\" entrypoint=\"%s\"/>\n",
	       pspie->menu_path, pspie->image_types, pspie->entrypoint_name);
    }

  fprintf (pspirc, "  </ps-plug-in>\n");
}

static int depth = 0;

typedef struct {
  PSPlugIn *pspi;
} UserData;

static void
set_error (GMarkupParseContext *context,
	   GError             **error)
{
  GError *tmp_error;
  
  tmp_error = g_error_new (G_MARKUP_ERROR,
			   G_MARKUP_ERROR_INVALID_CONTENT,
			   _("Invalid pspirc syntax"));
  g_propagate_error (error, tmp_error);
}

static void
start_element_handler (GMarkupParseContext *context,
		       const gchar         *element_name,
		       const gchar        **attribute_names,
		       const gchar        **attribute_values,
		       gpointer             user_data,
		       GError             **error)
{
  UserData *ud = (UserData *) user_data;
  int i;
  
  if (strcmp (element_name, "pspi-settings") == 0 &&
      depth == 0)
    {
    }
  else if (strcmp (element_name, "ps-plug-in") == 0 &&
	   depth == 1)
    {
      g_assert (ud->pspi == NULL);
      ud->pspi = g_new (PSPlugIn, 1);
      ud->pspi->present = FALSE;
      ud->pspi->entries = NULL;
      i = 0;
      while (attribute_names[i] != NULL)
	{
	  if (strcmp (attribute_names[i], "path") == 0)
	    ud->pspi->location = g_strdup (attribute_values[i]);
	  else if (strcmp (attribute_names[i], "timestamp") == 0)
	    ud->pspi->timestamp = atoi (attribute_values[i]);
	  else
	    set_error (context, error);
	  i++;
	}
    }
  else if (strcmp (element_name, "entrypoint") == 0 &&
	   depth == 2)
    {
      gchar *menu_path = NULL, *image_types = NULL, *entrypoint = NULL;

      i = 0;
      while (attribute_names[i] != NULL)
	{
	  if (strcmp (attribute_names[i], "menu-path") == 0)
	    if (menu_path != NULL)
	      set_error (context, error);
	    else
	      menu_path = g_strdup (attribute_values[i]);
	  else if (strcmp (attribute_names[i], "image-types") == 0)
	    if (image_types != NULL)
	      set_error (context, error);
	    else
	      image_types = g_strdup (attribute_values[i]);
	  else if (strcmp (attribute_names[i], "entrypoint") == 0)
	    if (entrypoint != NULL)
	      set_error (context, error);
	    else
	      entrypoint = g_strdup (attribute_values[i]);
	  else
	    set_error (context, error);
	  i++;
	}

      if (menu_path == NULL || image_types == NULL || entrypoint == NULL)
	set_error (context, error);
      else
	{
	  gchar *pdb_name = make_pdb_name (ud->pspi->location, entrypoint);
	  add_entry_to_plugin (ud->pspi, pdb_name, menu_path,
			       image_types, entrypoint);
	}
    }
  else
    set_error (context, error);
  depth++;
}

static void
end_element_handler (GMarkupParseContext *context,
		     const gchar         *element_name,
		     gpointer             user_data,
		     GError             **error)
{
  UserData *ud = (UserData *) user_data;

  depth--;
  if (strcmp (element_name, "ps-plug-in") == 0)
    {
      add_plugin_to_hash_tables (ud->pspi);
      ud->pspi = NULL;
    }
}

static void
get_saved_plugin_data (void)
{
  gchar *pspirc_name = gimp_personal_rc_file (PSPIRC);
  gchar *contents;
  gsize length;
  GMarkupParseContext *context;
  static GMarkupParser parser = {
    start_element_handler,
    end_element_handler,
    NULL,
    NULL,
    NULL
  };
  UserData user_data;

  plug_in_hash = g_hash_table_new (g_str_hash, g_str_equal);
  entry_hash = g_hash_table_new (g_str_hash, g_str_equal);

  /* Read data for the PS plug-ins found last time */
  if (!g_file_get_contents (pspirc_name, &contents, &length, NULL))
    {
      g_free (pspirc_name);
      return;
    }

  user_data.pspi = NULL;
  context = g_markup_parse_context_new (&parser, 0, &user_data, NULL);

  if (!g_markup_parse_context_parse (context, contents, length, NULL))
    {
      g_markup_parse_context_free (context);
      g_free (pspirc_name);
      return;
    }

  g_free (contents);
  g_markup_parse_context_end_parse (context, NULL);
  g_markup_parse_context_free (context);
}

static gint
my_ftw (const gchar *path,
	gint       (*function) (const gchar       *filename,
				const struct stat *statp))
{
  DIR *dir;
  struct dirent *dir_ent;

  dir = opendir (path);

  if (dir == NULL)
    {
      PSPI_DEBUG (PSPIRC, g_print ("opendir(%s) failed: %s\n", path, g_strerror (errno))); 
      if (errno == EACCES)
	return 0;
      return -1;
    }

  while ((dir_ent = readdir (dir)) != NULL)
    {
      gchar *file;
      struct stat s;

      if (strcmp (dir_ent->d_name, ".") == 0 ||
	  strcmp (dir_ent->d_name, "..") == 0)
	continue;

      file = g_strconcat (path, G_DIR_SEPARATOR_S,  dir_ent->d_name, NULL);

      if (stat (file, &s) == 0)
	{
	  gint retval;

	  retval = (*function) (file, &s);
	  if (retval == 0 && (s.st_mode & S_IFDIR))
	    retval = my_ftw (file, function);

	  if (retval != 0)
	    {
	      int saved_errno = errno;
	      g_free (file);
	      closedir (dir);
	      errno = saved_errno;
	      return retval;
	    }
	}
      else
	{
	  PSPI_DEBUG (PSPIRC, g_print ("stat(%s) failed: %s\n", file, g_strerror (errno)));
	  if (errno != EACCES)
	    {
	      int saved_errno = errno;
	      g_free (file);
	      closedir (dir);
	      errno = saved_errno;
	      return -1;
	    }
	}
      g_free (file);
    }
  closedir (dir);
  return 0;
}

void
install_pdb (gchar       *pdb_name,
	     const gchar *file,
	     gchar        *menu_path,
	     gchar       *image_types)
{
  gchar *blurb, *menu_path2, *pdb_name2;

  blurb = g_strdup_printf ("Photoshop plug-in %s", file);

  PSPI_DEBUG (PSPIRC, g_print ("Installing %s on %s\n", pdb_name, menu_path));

  gimp_install_procedure (pdb_name,
			  blurb,
			  "",
			  "",
			  "",
			  "",
			  menu_path,
			  image_types,
			  GIMP_PLUGIN,
			  standard_nargs, 0,
			  standard_args, NULL);
  g_free (blurb);
  
  /* Install also a menu entry for the Help About functionality */
  pdb_name2 = g_strconcat (HELP_ABOUT_PREFIX, pdb_name, NULL);
  menu_path2 = g_strconcat (_("<Toolbox>/Help/About Photoshop plug-ins/"), menu_path + strlen (FILTER_MENU_PREFIX), NULL);
  if (strcmp (menu_path2 + strlen (menu_path2) - 3, "...") == 0)
    menu_path2[strlen (menu_path2) - 3] = '\0';
  blurb = g_strdup_printf (_("About Photoshop plug-in %s"), file);

  PSPI_DEBUG (PSPIRC, g_print ("Installing %s on %s\n", pdb_name2, menu_path2));

  gimp_install_procedure (pdb_name2,
			  blurb,
			  "",
			  "",
			  "",
			  "",
			  menu_path2,
			  "",
			  GIMP_PLUGIN,
			  1, 0,
			  standard_args, NULL);
  g_free (blurb);
  g_free (menu_path2);
}

static gint
scan_filter (const gchar       *file,
	     const struct stat *statp)
{
  gchar *suffix = strrchr (file, '.');

  if (suffix != NULL &&
      (g_ascii_strcasecmp (suffix, ".8bf") == 0 ||
       /* Jernej Simoncic says also these suffixes are used. */
       g_ascii_strcasecmp (suffix, ".eff") == 0 ||
       g_ascii_strcasecmp (suffix, ".dll") == 0))
    {
      /* If it hasn't changed since last time, no need
       * to query it again.
       */
      PSPlugIn *pspi = g_hash_table_lookup (plug_in_hash, file);
      if (pspi == NULL ||
	  pspi->timestamp != statp->st_mtime)
	{
	  query_8bf (file, statp);
	}
      else if (pspi != NULL)
	{
	  GList *list = pspi->entries;

	  while (list != NULL)
	    {
	      PSPlugInEntry *pspie = list->data;
	      gchar *pdb_name;

	      list = list->next;
	      pdb_name = make_pdb_name (file, pspie->entrypoint_name);
	      install_pdb (pdb_name, file, pspie->menu_path, pspie->image_types);
	      g_free (pdb_name);
	    }
	  pspi->present = TRUE;
	}
    }
  return 0;
}

static void
scan_search_path (void)
{
  GList *path_list;
  GList *list; 

  /* Scan search_path looking for PS plug-ins */
  path_list = gimp_path_parse (search_path, 99, TRUE, NULL);

  list = path_list;
  while (list != NULL)
    {
      gchar *path = list->data;
      list = list->next;

      my_ftw (path, scan_filter);
    }
}

static void
setup_debug_mask (void)
{
  debug_mask = 0;

#ifdef PSPI_WITH_DEBUGGING
  if (getenv ("PSPI_DEBUG") != NULL)
    {
      gchar *p = getenv ("PSPI_DEBUG");

      while (TRUE)
	{
	  gchar *colon = strchr (p, ':');
	  if (colon != NULL)
	    *colon = '\0';
#define BIT(bit) if (g_ascii_strcasecmp (p, #bit) == 0) debug_mask |= PSPI_DEBUG_##bit
          BIT (ADVANCE_STATE);
	  BIT (BUFFER_SUITE);
	  BIT (CHANNEL_PORT_SUITE);
	  BIT (HANDLE_SUITE);
	  BIT (IMAGE_SERVICES_SUITE);
	  BIT (PROPERTY_SUITE);
	  BIT (RESOURCE_SUITE);
	  BIT (COLOR_SERVICES_SUITE);
	  BIT (SPBASIC_SUITE);
	  BIT (DEBUGGER);
	  BIT (PIPL);
	  BIT (CALL);
	  BIT (MISC_CALLBACKS);
	  BIT (ALL);
	  BIT (VERBOSE);
#undef BIT
	  if (colon != NULL)
	    p = ++colon;
	  else
	    break;
	}
    }

#endif

  PSPI_DEBUG (DEBUGGER, (g_print ("Sleeping %d s, attach pid %d in debugger NOW\n", DEBUGGER_SLEEP_TIME / 1000000, getpid ()),
			 g_usleep (DEBUGGER_SLEEP_TIME),
			 g_print ("Continuing\n")));
}

static void
init (void)
{
  GimpMessageHandlerType old_handler;

  gimp_plugin_domain_register (PLUGIN_NAME, NULL);

  setup_debug_mask ();

  old_handler = gimp_message_get_handler ();
  if (old_handler == GIMP_CONSOLE)
    gimp_message_set_handler (GIMP_MESSAGE_BOX);

  search_path = gimp_gimprc_query (PSPI_PATH_TOKEN);
  
  if (search_path == NULL)
    search_path = g_strdup ("");

  get_saved_plugin_data ();

  pspirc_values_modified = FALSE;
  scan_search_path ();

  /* Forget those PS plug-ins that weren't around any longer. */
  g_hash_table_foreach_remove (plug_in_hash, check_present, NULL);

  /* Rewrite the pspirc file if necessary */
  if (pspirc_values_modified)
    {
      gchar *pspirc_name = gimp_personal_rc_file (PSPIRC);
      gchar *temp_name = g_strconcat (pspirc_name, ".new", NULL);
      gchar *bak_name = g_strconcat (pspirc_name, ".bak", NULL);
      FILE *pspirc = fopen (temp_name, "w");

      if (pspirc == NULL)
	g_message (_("Could not open %s for writing"), temp_name);
      else
	{
	  PSPI_DEBUG (PSPIRC, g_print ("Saving pspirc file\n"));
	  fprintf (pspirc, "<pspi-settings>\n");
	  g_hash_table_foreach (plug_in_hash, save_pspirc_entry, pspirc);
	  fprintf (pspirc, "</pspi-settings>\n");
	  PSPI_DEBUG (PSPIRC, g_print ("\n"));
	  fclose (pspirc);
	  remove (bak_name);
	  if (g_file_test (pspirc_name, G_FILE_TEST_EXISTS) &&
	      rename (pspirc_name, bak_name) != 0)
	    g_message (_("Could not rename %s to %s"),
		       pspirc_name, bak_name);
	  else
	    {
	      if (rename (temp_name, pspirc_name) != 0)
		{
		  g_message (_("Could not rename %s to %s"),
			     temp_name, pspirc_name);
		  if (rename (bak_name, pspirc_name) != 0)
		    g_message (_("Could not rename %s to %s"),
			       bak_name, pspirc_name);
		}
	      else
		remove (bak_name);
	    }
	}
      g_free (pspirc_name);
      g_free (temp_name);
      g_free (bak_name);
    }

  if (old_handler == GIMP_CONSOLE)
    gimp_message_set_handler (GIMP_CONSOLE);
}

static void
query (void)
{
  gimp_plugin_domain_register (PLUGIN_NAME, NULL);

  gimp_install_procedure (PSPI_SETTINGS_NAME,
			  "Set the search path for pspi",
			  "Set the directory trees where the pspi plug-in searches for Photoshop filter plug-ins",
			  "Tor Lillqvist <tml@iki.fi>",
			  "Tor Lillqvist <tml@iki.fi>",
			  "2001",
			  N_("<Toolbox>/Xtns/Photoshop Plug-in Settings..."),
			  "",
			  GIMP_PLUGIN,
			  pspi_settings_nargs, 0,
			  pspi_settings_args, NULL);
}

static GimpPDBStatusType
run_pspi_settings (gint             n_params, 
		   const GimpParam *param)
{
  GimpRunMode run_mode = param[0].data.d_int32;
  GString *sp;
  int i;
  
  switch (run_mode)
    {
    case GIMP_RUN_NONINTERACTIVE:
      if (n_params != pspi_settings_nargs)
	return GIMP_PDB_CALLING_ERROR;

      if (param[1].data.d_int32 <= 0 || param[1].data.d_int32 > 10)
	return GIMP_PDB_CALLING_ERROR;

      sp = g_string_new ("");
      for (i = 0; i < param[1].data.d_int32; i++)
	{
	  if (sp->len > 0)
	    g_string_append_c (sp, G_SEARCHPATH_SEPARATOR);
	  g_string_append (sp, param[2].data.d_stringarray[i]);
	  search_path = sp->str;
	}
      break;

    case GIMP_RUN_INTERACTIVE:
      /*  Possibly retrieve data  */
      i = gimp_get_data_size (PSPI_PATH_TOKEN);
      if (i > 0)
	{
	  search_path = g_malloc (i);
	  gimp_get_data (PSPI_PATH_TOKEN, search_path);
	}
      else
	{
	  search_path = gimp_gimprc_query (PSPI_PATH_TOKEN);
	  if (search_path == NULL)
	    search_path = g_strdup ("");
	}

      if (! pspi_settings_dialog (&search_path))
	return GIMP_PDB_CANCEL;
      gimp_message (_("The new search path will be used next time GIMP is started"));
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      break;

    default:
      break;
    }

  if (search_path == NULL)
    search_path = g_strdup ("");
  gimp_set_data (PSPI_PATH_TOKEN, search_path, strlen (search_path) + 1);
  gimp_gimprc_set (PSPI_PATH_TOKEN, search_path);

  return GIMP_PDB_SUCCESS;
}

static GimpPDBStatusType
run_help_about (const gchar	*pdb_name,
		gint       	 n_params, 
		const GimpParam *param)
{
  GimpRunMode run_mode = param[0].data.d_int32;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  PSPlugInEntry *pspie;
  const gchar *name = pdb_name + strlen (HELP_ABOUT_PREFIX);

  get_saved_plugin_data ();

  if ((pspie = g_hash_table_lookup (entry_hash, name)) != NULL)
    {
      if (run_mode == GIMP_RUN_NONINTERACTIVE)
	{
	  if (n_params != 1)
	    return GIMP_PDB_CALLING_ERROR;
	}
      else if (run_mode == GIMP_RUN_INTERACTIVE)
	{
	  if ((status = pspi_about (pspie)) != GIMP_PDB_SUCCESS)
	    return status;
	}

      return GIMP_PDB_SUCCESS;
    }

  return GIMP_PDB_CALLING_ERROR;
}

static GimpPDBStatusType
run_pspi (const gchar  	  *pdb_name,
	  gint       	   n_params, 
	  const GimpParam *param)
{
  GimpRunMode run_mode = param[0].data.d_int32;
  GimpDrawable *drawable;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  PSPlugInEntry *pspie;
  gint x1, y1, x2, y2;

  get_saved_plugin_data ();

  if ((pspie = g_hash_table_lookup (entry_hash, pdb_name)) != NULL)
    {
      gchar *name;

      if (run_mode == GIMP_RUN_NONINTERACTIVE)
	{
	  if (n_params != standard_nargs)
	    return GIMP_PDB_CALLING_ERROR;
	}
      else if (run_mode == GIMP_RUN_INTERACTIVE)
	{
	  if ((status = pspi_params (pspie)) != GIMP_PDB_SUCCESS)
	    return status;
	}
      drawable = gimp_drawable_get (param[2].data.d_drawable);

      gimp_ui_init (PLUGIN_NAME, TRUE);

      if ((status = pspi_prepare (pspie, drawable)) != GIMP_PDB_SUCCESS)
	return status;

      name = g_strdup_printf (_("Applying %s:"),
			      strrchr (pspie->menu_path, '/') + 1);
      gimp_progress_init (name);
      g_free (name);

      if ((status = pspi_apply (pspie, drawable)) != GIMP_PDB_SUCCESS)
	return status;

      gimp_drawable_flush (drawable);
      gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
      gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);
      gimp_drawable_update (drawable->drawable_id, x1, y1, (x2 - x1), (y2 - y1));
      gimp_displays_flush ();

      return GIMP_PDB_SUCCESS;
    }

  return GIMP_PDB_CALLING_ERROR;
}

static void
run (const gchar     *name, 
     gint             n_params, 
     const GimpParam *param, 
     gint            *nreturn_vals,
     GimpParam      **return_vals)
{
  static GimpParam values[1];
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;

  setup_debug_mask ();

  *nreturn_vals = 1;
  *return_vals  = values;

  /*  Initialize i18n support  */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
  textdomain (GETTEXT_PACKAGE);

  if (strcmp (name, PSPI_SETTINGS_NAME) == 0)
    status = run_pspi_settings (n_params, param);
  else if (strncmp (name, HELP_ABOUT_PREFIX, strlen (HELP_ABOUT_PREFIX)) == 0)
    status = run_help_about (name, n_params, param);
  else
    status = run_pspi (name, n_params, param);

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
}
